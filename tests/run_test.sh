#!/bin/bash

DIR="$(cd "$(dirname "$0")" && pwd)"

echo "Running functional tests..."
bash "$DIR/test.sh" 2>&1 | grep -E "^(< HTTP|HTTP/1\.1|={5,}|CGI [0-9]+:|Static:|PASS:|FAIL:)"

echo ""
bash "$DIR/stress.sh" http://localhost:8080/ 100 10
