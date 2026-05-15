import os
import cgi

method = os.environ.get("REQUEST_METHOD", "")
form = cgi.FieldStorage()
name = form.getvalue("name", "unknown")
print("Hello, " + name)
