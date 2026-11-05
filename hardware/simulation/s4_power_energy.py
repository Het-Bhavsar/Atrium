"""S4. Bottom-up pod and case energy model with datasheet currents, and 30-day wear simulation.

The package budget uses block currents with no source and calls them "mA from the cell", though most blocks sit on the 1.8 V
buck rail. This rebuilds each mode from per-event charge. Every number below carries its source or [ASSUMED].

Rail currents are at 1.8 V and get converted through the buck at the live VBAT. LEDs and the fuel gauge sit on VBAT directly.
Usage: python3 -I s4_power_energy.py <out.json>
"""
import json, math, sys
import numpy as np

V_RAIL = 1.8
ETA_BUCK = {1e-5: 0.885, 1e-4: 0.895, 1e-3: 0.90, 1e-2: 0.91, 1e-1: 0.92}    # TPS62840 Fig. 16, 3.6 V to 1.8 V, curve read


def eta_buck(i_load):
    x = np.log10(list(ETA_BUCK)); y = list(ETA_BUCK.values())
    return float(np.interp(np.log10(max(i_load, 1e-6)), x, y))


# ---- Datasheet values
NRF_IDLE = 1.9e-6            # System ON idle, RTC, nRF52832 PS v1.9 (at 3 V DC/DC, LDO idle is close)
NRF_CPU_LDO = 7.4e-3         # flash, cache on, 64 MHz, LDO
NRF_TX0_LDO = 14.2e-3        # [ASSUMED] 2 x the 7.1 mA DC/DC figure, same 2:1 ratio as CPU; LDO row not found
NRF_RX_LDO = 13.0e-3         # [ASSUMED] 2 x 6.5 mA
NRF_HF_DMA = 1.5e-3          # [ASSUMED] HFCLK + EasyDMA with the CPU asleep
TWIM = 50e-6                 # nRF PS
IMU_60 = 17e-6               # LSM6DSV16X LPM1 accel at 60 Hz, DS13510 Table 3
IMU_15 = 7e-6                # [ASSUMED] between 4 uA at 1.875 Hz and 17 uA at 60 Hz
TMP_1HZ_8AVG = 16e-6         # TMP117
TMP_SD = 0.15e-6
FG_HIB = 3e-6                # MAX17048 hibernate
LPB = 0.5e-6
TPS_IQ = 60e-9
FLASH_DPD = 0.1e-6
FLASH_PROG_C = 15e-3 * 0.8e-3   # coulomb per 256 B page
FLASH_ERASE_C = 15e-3 * 45e-3   # coulomb per 4 KB sector
AFE_SD = 1.0e-6
T_LED_ON = 117.1e-6 + 12e-6  # MAX86174A ILED table implies about 12 us beyond tINT
# MAX86174A IDD for tINT 14.6 us: dual channel 480 uA at 2048 exposures/s, single 300 uA at 2048.
AFE_Q_SINGLE_14 = 300e-6 / 2048
AFE_Q_DUAL_14 = 480e-6 / 2048
AFE_TINT_SCALE = {"typ": (117.1 + 20) / (14.6 + 20), "wc": 117.1 / 14.6}   # [ASSUMED] 20 us overhead for typ, pure scaling for worst case


