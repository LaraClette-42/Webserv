#!/usr/bin/env python3
import os
import sys

try:
    content_length = int(os.environ.get("CONTENT_LENGTH", 0))
    upload_path = os.environ.get("UPLOAD_PATH", "")
    if content_length > 0:
        filename = sys.stdin.read(content_length).strip()
        filename = "./" + upload_path + filename
    else:
        print("Status: 400 Bad Request\r")
        print("Content-Type: text/plain\r")
        print("\r")
        print("No filename provided")
        sys.exit(1)
    
    if not filename:
        print("Status: 400 Bad Request\r")
        print("Content-Type: text/plain\r")
        print("\r")
        print("Empty filename")
        sys.exit(1)

    if filename.startswith("./" + upload_path) == False:
        print("Status: 403 Forbidden\r")
        print("Content-Type: text/plain\r")
        print("\r")
        print(f"Permission denied: cannot delete '{filename}'")
        sys.exit(1)

    if os.access(filename, os.F_OK):
        # Check if we have write permission
        if os.access(filename, os.W_OK):
            os.remove(filename)
            print("Status: 200 OK\r")
            print("Content-Type: text/plain\r")
            print("\r")
            print(f"File '{filename}' deleted successfully")
        else:
            print("Status: 403 Forbidden\r")
            print("Content-Type: text/plain\r")
            print("\r")
            print(f"Permission denied: cannot delete '{filename}'")
    else:
        print("Status: 404 Not Found\r")
        print("Content-Type: text/plain\r")
        print("\r")
        print(f"File '{filename}' cannot be found")

except Exception as e:
    import traceback
    sys.stderr.write("Exception:\n")
    sys.stderr.write(traceback.format_exc())
    print("Status: 500 Internal Server Error\r")
    print("Content-Type: text/plain\r")
    print("\r")
    print("Error while deleting file")
    sys.exit(1)
