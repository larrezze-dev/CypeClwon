#!/usr/bin/env python3
"""Extracts the tabulated properties of the OTHER rolled steel series that pfarcelor.dll embeds (IPE A, IPE AA, IPE O, HEAA, HE, HEC,
HL, HD, HP, UB, UBP, UC, J) and of the angles / UPE / PFC that perfil3.dll embeds, from the pseudocode written by the REA tool
`decompile_all` (doubles appear as decimal literals or as 0x... IEEE-754 bit patterns).

Usage:  python tools/extract_pfarc_extra.py <pfarcelor decompile dir> <perfil3 functions dir> <output dir>
  <perfil3 functions dir> must contain 0x18044de10.c (angles L), 0x180454930.c (UPE), 0x180452f60.c (PFC).

Writes (docs/research/ui/data):
  describir_perfiles_laminados_extra_rea.tsv   same columns as describir_perfiles_laminados_rea.tsv (I-shapes; J has sloped flanges)
  describir_perfiles_l_rea.tsv                 angles: h t r1 A Iy Iu Iv e  (see docs/research/ui/describir_barras.md section 18)
  describir_perfiles_upe_pfc_rea.tsv           UPE and PFC channels: h b tw tf r A Iy Iz It Iw Wply Wplz e m
"""
import csv
import json
import os
import re
import struct
import sys


def dbl(token):
    token = token.strip()
    if token.startswith("0x"):
        return struct.unpack("<d", struct.pack("<Q", int(token, 16)))[0]
    return float(token)


def fmt(v):
    return "%.10g" % v


def load_index(base):
    index = json.load(open(os.path.join(base, "decompile-index.json")))
    return {f["name"]: os.path.join(base, f["relative_path"]) for f in index["functions"] if f.get("relative_path")}


SERIES = [  # (function key, series label, kind)
    ("ipea", "IPE_A", "IPE_A", "I"), ("ipeaa", "IPE_AA", "IPE_AA", "I"), ("ipeo", "IPE_O", "IPE_O", "I"),
    ("heaa", "HEAA", "HEAA", "I"), ("he", "HE", "HE", "I"), ("hec", "HEC", "HEC", "I"), ("hl", "HL", "HL", "I"),
    ("hd", "HD", "HD", "I"), ("hp", "HP", "HP", "I"), ("ub", "UB", "UB", "I"), ("ubp", "UBP", "UBP", "I"),
    ("uc", "UC", "UC", "I"), ("j", "J", "J", "IPN"),
]


def main():
    pf_dir, p3_dir, out = sys.argv[1:4]
    files = load_index(pf_dir)
    rows = []
    for key, suffix, label, kind in SERIES:
        src = open(files["pfarc_%s_obtener_valores_perfil_%s" % (key, suffix)], encoding="utf-8").read()
        ref = open(files["pfarc_%s_referencia_perfil_%s" % (key, suffix)], encoding="utf-8").read()
        names = {int(m.group(1), 0): m.group(2) for m in re.finditer(r'case (0x[0-9a-f]+|\d+):\s*\w+ = L"([^"]*)"', ref)}
        count = 0
        for m in re.finditer(r"case (0x[0-9a-f]+|\d+):\s*pwVar1 = [^;]*;\s*local_10\[0\] = \(undefined8 \*\)\s*(pfarc\w+)\s*\(([^;]*?)\);",
                             src, re.S):
            idx = int(m.group(1), 0)
            vals = [dbl(a) for a in re.split(r"[,\s]+", m.group(3).strip())[1:] if a]
            if len(vals) != 19:
                raise SystemExit("%s %d: %d values" % (label, idx, len(vals)))
            if kind == "I":
                h, b, tw, tf, r, A, hi, d, Iy, Wely, Wply, iy, Avz, Iz, Welz, Wplz, iz, It, Iw = vals
                r2 = ""
            else:
                h, b, tw, tf, r, r2, A, d, Iy, Wely, Wply, iy, Avz, Iz, Welz, Wplz, iz, It, Iw = vals
                r2 = fmt(r2)
            rows.append([label, names[idx], fmt(h), fmt(b), fmt(tw), fmt(tf), fmt(r), r2, fmt(A), fmt(Iy), fmt(Wely), fmt(Wply),
                         fmt(iy), fmt(Avz), fmt(Iz), fmt(Welz), fmt(Wplz), fmt(iz), fmt(It), fmt(Iw)])
            count += 1
        if count != len(names):
            raise SystemExit("%s: %d cases but %d references" % (label, count, len(names)))
    head = ["serie", "perfil", "h_mm", "b_mm", "tw_mm", "tf_mm", "r_o_r1_mm", "r2_mm", "A_cm2", "Iyy_cm4", "Wely_cm3", "Wply_cm3", "iy_cm",
            "Avz_cm2", "Izz_cm4", "Welz_cm3", "Wplz_cm3", "iz_cm", "It_cm4", "Iw_1e3cm6"]
    with open(os.path.join(out, "describir_perfiles_laminados_extra_rea.tsv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(head)
        w.writerows(rows)

    # angles: FUN_180443bc0(name, h, t, r1, A, Iy, Iu, Iv, e, ctx)
    src = open(os.path.join(p3_dir, "0x18044de10.c"), encoding="utf-8").read()
    ang = []
    for m in re.finditer(r'FUN_180443bc0\(L"([^"]*)",([^;]*?),uVar1\);', src, re.S):
        vals = [dbl(a) for a in re.split(r"[,\s]+", m.group(2).strip()) if a]
        if len(vals) != 8:
            raise SystemExit("angle %s: %d values" % (m.group(1), len(vals)))
        ang.append([m.group(1)] + [fmt(v) for v in vals])
    with open(os.path.join(out, "describir_perfiles_l_rea.tsv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["perfil_original", "h_mm", "t_mm", "r1_mm", "A_cm2", "Iy_cm4", "Iu_cm4", "Iv_cm4", "e_cm"])
        w.writerows(ang)

    ch = []
    for fn, tag in (("0x180454930.c", "UPE"), ("0x180452f60.c", "PFC")):
        src = open(os.path.join(p3_dir, fn), encoding="utf-8").read()
        for m in re.finditer(r'FUN_1804441b0\(L"([^"]*)",([^;]*?),uVar1\);', src, re.S):
            vals = [dbl(a) for a in re.split(r"[,\s]+", m.group(2).strip()) if a]
            if len(vals) != 14:
                raise SystemExit("%s %s: %d values" % (tag, m.group(1), len(vals)))
            ch.append([tag, m.group(1)] + [fmt(v) for v in vals])
    with open(os.path.join(out, "describir_perfiles_upe_pfc_rea.tsv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["serie", "perfil", "h_mm", "b_mm", "tw_mm", "tf_mm", "r_mm", "A_cm2", "Iyy_cm4", "Izz_cm4", "It_cm4", "Iw_cm6",
                    "Wply_cm3", "Wplz_cm3", "e_cm", "m_cm"])
        w.writerows(ch)
    print(len(rows), "I/J rows,", len(ang), "angles,", len(ch), "UPE/PFC rows")


if __name__ == "__main__":
    main()
