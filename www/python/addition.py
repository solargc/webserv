import sys

body = sys.stdin.read()
numbers = body.split("+")
result = int(numbers[0]) + int(numbers[1])
print(result)
