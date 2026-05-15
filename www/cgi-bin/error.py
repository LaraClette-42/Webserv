#!/usr/bin/env python3
import os

code = int(os.environ.get('QUERY_STRING', '').replace('code=', '') or '500')
messages = {400: 'Bad Request', 403: 'Forbidden', 404: 'Not Found', 500: 'Internal Server Error'}
msg = messages.get(code, 'Error')
output = '<html><body><h1>CGI returned ' + str(code) + ' ' + msg + '</h1><p>Status header correctly forwarded.</p></body></html>'

import sys
sys.stdout.write('Status: ' + str(code) + ' ' + msg + '\r\n')
sys.stdout.write('Content-Type: text/html\r\n')
sys.stdout.write('Content-Length: ' + str(len(output)) + '\r\n')
sys.stdout.write('\r\n')
sys.stdout.write(output)
