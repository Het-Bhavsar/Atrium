"""Pairwise interference over the case assembly (pod docked).

Usage: cad run checks/case_interference.py
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve()
sys.path.insert(0, str(HERE.parents[1]))
sys.path.insert(0, str(HERE.parents[4] / "pod" / "mechanical" / "src"))

from case_assembly import case_assembly
from checks import interference as base

if __name__ == "__main__":
    base.report("case_assembly", case_assembly().shape)
