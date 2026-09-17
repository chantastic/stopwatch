#!/usr/bin/env python3
"""Actual badge AP + desktop-browser fixture test. Temporarily changes Mac Wi-Fi.
Never prints AP password, nonce, original Wi-Fi name or profile values. Requires
pyserial, Pillow, playwright and a local Chrome executable. Run only against a
blank conference profile or the synthetic fixture created by this script.
"""
import argparse, http.client, importlib.util, io, json, re, secrets, socket, subprocess, time
from pathlib import Path
from urllib.parse import urlsplit
from PIL import Image, ImageDraw
from playwright.sync_api import sync_playwright

spec=importlib.util.spec_from_file_location('usbverify',Path(__file__).with_name('verify-conference.py'))
usb=importlib.util.module_from_spec(spec);spec.loader.exec_module(usb)

def run(command,timeout=12):
    return subprocess.run(command,capture_output=True,text=True,timeout=timeout)

def wait_http():
    until=time.monotonic()+12
    while time.monotonic()<until:
        try:
            with socket.create_connection(('192.168.4.1',80),timeout=1): return
        except OSError:time.sleep(.25)
    raise RuntimeError('Badge portal did not become reachable from Mac Wi-Fi')

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True);parser.add_argument('--output',required=True)
    parser.add_argument('--phase',choices=['create','edit','cancel'],required=True)
    parser.add_argument('--browser-http-bridge',action='store_true',help='Relay browser HTTP over the Mac socket path when headless Chrome reports offline; explicitly recorded in evidence')
    args=parser.parse_args();out=Path(args.output);out.mkdir(parents=True,exist_ok=True)
    d=usb.Device(args.port);before=d.status()
    if args.phase=='create':
        assert not before['name_present'] and not before['configured_mask'] and not before['avatar'], 'Refusing to replace a personal badge'
    else: assert before['name_present'] and before['avatar'], 'Expected prior fixture'
    old=run(['networksetup','-getairportnetwork','en0']).stdout.strip()
    old=old.split(': ',1)[1] if old.startswith('Current Wi-Fi Network: ') else None
    playwright=sync_playwright().start()
    browser=playwright.chromium.launch(executable_path='/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless=True,args=['--no-proxy-server'])
    password=secrets.token_hex(10);d.send({'op':'setup_test','password':password})
    setup=json.loads(d.response(b'CONFERENCE_SETUP '));assert setup['active'];ssid=setup['ssid']
    joined=False;report={'phase':args.phase,'desktop_browser':True,'phone_captive_popup_tested':False,'http_via_local_bridge':args.browser_http_bridge}
    try:
        time.sleep(2)
        result=run(['networksetup','-setairportnetwork','en0',ssid,password],timeout=15)
        if result.returncode or result.stdout.strip():
            # Tool errors may include network names: keep only generic outcome.
            raise RuntimeError('macOS did not associate with the test badge hotspot')
        joined=True;wait_http()
        if browser:
            page=browser.new_page(viewport={'width':390,'height':844})
            if args.browser_http_bridge:
                # Exercise the real served HTML/JS and real AP endpoints. Only
                # Chrome's unavailable direct network path is replaced; no
                # responses, photo conversion, or device behavior are mocked.
                def relay(route):
                    request=route.request;url=urlsplit(request.url)
                    if url.scheme!='http' or url.hostname!='192.168.4.1':
                        route.abort();return
                    connection=http.client.HTTPConnection('192.168.4.1',80,timeout=10)
                    try:
                        headers={key:value for key,value in request.headers.items() if key.lower() not in ('host','content-length','connection','accept-encoding')}
                        connection.request(request.method,url.path or '/',body=request.post_data_buffer,headers=headers)
                        response=connection.getresponse();body=response.read()
                        route.fulfill(status=response.status,headers=dict(response.getheaders()),body=body)
                    finally:connection.close()
                page.route('**/*',relay)
            page.goto('http://192.168.4.1/',wait_until='domcontentloaded',timeout=10000)
            if args.phase=='create':
                assert page.locator('#name').input_value()==''
                assert all(page.locator('#'+x).input_value()=='' for x in ['github','x','linkedin'])
                page.screenshot(path=str(out/'empty-phone-layout.png'),full_page=True)
                page.locator('#name').fill('Conference Fixture Attendee With A Deliberately Long Name')
                page.locator('#github').fill('conference-fixture')
                fixture=Image.new('RGB',(640,480),'#2c256a');draw=ImageDraw.Draw(fixture)
                draw.ellipse((170,40,470,340),fill='#eeb4ef');draw.rectangle((160,360,480,480),fill='#8a73ff')
                blob=io.BytesIO();fixture.save(blob,format='PNG')
                page.locator('#photo').set_input_files({'name':'synthetic-fixture.png','mimeType':'image/png','buffer':blob.getvalue()})
            else:
                assert page.locator('#name').input_value()=='Conference Fixture Attendee With A Deliberately Long Name'
                assert page.locator('#github').input_value()=='https://github.com/conference-fixture'
                if args.phase=='edit':
                    assert page.locator('#x').input_value()=='' and page.locator('#linkedin').input_value()==''
                    page.locator('#x').fill('badge_fixture');page.locator('#linkedin').fill('conference-fixture')
                else:
                    assert page.locator('#x').input_value()=='https://x.com/badge_fixture'
                    assert page.locator('#linkedin').input_value()=='https://www.linkedin.com/in/conference-fixture/'
                    page.locator('#name').fill('This change must be discarded')
                    page.locator('#github').fill('discarded-change')
            page.locator('#cancel' if args.phase=='cancel' else '#save').click()
            page.locator('#form').wait_for(state='hidden',timeout=12000)
            message=page.locator('#status').inner_text()
            assert ('Cancelled.' if args.phase=='cancel' else 'Saved on your badge.') in message
            # Screenshot contains only the acknowledgment; form is hidden.
            page.screenshot(path=str(out/f'{args.phase}-phone-layout-ack.png'))
            report['ack_visible_before_ap_shutdown']=True
            browser.close()
        time.sleep(3.5);state=d.status()
        assert state['wifi_mode']==0 and state['bluetooth']==0 and not state['setup']
        assert state['avatar'] and state['configured_mask']==(1 if args.phase=='create' else 7)
        report['radios_off_after_ack']=True;report['passed']=True
        (out/f'{args.phase}.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps(report))
    finally:
        # Always close our temporary setup and restore the workstation network.
        ip=run(['ipconfig','getifaddr','en0']).stdout.strip()
        print(json.dumps({'cleanup_wifi_on_badge_subnet':ip.startswith('192.168.4.')}))
        try:
            cleanup=d.status()
            print(json.dumps({'cleanup_setup':cleanup['setup'],'cleanup_ap_clients':cleanup.get('ap_clients')}))
            if cleanup['setup']:d.action({'op':'button','value':'blue'})
        except Exception:pass
        d.close()
        browser.close();playwright.stop()
        run(['networksetup','-removepreferredwirelessnetwork','en0',ssid])
        if old:run(['networksetup','-setairportnetwork','en0',old],timeout=15)
        elif joined:
            run(['networksetup','-setairportpower','en0','off']);time.sleep(.5)
            run(['networksetup','-setairportpower','en0','on']);time.sleep(5)

if __name__=='__main__':main()
