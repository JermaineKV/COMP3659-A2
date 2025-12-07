#!/bin/bash

# ============================================================================
# COMP3659 Assignment 2 - Quick Server Test
# ============================================================================
# A simpler, faster test script for basic functionality verification
#
# Usage: ./quick_test.sh [port]
# ============================================================================

PORT=${1:-8080}

echo "======================================"
echo "Quick Server Test - Port $PORT"
echo "======================================"
echo ""

# Check if server is running
echo "[1] Testing if server is responding..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 http://localhost:$PORT/ 2>/dev/null)

if [ "$HTTP_CODE" = "000" ]; then
    echo "    ERROR: Server is not running on port $PORT"
    echo "    Start the server with: ./myserver $PORT"
    exit 1
fi

echo "    Server is responding (HTTP $HTTP_CODE)"
echo ""

# Test basic GET
echo "[2] Testing GET /index.html..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT/index.html)
echo "    Response: HTTP $HTTP_CODE"
[ "$HTTP_CODE" = "200" ] && echo "    PASS" || echo "    FAIL (expected 200)"
echo ""

# Test root path
echo "[3] Testing GET / (root)..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT/)
echo "    Response: HTTP $HTTP_CODE"
[ "$HTTP_CODE" = "200" ] && echo "    PASS" || echo "    FAIL (expected 200)"
echo ""

# Test CSS
echo "[4] Testing GET /style.css..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT/style.css)
echo "    Response: HTTP $HTTP_CODE"
[ "$HTTP_CODE" = "200" ] && echo "    PASS" || echo "    FAIL (expected 200)"
echo ""

# Test 404
echo "[5] Testing 404 (non-existent file)..."
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT/nonexistent.xyz)
echo "    Response: HTTP $HTTP_CODE"
[ "$HTTP_CODE" = "404" ] && echo "    PASS" || echo "    FAIL (expected 404)"
echo ""

# Test concurrent requests
echo "[6] Testing 10 concurrent requests..."
for i in {1..10}; do
    curl -s -o /dev/null http://localhost:$PORT/index.html &
done
wait
HTTP_CODE=$(curl -s -o /dev/null -w "%{http_code}" http://localhost:$PORT/index.html)
echo "    Server still responding: HTTP $HTTP_CODE"
[ "$HTTP_CODE" = "200" ] && echo "    PASS" || echo "    FAIL"
echo ""

# Test headers
echo "[7] Checking HTTP headers..."
curl -s -I http://localhost:$PORT/index.html | head -5
echo ""

echo "======================================"
echo "Quick test complete!"
echo "======================================"
