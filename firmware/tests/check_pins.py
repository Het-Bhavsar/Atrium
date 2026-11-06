"""Check the atrium_pod board files against the pod netlist.

Every nRF52832 pin the firmware uses must sit on the net the schematic puts there.
Usage: python3 -I firmware/tests/check_pins.py [--netlist PATH]   (exit code 1 on any mismatch)
The default netlist is hardware/pod/schematic/pod_pin_netlist.csv.
"""
import argparse
import csv
import os
import re
import sys

FW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
REPO = os.path.dirname(FW)
NETLIST = os.path.join(REPO, 'hardware', 'pod', 'schematic', 'pod_pin_netlist.csv')
BOARD = os.path.join(FW, 'boards', 'localforge', 'atrium_pod')

# board-file pattern -> the net it must land on
GPIO_PROPS = {
    'ship-gpios': 'SHIP', 'chg-inh-gpios': 'CHG_INH', 'chrg-n-gpios': 'CHRG_N',
    'ntc-pwr-gpios': 'NTC_PWR', 'int-gpios': 'PPG_INT', 'irq-gpios': 'IMU_INT1',
    'cs-gpios': 'FL_CS', 'en-gpios': 'PPG_EN', 'sync-gpios': 'PPG_SYNC',
    'gpios': 'LED_K',  # the status LED's gpio-leds child
}
PSELS = {'TWIM_SDA': 'SDA', 'TWIM_SCL': 'SCL', 'SPIM_SCK': 'FL_SCK', 'SPIM_MOSI': 'FL_MOSI',
         'SPIM_MISO': 'FL_MISO'}
AIN = {'NRF_SAADC_AIN0': 'VBUS_SENSE', 'NRF_SAADC_AIN1': 'NT1_SENSE', 'NRF_SAADC_AIN5': 'VBAT_SENSE',
       'NRF_SAADC_AIN6': 'NT3_SENSE'}
# nRF52832 analog inputs: AINn -> P0.xx
AIN_PIN = {0: 2, 1: 3, 2: 4, 3: 5, 4: 28, 5: 29, 6: 30, 7: 31}
# nets on U1 that the firmware does not drive, with the reason
NOT_IN_DT = {'IMU_INT2': 'LSM6DSOX INT2: MLC pulses; firmware polls MLC0_SRC every 39 samples instead',
             'NRESET': 'pin reset left off; SWD on TP7',
             'NC': 'no connection (P0.00, P0.01: no crystal, LFRC; P0.04: heater; P0.05: TH3)'}


def netlist_pins(path):
    """net -> P0 number for every U1 GPIO ball."""
    out = {}
    with open(path, encoding='utf-8') as f:
        for r in csv.DictReader(f):
            m = re.match(r'P0\.(\d+)', r['pin_name_or_value'])
            if r['ref'] == 'U1' and m:
                out[r['net']] = int(m.group(1))
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
