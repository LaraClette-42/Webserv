#!/usr/bin/env python3
import os
import sys
import re

try:
	content_length = int(os.environ.get("CONTENT_LENGTH", 0))
	content_type = os.environ.get("CONTENT_TYPE", "")
	upload_path = os.environ.get("UPLOAD_PATH", "")

	raw_body = b''
	while len(raw_body) < content_length:
		chunk = sys.stdin.buffer.read(content_length - len(raw_body))
		if not chunk:
			break
		raw_body += chunk

	if len(raw_body) < content_length:
		sys.stderr.write("ERROR: Incomplete body received\n")
		sys.stderr.write("body len = " + str(len(raw_body)) + " content len = " + str(content_length) + "\n")
		print("Status: 400 Bad Request\n\nIncomplete body")
		sys.exit(1)

	if "boundary=" not in content_type:
		sys.stderr.write("ERROR: No boundary in Content-Type\n")
		print("Status: 400 Bad Request")
		sys.stdout.write("\r\n\r\n")
		print("No boundary")
		sys.exit(1)

	boundary = content_type.split("boundary=")[1]
	header_end = raw_body.find(b'\r\n\r\n')
	if header_end == -1:
		sys.stderr.write("ERROR: Could not find end of headers\n")
		print("Status: 400 Bad Request")
		sys.stdout.write("\r\n\r\n")
		print("No header end")
		sys.exit(1)

	headers_section = raw_body[:header_end].decode('utf-8', errors='replace')

	filename = None
	for line in headers_section.split('\r\n'):
		if 'Content-Disposition' in line:
			filename_match = re.search(r'filename="([^"]+)"', line)
			if filename_match:
				filename = filename_match.group(1)

	file_start = header_end + 4

	closing_boundary = f"\r\n--{boundary}--".encode()
	file_end = raw_body.rfind(closing_boundary)

	if file_end == -1:
		sys.stderr.write("ERROR: Could not find closing boundary\n")
		print("Status: 400 Bad Request")
		sys.stdout.write("\r\n\r\n")
		print("Malformed request")
		sys.exit(1)

	file_data = raw_body[file_start:file_end]

	os.makedirs(upload_path, exist_ok=True)
	if not filename:
		filename = "uploaded_file.bin"
	safe_filename = os.path.basename(filename)
	filepath = os.path.join(upload_path, safe_filename)

	with open(filepath, "wb") as f:
		f.write(file_data)

	print("Status: 200 OK")
	print("Content-Type: " + content_type, end='')
	sys.stdout.write("\r\n\r\n")

except Exception as e:
	import traceback
	sys.stderr.write("Exception:\n")
	sys.stderr.write(traceback.format_exc())
	print("Status: 500 Internal Server Error")
	print("Content-Type: text/plain")
	sys.stdout.write("\r\n\r\n")
	print("Error")
	sys.exit(1)