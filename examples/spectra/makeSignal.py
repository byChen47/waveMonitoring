#!/usr/bin/env python3
"""
Generate a synthetic free-surface elevation record with a known JONSWAP
spectrum, in the format written by the waveElevation function object:

    # time<TAB>eta0

The spectrum is normalised so that Hm0 = Hs exactly, which makes the record a
reference signal for checking waveSpectra (see checkResults.py).

Usage:
    python3 makeSignal.py [--hs 0.05] [--tp 1.2] [--duration 120] [--dt 0.02]
                          [--out elevation.dat]

Writes:
    elevation.dat   the time series
    targets.txt     Hs, Tp and Tm02 of the discretised spectrum
"""

import argparse
import math
import random


def jonswap(f, fp, gamma=3.3):
    """Un-normalised JONSWAP shape (alpha = 1)."""
    if f <= 0.0:
        return 0.0
    sigma = 0.07 if f <= fp else 0.09
    r = math.exp(-((f/fp - 1.0)**2)/(2.0*sigma*sigma))
    return (f**-5)*math.exp(-1.25*(fp/f)**4)*(gamma**r)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--hs', type=float, default=0.05, help='significant wave height [m]')
    ap.add_argument('--tp', type=float, default=1.2, help='peak period [s]')
    ap.add_argument('--gamma', type=float, default=3.3, help='JONSWAP peak enhancement')
    ap.add_argument('--duration', type=float, default=120.0, help='record length [s]')
    ap.add_argument('--dt', type=float, default=0.02, help='time step [s]')
    ap.add_argument('--ncomp', type=int, default=400, help='number of components')
    ap.add_argument('--seed', type=int, default=12345)
    ap.add_argument('--out', default='elevation.dat')
    args = ap.parse_args()

    fp = 1.0/args.tp
    fmin = 0.25*fp
    fmax = 4.0*fp
    df = (fmax - fmin)/args.ncomp

    random.seed(args.seed)

    # discretised spectrum, normalised so that m0 = (Hs/4)^2
    freqs, amps = [], []
    for i in range(args.ncomp):
        f = fmin + (i + 0.5)*df
        freqs.append(f)
        amps.append(math.sqrt(jonswap(f, fp, args.gamma)))

    # m0 = sum(S df) with S = x^2  ->  scale so that 4*sqrt(m0) = Hs
    m0 = sum(a*a for a in amps)*df
    scale = (args.hs/4.0)/math.sqrt(m0)

    phases = [2.0*math.pi*random.random() for _ in range(args.ncomp)]
    a = [scale*x*math.sqrt(2.0*df) for x in amps]      # a_i = sqrt(2 S df)

    # exact targets of the discretised spectrum
    m0 = sum(x*x for x in a)/2.0
    m2 = sum((f*f*x*x) for f, x in zip(freqs, a))/2.0
    ipeak = max(range(len(freqs)), key=lambda i: a[i]*a[i])
    tp = 1.0/freqs[ipeak]
    tm02 = math.sqrt(m0/m2)
    hm0 = 4.0*math.sqrt(m0)

    n = int(round(args.duration/args.dt)) + 1

    with open(args.out, 'w') as f:
        f.write('# time\teta0\n')
        for k in range(n):
            t = k*args.dt
            eta = sum(ai*math.cos(2.0*math.pi*fi*t + pi)
                      for fi, ai, pi in zip(freqs, a, phases))
            f.write('%.6f\t%.8f\n' % (t, eta))

    with open('targets.txt', 'w') as f:
        f.write('Hs %.8f\n' % args.hs)
        f.write('Hm0 %.8f\n' % hm0)
        f.write('Tp %.8f\n' % tp)
        f.write('Tm02 %.8f\n' % tm02)
        f.write('duration %.6f\n' % args.duration)
        f.write('dt %.6f\n' % args.dt)

    print('wrote %s : %d samples, %.1f s, dt = %.3f s' % (args.out, n, args.duration, args.dt))
    print('target: Hs = %.4f m, Tp = %.3f s, Tm02 = %.3f s' % (args.hs, tp, tm02))


if __name__ == '__main__':
    main()
