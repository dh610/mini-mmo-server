#!/usr/bin/env bash
# 세 모드를 같은 조건으로 연속 측정한다. 조건은 인자로만 바꾼다.
set -u

SERVER=${SERVER:-./build/mmo-server}
BOT=${BOT:-./build/mmo-bot}
COUNT=${COUNT:-200}
MAP=${MAP:-3500}
AOI=${AOI:-500}
CELL=${CELL:-500}
TICK=${TICK:-20}
DURATION=${DURATION:-20}
PATTERN=${PATTERN:-uniform}
SEED=${SEED:-42}
RAMP=${RAMP:-10}
PORT=${PORT:-19400}

RAMP_TOTAL=$(( COUNT * RAMP / 1000 + 3 ))
SRV_DURATION=$(( DURATION + RAMP_TOTAL + 2 ))

echo "count=$COUNT map=$MAP aoi=$AOI cell=$CELL tick=${TICK}Hz pattern=$PATTERN seed=$SEED duration=${DURATION}s"
echo "commit=$(git rev-parse --short HEAD)"

for MODE in broadcast naive grid; do
    echo
    echo "########## $MODE ##########"
    $SERVER --port $PORT --mode $MODE --map $MAP --aoi $AOI --cell $CELL \
            --tick $TICK --max $(( COUNT * 2 )) --duration $SRV_DURATION > /tmp/bench_srv_$MODE.log 2>&1 &
    sleep 0.5
    $BOT --port $PORT --count $COUNT --map $MAP --cell $CELL --tick $TICK \
         --duration $DURATION --seed $SEED --ramp $RAMP --pattern $PATTERN > /tmp/bench_bot_$MODE.log 2>&1
    wait
    grep -E "bytes_per_sec|K_avg|candidates_avg|precision|tick_busy_avg_us|tick_overruns|cell_transitions|snapshots_dropped" /tmp/bench_srv_$MODE.log
    grep -E "connected|snapshot_gaps" /tmp/bench_bot_$MODE.log
    PORT=$(( PORT + 1 ))
done
