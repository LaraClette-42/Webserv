#!/usr/bin/env python3

import sys
import time

# Print HTTP headers
print("Content-Type: text/plain\r")
print("\r")

# Print initial message
print("Starting infinite loop test...")
sys.stdout.flush()

# Infinite loop
counter = 0
while True:
    counter += 1
    print(f"Loop iteration: {counter}")
    sys.stdout.flush()
    time.sleep(1)
