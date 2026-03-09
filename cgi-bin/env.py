#!/usr/bin/env python3
import os

print("Content-Type: text/html")
print()
print("<!DOCTYPE html>")
print("<html><head><title>CGI Environment</title></head>")
print("<body>")
print("<h1>CGI Environment Variables</h1>")
print("<table border='1' cellpadding='5'>")
print("<tr><th>Variable</th><th>Value</th></tr>")

for key in sorted(os.environ.keys()):
    print("<tr><td>{}</td><td>{}</td></tr>".format(key, os.environ[key]))

print("</table>")
print("<p><a href='/'>Back to home</a></p>")
print("</body></html>")
