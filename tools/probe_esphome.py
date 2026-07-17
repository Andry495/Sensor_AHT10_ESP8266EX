import urllib.request
import sys
from urllib.parse import quote

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
base = "http://192.168.4.144"

def get(path, timeout=5):
    url = base + path
    req = urllib.request.Request(url, headers={"Accept": "*/*"})
    with urllib.request.urlopen(req, timeout=timeout) as r:
        return r.status, r.headers.get("Content-Type"), r.read(2000)

candidates = [
    "/",
    "/events",
    "/sensor/Temperature",
    "/sensor/temperature",
    "/sensor/Humidity",
    "/sensor/humidity",
    "/sensor/" + quote("Температура"),
    "/sensor/" + quote("Влажность"),
    "/text_sensor/Hostname",
    "/button/Restart",
    "/button/restart",
]

for p in candidates:
    try:
        status, ct, body = get(p, timeout=6 if p == "/events" else 4)
        print(f"{p}\n  status={status} ct={ct}")
        print(" ", body[:600].decode("utf-8", "replace").replace("\n", " | "))
    except Exception as e:
        print(f"{p}\n  ERR {e}")
    print()
