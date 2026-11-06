"""Atrium Rev B.1 mechanical parameters. Every dimension in mm.

Pod frame: origin at the pod PCB bounding-box lower-left, +x toward the antenna,
+y across the pod, z = 0 at the skin face of the window (the pod's outer bottom,
adhesive excluded). Case frame: origin at the case PCB lower-left, z = 0 at the
case's outer bottom face.

Sources: hardware/pod/pcb/placement/pod_placement.csv, hardware/case/pcb/placement/
case_placement.csv, and the LP301020 / LP602525 / VEMD5080X01 / DRV5032 / TMP117 vendor
datasheets. Values marked ASSUMED or VERIFY are not from a drawing.
"""

from pathlib import Path

HARDWARE = Path(__file__).resolve().parents[4]          # <repo>/hardware
POD_PLACEMENT_CSV = HARDWARE / "pod" / "pcb" / "placement" / "pod_placement.csv"
CASE_PLACEMENT_CSV = HARDWARE / "case" / "pcb" / "placement" / "case_placement.csv"

# --------------------------------------------------------------------------- pod
POD_L = 30.0                 # outer stadium length
POD_W = 16.0                 # outer stadium width
POD_H = 7.0                  # outer height (raised from 6.8 for swelling margin)
SHELL_TOP_T = 0.5            # top shell thickness
SHELL_WALL_T = 1.0           # nominal side wall (outer to the 28 x 14 PCB line)
PCB_WALL_CLEAR = 0.10        # PCB to inner wall per side; wall is 0.9 at the PCB band
SKIRT_T = SHELL_WALL_T - PCB_WALL_CLEAR   # 0.9: straight bore, window plate flush with the rim
WINDOW_FIT_CLEAR = 0.05      # window plate to skirt, per side
TOP_EDGE_FILLET = 1.2        # outer top edge, comfort
SKIN_EDGE_FILLET = 0.8       # outer skin-contact edge; <= SKIRT_T, avoids a pressure line

PCB_L = 28.0
PCB_W = 14.0
PCB_T = 0.6
PCB_CX = PCB_L / 2           # stadium centre in the pod frame
PCB_CY = PCB_W / 2

# Z-stack, skin up
ADHESIVE_T = 0.2             # 0.2-0.3 range, outside the pod
ADHESIVE_FOAM_T = (0.5, 0.8) # compliant black foam options for the wear test
ADHESIVE_EDGE_INSET = 0.1    # patch ends this far inside the flat of the skin face
LINER_T = 0.05               # release liner under the adhesive [ASSUMED]
WINDOW_T = 0.5
SKIN_ZONE = 1.0              # skin-side parts <= 1.0
TOP_ZONE = 0.9               # top parts <= 0.9 under the cell
CELL_INSULATOR_T = 0.05      # polyimide tape under the cell, puncture barrier [ASSUMED]

Z_WINDOW_TOP = WINDOW_T
Z_PCB_BOT = Z_WINDOW_TOP + SKIN_ZONE
Z_PCB_TOP = Z_PCB_BOT + PCB_T
Z_CELL_BOT = Z_PCB_TOP + TOP_ZONE + CELL_INSULATOR_T
Z_CAVITY_TOP = POD_H - SHELL_TOP_T

# Cell: LP301020 class. The model name says 3.0 x 10 x 20; the supplier sheet allows
# 20.5 x 11.0 x 3.5 max (not including tabs). The model uses the nominal; the stack
# check runs both.
CELL_T = 3.0
CELL_T_TOL = 0.2             # +/- on nominal [ASSUMED: sheet gives only a 3.5 max]
CELL_T_SHEET_MAX = 3.5
CELL_L = 20.0
CELL_W = 10.0
CELL_L_SHEET_MAX = 20.5
CELL_W_SHEET_MAX = 11.0
CELL_X0, CELL_Y0 = 3.0, 2.0  # battery zone x 3-23, y 2-12
CELL_CORNER_R = 0.5
CELL_SWELL = 0.10            # 10 % thickness swell for the check
CELL_TAB_W = 1.6             # nickel tab width [VERIFY on the cell drawing]
CELL_TAB_T = 0.1
CELL_TAB_Y = (4.4, 9.6)      # BT+ and BT- pad centres
CELL_TAB_PAD_X = 3.6
CELL_TAB_FOLD_X = 2.55       # outermost x of the tab fold

