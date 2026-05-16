#!/bin/bash

URL=${1:-http://localhost:8080/}
TOTAL=${2:-100}
CONCURRENCY=${3:-10}

TMPDIR=$(mktemp -d)
trap "rm -rf $TMPDIR" EXIT

echo "================ STRESS TEST ================"
echo "URL: $URL  |  Requests: $TOTAL  |  Concurrency: $CONCURRENCY"

START=$(date +%s)

launched=0
while [ $launched -lt $TOTAL ]; do
    batch=0
    while [ $batch -lt $CONCURRENCY ] && [ $launched -lt $TOTAL ]; do
        (
            code=$(curl -s -o /dev/null -w "%{http_code}" --max-time 5 "$URL")
            if [ "$code" -ge 200 ] && [ "$code" -lt 400 ]; then
                echo "ok" >> "$TMPDIR/results"
            else
                echo "fail" >> "$TMPDIR/results"
            fi
        ) &
        batch=$((batch + 1))
        launched=$((launched + 1))
    done
    wait
done

END=$(date +%s)
ELAPSED=$((END - START))

OK=$(grep "^ok$" "$TMPDIR/results" 2>/dev/null | wc -l)
FAIL=$(grep "^fail$" "$TMPDIR/results" 2>/dev/null | wc -l)
AVAIL_X10=$(( OK * 1000 / TOTAL ))
AVAIL_INT=$(( AVAIL_X10 / 10 ))
AVAIL_DEC=$(( AVAIL_X10 % 10 ))

echo "Done in ${ELAPSED}s  |  OK: $OK  FAIL: $FAIL  |  Availability: ${AVAIL_INT}.${AVAIL_DEC}%"
if [ $AVAIL_X10 -ge 995 ]; then
    echo "PASS (>= 99.5%)"
else
    echo "FAIL (< 99.5%)"
fi