def mode_current(m, case="typ", vbat=3.8, split=False):
    """m: dict with leds=[(mA, Hz, channels_on)], ble_bytes, flash_bytes, cpu_ms_s, tmp_hz, imu, afe_out_bytes."""
    rows = {}
    q = 0.0
    for ma, hz, ch in m["leds"]:
        q += (AFE_Q_DUAL_14 if ch == 2 else AFE_Q_SINGLE_14) * hz
    rows["AFE"] = (q * AFE_TINT_SCALE[case], "rail")
    rows["LED"] = (sum(ma * 1e-3 * T_LED_ON * hz for ma, hz, ch in m["leds"]), "vbat")
    # I2C FIFO read: 3 bytes per channel sample, 400 kHz, 9 bits per byte, plus address overhead
    i2c_bytes = sum(ch * 3 * hz for ma, hz, ch in m["leds"]) + m["imu_bytes"]
    t_i2c = i2c_bytes * 9 / 400e3
    rows["I2C transfer"] = (t_i2c * (NRF_HF_DMA + TWIM + 0.5 * 2 * V_RAIL / 4.7e3), "rail")
    rows["CPU"] = (m["cpu_ms_s"] * 1e-3 * NRF_CPU_LDO + NRF_IDLE, "rail")
    # BLE: 244 byte payload packet about 2.1 ms air time, 0.08 ms empty ack, 0.15 ms IFS at RX. Connection event overhead every CI.
    pps = m["ble_bytes"] / 244.0
    q_pkt = (2.1e-3 * NRF_TX0_LDO + 0.3e-3 * NRF_RX_LDO)
    ci = m.get("ci", 0.5)
    q_evt = 0.6e-3 * 3e-3 + 0.14e-3 * NRF_RX_LDO + 0.08e-3 * NRF_TX0_LDO     # HFXO start at 3 mA [ASSUMED], plus ramp and an empty exchange
    rows["BLE"] = (pps * q_pkt + q_evt / ci, "rail")
    # Flash: page program per 256 B, sector erase per 4 KB
    rows["flash"] = ((m["flash_bytes"] / 256) * FLASH_PROG_C + (m["flash_bytes"] / 4096) * FLASH_ERASE_C + FLASH_DPD, "rail")
    rows["IMU"] = (IMU_60 if m["imu_hz"] >= 60 else IMU_15, "rail")
    rows["TMP117"] = (TMP_1HZ_8AVG * m["tmp_hz"] + TMP_SD, "rail")
    rows["NTC"] = (1.8 / 20e3 * 0.05e-3 * 1, "rail")           # 90 uA for 50 us per second
    rows["fuel gauge"] = (FG_HIB, "vbat")
    rows["protection"] = (LPB, "vbat")
    rail = sum(v for v, w in rows.values() if w == "rail")
    eta = eta_buck(rail)
    total = TPS_IQ + sum(v if w == "vbat" else v * V_RAIL / (vbat * eta) for v, w in rows.values())
    return total, rows, eta


MODES = {
    "full": dict(leds=[(50, 50, 2), (40, 50, 2), (15, 50, 2)], ble_bytes=1526, flash_bytes=0, cpu_ms_s=6.0, tmp_hz=1, imu_bytes=720, imu_hz=60, ci=0.2),
    "smart": dict(leds=[(15, 50, 2), (50, 25, 2), (40, 25, 2)], ble_bytes=758, flash_bytes=0, cpu_ms_s=5.0, tmp_hz=1, imu_bytes=360, imu_hz=60, ci=0.4),
    "lite": dict(leds=[(15, 50, 1), (50, 5, 2), (40, 5, 2)], ble_bytes=120, flash_bytes=0, cpu_ms_s=4.0, tmp_hz=1, imu_bytes=180, imu_hz=30, ci=1.0),
    "sleep": dict(leds=[(10, 64, 1), (20, 5, 1), (20, 5, 1)], ble_bytes=0, flash_bytes=297, cpu_ms_s=2.0, tmp_hz=0.1, imu_bytes=75, imu_hz=15, ci=4.0),
}
# Improved modes: PD1 open and at 3.5 mm, so green drops to 5 mA at 32 Hz, near IR and red run in their own 5 mA slots, far IR 50 mA only on demand
IMPROVED = {
    "full+": dict(leds=[(5, 50, 1), (5, 50, 1), (5, 50, 1), (50, 50, 1), (40, 50, 1)], ble_bytes=1526, flash_bytes=0, cpu_ms_s=6.0, tmp_hz=1, imu_bytes=720, imu_hz=60, ci=0.2),
    "smart+": dict(leds=[(5, 32, 1), (5, 16, 1), (5, 16, 1), (50, 16, 1), (40, 16, 1)], ble_bytes=480, flash_bytes=0, cpu_ms_s=5.0, tmp_hz=1, imu_bytes=360, imu_hz=60, ci=0.5),
    "lite+": dict(leds=[(5, 32, 1), (5, 2, 1)], ble_bytes=60, flash_bytes=0, cpu_ms_s=4.0, tmp_hz=1, imu_bytes=180, imu_hz=30, ci=1.0),
    "sleep+": dict(leds=[(5, 32, 1), (5, 2, 1), (5, 2, 1)], ble_bytes=0, flash_bytes=120, cpu_ms_s=2.0, tmp_hz=0.1, imu_bytes=75, imu_hz=15, ci=4.0),
}
PACKAGE = {"full": 1.61, "smart": 1.03, "lite": 0.56, "sleep": 0.46}


