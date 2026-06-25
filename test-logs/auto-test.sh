#!/bin/bash
LOGDIR="test-logs"
mkdir -p "$LOGDIR"

echo "=== VoxelCraft Automated Test ==="
echo ""

echo "[1/4] Testing startup (15 seconds)..."
timeout 15 ./build/voxelcraft > "$LOGDIR/auto-startup.log" 2>&1
RC=$?
if [ $RC -eq 0 ] || [ $RC -eq 124 ]; then
    echo "  PASS: Game launched and ran for 15 seconds"
else
    echo "  FAIL: Game crashed (exit code $RC)"
fi

if grep -q "FATAL" "$LOGDIR/auto-startup.log"; then
    echo "  FAIL: FATAL error found:"
    grep "FATAL" "$LOGDIR/auto-startup.log"
fi

if grep -q "\[engine\] OpenGL" "$LOGDIR/auto-startup.log"; then
    echo "  PASS: OpenGL initialized"
else
    echo "  FAIL: OpenGL not initialized"
fi

if grep -q "\[INFO\] \[world\] World initialized" "$LOGDIR/auto-startup.log"; then
    echo "  PASS: World initialized"
else
    echo "  FAIL: World not initialized"
fi

if grep -q "Loading complete" "$LOGDIR/auto-startup.log"; then
    echo "  PASS: Loading completed"
else
    echo "  WARN: Loading did not complete in 15 seconds"
fi

echo ""
echo "[2/4] Analyzing log levels..."
ERRORS=$(grep -c "\[ERROR\]" "$LOGDIR/auto-startup.log" 2>/dev/null || echo 0)
WARNS=$(grep -c "\[WARN\]" "$LOGDIR/auto-startup.log" 2>/dev/null || echo 0)
INFOS=$(grep -c "\[INFO\]" "$LOGDIR/auto-startup.log" 2>/dev/null || echo 0)
echo "  ERROR: $ERRORS"
echo "  WARN:  $WARNS"
echo "  INFO:  $INFOS"

if [ "$ERRORS" -gt 0 ]; then
    echo "  FAIL: $ERRORS errors found"
    grep "\[ERROR\]" "$LOGDIR/auto-startup.log"
else
    echo "  PASS: No errors"
fi

echo ""
echo "[3/4] Checking for crashes..."
if grep -q "Assertion" "$LOGDIR/auto-startup.log"; then
    echo "  FAIL: Assertion failure found:"
    grep "Assertion" "$LOGDIR/auto-startup.log"
else
    echo "  PASS: No assertion failures"
fi

if grep -q "Segmentation fault" "$LOGDIR/auto-startup.log"; then
    echo "  FAIL: Segfault detected"
else
    echo "  PASS: No segfaults"
fi

echo ""
echo "[4/4] Summary:"
echo "  Log file: $LOGDIR/auto-startup.log"
echo "  Total lines: $(wc -l < "$LOGDIR/auto-startup.log")"
echo ""
echo "=== Test complete ==="
