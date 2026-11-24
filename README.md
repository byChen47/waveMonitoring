# waveMonitoring

**English** | [中文](README.zh-CN.md)

[![License: GPL v3](https://img.shields.io/badge/license-GPL--3.0-blue.svg)](LICENSE)
[![OpenFOAM](https://img.shields.io/badge/OpenFOAM-v2206%2B-2b6cb0.svg)](https://www.openfoam.com)
[![Tested](https://img.shields.io/badge/tested-v2606%20%7C%20gcc%209.5-2f855a.svg)](#9-verification)

**Standalone wave measurement for OpenFOAM.** It provides free-surface elevation at
gauges, gauge/probe generation from `probeDefinitions`, and spectral analysis — with
**no waves2Foam solver and no waves2Foam library**. Load the library from
`controlDict` and it works with any solver that has a VOF field (`alpha.*`).

```
libwaveMonitoring.so  ──▶  waveElevation   free-surface elevation eta(t), one file
GaugesNProbes         ──▶  probeDefinitions -> gauges -> functions fragment
waveSpectra           ──▶  spectra and wave statistics
```

## Quick start

```bash
source ~/OpenFOAM/OpenFOAM-v2606/etc/bashrc        # any OpenFOAM v2206+
git clone https://github.com/byChen47/waveMonitoring.git
cd waveMonitoring && ./Allwmake                    # library + waveSpectra + GaugesNProbes
```

In any VOF case, `system/controlDict`:

```cpp
functions
{
    waveElevation1
    {
        type            waveElevation;
        libs            ("libwaveMonitoring.so");
        locations       ((0.10 0.05 0.005) (0.25 0.05 0.005) (0.40 0.05 0.005));
        writeControl    timeStep;
        writeInterval   1;
    }
}
```

After the run:

```bash
waveSpectra -file postProcessing/waveElevation/waveElevation1/elevation.dat \
            -out waveSpectra
```

Or let `GaugesNProbes` write the gauges from `constant/probeDefinitions`:

```bash
GaugesNProbes -case .
# then: functions { #include "gaugesNProbes.functions" }
```

Two self-contained examples are provided: a `damBreak` gauge test in [`examples/`](examples)
and a JONSWAP spectral-analysis check in [`examples/spectra/`](examples/spectra).

---

## 1. Why a separate module

The original waves2Foam measurement chain is tied to its own build system:

```
sampledSurfaceElevation (waves2FoamSampling)   <- version-specific directory
      ↓  raw alpha samples
postProcessWaves2Foam  (waves2FoamProcessing)  <- links libwaves2Foam + GSL
      ↓ eta(t)
powerSpectraLS / reflectionAnalysis            <- spectral analysis
```

Using that with your own solver (`interFoam`, `overInterFoam`, a sloshing solver with
porous baffles, ...) means dragging in `WAVES_*` environment variables, `bin/bashrc`
and `-lwaves2Foam`.

This module re-implements the measurement part against **OpenFOAM only**:

| Capability | waves2Foam | this module |
|---|---|---|
| Free-surface elevation | `surfaceElevation` (raw alpha samples, needs a second interpolation step) | `waveElevation` (writes eta(t) directly, single file) |
| Spectral analysis | `postProcessWaves2Foam` (links `-lwaves2Foam -lgsl`) | `waveSpectra` (links `-lOpenFOAM` only) |
| Dependencies | `libwaves2Foam.so`, `libwaves2FoamProcessing.so`, GSL | OpenFOAM only |

The type name is different (`waveElevation` vs `surfaceElevation`), so the module can be
loaded **alongside** waves2Foam without a duplicate registration.

## 2. Requirements

- OpenFOAM **v2206+** (developed and tested with **v2606 / g++ 9.5**)
- A case with an `alpha` field (default `alpha.water`) and `g` (`constant/g`)
- No GSL, no waves2Foam

## 3. Building

```bash
source ~/OpenFOAM/OpenFOAM-v2606/etc/bashrc
cd waveMonitoring
./Allwmake              # or ./Allwclean to start from scratch
```

Artefacts:

```
$FOAM_USER_LIBBIN/libwaveMonitoring.so
$FOAM_USER_APPBIN/waveSpectra
$FOAM_USER_APPBIN/GaugesNProbes
```

## 4. Measuring with `waveElevation`

```cpp
functions
{
    waveElevation1
    {
        type            waveElevation;
        libs            ("libwaveMonitoring.so");

        // Mandatory: gauge locations
        locations       ((0.10 0.05 0.005) (0.25 0.05 0.005) (0.40 0.05 0.005));

        // Optional (defaults shown)
        alpha               alpha.water;
        liquid              true;
        direction           (0 0 0);       // (0 0 0) -> use -g
        interpolationScheme cellPoint;
        subtractReference   false;
        fixedLocations      true;
        log                 true;

        // Sampling rate (independent of the solver's write times)
        writeControl    timeStep;
        writeInterval   1;
    }
}
```

### Parameters

| Entry | Meaning | Default |
|---|---|---|
| `locations` | Gauge coordinates (`vectorList`), mandatory | — |
| `alpha` | VOF field name | `alpha.water` |
| `liquid` | `true`: alpha = 1 is the liquid; `false`: alpha = 1 is the gas | `true` |
| `direction` | Interface normal (ray direction); leave at zero to use `-g` | `(0 0 0)` |
| `interpolationScheme` | Interpolation of alpha at the sample points | `cellPoint` |
| `subtractReference` | Subtract the elevation of the first write | `false` |
| `fixedLocations` | Keep the gauges fixed in space (`true`) or move them with the mesh (`false`) | `true` |
| `log` | Print every sample to the log | `true` |
| `writeControl` / `writeInterval` | Sampling rate (inherited from the function object) | `timeStep` / `1` |

### Output

```
postProcessing/waveElevation/waveElevation1/elevation.dat
```

```
# waveElevation: free-surface elevation (integrated liquid height)
# functionObject: waveElevation1
# alpha: alpha.water
# liquid: 1
# direction: (0 0 0)
# subtractReference: 0
# fixedLocations: 1
# nGauges: 3
# location 0: (0.1 0.05 0.005)
# location 1: (0.25 0.05 0.005)
# location 2: (0.4 0.05 0.005)
# time	eta0	eta1	eta2
0.00119904	0.290476	0	0
0.00263651	0.290458	1.28997e-21	1.11626e-66
...
```

One row per write time; column 1 is time, the remaining columns are the gauges in the
order of `locations`. The header is self-describing, so `waveSpectra` can read the file
without any additional input. In parallel the file is written by the master only; the
values are reduced over all processors.

### How the elevation is computed

A ray is cast through the gauge along `-direction` (by default vertically upwards) and
the liquid fraction is integrated along it:

```
eta = integral( alpha dl )
```

For a flat-bottom tank this is exactly the free-surface height above the bottom. Compared
with locating a single `alpha = 0.5` iso-surface crossing, the integral stays well behaved
for aerated, breaking or splashing surfaces, where no unique crossing may exist.

### Fixed vs. moving gauges (`fixedLocations`)

The semantics follow OpenFOAM's `probes`:

| `fixedLocations` | Behaviour |
|---|---|
| `true` (default) | The gauge stays at the given position in space; the cell is searched again on every write |
| `false` | The gauge is attached to the cell it was created in and **moves with the mesh/body**, keeping its relative position — for gauges mounted on a moving body (sloshing tank, floater, porous baffle) |

```cpp
movingGauge
{
    type            waveElevation;
    libs            ("libwaveMonitoring.so");
    locations       ((0.25 0.05 0.0025));
    fixedLocations  false;      // follows the mesh
    log             true;       // prints the current positions
}
```

Implementation: with `false`, the cell-centre position `C0` is recorded when the gauge is
created, and the displacement `C(t) - C0` is applied on every write (reduced over the
processors so that only the owner of the cell contributes). After a topology change the
gauges are re-attached at their last known positions.

**Verification** — still water in a tank translated as `x = 0.05 sin(10t)`, one fixed and
one moving gauge at the same initial point, 0.6 s / 43 samples:

```
fixed gauge : no position output in the log (it does not move)
moving gauge: x - 0.05 sin(10t) = 0.250000 +/- 1.2e-6 m
              maximum deviation of the relative position: 6.1e-7 m
              eta spread: 5e-6 m
```

## 5. Generating gauges from `probeDefinitions` (`GaugesNProbes`)

For a handful of gauges, writing `locations` by hand is fine. For many gauges (dozens of
positions in a sloshing tank, arrays along a wall, ...) use `GaugesNProbes`, which replaces
waves2Foam's `waveGaugesNProbes` and depends on OpenFOAM only.

```bash
cd <case>
GaugesNProbes -case .          # reads constant/probeDefinitions
# -> system/gaugesNProbes.functions  +  gaugesNProbes/<name>.vtk
```

Then, in `system/controlDict`:

```cpp
functions
{
    #include "gaugesNProbes.functions"
}
```

### `probeDefinitions` keywords

The keywords are the waves2Foam ones, so existing files work unchanged:

| Keyword | Meaning |
|---|---|
| `type` | `waveGauge` -> one `waveElevation` object; `probeGauge` -> one OpenFOAM `probes` object |
| `pointDistribution` | Distribution type (see below) |
| `N` | Number of points of this distribution |
| `linestart` / `lineend` / `stretch` | `lineDistribution`: points along a line (`stretch` optional) |
| `centre` / `radius` / `axis` | `circularDistribution`: points on a circle |
| `linestart0` / `lineend0` / `lineend1` / `N0` / `N1` / `stretch0` / `stretch1` | `quadrilateralDistribution`: points on a quadrilateral |
| `xValues` / `yValues` / `zValues` | `userDefinedDistribution`: explicit coordinates, Field syntax `nonuniform 3(0.1 0.2 0.3)` |
| sub-dictionaries | `concatenatedDistribution`: several distributions concatenated |
| `add` | Start -> end offset of each gauge line (used for the VTK view) |
| `fixedLocations` | `false` = the gauge moves with the mesh/body (default `true`), written into the generated object |
| `writeControl` / `writeInterval` | Written into the generated object (optional, defaults `timeStep` / `1`) |
| `alpha` | VOF field for `waveGauge` (optional, default `alpha.water`) |
| `fields` | Fields sampled by `probeGauge` (optional, default `alpha.water`) |

Distribution types: `lineDistribution`, `circularDistribution`, `quadrilateralDistribution`,
`concatenatedDistribution`, `userDefinedDistribution`.

### Example

```cpp
gaugeRow
{
    type                waveGauge;
    pointDistribution   lineDistribution;
    N                   3;
    linestart           (0.10 0.05 0.0025);
    lineend             (0.50 0.05 0.0025);
    add                 (0 0.30 0);
    axis                y;
    writeControl        timeStep;
    writeInterval       1;
}

pointProbes
{
    type                probeGauge;
    pointDistribution   userDefinedDistribution;
    N                   2;
    xValues             nonuniform 2(0.2 0.3);
    yValues             nonuniform 2(0.1 0.1);
    zValues             nonuniform 2(0.0025 0.0025);
    fields              (alpha.water U);
}
```

### Generated fragment

```cpp
    gaugeRow
    {
        type            waveElevation;
        libs            ("libwaveMonitoring.so");

        locations
        (
            (0.1 0.05 0.0025)
            (0.3 0.05 0.0025)
            (0.5 0.05 0.0025)
        );

        alpha           alpha.water;
        writeControl    timeStep;
        writeInterval   1;
        log             false;
    }

    pointProbes
    {
        type            probes;
        libs            (sampling);

        probeLocations
        (
            (0.2 0.1 0.0025)
            (0.3 0.1 0.0025)
        );

        fields          (alpha.water U);
        writeControl    timeStep;
        writeInterval   1;
    }
```

### Command line

| Option | Default | Meaning |
|---|---|---|
| `-case <dir>` | `.` | Case directory |
| `-dict <file>` | `constant/probeDefinitions` | Gauge layout |
| `-out <file>` | `system/gaugesNProbes.functions` | Generated fragment |
| `-functionObject <type>` | `waveElevation` | Object type used for `waveGauge` entries |
| `-libs <lib>` | `libwaveMonitoring.so` | Library of that object |
| `-noVTK` | off | Skip `gaugesNProbes/<name>.vtk` |

The group name in `probeDefinitions` becomes the function-object name, so the names must
be unique. After the run, the gauge data is in
`postProcessing/waveElevation/<name>/elevation.dat` and the point probes in
`postProcessing/<name>/<time>/<field>` (the usual `probes` layout).

## 6. Spectral analysis and statistics (`waveSpectra`)

```bash
waveSpectra [-file <elevation.dat>] [-out <dir>]
            [-tStart <t>] [-tEnd <t>] [-df <df>] [-fMax <f>]
            [-gauge <i>|all]
```

| Option | Meaning | Default |
|---|---|---|
| `-file` | Input file | `postProcessing/waveElevation/waveElevation1/elevation.dat` |
| `-out` | Output directory | `waveSpectra` |
| `-tStart` / `-tEnd` | Analysis window (skip the ramp-up / transient) | whole record |
| `-df` | Frequency resolution | `1/T` (T = record length) |
| `-fMax` | Highest analysed frequency | `1/(2dt)` (Nyquist) |
| `-gauge` | Analyse one gauge only (0, 1, 2, ...) | `all` |

Output:

```
waveSpectra/
├── waveStats.dat          # summary: Hm0, Tp, Tm02, Tm01, H1/3, T1/3, Hmax, Hmean, mwl, waves
├── eta0_spectrum.dat      # one per gauge: f  S(f)  (one-sided variance density)
├── eta1_spectrum.dat
└── ...
```

Statistics:

| Symbol | Meaning | Definition |
|---|---|---|
| `Hm0` | Significant wave height from the spectrum | `4*sqrt(m0)` |
| `Tp` | Peak period | `1/f(S_max)` |
| `Tm02` | Spectral mean period | `sqrt(m0/m2)` |
| `Tm01` | Spectral mean period (first moment) | `m0/m1` |
| `H1/3`, `T1/3` | Significant wave height/period (time domain) | zero up-crossing waves, mean of the highest third |
| `Hmax`, `Hmean` | Maximum / mean wave height | same decomposition |
| `mwl` | Mean water level | record mean |

### Worked example: a synthetic record with a known spectrum (`examples/spectra`)

`examples/spectra` contains a complete, self-checking example: it synthesises a JONSWAP
free-surface record (Hs = 0.05 m, Tp = 1.2 s), runs `waveSpectra` on it and compares the
result with the theoretical values of the discretised spectrum. **No solver and no mesh
are needed** — it validates the analysis itself.

```bash
cd waveMonitoring/examples/spectra
./Allrun
```

`Allrun` does three things:

1. `makeSignal.py` builds `elevation.dat` from 400 frequency components (same format as
   the `waveElevation` output, 120 s, dt = 0.02 s) and writes the exact targets of the
   discretised spectrum to `targets.txt`;
2. `waveSpectra -file elevation.dat -out waveSpectra`;
3. `checkResults.py` compares `waveSpectra/waveStats.dat` with `targets.txt` and returns
   non-zero if a quantity is outside the tolerance.

Measured output:

```
  quantity        target       recovered     rel.error
  --------------- ------------ ------------- ----------
  Hm0             0.050000     0.050124      2.48e-03
  Tp              1.194401     1.176470      1.50e-02
  Tm02            0.961586     0.966557      5.17e-03

  tolerance: 3.0%
  VERDICT  : PASS
```

Reading it:

| Quantity | Comment |
|---|---|
| `Hm0` | 0.25% from the input `Hs`; the remainder is frequency discretisation |
| `Tp` | 1.176 s vs 1.194 s: 1.5%, i.e. of the order of the frequency resolution `df = 1/120 = 0.0083 1/s` |
| `Tm02` | 0.967 s vs 0.962 s: 0.5% |

Vary the input directly on the command line:

```bash
python3 makeSignal.py --hs 0.1 --tp 1.6 --duration 180
waveSpectra -file elevation.dat -out waveSpectra && python3 checkResults.py
```

For real CFD data, point `-file` at your
`postProcessing/waveElevation/<name>/elevation.dat`; use `-tStart` to skip the ramp-up:

```bash
waveSpectra -file postProcessing/waveElevation/gauges/elevation.dat \
            -out spectra -tStart 5
```

## 7. Using it with any solver

As long as the case has an `alpha` field, the module is independent of the solver:

| Solver | Comment |
|---|---|
| `interFoam` / `overInterFoam` | Standard VOF, works directly (tested with `interFoam` + `damBreak`) |
| `porousWaveFoam` / `waveFoam` | Also works; coexists with waves2Foam's `surfaceElevation` (different type names) |
| your own solver | enough that `alpha.water` is in the `objectRegistry` |

The module only *measures*: wave generation and absorption remain the job of your
boundary conditions.

Choosing `locations`:

- flat-bottom tank: `(x, z_still_water, 0)` is enough — the ray spans the bounding box, so
  the exact vertical position only selects the column;
- for arrays, list the points or generate them with `#calc` / `#eval`;
- the gauges must be inside the mesh.

## 8. Limitations

- The spectral analysis assumes a **uniform time step** (`dt` is averaged); resample
  non-uniform output first.
- A restart overwrites `elevation.dat` (same function-object name); rename or back it up
  if you need to keep it.
- Only the master writes the file; the parallel output has the same rows as a serial run.
- `waveElevation` rebuilds the sampling ray on every write (a `meshSearch` per gauge), so
  very large meshes with many gauges cost extra; lower the sampling rate with
  `writeInterval` if needed.
- `userDefinedDistribution` coordinates must be Field syntax
  (`nonuniform 3(0.1 0.2 0.3)`); `(0.1 0.2 0.3)` is rejected with
  `Expected keyword 'uniform' or 'nonuniform'`.
- Topology-changing meshes are supported for moving gauges only approximately (the gauges
  are re-attached to the new cells at their last known positions).

## 9. Verification

| Test | Result |
|---|---|
| Clean build with a plain OpenFOAM environment (no waves2Foam) | `waveMonitoring BUILD OK` after `./Allwclean && ./Allwmake` |
| `interFoam` + `damBreak`, 3 gauges, 1 s | 488 rows; eta = 0.2905 m at x = 0.10 against the 0.292 m initial depth |
| `waveSpectra` on a synthetic sine (A = 0.05 m, T = 1.2 s) | Hm0 = 0.141398 vs 0.141421 theory, Tp = Tm02 = T1/3 = 1.200 s |
| `GaugesNProbes` from `probeDefinitions` (3+2 points) | fragment + 2 VTK files; after `#include`, `elevation.dat` (3 columns) and `probes` output (2 points) |
| `fixedLocations false` in a tank moving as x = 0.05 sin(10t) | relative position preserved to 6.1e-7 m, eta spread 5e-6 m |
| `examples/spectra` (JONSWAP) | Hm0 0.0501 vs 0.05, Tp 1.176 vs 1.194, Tm02 0.967 vs 0.962 — PASS |

## 10. Repository layout

```
waveMonitoring/
├── Allwmake / Allwclean
├── Make/{files,options}                  # -> $FOAM_USER_LIBBIN/libwaveMonitoring.so
├── waveElevation/{waveElevation.H,.C}    # the function object
├── waveSpectra/{waveSpectra.C,Make/}     # -> $FOAM_USER_APPBIN/waveSpectra
├── gaugesNProbes/
│   ├── GaugesNProbes.C                   # -> $FOAM_USER_APPBIN/GaugesNProbes
│   ├── Make/
│   └── pointDistributions/               # line/circular/quadrilateral/concatenated/userDefined
├── examples/
│   ├── README.md                         # 5-minute damBreak test
│   ├── probeDefinitions, waveElevation.controlDict
│   └── spectra/                          # synthetic JONSWAP check (Allrun, makeSignal.py)
├── README.md                             # this file (English)
├── README.zh-CN.md                       # Chinese documentation
└── LICENSE                               # GPL-3.0
```

## 11. License and citation

- **License**: GNU General Public License v3.0, see [LICENSE](LICENSE). The module derives
  from [waves2Foam](https://github.com/ogoe/waves2Foam) (GPL) and is built on OpenFOAM, so
  the same license applies.
- **Citation**: please cite the original waves2Foam paper when publishing results:

  > Jacobsen, N.G., Fuhrman, D.R. and Fredsøe, J. (2012). *A Wave Generation Toolbox for the
  > Open-Source CFD Library: OpenFOAM®*. Int. J. Numer. Meth. Fluids, 70(9), 1073–1088.
  > DOI: [10.1002/fld.2726](https://doi.org/10.1002/fld.2726)

## 12. Notes

- The module only *measures*: wave generation and absorption are up to your case setup.
- The eta(t) files, spectra and statistics are ready for plotting in papers; the numerical
  accuracy is documented in section 9.
- Issues and pull requests are welcome.
