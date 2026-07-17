import urllib.request
import sys
from urllib.parse import quote

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
base = "http://192.168.4.135"

def fetch(path, timeout=5, headers=None):
    req = urllib.request.Request(base + path, headers=headers or {"Accept": "*/*"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.status, dict(r.headers), r.read(5000)

print("=== ROOT ===")
try:
    st, h, body = fetch("/")
    print(st, h.get("Content-Type"))
    print(body.decode("utf-8", "replace"))
except Exception as e:
    print("ERR", e)

print("\n=== SSE ===")
try:
    req = urllib.request.Request(base + "/events", headers={"Accept": "text/event-stream"})
    with urllib.request.urlopen(req, timeout=12) as r:
        buf = b""
        while len(buf) < 12000:
            chunk = r.read(512)
            if not chunk:
                break
            buf += chunk
            if buf.count(b"event: state") >= 6:
                break
    print(buf.decode("utf-8", "replace"))
except Exception as e:
    print("SSE ERR", e)

# Try likely sensor paths from SSE ids later; also common names
print("\n=== REST probes ===")
names = [
    "/sensor/Temperature",
    "/sensor/temperature",
    "/sensor/Humidity",
    "/sensor/humidity",
    "/sensor/AHT10 Temperature",
    "/sensor/AHT10 Humidity",
    "/text_sensor/Hostname",
    "/button/Restart",
]
for p in names:
    # encode spaces
    enc = "/" + "/".join(quote(seg, safe="") for seg in p.strip("/").split("/"))
    try:
        st, h, body = fetch(enc)
        print(p, "->", enc, st, body[:300].decode("utf-8", "replace"))
    except Exception as e:
        print(p, "ERR", e)
