#!/usr/bin/env python3
"""Extracts the tabulated properties of the rolled steel series (IPE, HEA, HEB, HEM, IPN, UPN) that the original embeds as code in
pfarcelor.dll (I / H / IPN: pfarc_<s>_obtener_valores_perfil_<S>) and perfil3.dll (UPN: FUN_18006b9a0), from the pseudocode written by
the REA tool `decompile_all` (the doubles appear as decimal literals or as 0x... IEEE-754 bit patterns).

Usage:  python tools/extract_pfarc_tables.py <pfarcelor decompile dir> <perfil3 decompile dir with 0x18006b9a0.c> <output dir>

Writes docs/research/ui/data/describir_perfiles_laminados_rea.tsv (I-shapes and IPN) and ..._upn_rea.tsv.
Meaning of the columns (cm, cm2, cm4, cm3 as in the original's tables; verified against the (i) of the IPE 80, see
docs/research/ui/describir_barras.md section 17): the 19 values of pfarcdoblete_asigna_datos are
  h b tw tf r A hi d Iy Wel_y Wpl_y iy Avz Iz Wel_z Wpl_z iz It Iw
(hi, d: clear distances; Iw in 10^3 cm6) and, for the IPN (pfarcdoblete_alas_inclinadas_asigna_datos), without hi and with r1, r2:
  h b tw tf r1 r2 A d Iy Wel_y Wpl_y iy Avz Iz Wel_z Wpl_z iz It Iw
UPN (FUN_180054f50, 15 values; the last two read as the centroid distance e and the shear-centre distance m, as in the standard tables):
  h b tw tf r1 r2 A Iy Iz It Iw Wpl_y Wpl_z e m
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


def load_index(base):
    index = json.load(open(os.path.join(base, "decompile-index.json")))
    return {f["name"]: os.path.join(base, f["relative_path"]) for f in index["functions"] if f.get("relative_path")}


def fmt(v):
    return "%.10g" % v


def main():
    pf_dir, p3_dir, out = sys.argv[1:4]
    files = load_index(pf_dir)
    rows = []
    for key, series, kind in (("ipe", "IPE", "I"), ("hea", "HEA", "I"), ("heb", "HEB", "I"), ("hem", "HEM", "I"), ("ipn", "IPN", "IPN")):
        src = open(files["pfarc_%s_obtener_valores_perfil_%s" % (key, series)], encoding="utf-8").read()
        ref = open(files["pfarc_%s_referencia_perfil_%s" % (key, series)], encoding="utf-8").read()
        names = {int(m.group(1), 0): m.group(2) for m in re.finditer(r'case (0x[0-9a-f]+|\d+):\s*\w+ = L"([^"]*)"', ref)}
        count = 0
        for m in re.finditer(r"case (0x[0-9a-f]+|\d+):\s*pwVar1 = [^;]*;\s*local_10\[0\] = \(undefined8 \*\)\s*(pfarc\w+)\s*\(([^;]*?)\);", src, re.S):
            idx = int(m.group(1), 0)
            vals = [dbl(a) for a in re.split(r"[,\s]+", m.group(3).strip())[1:] if a]
            if len(vals) != 19:
                raise SystemExit("%s %d: %d values" % (series, idx, len(vals)))
            if kind == "I":
                h, b, tw, tf, r, A, hi, d, Iy, Wely, Wply, iy, Avz, Iz, Welz, Wplz, iz, It, Iw = vals
                r2 = ""
            else:
                h, b, tw, tf, r, r2, A, d, Iy, Wely, Wply, iy, Avz, Iz, Welz, Wplz, iz, It, Iw = vals
                r2 = fmt(r2)
            rows.append([series, names[idx], fmt(h), fmt(b), fmt(tw), fmt(tf), fmt(r), r2, fmt(A), fmt(Iy), fmt(Wely), fmt(Wply),
                         fmt(iy), fmt(Avz), fmt(Iz), fmt(Welz), fmt(Wplz), fmt(iz), fmt(It), fmt(Iw)])
            count += 1
        if count != len(names):
            raise SystemExit("%s: %d cases but %d references" % (series, count, len(names)))
    head = ["serie", "perfil", "h_mm", "b_mm", "tw_mm", "tf_mm", "r_o_r1_mm", "r2_mm", "A_cm2", "Iyy_cm4", "Wely_cm3", "Wply_cm3", "iy_cm",
            "Avz_cm2", "Izz_cm4", "Welz_cm3", "Wplz_cm3", "iz_cm", "It_cm4", "Iw_1e3cm6"]
    with open(os.path.join(out, "describir_perfiles_laminados_rea.tsv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(head)
        w.writerows(rows)

    src = open(os.path.join(p3_dir, "functions", "0x18006b9a0.c"), encoding="utf-8").read()
    upn = []
    for m in re.finditer(r'FUN_180054f50\(L"([^"]*)",([^;]*?),uVar1\);', src, re.S):
        vals = [dbl(a) for a in re.split(r"[,\s]+", m.group(2).strip()) if a]
        if len(vals) != 15:
            raise SystemExit("UPN %s: %d values" % (m.group(1), len(vals)))
        upn.append([m.group(1)] + [fmt(v) for v in vals])
    with open(os.path.join(out, "describir_perfiles_upn_rea.tsv"), "w", newline="", encoding="utf-8") as f:
        w = csv.writer(f, delimiter="\t", lineterminator="\n")
        w.writerow(["perfil", "h_mm", "b_mm", "tw_mm", "tf_mm", "r1_mm", "r2_mm", "A_cm2", "Iyy_cm4", "Izz_cm4", "It_cm4", "Iw_1e3cm6",
                    "Wply_cm3", "Wplz_cm3", "e_cm", "m_cm"])
        w.writerows(upn)
    print(len(rows), "I/IPN rows,", len(upn), "UPN rows")


if __name__ == "__main__":
    main()
