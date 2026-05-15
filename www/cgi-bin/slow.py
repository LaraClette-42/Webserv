#!/usr/bin/env python3
import os
import time

query = os.environ.get('QUERY_STRING', '')
delay = 2
for part in query.split('&'):
    if part.startswith('delay='):
        try:
            delay = int(part.split('=')[1])
        except ValueError:
            pass

time.sleep(delay)

output = '<html><body><h1>Slow CGI</h1><p>Slept ' + str(delay) + 's before responding. Server should have stayed available.</p></body></html>'

import sys
sys.stdout.write('Content-Type: text/html\r\n')
sys.stdout.write('Content-Length: ' + str(len(output)) + '\r\n')
sys.stdout.write('\r\n')
sys.stdout.write(output)
