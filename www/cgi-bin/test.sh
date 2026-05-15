#!/bin/bash

METHOD="${REQUEST_METHOD:-GET}"
QUERY="${QUERY_STRING:-}"

if [ "$METHOD" = "POST" ]; then
    LENGTH="${CONTENT_LENGTH:-0}"
    BODY=""
    if [ "$LENGTH" -gt 0 ]; then
        BODY=$(dd bs=1 count="$LENGTH" 2>/dev/null)
    fi
    OUTPUT="<html><body><h1>POST received (Bash)</h1><p>${BODY}</p></body></html>"
elif [ -n "$QUERY" ]; then
    OUTPUT="<html><body><h1>GET with query (Bash)</h1><p>${QUERY}</p></body></html>"
else
    OUTPUT="<html><body><h1>Hello from test.sh</h1><p>Bash CGI works.</p></body></html>"
fi

LENGTH=${#OUTPUT}
printf "Content-Type: text/html\r\n"
printf "Content-Length: %d\r\n" "$LENGTH"
printf "\r\n"
printf "%s" "$OUTPUT"
