#!/usr/bin/env python3
"""Test ESP32 Gateway local REST API authorization (FR-SEC-02)."""

import argparse
import json
import sys
import urllib.error
import urllib.request


def send_request(url: str, body: dict, headers: dict | None = None) -> int:
    data = json.dumps(body).encode("utf-8")
    req_headers = {"Content-Type": "application/json"}
    if headers:
        req_headers.update(headers)
    req = urllib.request.Request(url, data=data, headers=req_headers, method="POST")
    try:
        with urllib.request.urlopen(req, timeout=5) as resp:
            return resp.status
    except urllib.error.HTTPError as e:
        return e.code


def main() -> int:
    parser = argparse.ArgumentParser(description="Test local REST API authorization")
    parser.add_argument("--host", default="192.168.1.199", help="Target gateway host or IP")
    parser.add_argument("--key", required=True, help="Configured API key")
    args = parser.parse_args()

    url = f"http://{args.host}/api/relay"
    body_on = {"target": "relay_1", "command": "ON"}
    body_off = {"target": "relay_1", "command": "OFF"}

    test_cases = [
        ("(a) POST /api/relay no header -> 401", {}, body_on, 401),
        ("(b) POST /api/relay wrong key -> 401", {"X-API-Key": "wrong-key-12345"}, body_on, 401),
        ("(c) POST /api/relay correct key -> 200 (relay 1 ON)", {"X-API-Key": args.key}, body_on, 200),
        ("(d) POST /api/relay correct key -> 200 (relay 1 OFF)", {"X-API-Key": args.key}, body_off, 200),
    ]

    all_passed = True
    print(f"Testing Local REST Auth against {url}...")

    for desc, headers, body, expected_status in test_cases:
        try:
            status = send_request(url, body, headers)
            if status == expected_status:
                print(f"  [PASS] {desc} (status: {status})")
            else:
                print(f"  [FAIL] {desc} (expected: {expected_status}, got: {status})")
                all_passed = False
        except Exception as exc:
            print(f"  [FAIL] {desc} (exception: {exc})")
            all_passed = False

    if all_passed:
        print("All auth tests PASSED.")
        return 0
    else:
        print("One or more auth tests FAILED.")
        return 1


if __name__ == "__main__":
    sys.exit(main())
