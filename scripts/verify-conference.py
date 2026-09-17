#!/usr/bin/env python3
"""Bounded USB dispatch/frame verification; never claims physical touch tests.
Requires pyserial, Pillow, zxing-cpp. Output must remain private under .build.
"""
import argparse, json, time
from pathlib import Path
import serial
from PIL import Image, ImageDraw
import zxingcpp

class Device:
    def __init__(self, port):
        self.serial=serial.Serial(port,115200,timeout=.2)
    def send(self, obj):
        self.serial.write(json.dumps(obj).encode()+b"\n")
    def response(self, prefix, timeout=6):
        until=time.monotonic()+timeout
        while time.monotonic()<until:
            line=self.serial.readline()
            if line.startswith(prefix): return line[len(prefix):].strip()
        raise RuntimeError("No expected device acknowledgment: "+prefix.decode())
    def status(self):
        self.send({"op":"status"})
        return json.loads(self.response(b"CONFERENCE_STATUS "))
    def action(self,obj):
        self.send(obj)
        return json.loads(self.response(b"CONFERENCE_STATUS "))
    def page(self,target):
        state=self.status()
        for _ in range(5):
            if state["page"]==target:return state
            state=self.action({"op":"page","step":1})
        raise AssertionError("Page not reachable")
    def touch(self,phase,x,y):
        return self.action({"op":"touch","phase":phase,"x":x,"y":y})
    def swipe(self,x0,y0,x1,y1):
        self.touch("begin",x0,y0)
        for i in range(1,4):
            self.touch("move",x0+(x1-x0)*i//3,y0+(y1-y0)*i//3)
        return self.touch("end",x1,y1)
    def tap(self,x,y):
        self.touch("begin",x,y)
        return self.touch("end",x,y)
    def capture(self,path):
        self.send({"op":"capture_badge"})
        size=self.response(b"BADGE_CAPTURE ").split()
        w,h=map(int,size);assert (w,h)==(468,468)
        total=w*h*3;payload=bytearray();until=time.monotonic()+8
        while len(payload)<total and time.monotonic()<until:
            payload.extend(self.serial.read(total-len(payload)))
        assert len(payload)==total,"Incomplete frame"
        self.response(b"BADGE_CAPTURE_END",3)
        picture=Image.frombytes("RGB",(w,h),bytes(payload));picture.save(path)
        return picture
    def close(self): self.serial.close()

def qr(image):
    whole=[r.text for r in zxingcpp.read_barcodes(image)]
    mask=Image.new('L',image.size,0);ImageDraw.Draw(mask).ellipse((2,2,466,466),fill=255)
    crop=Image.new('RGB',image.size,'black');crop.paste(image,mask=mask)
    circular=[r.text for r in zxingcpp.read_barcodes(crop)]
    return whole,circular

def sheet(images,path):
    result=Image.new('RGB',(468*len(images),506),'#171719');draw=ImageDraw.Draw(result)
    for i,(label,picture) in enumerate(images):
        mask=Image.new('L',(468,468),0);ImageDraw.Draw(mask).ellipse((2,2,466,466),fill=255)
        result.paste(picture,(468*i,0),mask);draw.text((468*i+24,480),label,fill='white')
    result.save(path)

def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port',required=True);parser.add_argument('--output',required=True)
    parser.add_argument('--mode',choices=['scaffold','profile','final'],default='scaffold')
    args=parser.parse_args();out=Path(args.output);out.mkdir(parents=True,exist_ok=True)
    d=Device(args.port);report={'mode':args.mode,'physical_controls_tested':False};images=[]
    try:
        state=d.status();assert state['board']==30 and state['flash_bytes']==16777216 and state['psram_bytes']==8388608
        assert state['store_ready'] and state['clock_valid'] and state['rtc']
        assert state['wifi_mode']==0 and state['bluetooth']==0 and not state['setup']
        report['initial_status']=state
        if args.mode in ('scaffold','final'):
            assert not state['configured_mask'] and not state['avatar'] and not state['name_present'], 'Refuse to capture/test a nonempty personal profile'
            d.page(0);frame=d.capture(out/'00-init.png');time.sleep(.3)
            later=d.capture(out/'00-init-later.png');assert frame.tobytes()!=later.tobytes();report['animation_changes']=True
            names=['init()','Schedule','After Dark','Badge','Hack your Badge']
            for index,name in enumerate(names):
                d.page(index);picture=d.capture(out/f'{index:02d}-page.png');images.append((name,picture))
                decoded=qr(picture)
                if index==4: assert decoded==(['https://drop.workos.cloud/stopwatch'],['https://drop.workos.cloud/stopwatch'])
                if index in (2,3): assert decoded==([],[]), 'Unexpected QR on placeholder'
            sheet(images,out/'five-pages.png')
            state=d.page(0)
            for step in (1,-1):
                for _ in range(5):
                    expected=(state['page']+step)%5;state=d.action({'op':'page','step':step});assert state['page']==expected
            for value in ('blue','yellow'):
                before=d.status();reverse=before['rotation']==2
                direction=(-1 if reverse else 1)*(1 if value=='blue' else -1)
                state=d.action({'op':'button','value':value});assert state['page']==(before['page']+direction)%5
            d.page(1);d.swipe(234,360,234,145);state=d.swipe(234,360,234,145)
            assert state['scroll']==state['scroll_max'];d.capture(out/'schedule-last-row.png')
            d.swipe(234,145,234,360);state=d.swipe(234,145,234,360);assert state['scroll']==0
            d.page(3)
            for _ in range(3):
                before=d.status();frame=d.capture(out/f"empty-network-{before['network']}.png");assert qr(frame)==([],[])
                state=d.swipe(234,345,234,260);assert state['page']==3 and state['network']==(before['network']+1)%3
            # Drag out/back is not a tap-to-configure gesture.
            d.touch('begin',234,320);d.touch('move',234,270);d.touch('move',234,320);state=d.touch('end',234,320);assert not state['setup']
            state=d.tap(234,320);assert state['setup'] and state['wifi_mode']==2
            before=state
            state=d.swipe(320,250,110,250);assert state['setup'] and state['page']==before['page'] and state['network']==before['network']
            state=d.swipe(234,345,234,170);assert state['network']==before['network']
            state=d.tap(234,423);assert not state['setup'] and state['wifi_mode']==0
            state=d.action({'op':'button','value':'both'});assert state['setup']
            state=d.action({'op':'button','value':'blue'});assert not state['setup'] and state['wifi_mode']==0
            report['navigation_and_setup_dispatch']=True
        elif args.mode=='profile':
            expected={0:'https://github.com/conference-fixture',1:'https://x.com/badge_fixture',2:'https://www.linkedin.com/in/conference-fixture/'}
            d.page(3)
            for _ in range(3):
                state=d.status();slot=state['network'];picture=d.capture(out/f'profile-{slot}.png')
                wanted=[expected[slot]] if state['configured_mask']&(1<<slot) else []
                assert qr(picture)==(wanted,wanted),f'QR mismatch slot {slot}'
                images.append((['GitHub','X / Twitter','LinkedIn'][slot],picture))
                if wanted:
                    state=d.tap(234,320);assert state['expanded'];expanded=d.capture(out/f'profile-{slot}-expanded.png');assert qr(expanded)==(wanted,wanted)
                    d.tap(234,320)
                d.swipe(234,345,234,260)
            sheet(images,out/'profile-networks.png');report['qr_slots_and_circle']=True
        before=d.status();time.sleep(1.4);d.send({'op':'reboot'});d.serial.close();time.sleep(2)
        d=Device(args.port);state=d.status()
        assert state['page']==0 and state['network']==before['network']
        assert state['configured_mask']==before['configured_mask'] and state['avatar']==before['avatar']
        assert state['clock_valid'] and state['wifi_mode']==0 and state['bluetooth']==0
        report['reboot_restoration']=True;report['final_status']=state;report['passed']=True
        (out/'verification.json').write_text(json.dumps(report,indent=2)+'\n')
        print(json.dumps({'passed':True,'mode':args.mode,'output':str(out),'physical_controls_tested':False}))
    finally:d.close()

if __name__=='__main__':main()
