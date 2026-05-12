#!/usr/bin/env python3
import os
import sys

try:
    # Get the page parameter from query string
    content_type = os.environ.get("CONTENT_TYPE", "")

    # Default page if none specified
    page = "index.html"

    # Security: prevent directory traversal
    page = os.path.basename(page)

    file_path = os.path.join("../assets/html/", page) # No idea how normal that is. Works tho

#    sys.stderr.write(f"Requested page: {page}\n File path: {file_path}\n Content-Type: {content_type}\n")

    # Read and serve the file
    if os.path.exists(file_path):
#        sys.stderr.write(f"File found: {file_path}\n")
        sys.stdout.write("Status: 200\r\n")
        sys.stdout.write("Content-Type: text/html; charset=UTF-8\r\n")
    # Construct file path (adjust to your server's root)
        with open(file_path, 'r') as f:
            content = f.read()
        sys.stdout.write("Content-Length:" + str(len(content)) +  "\r\n")
        sys.stdout.write("\r\n\r\n")
        print(content)

    else:
#        sys.stderr.write(f"File not found: {file_path}, should 404\n")
        print("Status: 404 Not Found")
        print("Content-Type: text/plain\n")
        print(f"Page '{page}' not found")

except Exception as e:
    import traceback
    sys.stderr.write(traceback.format_exc())
    print("Status: 500 Internal Server Error")
    print("Content-Type: text/plain\n")
    print("Error")
