#!/usr/bin/env python3
"""Actual AP slow-upload test; uses USB input dispatch, not physical pushers.

No profile is saved. Temporarily joins the badge hotspot and restores normal
Wi-Fi in finally. Requires pyserial, Pillow and zxing-cpp for the USB helper.
"""
import argparse
import http.client
import importlib.util
import json
from pathlib import Path
import re
import secrets
import socket
import subprocess
import time

spec = importlib.util.spec_from_file_location('usbverify', Path(__file__).with_name('verify-conference.py'))
usb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(usb)


def run(command):
    return subprocess.run(command, capture_output=True, text=True, timeout=15)


def index():
    last = None
    for _ in range(12):
        connection = http.client.HTTPConnection('192.168.4.1', 80, timeout=1)
        try:
            connection.request('GET', '/')
            response = connection.getresponse()
            assert response.status == 200
            return response.read().decode()
        except OSError as error:
            last = error
            time.sleep(.25)
        finally:
            connection.close()
    raise RuntimeError('Badge HTTP page was not reachable') from last


def slow_request(nonce):
    connection = socket.create_connection(('192.168.4.1', 80), timeout=2)
    header = ('POST /image HTTP/1.1\r\nHost: 192.168.4.1\r\n'
              'Content-Type: image/jpeg\r\nContent-Length: 4096\r\n'
              f'X-Conference-Nonce: {nonce}\r\nConnection: close\r\n\r\n')
    connection.sendall(header.encode())
    connection.setblocking(False)
    return connection


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', required=True)
    parser.add_argument('--output', required=True)
    args = parser.parse_args()
    output = Path(args.output)
    output.mkdir(parents=True, exist_ok=True)
    device = usb.Device(args.port)
    ssid = None
    connection = None
    report = {'physical_controls_tested': False, 'profile_write_attempted': False}
    try:
        before = device.status()
        assert not before['setup']
        password = secrets.token_hex(10)
        device.send({'op': 'setup_test', 'password': password})
        setup = json.loads(device.response(b'CONFERENCE_SETUP '))
        assert setup['active']
        ssid = setup['ssid']
        time.sleep(2)
        joined = run(['networksetup', '-setairportnetwork', 'en0', ssid, password])
        if joined.returncode or joined.stdout.strip():
            raise RuntimeError('macOS did not associate with the badge hotspot')
        page = index()
        nonce = re.search(r"const nonce='([a-f0-9]+)'", page).group(1)
        report['ap_clients'] = device.status().get('ap_clients')
        connection = slow_request(nonce)
        started = time.monotonic()
        next_byte = started
        responsive = 0
        max_latency = 0
        closed = False
        response_status = None
        client_close_reason = None
        while time.monotonic() - started < 12:
            now = time.monotonic()
            if now >= next_byte:
                try:
                    connection.send(b'x')
                except (BrokenPipeError, ConnectionResetError) as error:
                    client_close_reason=type(error).__name__
                    closed = True
                    break
                checkpoint = time.monotonic()
                status = device.status()
                latency = time.monotonic() - checkpoint
                assert status['setup'] and latency < .8, 'Upload blocked input processing'
                max_latency = max(max_latency, latency)
                responsive += 1
                next_byte += .5
            try:
                received = connection.recv(256)
                if received.startswith(b'HTTP/'):
                    response_status = received.split(b'\r\n', 1)[0].decode('ascii', errors='replace')
                if received == b'':
                    client_close_reason='peer_eof'
                    closed = True
                    break
            except BlockingIOError:
                pass
            except ConnectionResetError:
                client_close_reason='recv_reset'
                closed = True
                break
            time.sleep(.02)
        elapsed = time.monotonic() - started
        print(json.dumps({'trickle_closed': closed, 'elapsed_seconds': round(elapsed, 3), 'responsive_checks': responsive, 'response_status': response_status,'client_close_reason':client_close_reason}), flush=True)
        device.send({'op': 'portal_status'})
        diagnostics = json.loads(device.response(b'PORTAL_STATUS '))
        print(json.dumps({'transport': diagnostics}), flush=True)
        assert closed and 9.5 <= elapsed <= 11.5 and responsive >= 18, 'Absolute upload deadline was not enforced'
        report.update(deadline_seconds=round(elapsed, 3), responsive_status_checks=responsive,
                      maximum_status_latency_ms=round(max_latency * 1000, 1))
        connection.close()
        connection = slow_request(nonce)
        for _ in range(4):
            connection.send(b'x')
            time.sleep(.5)
            assert device.status()['setup']
        started = time.monotonic()
        state = device.action({'op': 'button', 'value': 'blue'})
        elapsed = time.monotonic() - started
        assert elapsed < 1 and not state['setup'] and state['wifi_mode'] == 0 and state['bluetooth'] == 0
        assert state['configured_mask'] == before['configured_mask'] and state['avatar'] == before['avatar']
        report.update(cancel_latency_ms=round(elapsed * 1000, 1), radios_off_after_cancel=True, passed=True)
        (output / 'responsiveness.json').write_text(json.dumps(report, indent=2) + '\n')
        print(json.dumps(report))
    finally:
        if connection:
            connection.close()
        try:
            state = device.status()
            print(json.dumps({'cleanup_setup': state['setup'], 'cleanup_ap_clients': state.get('ap_clients')}))
            if state['setup']:
                device.action({'op': 'button', 'value': 'blue'})
        finally:
            device.close()
            if ssid:
                run(['networksetup', '-removepreferredwirelessnetwork', 'en0', ssid])
                run(['networksetup', '-setairportpower', 'en0', 'off'])
                time.sleep(.5)
                run(['networksetup', '-setairportpower', 'en0', 'on'])
                time.sleep(5)


if __name__ == '__main__':
    main()
