# waveSpectra example: a synthetic record with a known spectrum

Self-checking example for the spectral analysis. It needs **no solver and no mesh**:
a JONSWAP free-surface record (Hs = 0.05 m, Tp = 1.2 s) is synthesised, analysed with
`waveSpectra`, and the result is compared with the exact values of the discretised
spectrum.

```bash
cd waveMonitoring/examples/spectra
./Allrun          # ~2 s
```

## Files

| File | Purpose |
|---|---|
| `makeSignal.py` | synthesises `elevation.dat` (400 components, 120 s, dt = 0.02 s) and writes `targets.txt` with the exact Hs / Tp / Tm02 of the discretised spectrum |
| `Allrun` | `makeSignal.py` -> `waveSpectra` -> `checkResults.py` |
| `checkResults.py` | compares `waveSpectra/waveStats.dat` with `targets.txt` (3% tolerance), returns non-zero on failure |
| `Allclean` | removes the generated files |

## Expected output

```
  quantity        target       recovered     rel.error
  --------------- ------------ ------------- ----------
  Hm0             0.050000     0.050124      2.48e-03
  Tp              1.194401     1.176470      1.50e-02
  Tm02            0.961586     0.966557      5.17e-03

  tolerance: 3.0%
  VERDICT  : PASS
```

`Tp` is recovered within one frequency bin (`df = 1/T = 0.0083 1/s`), `Hm0` within 0.3%.

## Varying the sea state

```bash
python3 makeSignal.py --hs 0.1 --tp 1.6 --duration 180
waveSpectra -file elevation.dat -out waveSpectra
python3 checkResults.py
```

Use `python3 makeSignal.py --help` for all options (`--gamma`, `--dt`, `--ncomp`, `--seed`).

## Using it on CFD data

The same command works on the output of the `waveElevation` function object:

```bash
waveSpectra -file postProcessing/waveElevation/<name>/elevation.dat \
            -out waveSpectra -tStart <skip-the-ramp-up>
```
