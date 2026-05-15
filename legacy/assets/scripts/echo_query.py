#!/usr/bin/env python3

import os

# Get the query string from environment
query_string = os.environ.get('QUERY_STRING', '')

print("Content-type: text/plain\n")

print("Query String: " + query_string)

if query_string:
    print("\nParsed parameters:")
    params = query_string.split('&')
    for param in params:
        if '=' in param:
            key, value = param.split('=', 1)
            print(f"{key}: {value}")
        else:
            print(f"{param}: (no value)")
else:
    print("No query string provided.")