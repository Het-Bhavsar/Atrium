# Copyright (c) 2026 Het Bhavsar
# SPDX-License-Identifier: Apache-2.0
"""Check that a signed pod image reports the version it is signed with.

The app's update check (#21) compares the version in the MCUboot header of the published image with
the version the pod reports over BLE after the restart (DIS Firmware Revision, "<x.y.z>+<commit>").
Both must come from firmware/src/VERSION. If they differ, a good update shows as failed, or a new
release shows as already installed.

Usage:
  python3 -I firmware/tests/check_version.py --rev <commit> build/src/zephyr/zephyr.signed.bin
  python3 -I firmware/tests/check_version.py --self-test
Exit code 1 on any mismatch.
"""
import argparse
import os
import re
import struct
import sys

FW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
VERSION_FILE = os.path.join(FW, 'src', 'VERSION')
IMAGE_MAGIC = 0x96F3B83D


def read_version_file(path=VERSION_FILE):
    text = open(path, encoding='utf-8').read()
    v = {}
    for key in ('VERSION_MAJOR', 'VERSION_MINOR', 'PATCHLEVEL'):
        m = re.search(r'^\s*' + key + r'\s*=\s*(\d+)', text, re.M)
        if not m:
            raise ValueError(f'{path}: no {key}')
        v[key] = int(m.group(1))
    return f"{v['VERSION_MAJOR']}.{v['VERSION_MINOR']}.{v['PATCHLEVEL']}"


def header_version(image):
    """x.y.z from the MCUboot image header (bootutil image.h: magic at 0, version at 20)."""
    if len(image) < 32 or struct.unpack_from('<I', image, 0)[0] != IMAGE_MAGIC:
        raise ValueError('not an MCUboot-signed image')
    major, minor, rev = struct.unpack_from('<BBH', image, 20)
    return f'{major}.{minor}.{rev}'


def reported_versions(image, rev):
    """Every x.y.z the image carries in front of "+<rev>": the DIS Firmware Revision string."""
    pat = rb'(\d+)\.(\d+)\.(\d+)(?:-[0-9A-Za-z.]+)?\+' + re.escape(rev.encode())
    return {f'{int(a)}.{int(b)}.{int(c)}' for a, b, c in re.findall(pat, image)}


def check(image, rev, expected):
    """Returns a list of problems (empty when the image is consistent)."""
    problems = []
    signed = header_version(image)
    reported = reported_versions(image, rev)
    if signed != expected:
        problems.append(f'MCUboot header says {signed}, VERSION says {expected}')
    if not reported:
        problems.append(f'no "<x.y.z>+{rev}" Firmware Revision string in the image')
    elif reported != {expected}:
        problems.append(f'the pod would report {", ".join(sorted(reported))} over BLE, '
                        f'VERSION says {expected}')
    return problems


def _fake(header_ver, dis):
    major, minor, rev = (int(x) for x in header_ver.split('.'))
    hdr = struct.pack('<IIHHIIBBHI', IMAGE_MAGIC, 0, 0x200, 0, 0, 0, major, minor, rev, 0)
    return hdr + b'\0' * (0x200 - len(hdr)) + b'rodata\0' + dis + b'\0more'


def self_test():
    cases = [
        ('0.1.0', b'0.1.0+abc1234', 'abc1234', '0.1.0', True),
        ('0.1.1', b'0.1.1+abc1234-dirty', 'abc1234-dirty', '0.1.1', True),
        ('0.1.1', b'0.1.1-rc1+unknown', 'unknown', '0.1.1', True),
        # the case this check exists for: header bumped, BLE string not
        ('0.1.1', b'0.1.0+abc1234', 'abc1234', '0.1.1', False),
        # VERSION bumped but the image signed with an old number
        ('0.1.0', b'0.1.1+abc1234', 'abc1234', '0.1.1', False),
        # no Firmware Revision string at all
        ('0.1.0', b'nothing here', 'abc1234', '0.1.0', False),
    ]
    bad = 0
    for hv, dis, rev, expected, ok in cases:
        got = not check(_fake(hv, dis), rev, expected)
        if got != ok:
            bad += 1
            print(f'self-test FAIL: header {hv}, string {dis!r}, VERSION {expected}: '
                  f'expected {"pass" if ok else "fail"}')
    try:
        header_version(b'\0' * 64)
        bad += 1
        print('self-test FAIL: an unsigned file was accepted')
    except ValueError:
        pass
    print(f'check_version self-test: {len(cases) + 1 - bad} of {len(cases) + 1} passed')
    return 1 if bad else 0


def main():
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('image', nargs='?', help='zephyr.signed.bin')
    ap.add_argument('--rev', help='the commit fw.sh built with (ATRIUM_GIT_REV)')
    ap.add_argument('--self-test', action='store_true')
    a = ap.parse_args()
    if a.self_test:
        return self_test()
    if not a.image or not a.rev:
        ap.error('give an image and --rev, or --self-test')
    expected = read_version_file()
    problems = check(open(a.image, 'rb').read(), a.rev, expected)
    for p in problems:
        print(f'check_version: {p}')
    if not problems:
        print(f'check_version: {expected} in the MCUboot header and in the BLE Firmware Revision')
    return 1 if problems else 0


if __name__ == '__main__':
    sys.exit(main())
