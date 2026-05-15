#!/usr/bin/env python3
import os
import sys

method = os.environ.get('REQUEST_METHOD', 'GET')
query  = os.environ.get('QUERY_STRING', '')

if method == 'POST':
    length = int(os.environ.get('CONTENT_LENGTH', '0') or '0')
    body   = sys.stdin.read(length) if length > 0 else ''
    output = '<html><body><h1>POST received</h1><p>' + body + '</p></body></html>'
elif query:
    output = '<html><body><h1>GET with query</h1><p>' + query + '</p></body></html>'
else:
    output = '<html><body><h1>Hello from test.py</h1><p>CGI works.</p></body></html>'

sys.stdout.write('Content-Type: text/html\r\n')
sys.stdout.write('Content-Length: ' + str(len(output)) + '\r\n')
sys.stdout.write('\r\n')
sys.stdout.write(output)
