#!/usr/bin/env python3
"""A pretend Patternflow panel for trying the dashboard settings page without hardware.

    python3 tools/mock_panel.py [port] [--accel]   then open http://localhost:8765/dashboard

Serves the real page (feature/dashboard/dashboard.html, stamped the way the panel
serves it) with Patternflow's console chrome, and imitates its API. Needs a build
first (./build.sh), for the Patternflow checkout in .build/. Uploaded GIF clips land in .mock_panel/ (checked the same way the panel
checks them), so they can be inspected or played back by a test.
"""
import json
import re
import sys
from email.parser import BytesParser
from email.policy import default
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import parse_qs, urlparse

ROOT = Path(__file__).resolve().parent.parent
STORE = ROOT / ".mock_panel"
STORE.mkdir(exist_ok=True)
SKETCH = ROOT / ".build/patternflow/firmware/patternflow"
sys.path.insert(0, str(ROOT / "tools"))
import console_page  # noqa: E402
cp = console_page.tool(str(SKETCH))
CHROME = cp.split(cp.read(str(SKETCH / "src/theme_index.h")), "pf-console.js", "JS")[1]
STATUS = {"version": "3.10.5", "build": "mock", "variant": "dashboard", "variantVersion": "mock",
          "caps": ["patterns", "params", "sleep", "dashboard"], "panel": "128x64", "wifi": True,
          "featureNav": [["/dashboard", "Dashboard", "Location, night mode, rotation and GIFs."]]}


def page():  # read on every request, so an edit shows on refresh
    return console_page.stamped(str(SKETCH), (ROOT / "feature/dashboard/dashboard.html").read_text())
state = {"place": "", "lat": 0.0, "lon": 0.0, "night": {"on": True, "start": 22 * 60, "end": 8 * 60},
         "clocks": "NEW YORK|-300,60,3.2.0/120,11.1.0/120|America/New_York\nLONDON|0,60,3.5.0/60,10.5.0/120|Europe/London\n"
                   "TOKYO|540,0|Asia/Tokyo\nSYDNEY|600,60,10.1.0/120,4.1.0/180|Australia/Sydney\n"}
# No accelerometer on a pretend panel (pass --accel to pretend there is one, hanging upright)
accel = {"present": "--accel" in sys.argv, "auto": True, "flip": True, "orientation": 1, "sensed": 1, "g": [1.0, 0.02, 0.04]}


def clips():
    return sorted(p.name[:-6] for p in STORE.glob("*.p.dgf"))


class Handler(BaseHTTPRequestHandler):
    def send(self, code, body, kind="application/json"):
        data = body.encode() if isinstance(body, str) else body
        self.send_response(code)
        self.send_header("Content-Type", kind)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        url = urlparse(self.path)
        if url.path == "/dashboard":
            return self.send(200, page(), "text/html; charset=utf-8")
        if url.path == "/pf-console.js":
            return self.send(200, CHROME, "application/javascript")
        if url.path == "/api/status":
            return self.send(200, json.dumps(STATUS))
        if url.path == "/api/dashboard":
            return self.send(200, json.dumps({**state, "updated": 0, "error": "", "free": 8 * 1048576, "gifs": clips()}))
        if url.path == "/api/dashboard/orientation":
            return self.send(200, json.dumps(accel if accel["present"] else {**accel, "sensed": -1, "g": [0, 0, 0]}))
        if url.path.startswith("/test/"):  # serves files for automated tests
            f = ROOT / ".mock_panel" / "test" / Path(url.path).name
            if f.exists():
                return self.send(200, f.read_bytes(), "image/gif")
        self.send(404, '{"error":"not found"}')

    def do_POST(self):
        url = urlparse(self.path)
        q = {k: v[0] for k, v in parse_qs(url.query).items()}
        body = self.rfile.read(int(self.headers.get("Content-Length", 0)))
        if url.path == "/api/dashboard":
            f = {k: v[0] for k, v in parse_qs(body.decode()).items()}
            state.update(place=f.get("place", ""), lat=float(f["lat"]), lon=float(f["lon"]))
            return self.send(200, json.dumps(state))
        if url.path == "/api/dashboard/orientation":
            f = {k: v[0] for k, v in parse_qs(body.decode()).items()}
            if "auto" in f: accel["auto"] = f["auto"] == "1"
            if "flip" in f: accel["flip"] = f["flip"] == "1"
            return self.send(200, json.dumps(accel))
        if url.path == "/api/dashboard/clocks":
            f = {k: v[0] for k, v in parse_qs(body.decode(), keep_blank_values=True).items()}
            lines = [l for l in f.get("clocks", "").split("\n") if l]
            ok = [l for l in lines if re.fullmatch(r"[ -~]{1,10}\|-?\d+,\d+(,\d+\.\d\.\d/-?\d+,\d+\.\d\.\d/-?\d+)?\|[A-Za-z0-9/_+-]{0,40}", l)]
            if len(ok) != len(lines) or len(lines) > 4:
                return self.send(400, '{"error":"some clocks could not be read"}')
            state["clocks"] = "".join(l + "\n" for l in lines)
            return self.send(200, json.dumps({"ok": True, "count": len(lines)}))
        if url.path == "/api/dashboard/night":
            f = {k: v[0] for k, v in parse_qs(body.decode()).items()}
            hm = lambda s: int(s[:2]) * 60 + int(s[3:5])
            state["night"] = {"on": f.get("on") == "1", "start": hm(f["start"]), "end": hm(f["end"])}
            return self.send(200, '{"ok":true}')
        if url.path == "/api/dashboard/gif":
            name, o = q.get("name", ""), q.get("o", "")
            if not re.fullmatch(r"[a-z0-9-]{1,24}", name) or o not in ("p", "l"):
                return self.send(400, '{"error":"bad name or orientation"}')
            msg = BytesParser(policy=default).parsebytes(
                b"Content-Type: " + self.headers["Content-Type"].encode() + b"\r\n\r\n" + body)
            data = next(part.get_payload(decode=True) for part in msg.iter_parts())
            w, h, n = (int.from_bytes(data[i:i + 2], "little") for i in (4, 6, 8))
            ok = (data[:4] == b"DGF1" and (w, h) == ((64, 128) if o == "p" else (128, 64))
                  and 1 <= n <= 120 and len(data) == 12 + 2 * n + n * w * h)
            if not ok:
                return self.send(400, '{"error":"not a valid clip"}')
            (STORE / f"{name}.{o}.dgf").write_bytes(data)
            return self.send(200, '{"ok":true}')
        if url.path == "/api/dashboard/gif/delete":
            for o in "pl":
                (STORE / f"{q.get('name', '')}.{o}.dgf").unlink(missing_ok=True)
            return self.send(200, '{"ok":true}')
        self.send(404, '{"error":"not found"}')

    def log_message(self, *args):
        pass


if __name__ == "__main__":
    port = int(next((a for a in sys.argv[1:] if a.isdigit()), 8765))
    print(f"mock panel: http://localhost:{port}/dashboard")
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
