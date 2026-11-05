# Case mechanical

Parametric build123d models of the charging case. Shared parameters come from `hardware/pod/mechanical/src/lib/params.py`.

**Case frame:** origin at the case PCB lower-left, z = 0 at the outer bottom face.

```
cd src
cad run case_assembly.py              # closed, lid open 105°, and lid off
cad run checks/case_interference.py   # pairwise overlap
cad run checks/fit.py                 # pogo engagement, USB-C clearance, cell swell, hall field, lid swing
cad run exports.py                    # STEP/ STL/ 3MF/
```

| Part | Notes |
|---|---|
| Base | Cell pocket of 7.2 mm (LP602525 6.3 max + 10 % swell + foam), PCB ledge, USB-C opening, hinge knuckles |
| Cradle | Insert over the PCB: stadium pocket 0.15 mm larger than the pod, 2.5 mm deep with a lead-in, finger scoop, pogo barrel slot, magnet pocket, LED light holes |
| Lid | Barrel-spine hinge on the short end: five Ø3.0 knuckles alternating base and lid, axis on the outer wall face, Ø1.0 pin flush and hidden. Swing-checked 10° to 110° with no contact. N52 Ø3 × 2 magnet in a boss over the DRV5032 (169 mT closed, below 0.5 mT once opened past about 25°) |

**Outer size:** 44.4 × 29.4 × 19.9 mm. The hinge barrel adds 1.5 mm.
