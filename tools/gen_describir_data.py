#!/usr/bin/env python3
"""Generates the catalogue data of the "Describir" dialog from the TSV transcriptions of the user's manual captures
(docs/research/ui/data/describir_*.tsv, see docs/research/ui/describir_barras_capturas_manuales.md).

Usage:  python tools/gen_describir_data.py

Writes
  src/core/describir_catalogue_data.inc   series of the library: built-up (IPC), cold-formed (U, C), aluminium ((EP) I), timber
  src/core/describir_tie_data.inc         profiles of the five tie shapes (kept OUT of the library: the tie series "L" would collide
                                          with the library series "L" of the rolled angles)

Only the numbers that the designation of the profile gives are extracted (millimetres; the mass of the IPC in kg/m). Nothing else
is derived. The script aborts when a row count differs from the transcription (85 / 14 / 11 / 836 / 619, plus the REA tables 85 / 14 / 36 / 11 / 17 / 36) or when a profile
reference is repeated inside its series.
"""
import csv
import os
import re
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DATA = os.path.join(ROOT, "docs", "research", "ui", "data")
OUT = os.path.join(ROOT, "src", "core")
NUM = r"(\d+(?:\.\d+)?)"


def read(name):
    with open(os.path.join(DATA, name), encoding="utf-8", newline="") as f:
        return list(csv.DictReader(f, delimiter="\t"))


def numbers(text, pattern):
    m = re.fullmatch(pattern, text)
    if not m:
        raise SystemExit("unexpected designation: %r" % text)
    return [float(g) for g in m.groups()]


def fmt(values):
    return ", ".join(("%g" % v) for v in values)


def expect(label, got, want):
    if got != want:
        raise SystemExit("%s: %d rows, expected %d" % (label, got, want))


def check_unique(rows, label):
    seen = set()
    for series, profile, _ in rows:
        if (series, profile) in seen:
            raise SystemExit("%s: repeated profile %s / %s" % (label, series, profile))
        seen.add((series, profile))


def rea_rows(name, count, label):
    rows = read(name)
    expect(label + " (REA)", len(rows), count)
    return rows


def check_names(captured, rea, key, label):
    """The REA table must list exactly the designations of the manual captures, in the same order."""
    if [r["perfil"] for r in captured] != [r[key] for r in rea]:
        raise SystemExit(label + ": the REA table and the captured combo list different profiles")