PORON_T = 0.50               # free thickness; installed in the 0.45 gap (10 % preload)
PORON_SOLID_FRAC = 0.20      # fully compressed thickness / free [VERIFY: grade data]
PORON_INSET = 0.5            # pad inset from the cell edge

# Window apertures in PCB coordinates (PD1 moved to x 5.7, slot removed)
LED_XY = (2.2, 7.0)
LED_D = 2.0
PD1_XY = (5.7, 7.0)
PD1_WIN = 3.4
PD2_XY = (20.7, 7.0)
PD2_WIN = 2.8
TMP_XY = (14.0, 7.0)
TMP_D = 2.5
APERTURE_CORNER_R = 0.2      # square window corner radius (tool radius)

BUTTON_CLEAR = 0.025         # 316L button to bore, per side
BUTTON_FLANGE_D = 3.0        # retaining flange on the inner face
BUTTON_FLANGE_T = 0.10
GAP_PAD_T = 0.20             # installed at U4 max height 0.8; specify 0.3 free
GAP_PAD_SIZE = 2.0
GAP_PAD_FREE_T = 0.30
U4_H_TYP, U4_H_MAX = 0.75, 0.80   # TMP117 WSON-6 [VERIFY on the TI package drawing]

# Light-barrier ribs (window top to PCB). x centres in PCB coordinates.
RIB_W = 0.8
RIB_PD1_TMP_X = 10.3
RIB_TMP_PD2_X = 16.85
PART_CLEAR = 0.05            # rib to part body, per side
RIM_W = 0.35                 # perimeter rim under the PCB edge band (edge margin 0.3)
RIB_TOP_CLEAR = 0.0          # ribs and bosses are the PCB z datum

# Pogo pads (skin side): five pads, 2.54 pitch, from the DXF
POGO_PADS = [(9.0, 12.0), (11.54, 12.0), (14.08, 12.0), (16.62, 12.0), (19.16, 12.0)]
POGO_PAD_D = 1.2
POGO_PAD_T = 0.02             # ENIG + hard gold pad proud of the PCB face [ASSUMED]
POGO_HOLE_D = 1.6            # window hole (DXF POGO_HOLES)
POGO_BAR_W = 2.2             # sealing bar across all pads, y
POGO_BAR_END = 1.2           # bar end beyond the outer pad centres
# Seal mode while the IP target is open: "none", "gasket" or "glue".
POGO_SEAL = "gasket"
POGO_GASKET_T = 0.15         # compressed silicone ring between bar and PCB solder mask

# Antenna
ANT_KEEPOUT_X = 24.0
ANT_XY = (26.0, 7.6)
ANT_SIZE = (1.6, 3.2, 0.6)   # Walsin RFECA3216 body, long axis along y [VERIFY orientation]
ANT_LIFT = 2.5               # raise into the shell (0 = on the PCB). Sim: +4.5 dB
ANT_RISER = (2.4, 4.0)       # FR-4 interposer footprint under the raised antenna

# Skin-side part height corrections from datasheets
PART_HEIGHT_OVERRIDES = {
    "PD1": 0.90,             # VEMD5080X01: 5 x 4 x 0.9 (placement CSV said 0.85)
    "PD2": 0.90,
}
PART_XY_OVERRIDES = {"PD1": (5.7, 7.0)}
PART_SIZE_OVERRIDES = {"PD1": (4.0, 5.0), "PD2": (4.0, 5.0)}   # datasheet body

