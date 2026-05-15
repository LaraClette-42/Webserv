#!/usr/bin/env python3
import os, json, uuid, datetime

SESSION_DIR = "/tmp/webserv_sessions"

def parse_cookies(s):
    out = {}
    for part in (s or "").split(";"):
        part = part.strip()
        if "=" in part:
            k, v = part.split("=", 1)
            out[k.strip()] = v.strip()
    return out

def load(sid):
    p = os.path.join(SESSION_DIR, sid + ".json")
    if os.path.exists(p):
        with open(p) as f:
            return json.load(f)
    return None

def save(sid, data):
    os.makedirs(SESSION_DIR, exist_ok=True)
    with open(os.path.join(SESSION_DIR, sid + ".json"), "w") as f:
        json.dump(data, f)

cookies = parse_cookies(os.environ.get("HTTP_COOKIE", ""))
sid = cookies.get("session_id", "")
query = os.environ.get("QUERY_STRING", "")

if "reset=1" in query or not sid:
    sid = str(uuid.uuid4())
    session = None
else:
    session = load(sid)
    if session is None:
        sid = str(uuid.uuid4())

now = datetime.datetime.now().strftime("%Y-%m-%d %H:%M:%S")
if session is None:
    session = {"visits": 0, "first_visit": now}

session["visits"] += 1
session["last_visit"] = now
save(sid, session)

body = """<!DOCTYPE html>
<html lang="en">
<head><meta charset="UTF-8"><title>Session demo</title>
<style>
body{{font-family:Arial,sans-serif;background:#f0f0f0;padding:40px;}}
.card{{background:white;border-radius:6px;padding:32px;max-width:520px;margin:0 auto;box-shadow:0 2px 8px rgba(0,0,0,.1);}}
h1{{color:#2c3e50;border-bottom:2px solid #3498db;padding-bottom:10px;}}
code{{background:#f0f0f0;padding:2px 6px;border-radius:3px;font-size:.9em;}}
a{{color:#2980b9;}}
</style></head>
<body><div class="card">
<h1>Session demo</h1>
<table style="width:100%;border-collapse:collapse;">
<tr><td style="padding:8px 0;color:#666;">Session ID</td><td><code>{sid}</code></td></tr>
<tr><td style="padding:8px 0;color:#666;">Visits</td><td><strong>{visits}</strong></td></tr>
<tr><td style="padding:8px 0;color:#666;">First visit</td><td>{first}</td></tr>
<tr><td style="padding:8px 0;color:#666;">Last visit</td><td>{last}</td></tr>
</table>
<p style="margin-top:24px;">
  <a href="/cgi-bin/session.py">Refresh (increment counter)</a> &nbsp;|&nbsp;
  <a href="/cgi-bin/session.py?reset=1">Reset session</a> &nbsp;|&nbsp;
  <a href="/">Home</a>
</p>
</div></body></html>""".format(
    sid=sid,
    visits=session["visits"],
    first=session["first_visit"],
    last=session["last_visit"]
)

print("Content-Type: text/html")
print("Set-Cookie: session_id={}; Path=/".format(sid))
print("Content-Length: {}".format(len(body.encode("utf-8"))))
print("")
print(body)
