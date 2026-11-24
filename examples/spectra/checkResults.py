#!/usr/bin/env python3
"""
Compare the waveSpectra output with the targets of the synthetic record.

Usage:
    python3 checkResults.py [--stats waveSpectra/waveStats.dat] [--tol 0.03]
"""

import argparse
import sys


def read_targets(path='targets.txt'):
    out = {}
    with open(path) as f:
        for line in f:
            parts = line.split()
            if len(parts) == 2:
                out[parts[0]] = float(parts[1])
    return out


def read_stats(path):
    """First non-comment data row of waveStats.dat: gauge Hm0 Tp Tm02 ..."""
    with open(path) as f:
        for line in f:
            if line.startswith('#') or not line.strip():
                continue
            cols = line.split()
            if len(cols) < 4:
                continue
            return {
                'gauge': cols[0],
                'Hm0': float(cols[1]),
                'Tp': float(cols[2]),
                'Tm02': float(cols[3]),
            }
    raise SystemExit('no data row in %s' % path)


def rel_err(a, b):
    return abs(a - b)/b if b else 0.0


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--stats', default='waveSpectra/waveStats.dat')
    ap.add_argument('--targets', default='targets.txt')
    ap.add_argument('--tol', type=float, default=0.03,
                    help='relative tolerance for Hm0/Tp/Tm02 (default 3%%)')
    args = ap.parse_args()

    t = read_targets(args.targets)
    s = read_stats(args.stats)

    print()
    print('  quantity        target       recovered     rel.error')
    print('  --------------- ------------ ------------- ----------')

    ok = True
    for key, tgt in (('Hm0', t['Hm0']), ('Tp', t['Tp']), ('Tm02', t['Tm02'])):
        got = s[key]
        err = rel_err(got, tgt)
        flag = '' if err <= args.tol else '   <-- FAIL'
        if err > args.tol:
            ok = False
        print('  %-15s %-12.6f %-13.6f %.2e%s' % (key, tgt, got, err, flag))

    print()
    print('  tolerance: %.1f%%' % (100.0*args.tol))
    print('  VERDICT  : %s' % ('PASS' if ok else 'FAIL'))
    print()

    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(main())