def show(name, m):
    t, rows, eta = mode_current(m, "typ")
    tw, _, _ = mode_current(m, "wc")
    parts = sorted(((v if w == "vbat" else v * V_RAIL / (3.8 * eta), k) for k, (v, w) in rows.items()), reverse=True)[:7]
    print(f"{name:<8} {t*1e3:6.3f} mA typ, {tw*1e3:6.3f} wc AFE  | " + "  ".join(f"{k} {v*1e6:.0f}" for v, k in parts))
    return t, tw, {k: (v if w == "vbat" else v * V_RAIL / (3.8 * eta)) for k, (v, w) in rows.items()}


OCV_S = [0, 0.03, 0.05, 0.1, 0.2, 0.3, 0.4, 0.5, 0.6, 0.7, 0.8, 0.9, 1.0]
OCV_V = [3.0, 3.30, 3.45, 3.60, 3.68, 3.73, 3.77, 3.81, 3.87, 3.93, 4.00, 4.08, 4.18]    # generic LCO curve [ASSUMED]


def ocv(s):
    return float(np.interp(s, OCV_S, OCV_V))


def wear(cap, day_ma, night_ma, dock_h, i_chg, days=30, wake=7.0, dock_at=7.0):
    """One-minute steps. Pod docks each morning. Returns min SoC and hours lost."""
    dt = 1 / 60; soc = 1.0; lost = 0.0; soc_min = 1.0; dt_day = int(24 * 60)
    for n in range(int(days * 24 * 60)):
        h = (n * dt) % 24
        if dock_at <= h < dock_at + dock_h:
            ic = i_chg if soc < 0.8 else max(i_chg * (1 - soc) / 0.2, 0)
            if ic < 0.1 * i_chg:
                ic = 0
            soc = min(1.0, soc + ic * dt / cap)
        else:
            i = day_ma if (dock_at + dock_h) <= h < 23 else night_ma
            if soc > 0.03:
                soc -= i * dt / cap
            else:
                lost += dt
            soc_min = min(soc_min, soc)
    return soc_min, lost / days


def case_ratio(v5=5.0, vcase=3.7, eta=0.935, drop=0.0):
    """Case mAh per pod mAh. A linear charger passes charge one-to-one, so this is V5/(Vcase * eta_boost)."""
    return v5 / (vcase * eta)


def case_days(mah_per_day, v5=5.0, cap=300.0, usable=0.90, standby_ua=None, dock_h=1.0, lid_closed_h=None):
    """How many days a full case keeps the pod topped up, counting standby draw."""
    r = case_ratio(v5)
    sb = standby_ua if standby_ua is not None else case_standby(v5)["closed_empty"]
    per_day = mah_per_day * r + sb * 1e-3 * 24
    return cap * usable / per_day


def case_standby(v5=5.0):
    """uA from the case cell. Lid closed runs boost and switch. Values from TPS61099, SY6280, DRV5032, BQ24075, R11."""
    hall = 1.6
    bq = 4.3                     # BQ24075 IBAT(PDWN) typ, listed at 85 C
    pcm = 3.0                    # typical pack PCM [ASSUMED]
    fb = v5 / 5e6                # 5 M total FB divider current, uA later
    sw_iq = 38.0
    boost_iq = 1.0
    closed = (sw_iq + fb * 1e6 + boost_iq * 0) * v5 / (3.7 * 0.90) + boost_iq + hall + bq + pcm
    opened = hall + bq + pcm + 3.7 / 1e6 * 1e6 + 0.2
    pod_done = 125.0 * v5 / (3.7 * 0.90)   # BQ25101 IIN(STDBY) max with a charged pod seated
    return dict(closed_empty=closed, open=opened, closed_full_pod=closed + pod_done)


