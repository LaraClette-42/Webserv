#!/usr/bin/env python3

import cgi
import os

# Parse the query string for GET parameters
form = cgi.FieldStorage()

print("Content-type: text/html\n")

print("<html>")
print("<head><title>CGI GET Parameters</title></head>")
print("<body>")
print("<h1>GET Parameters Received</h1>")

if form:
    print("<ul>")
    for key in form.keys():
        value = form[key].value
        print(f"<li><strong>{key}:</strong> {value}</li>")
    print("</ul>")
else:
    print("<p>No parameters received.</p>")

print("<p><a href='?name=John&age=30'>Example with parameters</a></p>")
print("</body>")
print("</html>")