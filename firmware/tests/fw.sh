#!/usr/bin/env bash
# Build and test the Atrium pod firmware in Docker (Zephyr v4.4.2, SDK 1.0.1).
# Usage: firmware/tests/fw.sh image | test | build | shell
set -euo pipefail

fw=$(cd "$(dirname "$0")/.." && pwd)
repo=$(dirname "$fw")
img=atrium-fw:4.4.2
run() { docker run --rm -v "$repo":/atrium -w /atrium/firmware "$img" bash -c "$1"; }

case "${1:-}" in
image)
	docker build -t "$img" -f "$fw/tests/Dockerfile" "$fw"
	;;
test)
	# host unit tests on native_sim, plus a build of the firmware for the board
	run '$ZEPHYR_BASE/scripts/twister -T . -A /atrium/firmware/boards \
	     -p native_sim/native/64 -p atrium_pod -O /tmp/twister --inline-logs'
	;;
build)
	run 'west build -b atrium_pod --sysbuild -p -d build src -- -DBOARD_ROOT=/atrium/firmware &&
	     python3 $ZEPHYR_BASE/scripts/build/mergehex.py -o build/atrium_pod_merged.hex \
	       build/mcuboot/zephyr/zephyr.hex build/src/zephyr/zephyr.signed.hex &&
	     ls -l build/atrium_pod_merged.hex build/src/zephyr/zephyr.signed.bin'
	;;
shell)
	docker run --rm -it -v "$repo":/atrium -w /atrium/firmware "$img" bash
	;;
*)
	echo "usage: $0 image | test | build | shell" >&2
	exit 2
	;;
esac
