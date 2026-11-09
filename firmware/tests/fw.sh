#!/usr/bin/env bash
# Build and test the Atrium pod firmware in Docker (Zephyr v4.4.2, SDK 1.0.1).
# Usage: firmware/tests/fw.sh image | test | build | debug | bench | shell
set -euo pipefail

fw=$(cd "$(dirname "$0")/.." && pwd)
repo=$(dirname "$fw")
img=atrium-fw:4.4.2
# the commit goes into DIS Firmware Revision (0.1.0+<commit>), so recordings can be traced to
# the build; read here, because a worktree's git data sits outside the mount
rev=$(git -C "$repo" rev-parse --short=7 HEAD 2>/dev/null || echo unknown)
git -C "$repo" diff --quiet HEAD 2>/dev/null || rev="$rev-dirty"
run() { docker run --rm -e ATRIUM_GIT_REV="$rev" -v "$repo":/atrium -w /atrium/firmware "$img" bash -c "$1"; }

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
debug)
	# bring-up image with RTT logs, no MCUboot (src/debug.conf, src/debug.overlay)
	run 'west build -b atrium_pod -p -d build/debug src -- -DBOARD_ROOT=/atrium/firmware \
	       -DEXTRA_CONF_FILE=debug.conf -DEXTRA_DTC_OVERLAY_FILE=debug.overlay &&
	     ls -l build/debug/zephyr/zephyr.hex'
	;;
bench)
	# vibration bench rig for the nRF52 DK (tests/vib_bench, docs/ble_gatt.md 4.5); no MCUboot
	run 'west build -b nrf52dk/nrf52832 -p -d build/vib_bench tests/vib_bench &&
	     ls -l build/vib_bench/zephyr/zephyr.hex'
	;;
shell)
	docker run --rm -it -v "$repo":/atrium -w /atrium/firmware "$img" bash
	;;
*)
	echo "usage: $0 image | test | build | debug | bench | shell" >&2
	exit 2
	;;
esac
