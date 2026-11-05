"""T-RE-04: merges the "<id>text" tables of every DLL of a CYPE install into one id -> language table.

Usage: python -I -X utf8 tools/re04_merge.py <install dir> <out.tsv> [<dll stem> ...]
An id is stored with the full language set (es, fr, pt-BR, pt-PT, ca, it, en, de + extra languages) only in the DLL that
owns it; every other DLL that references it carries a shadow copy with only the extra languages (bg, ro, zh, ru, pl, tr, ja).
The owner is the largest group; its Latin strings are those that no smaller shadow group carries.
Those are peeled by script and aligned to the master order of re04_langs.MASTER (DP alignment); if there are more than 13 they stay verbatim in 'unk'.
"""
import glob
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from re04_extract import groups  # noqa: E402
from re04_langs import confidence, split_group  # noqa: E402

KEEP = ['es', 'fr', 'pt_BR', 'pt_PT', 'ca', 'it', 'en', 'gl', 'de', 'ro', 'sl', 'pl', 'tr', 'cs', 'hu', 'sk']


def merge_id(per_dll):
    """per_dll: {stem: [(offset, text), ...]} -> (row, owner stem)."""
    owner = max(per_dll, key=lambda k: len(per_dll[k]))
    items = per_dll[owner]
    shadows = [v for k, v in per_dll.items() if k != owner and len(v) <= 7 and len(v) + 4 <= len(items)]
    if not shadows:
        row, _ = split_group(items)
        return row, owner
    extra_set = {t for v in shadows for _, t in v}
    latin = [t for _, t in items if t not in extra_set]
    row, _ = split_group([(o, t) for o, t in items if t in extra_set])
    row2, _ = split_group([(0, t) for t in latin])  # peels scripts/diacritics the shadows lacked, then aligns the rest
    for k, v in row2.items():
        row.setdefault(k, v)
    return row, owner


def main():
    root, out = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3:])
    allg = {}
    for path in sorted(glob.glob(os.path.join(root, '*.dll'))):
        stem = os.path.splitext(os.path.basename(path))[0]
        if only and stem not in only:
            continue
        for gid, items in groups(path).items():
            allg.setdefault(gid, {})[stem] = items
    n = unk = 0
    with open(out, 'w', encoding='utf-8', newline='\n') as f:
        f.write('id\towner_dll\tn_dlls\t' + '\t'.join(KEEP + ['unk', 'conf']) + '\n')
        for gid in sorted(allg):
            row, owner = merge_id(allg[gid])
            cells = [row.get(c, '').replace('\t', ' ').replace('\r', '').replace('\n', '\\n') for c in KEEP + ['unk']]
            if not row.get('es'):  # rows without Spanish are not useful as a glossary; rebuild with this script if needed
                continue
            cells.append(confidence(row))
            n += 1
            unk += bool(row.get('unk'))
            f.write(str(gid) + '\t' + owner + '\t' + str(len(allg[gid])) + '\t' + '\t'.join(cells) + '\n')
    print(len(allg), 'ids total;', n, 'written;', unk, 'with unresolved (unk) strings ->', out)


if __name__ == '__main__':
    main()