def library_rows():
    out = []  # (family, kind, series, profile, dims, tabulated)

    # Built-up double T (IPC): h, b, tw, tf (mm) from the REA dump of perfil3.dll FUN_18002cfe0. The mass of the designation is NOT
    # stored (the original shows A x 0.785, not the nominal mass). The tabulated A / Iyy of 4 control rows equal the formulas and the
    # dialog shows the calculated values, so they are not stored either.
    armado = read("describir_perfiles_armado_ipc.tsv")
    expect("armado", len(armado), 85)
    rea = rea_rows("describir_perfiles_ipc_rea.tsv", 85, "armado")
    check_names(armado, rea, "designacion", "armado")
    rows = []
    for c, r in zip(armado, rea):
        nom = numbers(r["designacion"], r"IPC " + NUM + "x" + NUM + "x" + NUM)
        h, b, tw, tf = (float(r[k]) for k in ("h_mm", "b_mm", "tw_mm", "tf_mm"))
        if abs(h - nom[0]) > 1.0 or abs(b - nom[1]) > 1.0:  # designation = inches rounded to the nearest mm
            raise SystemExit("IPC: designation and REA dimensions disagree: " + r["designacion"])
        rows.append((c["serie"], c["perfil"], [h, b, tw, tf], []))
    check_unique([(s_, p_, d_) for s_, p_, d_, _ in rows], "armado")
    out += [("BuiltUpSteel", "BuiltUpDoubleT", s_, p_, d_, t_) for s_, p_, d_, t_ in rows]

    # Cold-formed U (UF): h, b, t, r (mm) + tabulated A (cm2) and Iyy (cm4) (REA dump, FUN_1801fde40).
    conf = read("describir_perfiles_conformado_u.tsv")
    expect("conformado U", len(conf), 14)
    rea = rea_rows("describir_perfiles_conformado_u_rea.tsv", 14, "conformado U")
    check_names(conf, rea, "designacion", "conformado U")
    rows = []
    for c, r in zip(conf, rea):
        nom = numbers(r["designacion"], r"UF-" + NUM + "x" + NUM)
        h, b, t, ri = (float(r[k]) for k in ("h_mm", "b_mm", "t_mm", "r_interior_mm"))
        if nom != [h, t]:
            raise SystemExit("UF: designation and REA dimensions disagree: " + r["designacion"])
        rows.append((c["serie"], c["perfil"], [h, b, t, ri], [float(r["A_cm2_tab"]), float(r["Iyy_cm4_tab"])]))
    check_unique([(s_, p_, d_) for s_, p_, d_, _ in rows], "conformado")
    out += [("ColdFormedSteel", "ColdFormedU", s_, p_, d_, t_) for s_, p_, d_, t_ in rows]

    # Cold-formed C (CF, series "C"): h, b, c (lip), t, r (mm) + tabulated A, Iyy, Izz (cm2, cm4, cm4). The meaning of the last three
    # REA columns is DEDUCED by analogy with UF (checked numerically: a contour model reproduces them within ~1 % for most rows).
    # NOTE: the REA h of CF-140/180/225/250/275/300 differs from the designation (136, 176, 224, 248, 272, 288): kept as dumped.
    rea = rea_rows("describir_perfiles_conformado_c_rea.tsv", 36, "conformado C")
    rows = []
    for r in rea:
        numbers(r["designacion"], r"CF-" + NUM + "x" + NUM)
        d = [float(r[k]) for k in ("h_mm", "b_mm", "c_labio_mm", "t_mm", "r_interior_mm")]
        rows.append(("C", r["designacion"], d, [float(r["A_cm2_tab"]), float(r["Iyy_cm4_tab"]), float(r["Izz_cm4_tab"])]))
    check_unique([(s_, p_, d_) for s_, p_, d_, _ in rows], "conformado C")
    out += [("ColdFormedSteel", "ColdFormedC", s_, p_, d_, t_) for s_, p_, d_, t_ in rows]

    # Aluminium (EP) I: h, b, tw, tf (mm) (REA dump, FUN_18003b720); tw = tf in the 11 profiles.
    alu = read("describir_perfiles_aluminio_doble_t.tsv")
    expect("aluminio", len(alu), 11)
    rea = rea_rows("describir_perfiles_aluminio_rea.tsv", 11, "aluminio")
    check_names(alu, rea, "designacion", "aluminio")
    rows = []
    for c, r in zip(alu, rea):
        nom = numbers(r["designacion"], r"I-" + NUM + "x" + NUM + "x" + NUM)
        h, b, tw, tf = (float(r[k]) for k in ("h_mm", "b_mm", "tw_mm", "tf_mm"))
        if nom != [h, b, tw]:
            raise SystemExit("aluminium: designation and REA dimensions disagree: " + r["designacion"])
        rows.append((c["serie"], c["perfil"], [h, b, tw, tf], []))
    check_unique([(s_, p_, d_) for s_, p_, d_, _ in rows], "aluminio")
    out += [("Aluminium", "AluminiumI", s_, p_, d_, t_) for s_, p_, d_, t_ in rows]

    madera = read("describir_perfiles_madera.tsv")
    expect("madera", len(madera), 836)
    rows = []
    for r in madera:
        forma, serie, perfil = r["forma"], r["serie"], r["perfil"]
        if forma.startswith("Redondo"):
            kind, dims = "TimberRound", numbers(perfil, "Ø" + NUM)  # d
        elif forma.startswith("Cuadrado"):
            kind = "TimberSquare"
            a, b = numbers(perfil, r"(?:S|DUO|TRIO|GL)-" + NUM + "x" + NUM)
            if a != b:
                raise SystemExit("square profile with different sides: " + perfil)
            dims = [a]
        elif forma.startswith("Rectangular, canto"):
            kind = "TimberRectangularVariable"
            serie = "Rectangular, canto variable"  # the capture shows the name truncated ("Rectangular, canto vari...")
            b = numbers(perfil, r"RV-" + NUM)[0]
            dims = [b, 100.0, 100.0]  # b, initial depth, final depth (defaults 100.0 mm of the capture 101)
        else:
            kind = "TimberRectangular"
            # "S-NxB": N is the depth h and B the width b (series "Maciza hN", "Laminada bB"). "Cabios/Viguetas" 100x70 follows the
            # same order BY ANALOGY (not verified).
            if serie == "Cabios/Viguetas":
                h, b = numbers(perfil, NUM + "x" + NUM)
            else:
                h, b = numbers(perfil, r"(?:S|DUO|TRIO|GL)-" + NUM + "x" + NUM)
            dims = [b, h]  # section dimension order of the core: b, h
        rows.append((serie, perfil, dims, kind))
    check_unique([(s, p, d) for s, p, d, _ in rows], "madera")
    out += [("Timber", k, s, p, d, []) for s, p, d, k in rows]
    return out


