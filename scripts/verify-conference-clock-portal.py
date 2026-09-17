#!/usr/bin/env python3
"""One bounded real-device clock endpoint check; temporarily changes Mac Wi-Fi.

Uses HTTP directly, not a phone/browser. Never saves or clears a profile and
never prints hotspot credentials, nonce, SSID, or profile values. Requires the
same dependencies as verify-conference.py. Evidence stays under .build.
"""
import argparse
import datetime
import http.client
import importlib.util
import json
import re
import secrets
import subprocess
import time
from pathlib import Path

spec = importlib.util.spec_from_file_location("conference_usb", Path(__file__).with_name("verify-conference.py"))
usb = importlib.util.module_from_spec(spec)
spec.loader.exec_module(usb)


def run(command, timeout=12):
    return subprocess.run(command, capture_output=True, text=True, timeout=timeout)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", required=True)
    parser.add_argument("--output", required=True)
    args = parser.parse_args()
    out = Path(args.output).resolve()
    assert Path(__file__).resolve().parents[1] / ".build" in out.parents
    out.mkdir(parents=True, exist_ok=True)
    device = usb.Device(args.port)
    initial = device.status()
    assert initial["build"] == "conference-settings-2" and not initial["setup"]
    assert not initial["name_present"] and not initial["configured_mask"] and not initial["avatar"]
    old_network = run(["networksetup", "-getairportnetwork", "en0"]).stdout.strip()
    old_network = old_network.split(": ", 1)[1] if old_network.startswith("Current Wi-Fi Network: ") else None
    report = {"passed": False, "transport": "actual AP/direct HTTP", "phone_browser_tested": False}
    ssid = None
    joined = False

    def request(method, path, body=None, nonce=None):
        connection = http.client.HTTPConnection("192.168.4.1", 80, timeout=8)
        headers = {"Connection": "close"}
        if nonce:
            headers.update({"Content-Type": "application/json", "X-Conference-Nonce": nonce})
        try:
            connection.request(method, path, body=json.dumps(body) if body is not None else None, headers=headers)
            response = connection.getresponse()
            payload = response.read()
            assert response.status == 200, "Endpoint rejected request"
            return payload
        finally:
            connection.close()

    def clock():
        nonce = secrets.token_hex(8)
        device.send({"op": "clock_status", "nonce": nonce})
        result = json.loads(device.response(b"CLOCK_STATUS "))
        assert result.pop("nonce") == nonce
        return result

    try:
        device.page(5)
        password = secrets.token_hex(10)
        device.send({"op": "setup_test", "password": password})
        setup = json.loads(device.response(b"CONFERENCE_SETUP "))
        assert setup["active"]
        ssid = setup["ssid"]
        time.sleep(1)
        report["phase"] = "hotspot_association"
        result = run(["networksetup", "-setairportnetwork", "en0", ssid, password], timeout=15)
        assert result.returncode == 0 and not result.stdout.strip(), "Hotspot association failed"
        joined = True
        time.sleep(2)
        report["associated_clients"] = device.status()["ap_clients"]
        report["phase"] = "setup_get"
        html = request("GET", "/").decode()
        report["setup_html_received"] = True
        match = re.search(r"const nonce='([a-f0-9]+)'", html)
        assert match, "Authorized portal nonce missing"
        nonce = match[1]
        epoch = int(time.time())
        offset = int(datetime.datetime.now().astimezone().utcoffset().total_seconds() // 60)
        report["phase"] = "clock_post"
        response = json.loads(request("POST", "/clock", {"epoch": epoch, "offset_minutes": offset}, nonce))
        assert response["ok"] is True and response["valid"] is True and response["source"] == "phone"
        assert abs(response["epoch"] - time.time()) < 4 and abs(response["rtc_epoch"] - response["epoch"]) <= 1
        assert response["offset_minutes"] == offset
        report["clock_endpoint_verified"] = True
        live = device.status()
        assert live["setup"] and not live["name_present"] and not live["configured_mask"] and not live["avatar"]
        report["profile_unchanged_without_save"] = True
        report["phase"] = "cancel_after_clock"
        request("POST", "/cancel", {}, nonce)
        time.sleep(3.5)
        live = device.status()
        assert not live["setup"] and live["page"] == 5 and live["wifi_mode"] == 0
        persisted = clock()
        assert persisted["source"] == "phone" and persisted["valid"]
        report["clock_retained_after_cancel"] = True
        report["clock_status"] = persisted
        report["passed"] = True
    except Exception as error:
        # Do not expose network identifiers or an authorized URL in exceptions.
        report["failure_type"] = type(error).__name__
        device.send({"op": "portal_status"})
        report["transport_status"] = json.loads(device.response(b"PORTAL_STATUS "))
    finally:
        try:
            if device.status()["setup"]:
                device.action({"op": "button", "value": "blue"})
            device.page(0)
            report["final_device_status"] = device.status()
        finally:
            device.close()
        if ssid:
            run(["networksetup", "-removepreferredwirelessnetwork", "en0", ssid])
        if old_network:
            run(["networksetup", "-setairportnetwork", "en0", old_network], timeout=15)
        elif joined:
            run(["networksetup", "-setairportpower", "en0", "off"])
            time.sleep(.5)
            run(["networksetup", "-setairportpower", "en0", "on"])
        deadline = time.monotonic() + 20
        while time.monotonic() < deadline:
            ip = run(["ipconfig", "getifaddr", "en0"]).stdout.strip()
            route = run(["route", "-n", "get", "default"]).stdout
            normal = bool(ip) and not ip.startswith(("192.168.4.", "169.254.")) and "gateway:" in route and "192.168.4.1" not in route
            if normal:
                public = run(["curl", "--silent", "--fail", "--max-time", "8", "https://example.com"], timeout=10)
                if public.returncode == 0:
                    report["normal_network_restored"] = True
                    break
            time.sleep(1)
        report.setdefault("normal_network_restored", False)
        (out / "clock-portal.json").write_text(json.dumps(report, indent=2) + "\n")
        print(json.dumps({key: report.get(key) for key in ("passed", "setup_html_received", "clock_endpoint_verified", "normal_network_restored", "failure_type")}))
    assert report["normal_network_restored"], "Normal workstation network restoration needs attention"
    assert report["passed"], "Live clock HTTP check did not complete; see private evidence"


if __name__ == "__main__":
    main()
