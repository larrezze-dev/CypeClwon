"""T-RE-04: inventory and extraction of the example works (.ed3/.c3e) embedded in the CYPE 3D example DLLs.

ejd3d{ale,bra,bul,fra,ing,ita,por}.dll, ejed3d.dll hold gzip streams (1f 8b 08, mtime 0, OS 0x0b) in their .data section.
Each decompressed stream is: [wrapper: u32 flag, u32 1, u32 n + n bytes licence-holder name, ..., u32 size, u32 pathlen,
virtual example path (ANSI or UTF-16LE)] followed by the real work file, which starts with 02 01 <u16 len> "CYPE-yyyy-x".

Usage:
  python -I -X utf8 tools/re04_examples.py <install dir> inventory <out.tsv>
  python -I -X utf8 tools/re04_examples.py <install dir> extract <out dir> [<dll stem> ...]   (writes <stem>_<n>.ed3/.c3e)
"""
import glob
import hashlib
import os
import re
import struct
import sys
import zlib

GZ = re.compile(rb'\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\x0b')
VER = re.compile(rb'\x02\x01([\x05-\x20])\x00((?:CYPE|EJPE)[-\w.]*)')


def streams(path):
    data = open(path, 'rb').read()
    for m in GZ.finditer(data):
        z = zlib.decompressobj(31)
        try:
            out = z.decompress(data[m.start():])
        except zlib.error:
            continue
        if len(out) > 64:
            yield m.start(), len(data[m.start():]) - len(z.unused_data) - 0, out


def split(blob):
    """Return (virtual path, work bytes, version tag) of one decompressed stream, or (None, blob, '')."""
    for m in VER.finditer(blob[:2000]):
        start = m.start()
        for utf16 in (False, True):
            for plen in range(8, 400):
                off = start - plen
                if off < 4 or struct.unpack_from('<I', blob, off - 4)[0] != plen:
                    continue
                raw = blob[off:start]
                try:
                    text = raw.decode('utf-16le') if utf16 else raw.decode('cp1252')
                except UnicodeDecodeError:
                    continue
                if text.startswith('\\') and text.lower().endswith(('.ed3', '.c3e', '.ed$')):
                    return text, blob[start:], m.group(2).decode()
    return None, blob, ''


def main():
    root, mode, out = sys.argv[1], sys.argv[2], sys.argv[3]
    only = set(sys.argv[4:])
    rows = []
    if mode == 'extract':
        os.makedirs(out, exist_ok=True)
    for path in sorted(glob.glob(os.path.join(root, 'ej*.dll'))):
        stem = os.path.splitext(os.path.basename(path))[0]
        if only and stem not in only:
            continue
        for k, (off, _, blob) in enumerate(streams(path)):
            vpath, work, ver = split(blob)
            ext = (os.path.splitext(vpath)[1].lower() if vpath else '.bin') or '.bin'
            rows.append((stem, k, off, len(blob), len(work), ver, hashlib.md5(work).hexdigest()[:10], vpath or '?'))
            if mode == 'extract':
                open(os.path.join(out, '%s_%d%s' % (stem, k, ext)), 'wb').write(work)
    if mode == 'inventory':
        with open(out, 'w', encoding='utf-8', newline='\n') as f:
            f.write('dll\tn\tgz_offset\tstream_bytes\twork_bytes\tfile_version\tmd5_10\tvirtual_path\n')
            for r in rows:
                f.write('\t'.join(str(x) for x in r) + '\n')
    print(len(rows), 'streams')


if __name__ == '__main__':
    main()