def tie_rows():
    rows = read("describir_perfiles_tirante.tsv")
    expect("tirante", len(rows), 619)
    out = []
    for r in rows:
        forma, serie, perfil = r["forma"], r["serie"], r["perfil"]
        if forma.startswith("Angular sim"):
            a, a2, t = numbers(perfil, r"L " + NUM + " x " + NUM + " x " + NUM)
            if a != a2:
                raise SystemExit("equal angle with different legs: " + perfil)
            kind, dims = "AngleEqual", [a, t, 0.0, 0.0]  # h, t, r1, r2: radii not given by the designation (PROVISIONAL 0)
        elif forma == "Angular":
            h, b, t = numbers(perfil, r"L " + NUM + " x " + NUM + " x " + NUM)
            kind, dims = "AngleUnequal", [h, b, t, 0.0, 0.0]
        elif forma == "Platabanda":
            first, second = numbers(perfil, r"FL " + NUM + " x " + NUM)
            kind, dims = "FlatBar", [first, second]  # h, b: orientation not verified (PROVISIONAL)
        elif forma.startswith("Redondo"):
            kind, dims = "RoundBar", numbers(perfil, r"R " + NUM)
        elif forma.startswith("Cuadrado"):
            a, b = numbers(perfil, r"SQ " + NUM + " x " + NUM)
            kind, dims = "SquareBar", [a]
        else:
            raise SystemExit("unknown tie shape: " + forma)
        out.append((kind, serie, perfil, dims))
    check_unique([(k + s, p, d) for k, s, p, d in out], "tirante")
    return out


def material_rows():
    """Concrete grades (E in MPa, EC2 block of datmater, REA) and timber strength classes (two property sets, REA ec5.dll)."""
    concrete = read("hormigon_eurocodigo2_rea.tsv")
    expect("hormigon", len(concrete), 17)
    for r in concrete:
        if r["nu"] != "0.2" or float(r["alpha_1perC"]) != 1e-5 or r["peso_T_m3"] != "2.5":
            raise SystemExit("concrete: nu / alpha / weight differ from the REA constants for " + r["tipo"])
    timber = read("ec5_clases_madera_rea.tsv")
    expect("madera clases", len(timber), 36)
    return concrete, timber


def main():
    header = ("// GENERATED by tools/gen_describir_data.py from docs/research/ui/data/describir_*.tsv (manual captures of the original).\n"
              "// Dimensions in mm (REA dumps of perfil3.dll for IPC, UF, CF and (EP) I; designation numbers for timber). Do not edit by hand.\n")
    with open(os.path.join(OUT, "describir_catalogue_data.inc"), "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
        f.write("// {family, kind, series, profile, {dims mm}, {tabulated A cm2, Iyy cm4 [, Izz cm4]}}\n")
        for fam, kind, series, profile, dims, tab in library_rows():
            f.write('{SectionFamily::%s, ShapeKind::%s, "%s", "%s", {%s}, {%s}},\n' % (fam, kind, series, profile, fmt(dims), fmt(tab)))
    with open(os.path.join(OUT, "describir_tie_data.inc"), "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
        f.write("// {kind, series, profile, {dims}}\n")
        for kind, series, profile, dims in tie_rows():
            f.write('{ShapeKind::%s, "%s", "%s", {%s}},\n' % (kind, series, profile, fmt(dims)))
    concrete, timber = material_rows()
    with open(os.path.join(OUT, "describir_concrete_data.inc"), "w", encoding="utf-8", newline="\n") as f:
        f.write("// GENERATED by tools/gen_describir_data.py from hormigon_eurocodigo2_rea.tsv (REA). Do not edit by hand.\n")
        f.write("// {grade, E MPa}\n")
        for r in concrete:
            f.write('{"%s", %s},\n' % (r["tipo"], r["E_MPa"]))
    with open(os.path.join(OUT, "describir_timber_data.inc"), "w", encoding="utf-8", newline="\n") as f:
        f.write("// GENERATED by tools/gen_describir_data.py from ec5_clases_madera_rea.tsv (REA, ec5.dll). Do not edit by hand.\n")
        f.write("// {class, type, E0mean GPa set 1 (EN), set 2 (CTE), G GPa set 1, set 2, rho kg/m3 set 1, set 2, rare}\n")
        for r in timber:
            f.write('{"%s", "%s", %s, %s, %s, %s, %s, %s, %s},\n' % (r["clase"], r["tipo"], r["E_f1000"], r["E_f1290"], r["G_f1530"],
                                                                   r["G_f17d0"], r["R_f1ba0"], r["R_f1e50"], r["poco_frecuente"]))
    print("ok")


if __name__ == "__main__":
    sys.exit(main())
