#!/usr/bin/env python3
"""Generates the predefined profile data of the clone from the ArcelorMittal catalogue
"Sections and Merchant Bars" (Long Carbon Europe, 2008, public PDF; dimensions of IPE per EU 19-57, HE per EU 53-62
(both taken over by EN 10365), IPN per DIN 1025-1, UPN per DIN 1026-1, equal angles per EN 10056-1).

Usage:  pdftotext -layout catalogue.pdf catalogue.txt  (then convert to UTF-8)
        python tools/gen_profile_data.py catalogue.txt

Writes
  src/core/profile_catalogue_data.inc    nominal dimensions only (mm); properties are COMPUTED by the clone
  tests/profile_reference_data.inc       published A, I, W... of the same catalogue, used by the tests as reference
NOTE (2026-11-05): the angles "L" of src/core/profile_catalogue_data.inc were removed by hand afterwards (the clone now uses the original's own list,
tools/gen_rolled_catalogue.py) and the IPN 120/140/160/180 correction of the header of the .inc must be re-applied if this script is re-run
(IPN/UPN dimensions were verified identical to the original's tables, docs/research/ui/data/describir_perfiles_*_rea.tsv).
Each dimension row is joined with its property row by mass per metre; the mass must agree with A * 0.785 kg/m per cm2
and every join must be unique, otherwise the script aborts (nothing is guessed).
"""
import re
import sys

NUM = re.compile(r'^\d+(?:,\d+)?$')


def val(tok):
    return float(tok.replace(',', '.'))


def numeric_runs(line, n):
    """Yield the lists of n consecutive numeric tokens found in the line."""
    toks = line.split()
    for i in range(len(toks) - n + 1):
        if all(NUM.match(t) for t in toks[i:i + n]):
            yield [val(t) for t in toks[i:i + n]]


def dims_rows(lines, lo, hi, n, a_idx, filt):
    """Rows of a dimensions table: n numbers, mass G = 0.785 * A[cm2] within 1.5 %."""
    out = []
    for line in lines[lo:hi]:
        for run in numeric_runs(line, n):
            g, a = run[0], run[a_idx]
            if a > 0 and abs(g - 0.785 * a) / g < 0.015 and filt(run):
                out.append(run)
                break
    return out


def prop_rows(lines, lo, hi, name_re, n):
    out = []
    pat = re.compile(r'^\s*(' + name_re + r')\s+(.*)$')
    for line in lines[lo:hi]:
        m = pat.match(line)
        if not m:
            continue
        toks = m.group(m.lastindex).split()
        nums = []
        for t in toks:
            if NUM.match(t):
                nums.append(val(t))
            else:
                break
        if len(nums) >= n:
            out.append((m.group(1), nums))
    return out


def find(lines, text, start=0):
    for i in range(start, len(lines)):
        if text in lines[i]:
            return i
    raise SystemExit('marker not found: ' + text)


def fmt_t(t):
    return ('%g' % t)


