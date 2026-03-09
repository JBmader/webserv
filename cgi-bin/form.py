#!/usr/bin/env python3
import os
import sys

# Read POST body from stdin
content_length = int(os.environ.get("CONTENT_LENGTH", "0"))
body = ""
if content_length > 0:
    body = sys.stdin.read(content_length)

# Simple URL-encoded form parsing
params = {}
if body:
    for pair in body.split("&"):
        if "=" in pair:
            key, value = pair.split("=", 1)
            # Basic URL decoding
            value = value.replace("+", " ").replace("%20", " ")
            params[key] = value

name = params.get("name", "World")

print("Content-Type: text/html")
print()
print("<!DOCTYPE html>")
print("<html><head><title>Form Result</title></head>")
print("<body>")
print("<h1>Hello, {}!</h1>".format(name))
print("<p>Received POST data: {}</p>".format(body))
print("<p><a href='/'>Back to home</a></p>")
print("</body></html>")
