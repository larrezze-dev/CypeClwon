"""T-RE-04: robust extractor of "<id>text" UTF-16LE NUL-terminated string tables from CYPE DLLs.

Usage: python -I tools/re04_extract.py <dll> [--dump id ...]
Library use: groups(path) -> OrderedDict id -> list of (offset, text) in file order.
Unlike tools/extract_dll_strings.py it has no minimum length and accepts any code unit (CJK, Cyrillic, ...).
"""
import re
import sys
from collections import OrderedDict

HEAD = re.compile(rb'<\x00((?:\d\x00){3,7})>\x00')


def groups(path):
    data = open(path, 'rb').read()
    out = OrderedDict()
    for m in HEAD.finditer(data):
        if m.start() % 2:
            continue
        gid = int(m.group(1)[::2].decode())
        p = m.end()
        end = p
        while end + 1 < len(data) and (data[end] or data[end + 1]):
            end += 2
        text = data[p:end].decode('utf-16le', errors='replace')
        out.setdefault(gid, []).append((m.start(), text))
    return out


if __name__ == '__main__':
    g = groups(sys.argv[1])
    ids = [int(x) for x in sys.argv[3:]] if len(sys.argv) > 2 and sys.argv[2] == '--dump' else []
    if ids:
        for i in ids:
            for off, t in g.get(i, []):
                print(i, off, repr(t))
    else:
        print(len(g), 'ids', sum(len(v) for v in g.values()), 'strings')
