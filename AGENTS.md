# Atrium: rules for contributors and coding agents

## Layout

Keep this layout. Don't add top-level folders. The local checkout matches the remote.

```
hardware/{pod,case}/{schematic,pcb,bom,mechanical,production}
hardware/simulation
firmware/{src,drivers,boards,tests}
app/{android-ios,python}
cloud/
docs/images/
LICENSE  README.md  AGENTS.md
```

## Content

- **Atrium's own work only.** No competitor names, products, papers, patents or teardown notes, anywhere: code, comments, docs, commit messages or file names.
- **No vendor datasheet PDFs.** Link to the vendor page instead.
- **Wellness and research only.** No medical claims, diagnoses or treatment language.
- **No file named "final".** Revisions use a tag such as `_revB1-1`.
- **Parts:** every part from the LCSC / JLCPCB catalogue, or stated as hand-fitted with its source.
- **Docs:** short and factual. Numbers carry units and a source. Unknowns are marked `[VERIFY]` or `[ASSUMED]`. No filler or hype.

## Commits and branches

- One branch per change, merged by pull request as one squashed commit. Delete the branch after it merges.
- Imperative subject lines of 72 characters or fewer. No AI attribution lines.
- Each workstream touches only its own folders, so `git diff --stat main` on its branch lists nothing else:

  | Workstream | Folders |
  |---|---|
  | Electrical | `hardware/{pod,case}/{schematic,pcb,bom,production}`, `hardware/simulation` |
  | Mechanical | `hardware/{pod,case}/mechanical`, `docs/images`, `docs/make_figures.py` |
  | Firmware | `firmware/` |
  | App, cloud | `app/`, `cloud/` |

- No one-off scripts, logs or caches. Every script in the repo has a usage line that runs from the repo root.

## Design rules that must keep passing

Run these before any mechanical change is merged:

```
cd hardware/pod/mechanical/src
cad run checks/interference.py
cad run checks/stackup.py
cad run checks/design_rules.py   # outline convex and symmetric, flat top, centreline apertures, adhesive not a ring

cd hardware/case/mechanical/src
cad run checks/case_interference.py
cad run checks/fit.py
```

Rebuild the README figures with `cad run docs/make_figures.py`.

**Figure style:** SVG line art.
- CAD drawings are projected from the models with hidden-line removal; never hand-drawn shapes.
- Ground `#fbfbfd`, ink `#1d1d1f`, hairlines `#d2d2d7`, one blue accent (`#0071e3`).
- Optics colours: green `#34c759`, red `#ff3b30`, infrared `#af52de`.
- Helvetica Neue with system sans fallbacks. Labels never overlap.

## Licences

| Part | Licence |
|---|---|
| Hardware | CERN-OHL-S-2.0 |
| Firmware, app, cloud | Apache-2.0 |
| Docs and images | CC BY-SA 4.0 |