# --------------------------------------------------------------------------- case
CASE_PCB_L = 40.0
CASE_PCB_W = 26.0
CASE_PCB_R = 3.0
CASE_PCB_T = 1.0
CASE_WALL = 1.5
CASE_PCB_CLEAR = 0.2
CASE_WIRE_CHANNEL = 1.0      # extra room at the +x end for the cell leads
CASE_FLOOR_T = 1.0
CASE_CELL = (26.0, 25.0, 6.0)        # LP602525 nominal incl. PCM (L, W, T)
CASE_CELL_MAX = (27.0, 25.5, 6.3)    # +1.0 / +0.5 / +0.3 per the LiPol drawing
CASE_CELL_X0 = 12.6
CASE_CELL_FOAM_T = 1.2            # PORON free thickness over the case cell (1.1 gap at nominal)
CASE_CELL_POCKET_H = 7.2          # >= 6.3 max x 1.1 swell + foam solid height
CASE_PARTS_MAX_H = 1.1       # tallest case part under the cradle except J1 and pogos
CASE_CRADLE_UNDER_CLEAR = 0.2

# Pogo pins P1/P2, BWCD-L4.5W2.0H2.3 (C2826546). Only L and D are on the LCSC page.
POGO_PIN_D = 2.0
POGO_PIN_L = 4.5             # free length
POGO_BARREL_H = 2.3          # [VERIFY: read from the part number]
POGO_PLUNGER_D = 1.0         # [VERIFY]
POGO_STROKE_MAX = 1.2        # [VERIFY]
POGO_WORK_COMP = 0.6         # design compression when docked

CASE_POGO = [(18.73, 18.5), (21.27, 18.5)]
POD_DOCK_OFFSET = (CASE_POGO[0][0] - POGO_PADS[0][0], CASE_POGO[0][1] - POGO_PADS[0][1])

CRADLE_POCKET_CLEAR = 0.15
CRADLE_POCKET_DEPTH = 2.5
CRADLE_LEADIN = 0.4
LID_POD_CLEAR = 0.3
LID_T = 1.0

MAGNET_D = 3.0               # N52 axial
MAGNET_T = 2.0
MAGNET_BR = 1.45             # T, N52 remanence (1.42-1.48)
MAGNET_XY = (36.0, 22.0)     # over DRV5032 U4
HALL_DIE_Z = 0.55            # die height above the case PCB top [ASSUMED for SOT-23]
HALL_BOP_MAX = 4.8e-3        # T, DRV5032FA
HALL_BRP_MIN = 0.5e-3

USB_OPEN = (9.4, 3.6)        # receptacle opening w x h [VERIFY against the plug overmold]
USB_H = 3.3

# Derived case z levels (case frame)
Z_CASE_PCB_BOT = CASE_FLOOR_T + CASE_CELL_POCKET_H
Z_CASE_PCB_TOP = Z_CASE_PCB_BOT + CASE_PCB_T
POGO_WORK_H = POGO_PIN_L - POGO_WORK_COMP
POD_PAD_DEPTH = Z_PCB_BOT - POGO_PAD_T   # pad face height above the pod skin face
Z_CRADLE_FLOOR = Z_CASE_PCB_TOP + POGO_WORK_H - POD_PAD_DEPTH
Z_CASE_RIM = Z_CRADLE_FLOOR + CRADLE_POCKET_DEPTH
Z_LID_INNER = Z_CRADLE_FLOOR + POD_H + LID_POD_CLEAR
Z_LID_TOP = Z_LID_INNER + LID_T

CASE_IN_X0 = -CASE_PCB_CLEAR
CASE_IN_X1 = CASE_PCB_L + CASE_PCB_CLEAR + CASE_WIRE_CHANNEL
CASE_IN_Y0 = -CASE_PCB_CLEAR
CASE_IN_Y1 = CASE_PCB_W + CASE_PCB_CLEAR
CASE_OUT_R = CASE_PCB_R + CASE_PCB_CLEAR + CASE_WALL

# Colours
BLACK_PC = "#1b1b1d"
SHELL_COLOR = "#d9d4cb"
CLEAR_PC = "#bfe3f2"
STEEL = "#b8bcc2"
FR4 = "#2f6b3a"
CELL_COLOR = "#9aa0a8"
PORON_COLOR = "#33363a"
ADHESIVE_COLOR = "#2a2a2a"
LINER_COLOR = "#e9e3c9"
CASE_COLOR = "#ece9e4"
LID_COLOR = "#cfcac2"
GOLD = "#d4af37"