def main(path):
    lines = open(path, encoding='utf-8', errors='replace').read().split('\n')
    # windows (0-based indices derived from the section headers of the catalogue)
    ipe = (find(lines, 'Wymiary: IPE 80 - 600'), find(lines, 'Wymiary : IPN 80'))
    ipn = (ipe[1], find(lines, 'Wymiary: HE A, HE B'))
    he = (ipn[1], find(lines, 'Wymiary: ASTM A 6/A 6M - 07'))
    upn = (find(lines, 'Wymiary : DIN 1026-1: 2000'), find(lines, 'Wymiary zgodne z norm', find(lines, 'Wymiary : DIN 1026-1: 2000')))
    ang = (find(lines, 'Wymiary: EN 10056-1: 1998'), find(lines, 'Unequal leg angles', find(lines, 'Wymiary: EN 10056-1: 1998')))

    cat, ref = [], []

    def join_by_mass(series, dims, props, name_ok, dim_fields, prop_fields, extra=lambda name, d: True):
        used = set()
        for name, p in props:
            if not name_ok(name):
                continue
            g = p[0]
            cand = [i for i, d in enumerate(dims) if abs(d[0] - g) < 0.051 and i not in used and extra(name, d)]
            if len(cand) != 1:
                raise SystemExit('%s: ambiguous join for %s (G=%s): %d candidates' % (series, name, g, len(cand)))
            used.add(cand[0])
            d = dims[cand[0]]
            cat.append((series, name, dim_fields(d)))
            ref.append((series, name, prop_fields(d, p)))

    # IPE: G h b tw tf r A
    dims = dims_rows(lines, *ipe, 7, 6, lambda r: 70 < r[1] < 620 and r[5] <= 30)
    props = prop_rows(lines, *ipe, r'IPE \d+', 13)
    join_by_mass('IPE', dims, props, lambda n: True, lambda d: d[1:6], lambda d, p: [d[6]] + p[1:],
                 lambda name, d: abs(d[1] - float(name.split()[1])) <= 2.5)
    # HE A/B/M: G h b tw tf r A
    dims = dims_rows(lines, *he, 7, 6, lambda r: 90 < r[1] < 1100 and r[5] <= 30)
    props = prop_rows(lines, *he, r'HE \d+ [ABM]', 13)
    for letter in 'ABM':
        sub = [(n, p) for n, p in props if n.endswith(' ' + letter)]
        join_by_mass('HE' + letter, dims, sub, lambda n: True, lambda d: d[1:6], lambda d, p: [d[6]] + p[1:],
                     lambda name, d: abs(d[1] - float(name.split()[1])) <= max(45.0, 0.04 * float(name.split()[1])))
    # IPN: G h b tw tf r1 r2 A
    dims = dims_rows(lines, *ipn, 8, 7, lambda r: 70 < r[1] < 620)
    props = prop_rows(lines, *ipn, r'IPN \d+', 13)
    join_by_mass('IPN', dims, props, lambda n: True, lambda d: d[1:7], lambda d, p: [d[7]] + p[1:])
    # UPN: G h b tw tf r1 r2 A
    dims = dims_rows(lines, *upn, 8, 7, lambda r: 45 < r[1] < 420)
    props = prop_rows(lines, *upn, r'UPN \d+', 13)
    join_by_mass('UPN', dims, props, lambda n: True, lambda d: d[1:7], lambda d, p: [d[7]] + p[1:])
    # Equal angles: dims G h t r1 r2 A ; props G Iy Wely iy ...
    dims = dims_rows(lines, *ang, 6, 5, lambda r: 15 < r[1] < 260)
    props = prop_rows(lines, *ang, r'L \d+ x \d+ x [\d,]+', 4)
    uniq = {}
    for d in dims:
        key = (d[1], d[2])
        if key in uniq and uniq[key] != d:
            raise SystemExit(r'angle dimension rows disagree for %s' % (key,))
        uniq[key] = d
    byg = {}
    for line in lines[ang[0]:ang[1]]:
        body = re.sub(r'^\s*L \d+ x \d+ x [\d,]+\s*', '', line)
        toks = body.split()
        nums = []
        for t in toks:
            if NUM.match(t):
                nums.append(val(t))
            else:
                break
        # property rows: G Iy Wel iy Iu iu Iv iv (>= 8 numbers) with Iy/Wel plausible
        if len(nums) >= 8 and nums[2] > 0 and 0.2 < nums[1] / nums[2] / (nums[3] * 0 + 1) < 400 and nums[1] < 3000:
            byg.setdefault(round(nums[0], 3), []).append(nums)
    for (h, t), d in sorted(uniq.items()):
        label = 'L %d x %d x %s' % (h, h, fmt_t(t))
        cat.append(('L', label, d[1:5]))
        cands = byg.get(round(d[0], 3), [])
        p = cands[0] if len(cands) == 1 else None
        if p is not None and abs(p[3] * p[3] * d[5] - p[1]) / p[1] > 0.03:
            p = None  # misaligned property row of the PDF text: i^2 * A must reproduce Iy
        if p is not None:
            ref.append(('L', label, [d[5], p[1], p[2], p[3]]))

    def fmt(x):
        return repr(int(x)) if x == int(x) else repr(x)

    with open('src/core/profile_catalogue_data.inc', 'w', encoding='utf-8') as f:
        f.write('// GENERATED by tools/gen_profile_data.py from the ArcelorMittal catalogue "Sections and Merchant Bars"\n')
        f.write('// (nominal dimensions in mm; see docs/research/ui/perfiles_datos.md). Do not edit by hand.\n')
        for s, n, d in cat:
            f.write('{"%s", "%s", {%s}},\n' % (s, n, ', '.join(fmt(x) for x in d)))
    with open('tests/profile_reference_data.inc', 'w', encoding='utf-8') as f:
        f.write('// GENERATED by tools/gen_profile_data.py: published catalogue values used as test reference.\n')
        f.write('// Units of the catalogue: A cm2, then Iy cm4, Wel,y cm3, Wpl,y cm3, iy cm, Avz cm2, Iz cm4, Wel,z, Wpl,z, iz cm,\n')
        f.write('// ss mm, It cm4, Iw 1e9 mm6 (UPN: then ys, ym cm), then classes; for L: A, Iy cm4, Wel cm3, i cm.\n')
        for s, n, v in ref:
            f.write('{"%s", "%s", {%s}},\n' % (s, n, ', '.join(fmt(x) for x in v)))
    from collections import Counter
    print(Counter(s for s, _, _ in cat))


main(sys.argv[1])
