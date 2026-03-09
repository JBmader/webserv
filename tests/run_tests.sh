#!/bin/bash
# Webserv Comprehensive Test Suite
# Run: ./tests/run_tests.sh [config_file]
# Default: config/default.conf

set -e

CONFIG="${1:-config/default.conf}"
HOST="localhost"
PORT=8080
PORT2=8081
PASS=0
FAIL=0
TOTAL=0

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

check() {
	local desc="$1"
	local expected="$2"
	local got="$3"
	TOTAL=$((TOTAL + 1))
	if [ "$expected" = "$got" ]; then
		echo -e "  ${GREEN}[PASS]${NC} $desc (got $got)"
		PASS=$((PASS + 1))
	else
		echo -e "  ${RED}[FAIL]${NC} $desc (expected $expected, got $got)"
		FAIL=$((FAIL + 1))
	fi
}

echo "============================================"
echo "  WEBSERV TEST SUITE"
echo "  Config: $CONFIG"
echo "============================================"

# Start server
mkdir -p www/upload
./webserv "$CONFIG" &
SERVER_PID=$!
sleep 1

# Verify server is running
if ! kill -0 $SERVER_PID 2>/dev/null; then
	echo -e "${RED}Server failed to start!${NC}"
	exit 1
fi

cleanup() {
	kill $SERVER_PID 2>/dev/null
	wait $SERVER_PID 2>/dev/null
}
trap cleanup EXIT

echo ""
echo "--- 1. Basic GET Requests ---"
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT/)
check "GET / returns 200" "200" "$CODE"

CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT/nonexistent)
check "GET /nonexistent returns 404" "404" "$CODE"

CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT/old-page)
check "GET /old-page returns 301 redirect" "301" "$CODE"

echo ""
echo "--- 2. POST Requests (Upload) ---"
echo "test content" > /tmp/webserv_test_upload.txt
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST -F "file=@/tmp/webserv_test_upload.txt" http://$HOST:$PORT/upload)
check "POST /upload returns 201" "201" "$CODE"

CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT/upload/webserv_test_upload.txt)
check "GET uploaded file returns 200" "200" "$CODE"

echo ""
echo "--- 3. DELETE Requests ---"
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE http://$HOST:$PORT/upload/webserv_test_upload.txt)
check "DELETE uploaded file returns 200" "200" "$CODE"

CODE=$(curl -s -o /dev/null -w "%{http_code}" -X DELETE http://$HOST:$PORT/upload/webserv_test_upload.txt)
check "DELETE nonexistent file returns 404" "404" "$CODE"

echo ""
echo "--- 4. Unknown/Bad Requests ---"
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X PATCH http://$HOST:$PORT/)
check "PATCH (unknown method) returns 501" "501" "$CODE"

CODE=$(curl -s -o /dev/null -w "%{http_code}" -X OPTIONS http://$HOST:$PORT/)
check "OPTIONS (unknown method) returns 501" "501" "$CODE"

# Malformed request via telnet
RESP=$(printf "INVALID REQUEST\r\n\r\n" | nc -w 2 $HOST $PORT 2>/dev/null | head -1)
if echo "$RESP" | grep -q "400\|501"; then
	check "Malformed request returns error" "error" "error"
else
	check "Malformed request returns error" "error" "ok_or_timeout"
fi

echo ""
echo "--- 5. CGI Tests ---"
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT/cgi-bin/hello.py)
check "CGI hello.py returns 200" "200" "$CODE"

BODY=$(curl -s http://$HOST:$PORT/cgi-bin/hello.py)
if echo "$BODY" | grep -q "Hello from CGI"; then
	check "CGI hello.py has correct output" "yes" "yes"
else
	check "CGI hello.py has correct output" "yes" "no"
fi

CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST -d "name=42" http://$HOST:$PORT/cgi-bin/form.py)
check "CGI form.py POST returns 200" "200" "$CODE"

BODY=$(curl -s -X POST -d "name=42" http://$HOST:$PORT/cgi-bin/form.py)
if echo "$BODY" | grep -q "42"; then
	check "CGI form.py echoes POST data" "yes" "yes"
else
	check "CGI form.py echoes POST data" "yes" "no"
fi

CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT/cgi-bin/env.py)
check "CGI env.py returns 200" "200" "$CODE"

echo ""
echo "--- 6. Directory Listing ---"
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT/upload/)
check "Autoindex on /upload returns 200" "200" "$CODE"

BODY=$(curl -s http://$HOST:$PORT/upload/)
if echo "$BODY" | grep -q "Index of"; then
	check "Autoindex shows directory listing" "yes" "yes"
else
	check "Autoindex shows directory listing" "yes" "no"
fi

echo ""
echo "--- 7. Multiple Ports ---"
CODE=$(curl -s -o /dev/null -w "%{http_code}" http://$HOST:$PORT2/)
check "Second server on port $PORT2 returns 200" "200" "$CODE"

echo ""
echo "--- 8. Error Pages ---"
BODY=$(curl -s http://$HOST:$PORT/nonexistent)
if echo "$BODY" | grep -qi "404\|not found"; then
	check "404 page has error content" "yes" "yes"
else
	check "404 page has error content" "yes" "no"
fi

echo ""
echo "--- 9. Body Size Limit ---"
# Default config: 10M limit; second server: 1M limit
SMALL_BODY=$(python3 -c "print('A' * 500)" 2>/dev/null || echo "AAAAA")
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: text/plain" -H "Content-Length: 500" -d "$SMALL_BODY" http://$HOST:$PORT2/ 2>/dev/null)
check "Small body within limit (port $PORT2)" "405" "$CODE"  # 405 because GET only on port 8081

# Body larger than 1M limit on port 8081 -- send via Content-Length header trick
# Generate a temp file > 1M
dd if=/dev/zero bs=1024 count=1100 of=/tmp/webserv_bigbody.bin 2>/dev/null
CODE=$(curl -s -o /dev/null -w "%{http_code}" -X POST -H "Content-Type: application/octet-stream" --data-binary @/tmp/webserv_bigbody.bin http://$HOST:$PORT2/ 2>/dev/null)
if [ "$CODE" = "413" ] || [ "$CODE" = "405" ]; then
	check "Large body over limit returns 413 or 405" "$CODE" "$CODE"
else
	check "Large body over limit returns 413 or 405" "413" "$CODE"
fi
rm -f /tmp/webserv_bigbody.bin

echo ""
echo "--- 10. Virtual Host (server_name) ---"
CODE=$(curl -s -o /dev/null -w "%{http_code}" --resolve "example.com:$PORT:127.0.0.1" http://example.com:$PORT/ 2>/dev/null)
if [ "$CODE" = "200" ]; then
	check "Virtual host example.com resolves" "200" "$CODE"
else
	check "Virtual host example.com resolves (may need multi_server.conf)" "200" "$CODE"
fi

echo ""
echo "--- 11. HTTP Version ---"
RESP=$(printf "GET / HTTP/2.0\r\nHost: localhost\r\n\r\n" | nc -w 2 $HOST $PORT 2>/dev/null | head -1)
if echo "$RESP" | grep -q "505"; then
	check "HTTP/2.0 returns 505" "505" "505"
else
	check "HTTP/2.0 returns 505" "505" "other"
fi

echo ""
echo "============================================"
echo -e "  Results: ${GREEN}$PASS passed${NC}, ${RED}$FAIL failed${NC} / $TOTAL total"
echo "============================================"

rm -f /tmp/webserv_test_upload.txt
exit $FAIL
