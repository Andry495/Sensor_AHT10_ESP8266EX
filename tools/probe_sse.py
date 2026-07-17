import urllib.request
import sys

sys.stdout.reconfigure(encoding="utf-8", errors="replace")
base = "http://192.168.4.144"
req = urllib.request.Request(base + "/events", headers={"Accept": "text/event-stream"})
with urllib.request.urlopen(req, timeout=15) as r:
    buf = b""
    while len(buf) < 8000:
        chunk = r.read(256)
        if not chunk:
            break
        buf += chunk
        # stop after we likely got initial states
        if buf.count(b"event: state") >= 3 or buf.count(b"\n\n") >= 8:
            break
print(buf.decode("utf-8", "replace"))
