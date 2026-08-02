#!/bin/bash
# Comprehensive QPS benchmark script
set -e

BENCHMARK=/home/jianp/Documents/muduoSelf/bin/benchmark
BENCHSERVER=/home/jianp/Documents/muduoSelf/bin/benchserver
BUILD_DIR=/home/jianp/Documents/muduoSelf/build

# Build
cd "$BUILD_DIR" && cmake --build . -j4 2>/dev/null

echo "=========================================="
echo "  muduoSelf Network Library QPS Benchmark"
echo "=========================================="
echo ""

run_test() {
    local desc="$1"
    shift

    echo "------------------------------------------"
    echo "Test: $desc"
    echo "Params: $@"
    echo ""

    # Start server (silence logs)
    $BENCHSERVER > /dev/null 2>&1 &
    local server_pid=$!
    sleep 1

    # Run benchmark
    $BENCHMARK "$@"

    # Stop server
    kill -9 $server_pid 2>/dev/null
    wait $server_pid 2>/dev/null
    sleep 2  # wait for TIME_WAIT sockets to clear
    echo ""
}

# Test 1: Small connections
run_test "Low concurrency baseline" -c 500 -t 2 -d 10 -s 64

# Test 2: Default
run_test "Medium concurrency" -c 2000 -t 4 -d 10 -s 64

# Test 3: High concurrency
run_test "High concurrency" -c 4000 -t 4 -d 10 -s 64

# Test 4: Large messages
run_test "Large messages (256B)" -c 1000 -t 4 -d 10 -s 256

# Test 5: Very large messages
run_test "Very large messages (1KB)" -c 500 -t 4 -d 10 -s 1024

# Test 6: Small msg, high conn, many threads
run_test "Many connections + threads" -c 4000 -t 8 -d 10 -s 64

echo "=========================================="
echo "  Benchmark Complete"
echo "=========================================="
