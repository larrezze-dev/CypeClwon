"""T-RE-04: extracts the example works stored as two concatenated pieces (ejd3dale/fra/por `*_cyp_datos` exports).

cypeio.dll!fichmem_genera_contenido2(&slot, cb1, cb2) calls two callbacks that each return {ptr,size} and builds one buffer
from them (REA: FUN_180010f20 x2, FUN_180010be0 x2). The callbacks are `lea rax,[rip+d]; mov [g],rax; mov [g2],size`
stubs into .data. This script reads both stubs for every export that references them, concatenates piece 1 and piece 2 (cypeio.dll builds one buffer from both)
and gunzips the result when it is a gzip stream.

Usage: python -I -X utf8 tools/re04_examples2.py <install dir> <out dir> [<dll stem> ...]
"""
import os
import re
import struct
import sys
import zlib

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from re04_examples import split  # noqa: E402

BASE_TEXT_RAW = None


def pe_info(d):
    pe = struct.unpack_from('<I', d, 0x3c)[0]
    opt = struct.unpack_from('<H', d, pe + 20)[0]
    ns = struct.unpack_from('<H', d, pe + 6)[0]
    secs = []
    for i in range(ns):
        o = pe + 24 + opt + i * 40
        vs, va, rs, ro = struct.unpack_from('<IIII', d, o + 8)
        secs.append((va, max(vs, rs), ro))
    return pe, secs


def r2o(secs, rva):
    for va, sz, ro in secs:
        if va <= rva < va + sz:
            return rva - va + ro
    raise ValueError(hex(rva))


def exports(d, pe, secs):
    er, _ = struct.unpack_from('<II', d, pe + 24 + 112)
    o = r2o(secs, er)
    nn, af, an, ao = struct.unpack_from('<IIII', d, o + 24)
    out = []
    for i in range(nn):
        nm = struct.unpack_from('<I', d, r2o(secs, an) + 4 * i)[0]
        no = r2o(secs, nm)
        name = d[no:d.index(b'\0', no)].decode()
        ordn = struct.unpack_from('<H', d, r2o(secs, ao) + 2 * i)[0]
        out.append((name, struct.unpack_from('<I', d, r2o(secs, af) + 4 * ordn)[0]))
    return out


def stub(d, secs, rva):
    """Decode `lea rax,[rip+disp]; mov [g],rax; mov qword [g2],imm32` -> (ptr rva, size)."""
    o = r2o(secs, rva)
    if d[o:o + 3] != b'\x48\x8d\x05':
        return None
    disp = struct.unpack_from('<i', d, o + 3)[0]
    ptr = rva + 7 + disp
    m = re.match(rb'\x48\x89\x05....\x48\xc7\x05....(....)', d[o + 7:o + 7 + 20], re.S)
    return (ptr, struct.unpack('<I', m.group(1))[0]) if m else None


def main():
    root, out = sys.argv[1], sys.argv[2]
    only = set(sys.argv[3:])
    os.makedirs(out, exist_ok=True)
    for stem in ('ejd3dale', 'ejd3dfra', 'ejd3dpor'):
        if only and stem not in only:
            continue
        d = open(os.path.join(root, stem + '.dll'), 'rb').read()
        pe, secs = pe_info(d)
        for name, rva in exports(d, pe, secs):
            o = r2o(secs, rva)
            code = d[o:o + 0x30]
            direct = stub(d, secs, rva)
            if direct:  # `*_datos` returning {ptr,size} directly
                shares = [direct]
            else:
                # 48 83 ec 28 | 48 83 3d <rel> 00 | 75 xx | 4c 8d 05 <cb2> | 48 8d 15 <cb1> ...
                m = re.search(rb'\x4c\x8d\x05(....)\x48\x8d\x15(....)', d[o:o + 0x60], re.S)
                if not m:
                    print(stem, name, 'unrecognised stub', code[:16].hex())
                    continue
                pos = o + m.start()
                cb3 = rva + (pos - o) + 7 + struct.unpack('<i', m.group(1))[0]
                cb2 = rva + (pos - o) + 7 + 7 + struct.unpack('<i', m.group(2))[0]
                shares = [stub(d, secs, cb2), stub(d, secs, cb3)]
            blobs = [d[r2o(secs, p):r2o(secs, p) + s] for p, s in shares if (p, s)]
            res = None
            if len(blobs) == 2:  # fichmem_genera_contenido2 concatenates both pieces
                res = ('concat', blobs[0] + blobs[1])
            elif len(blobs) == 1:
                res = ('raw', blobs[0])
            if res is None:
                print(stem, name, 'shares', [(hex(p), s) for p, s in shares if p])
                continue
            kind, data = res
            note = kind
            try:
                data = zlib.decompressobj(31).decompress(data)
                note += '+gunzip'
            except zlib.error:
                pass
            vpath, work, ver = split(data)
            fn = os.path.join(out, '%s_%s.bin' % (stem, name))
            open(fn, 'wb').write(work if vpath else data)
            print(stem, name, note, len(data), ver, vpath or data[:24].hex())


if __name__ == '__main__':
    main()
