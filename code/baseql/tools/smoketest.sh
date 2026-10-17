#!/bin/sh
#
# Smoke test for the baseql game module: start a dedicated server on a Quake
# Live map with bots, let it run, and check that the map, the bots and the
# game module all came up and that nobody crashed.
#
# Needs a Quake Live dedicated server install for its maps and bot files
# (steamcmd +login anonymous +app_update 349090), and a build made with
# -DBUILD_BASEQL=ON.
#
# usage: smoketest.sh <build/Release dir> <qlds dir> [map] [seconds] [extra server args...]

set -u

if [ $# -lt 2 ]; then
	echo "usage: $0 <build/Release dir> <qlds dir> [map] [seconds] [extra server args...]" >&2
	exit 2
fi

BUILD_DIR=$1
QLDS_DIR=$2
MAP=${3:-campgrounds}
SECONDS_TO_RUN=${4:-20}
shift 2
[ $# -gt 0 ] && shift
[ $# -gt 0 ] && shift

for f in "$BUILD_DIR/ioq3ded" "$BUILD_DIR/baseql/vm/qagame.qvm" "$QLDS_DIR/baseq3/pak00.pk3"; do
	if [ ! -e "$f" ]; then
		echo "missing $f" >&2
		exit 2
	fi
done

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
mkdir -p "$WORK/baseql" "$WORK/home"
ln -s "$(cd "$QLDS_DIR/baseq3" && pwd)/pak00.pk3" "$WORK/baseql/pak00.pk3"
ln -s "$(cd "$BUILD_DIR/baseql" && pwd)/vm" "$WORK/baseql/vm"
LOG="$WORK/server.log"

"$BUILD_DIR/ioq3ded" \
	+set com_basegame baseql +set fs_basepath "$WORK" +set fs_homepath "$WORK/home" \
	+set sv_pure 0 +set vm_game 2 +set bot_enable 1 \
	+set g_doWarmup 0 +set fraglimit 0 +set timelimit 0 \
	"$@" \
	+map "$MAP" +addbot anarki 4 +addbot sarge 4 +addbot klesk 4 \
	> "$LOG" 2>&1 &
PID=$!

sleep "$SECONDS_TO_RUN"
ALIVE=1
kill -0 "$PID" 2>/dev/null || ALIVE=0
kill "$PID" 2>/dev/null
wait "$PID" 2>/dev/null

FAIL=0
check() {
	if [ "$1" -eq 0 ]; then
		echo "FAIL: $2"
		FAIL=1
	else
		echo "ok:   $2"
	fi
}

BOTS=$(grep -c 'entered the game' "$LOG")
# "Received signal 15" is the server shutting down after our kill
ERRORS=$(grep -iE 'Sys_Error|VM_Error|ERROR:|Segmentation|signal' "$LOG" | grep -vc 'Received signal 15')

check "$ALIVE" "server still running after ${SECONDS_TO_RUN}s"
check "$(grep -c 'File "vm/qagame.qvm" found' "$LOG")" "baseql qagame.qvm loaded"
check "$(grep -c '^AAS initialized' "$LOG")" "bot navigation (AAS) loaded for $MAP"
check "$( [ "$BOTS" -ge 3 ] && echo 1 || echo 0)" "3 bots entered the game"
check "$(grep -c '^Kill: ' "$LOG")" "at least one kill"
check "$( [ "$ERRORS" -eq 0 ] && echo 1 || echo 0)" "no errors or crashes in the log"

if [ "$FAIL" -ne 0 ]; then
	echo "--- server log ($LOG) ---"
	cat "$LOG"
fi
exit "$FAIL"
