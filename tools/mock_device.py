"""Simulates the device API to work on web/setup.html in a normal browser.

Usage:  python tools/mock_device.py [ap|sta] [port]
Then open http://localhost:8766/ . Settings are only kept in memory.
"""
import json
import os
import sys
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
MODE = sys.argv[1] if len(sys.argv) > 1 else "sta"
PORT = int(sys.argv[2]) if len(sys.argv) > 2 else 8766

settings = {
    "lang": "de", "unit": "kn", "interval": 120, "ssid": "FRITZ!Box 7590", "version": "1.0.0", "maxSpots": 6,
    "spots": [
        {"name": "Mirns", "lat": 52.8491, "lon": 5.4846, "dirs": 0b0001111111110000, "min": 14, "sea": True},
        {"name": "Xanten", "lat": 51.6853, "lon": 6.4482, "dirs": 0xFFFF, "min": 14, "sea": False},
    ],
}
join = {"state": "idle", "since": 0}
NETWORKS = [
    {"ssid": "FRITZ!Box 7590", "rssi": -48, "secure": True},
    {"ssid": "Vodafone-A1B2", "rssi": -71, "secure": True},
    {"ssid": "Gast", "rssi": -80, "secure": False},
]


class Handler(BaseHTTPRequestHandler):
    def send_json(self, code, body):
        data = json.dumps(body).encode()
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path == "/":
            with open(os.path.join(ROOT, "web", "setup.html"), "rb") as f:
                data = f.read()
            self.send_response(200)
            self.send_header("Content-Type", "text/html; charset=utf-8")
            self.end_headers()
            self.wfile.write(data)
        elif self.path == "/api/state":
            self.send_json(200, {"mode": "ap", "version": "1.0.0"} if MODE == "ap" else {**settings, "mode": "sta"})
        elif self.path.startswith("/api/scan"):
            self.send_json(200, {"networks": NETWORKS})
        elif self.path == "/api/wifi":
            if join["state"] == "connecting" and time.time() - join["since"] > 3:
                join["state"] = "ok" if join.get("pass") else "fail"
            self.send_json(200, {"state": join["state"], "ip": "192.168.178.57"})
        else:
            self.send_json(404, {"error": "not found"})

    def do_POST(self):
        body = json.loads(self.rfile.read(int(self.headers.get("Content-Length", 0))) or b"{}")
        if self.path == "/api/wifi":
            join.update(state="connecting", since=time.time(), **{"pass": body.get("pass")})
            self.send_json(200, {"ok": True})
        elif self.path == "/api/settings":
            print("saved:", json.dumps(body, indent=1))
            body.pop("pass", None)
            settings.update(body)
            self.send_json(200, {"ok": True})
        else:
            self.send_json(404, {"error": "not found"})


print(f"Mock device ({MODE}) on http://localhost:{PORT}/")
ThreadingHTTPServer(("", PORT), Handler).serve_forever()
