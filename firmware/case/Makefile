# Atrium charging case firmware, CH32V003F4U6 (ch32fun, no RTOS).
# Usage, from the repo root:
#   make -C firmware/case test            host unit tests (any C compiler)
#   firmware/case/tools/case.sh build     the MCU image in Docker: firmware/case/build/atrium_case.hex

OUT := build
HOSTCC ?= cc
CORE := src/case_sm.c src/led.c src/sense.c src/swd.c src/oc_fast.c
POD := ../src/power/vinpat.c
TESTS := tests/main.c tests/test_sense.c tests/test_led.c tests/test_swd.c tests/test_case_sm.c \
	tests/swd_sim.c tests/test_oc_fast.c
HOST_CFLAGS := -std=c11 -O1 -g -Wall -Wextra -Werror -Isrc -Itests -I../src/power \
	-fsanitize=address,undefined -fno-omit-frame-pointer

.PHONY: test firmware clean

test: $(OUT)/case_tests
	./$(OUT)/case_tests

$(OUT)/case_tests: $(CORE) $(POD) $(TESTS) $(wildcard src/*.h tests/*.h ../src/power/vinpat.h)
	@mkdir -p $(OUT)
	$(HOSTCC) $(HOST_CFLAGS) -o $@ $(CORE) $(POD) $(TESTS) -lm

clean:
	rm -rf $(OUT)

# The MCU image: needs CH32FUN (set in the Docker image, tools/Dockerfile)
ifdef CH32FUN
TARGET := $(OUT)/atrium_case
TARGET_MCU := CH32V003
ADDITIONAL_C_FILES := $(CORE)
EXTRA_CFLAGS := -Isrc -Wsign-compare
include $(CH32FUN)/ch32fun.mk

# ch32fun builds $(TARGET).c: a one-line wrapper keeps the sources in src/
$(OUT)/atrium_case.c: src/main.c
	@mkdir -p $(OUT)
	printf '#include "main.c"\n' > $@

firmware: $(OUT)/atrium_case.c $(TARGET).bin
endif
