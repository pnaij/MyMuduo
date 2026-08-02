#!/bin/bash
# QPS Benchmark - outputs to files to avoid log noise

BINDIR=/home/jianp/Documents/muduoSelf/bin
LIBDIR=/home/jianp/Documents/muduoSelf/lib
OUTDIR=/tmp/bench_results
mkdir -p $OUTDIR

export LD_LIBRARY_PATH=$LIBDIR

run_test() {
    local name="$1"
    shift

    echo "=== $name ===" | tee -a $OUTDIR/summary.txt
    echo "Params: $@" | tee -a $OUTDIR/summary.txt

    # Start server with logs to file
    $BINDIR/benchserver > $OUTDIR/server.log 2>&1 &
    local pid=$!
    sleep 2

    # Check server is alive
    if ! kill -0 $pid 2>/dev/null; then
        echo "ERROR: server failed to start" | tee -a $OUTDIR/summary.txt
        return 1
    fi

    # Run benchmark
    $BINDIR/benchmark "$@" 2>&1 | tee -a $OUTDIR/summary.txt

    # Stop server
    kill -9 $pid 2>/dev/null
    wait $pid 2>/dev/null
    sleep 2
    echo "" | tee -a $OUTDIR/summary.txt
}

# Clean start
pkill -9 -f benchserver 2>/dev/null
sleep 1

echo "muduoSelf QPS Benchmark Results" > $OUTDIR/summary.txt
echo "Date: $(date)" >> $OUTDIR/summary.txt
echo "==========================================" >> $OUTDIR/summary.txt
echo "" >> $OUTDIR/summary.txt

run_test "Test 1: Low Concurrency (500 conns, 2 threads, 64B)" -c 500 -t 2 -d 10 -s 64
run_test "Test 2: Medium Concurrency (2000 conns, 4 threads, 64B)" -c 2000 -t 4 -d 10 -s 64
run_test "Test 3: High Concurrency (4000 conns, 4 threads, 64B)" -c 4000 -t 4 -d 10 -s 64
run_test "Test 4: Large Messages - 256B" -c 1000 -t 4 -d 10 -s 256
run_test "Test 5: Large Messages - 1024B" -c 500 -t 4 -d 10 -s 1024
run_test "Test 6: 8 threads, 4000 conns" -c 4000 -t 8 -d 10 -s 64

echo "" | tee -a $OUTDIR/summary.txt
echo "==========================================" | tee -a $OUTDIR/summary.txt
echo "Full results saved to $OUTDIR/summary.txt" | tee -a $OUTDIR/summary.txt