def main():
    R = {"package": PACKAGE}
    print("Pod current by mode at 3.8 V, uA per block at the battery\n")
    R["modes"] = {}
    for k, m in MODES.items():
        t, tw, parts = show(k, m)
        print(f"         package budget {PACKAGE[k]:.2f} mA, ratio {PACKAGE[k]/(t*1e3):.2f}x")
        R["modes"][k] = dict(typ=t * 1e3, wc=tw * 1e3, parts=parts)
    print("\nWith PD1 open at 3.5 mm and per-slot LED current\n")
    for k, m in IMPROVED.items():
        t, tw, parts = show(k, m)
        R["modes"][k] = dict(typ=t * 1e3, wc=tw * 1e3, parts=parts)

    # What if firmware is heavier than assumed? CPU 20 ms/s instead of 2-6, and BLE transmit at +4 dBm
    print("\nSensitivity at the battery, typ AFE: CPU 20 ms/s, then radio currents +40 % (+4 dBm)")
    R["sens"] = {}
    for k in ("smart", "lite", "sleep", "smart+", "lite+", "sleep+"):
        m = dict((MODES | IMPROVED)[k])
        a = mode_current(m)[0]
        m2 = dict(m, cpu_ms_s=20.0)
        b = mode_current(m2)[0]
        print(f"  {k:<8} {a*1e3:.3f} mA -> {b*1e3:.3f} mA with heavy firmware")
        R["sens"][k] = dict(base=a * 1e3, heavy_fw=b * 1e3)
    # Radio transmit power and connection interval sensitivities
    print("\nDaily use. 16 h day, 8 h night, 1 h dock")
    R["daily"] = {}
    for label, d, n in (("package smart", PACKAGE["smart"], PACKAGE["sleep"]), ("package lite", PACKAGE["lite"], PACKAGE["sleep"]),
                        ("bottom-up smart", R["modes"]["smart"]["typ"], R["modes"]["sleep"]["typ"]),
                        ("bottom-up lite", R["modes"]["lite"]["typ"], R["modes"]["sleep"]["typ"]),
                        ("bottom-up smart, worst AFE", R["modes"]["smart"]["wc"], R["modes"]["sleep"]["wc"]),
                        ("improved smart", R["modes"]["smart+"]["typ"], R["modes"]["sleep+"]["typ"]),
                        ("improved lite", R["modes"]["lite+"]["typ"], R["modes"]["sleep+"]["typ"])):
        mah = d * 15 + n * 8
        days45 = 45 * 0.95 / mah * 1.0
        days_eol = 45 * 0.8 * 0.95 / mah
        cd = case_days(mah)
        print(f"  {label:<28} {mah:5.1f} mAh/day  pod alone {days45:5.1f} d new, {days_eol:5.1f} d at 80 % capacity  case {cd:5.1f} d (V5 5.0) {case_days(mah, 4.7):5.1f} d (V5 4.7)")
        R["daily"][label] = dict(mah=mah, days_new=days45, days_eol=days_eol, case=cd, case47=case_days(mah, 4.7))

    # Charge rate: datasheet K_ISET window, and what the cell allows
    print("\nCharge current from R1 (BQ25101 K_ISET 129-145, typ 135 A.ohm)")
    R["iset"] = {}
    for r1, name in ((3300, "package 3.3k"), (4020, "4.02k"), (6040, "6.04k")):
        lo, typ, hi = 129 / r1 * 1e3, 135 / r1 * 1e3, 145 / r1 * 1e3
        print(f"  {name:<14} {lo:4.1f}/{typ:4.1f}/{hi:4.1f} mA, on 45 mAh {typ/45:.2f}C, worst {hi/40:.2f}C on a 40 mAh cell. Term {0.1*typ:.1f} mA, pre {0.2*typ:.1f} mA")
        R["iset"][name] = dict(lo=lo, typ=typ, hi=hi, c_45=typ / 45, c_worst=hi / 40)
    for i_chg, name in ((40.9, "3.3k"), (33.6, "4.02k"), (22.4, "6.04k")):
        soc_min, lost = wear(45, R["modes"]["smart"]["typ"], R["modes"]["sleep"]["typ"], 1.0, i_chg)
        soc_min2, lost2 = wear(45, R["modes"]["smart"]["typ"], R["modes"]["sleep"]["typ"], 0.5, i_chg)
        print(f"  daily dock, smart + sleep, {name}: 1 h dock min SoC {soc_min*100:.0f} %, gap {lost:.2f} h/day; 30 min dock min SoC {soc_min2*100:.0f} %, gap {lost2:.2f} h/day")
        R["iset"][name + "_wear"] = dict(min_1h=soc_min, gap_1h=lost, min_30=soc_min2, gap_30=lost2)

    # LED driver headroom vs battery, with pulse droop from S5b (60 mV at 3.45 V)
    print("\nLED headroom. VLED needs Vf + driver minimum. MAX86174A Table 2: 300 mV on 32 mA range, 500 mV on 64 mA range")
    R["headroom"] = {}
    needs = {"green 15 mA max bin": 2.75 + 0.30, "green 5 mA max bin": 2.65 + 0.30, "red 40 mA max bin": 2.32 + 0.50, "IR 50 mA max bin": 1.59 + 0.50}
    for k, v in needs.items():
        s = float(np.interp(v + 0.06, OCV_V, OCV_S))
        print(f"  {k:<22} needs {v:.2f} V + droop, reached at SoC {s*100:4.1f} %")
        R["headroom"][k] = dict(need=v, soc=s)
    print("  Protection UVP 2.8 V, AFE VLED minimum 2.7 V")

    # Case
    print("\nCase chain")
    print(f"  Boost efficiency, datasheet curve at 3.6 V to 5 V, 50 mA: 93.8 %. Package used 85 %. Case mAh per pod mAh: {case_ratio(5.0):.2f}  (package {(3.8/3.7)/(0.85*0.76):.2f})")
    for v5 in (5.0, 4.7, 4.6):
        print(f"  V5 {v5} V: {case_ratio(v5):.3f} case mAh per pod mAh, charger loss at 41 mA {(v5-3.8)*41:.0f} mW. Margin over the BQ25101 4.45 V operating minimum with VREF -2 %: {v5*0.98-0.009-4.45:+.3f} V")
    sb = case_standby()
    print(f"  Standby: lid closed, empty {sb['closed_empty']:.0f} uA ({300*0.9/(sb['closed_empty']*1e-3)/24:.0f} days to empty), lid open {sb['open']:.1f} uA, lid closed with a full pod {sb['closed_full_pod']:.0f} uA ({sb['closed_full_pod']*1e-3*24:.1f} mAh/day). Package assumed 10 uA")
    R["case"] = dict(standby=sb, ratio_50=case_ratio(5.0), ratio_47=case_ratio(4.7), ratio_pkg=(3.8 / 3.7) / (0.85 * 0.76))
    print("  SY6280 current limit: 6800/RSET, datasheet minimum 0.4 A. A 100 mA limit is not available on this part. RSET 16.9k gives 0.40 A")
    # Case cell needed for 25 days
    for name in ("bottom-up smart", "bottom-up lite", "improved smart", "improved lite"):
        mah = R["daily"][name]["mah"]
        need = (mah * case_ratio(4.7) + sb["closed_empty"] * 1e-3 * 24) * 25 / 0.90
        print(f"  Case cell for 25 days with {name}: {need:.0f} mAh")
        R["daily"][name]["case_for_25d"] = need
    json.dump(R, open(sys.argv[1], "w"), indent=1, default=float)


if __name__ == "__main__":
    main()
