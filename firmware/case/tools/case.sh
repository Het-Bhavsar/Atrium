#!/usr/bin/env bash
# Copyright (c) 2026 Het Bhavsar
# SPDX-License-Identifier: Apache-2.0
# Build and test the Atrium charging case firmware in Docker (CH32V003, ch32fun).
# Usage: firmware/case/tools/case.sh image | test | build | shell
#   image  build the toolchain image (once)
#   test   host unit tests (the case state machine, SWD, sensing, LED; and the pod's
#          VIN pattern detector against the case's output)
#   build  the MCU image: firmware/case/build/atrium_case.{bin,hex}, with the size
set -euo pipefail

case_dir=$(cd "$(dirname "$0")/.." && pwd)
fw=$(dirname "$case_dir")
img=atrium-case:ch32fun-b28d285
run() { docker run --rm -v "$fw":/fw -w /fw/case "$img" bash -c "$1"; }

case "${1:-}" in
image)
	docker build -t "$img" -f "$case_dir/tools/Dockerfile" "$case_dir/tools"
	;;
test)
	run 'make clean test'
	;;
build)
	run 'make firmware && riscv64-unknown-elf-size build/atrium_case.elf'
	;;
shell)
	docker run --rm -it -v "$fw":/fw -w /fw/case "$img" bash
	;;
*)
	echo "usage: $0 image | test | build | shell" >&2
	exit 2
	;;
esac
