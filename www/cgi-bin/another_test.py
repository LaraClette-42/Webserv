#!/usr/bin/env python3
import os
import sys

query = os.environ.get('QUERY_STRING', '')
method = os.environ.get('REQUEST_METHOD', 'GET')

params = {}
for part in query.split('&'):
    if '=' in part:
        k, v = part.split('=', 1)
        params[k] = v

if method == 'POST':
    length = int(os.environ.get('CONTENT_LENGTH', '0') or '0')
    body   = sys.stdin.read(length) if length > 0 else ''
    output = '<html><body><h1>another_test POST</h1><p>' + body + '</p></body></html>'
else:
    req_id = params.get('id', 'none')
    output = '<html><body><h1>another_test GET</h1><p>id=' + req_id + '</p></body></html>'

sys.stdout.write('Content-Type: text/html\r\n')
sys.stdout.write('Content-Length: ' + str(len(output)) + '\r\n')
sys.stdout.write('\r\n')
sys.stdout.write(output)
