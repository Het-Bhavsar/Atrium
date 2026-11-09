# Copyright (c) 2026 Het Bhavsar
# SPDX-License-Identifier: Apache-2.0
"""Check the atrium_pod board files against the pod netlist.

Every nRF52832 pin the firmware uses must sit on the net the netlist puts there, and every
net on a U1 GPIO must be used or explained.
Usage: python3 -I firmware/tests/check_pins.py [--netlist PATH]   (exit code 1 on any mismatch)
The default netlist is hardware/design-data/netlist/atrium_nets.yaml.
"""
import argparse
import os
import re
import sys

FW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(FW)
NETLIST = os.path.join(REPO, 'hardware', 'design-data', 'netlist', 'atrium_nets.yaml')
BOARD = os.path.join(FW, 'boards', 'atrium', 'atrium_pod')

# board-file pattern -> the net it must land on
GPIO_PROPS = {'ntc-en-gpios': 'NTC_EN', 'int-gpios': 'PPG_INT', 'ldo-en-gpios': 'PPG_LDO_EN',
              'irq-gpios': 'IMU_INT1', 'pg-gpios': 'CHG_PG'}
PSELS = {'TWIM_SDA': 'I2C_SDA', 'TWIM_SCL': 'I2C_SCL'}
AIN = {'NRF_SAADC_AIN1': 'TH1_SENSE', 'NRF_SAADC_AIN0': 'TH4_SENSE', 'NRF_SAADC_AIN4': 'VSYS_SENSE'}
# nRF52832 analog inputs: AINn -> P0.xx
AIN_PIN = {0: 2, 1: 3, 2: 4, 3: 5, 4: 28, 5: 29, 6: 30, 7: 31}
# nets on U1 GPIOs that the board files do not name, with the reason
NOT_IN_DT = {'NRESET_TP': 'P0.21 pin reset on TP1 for the bench: &uicr gpio-as-nreset'}


def netlist_pins(path):
    """net -> P0 number for every U1.P0.xx in the `nets:` section (a plain YAML subset)."""
    text = open(path, encoding='utf-8').read()
    body = text.split('\nnets:', 1)[1]
    body = re.split(r'\n\S', body, maxsplit=1)[0]          # up to the next top-level key
    out = {}
    for m in re.finditer(r'^\s+([A-Z0-9_]+):\s*\[([^\]]*)\]', body, re.M):
        for pin in re.findall(r'U1\.P0\.(\d+)', m.group(2)):
            out[m.group(1)] = int(pin)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--netlist', default=NETLIST)
    nets = netlist_pins(ap.parse_args().netlist)
    text = ''.join(open(os.path.join(BOARD, n), encoding='utf-8').read()
                   for n in ('atrium_pod.dts', 'atrium_pod-pinctrl.dtsi'))
    found, bad = {}, []

    for prop, net in GPIO_PROPS.items():
        for m in re.finditer(r'(?<![\w-])' + re.escape(prop) + r'\s*=\s*<&gpio0\s+(\d+)', text):
            found[net] = int(m.group(1))
    for psel, net in PSELS.items():
        pins = {int(p) for p in re.findall(r'NRF_PSEL\(' + psel + r',\s*0,\s*(\d+)\)', text)}
        if len(pins) == 1:
            found[net] = pins.pop()
        elif pins:
            bad.append(f'{psel}: default and sleep states differ {sorted(pins)}')
    for ain, net in AIN.items():
        if re.search(r'zephyr,input-positive\s*=\s*<' + ain + '>', text):
            found[net] = AIN_PIN[int(ain[-1])]

    for net, pin in sorted(found.items()):
        want = nets.get(net)
        status = 'ok' if want == pin else f'MISMATCH, netlist P0.{want:02d}' if want is not None else 'net not on U1'
        print(f'{net:<11} P0.{pin:02d}  {status}')
        if status != 'ok':
            bad.append(net)
    for net in sorted(set(nets) - set(found)):
        why = NOT_IN_DT.get(net)
        print(f'{net:<11} P0.{nets[net]:02d}  not in the board files: {why or "UNEXPLAINED"}')
        if not why:
            bad.append(net)

    print(f'\n{len(found)} pins checked, {len(bad)} problems')
    return 1 if bad else 0


if __name__ == '__main__':
    sys.exit(main())
