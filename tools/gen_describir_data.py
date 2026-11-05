#!/usr/bin/env python3
"""Generates the catalogue data of the "Describir" dialog from the TSV transcriptions of the user's manual captures
(docs/research/ui/data/describir_*.tsv, see docs/research/ui/describir_barras_capturas_manuales.md).

Usage:  python tools/gen_describir_data.py

Writes
  src/core/describir_catalogue_data.inc   series of the library: built-up (IPC), cold-formed (U, C), aluminium ((EP) I), timber
  src/core/describir_tie_data.inc         profiles of the five tie shapes (kept OUT of the library: the tie series "L" would collide
                                          with the library series "L" of the rolled angles)

Only the numbers that the designation of the profile gives are extracted (millimetres; the mass of the IPC in kg/m). Nothing else
is derived. The script aborts when a row count differs from the transcription (85 / 14 / 11 / 836 / 619) or when a profile
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


def library_rows():
    out = []  # (family, kind, series, profile, dims)

    armado = read("describir_perfiles_armado_ipc.tsv")
    expect("armado", len(armado), 85)
    rows = []
    for r in armado:
        rows.append((r["serie"], r["perfil"], numbers(r["perfil"], r"IPC " + NUM + "x" + NUM + "x" + NUM)))
    check_unique(rows, "armado")
    out += [("BuiltUpSteel", "BuiltUpDoubleT", s, p, d) for s, p, d in rows]  # h, b, mass (kg/m)

    conf = read("describir_perfiles_conformado_u.tsv")
    expect("conformado U", len(conf), 14)
    rows = [(r["serie"], r["perfil"], numbers(r["perfil"], r"UF-" + NUM + "x" + NUM)) for r in conf]
    check_unique(rows, "conformado")
    out += [("ColdFormedSteel", "ColdFormedU", s, p, d) for s, p, d in rows]  # h, t
    # Series "C" (U conformado rigidizado): only the default profile was seen (capture 10).
    out.append(("ColdFormedSteel", "ColdFormedC", "C", "CF-60x2.0", numbers("CF-60x2.0", r"CF-" + NUM + "x" + NUM)))

    alu = read("describir_perfiles_aluminio_doble_t.tsv")
    expect("aluminio", len(alu), 11)
    rows = [(r["serie"], r["perfil"], numbers(r["perfil"], r"I-" + NUM + "x" + NUM + "x" + NUM)) for r in alu]
    check_unique(rows, "aluminio")
    out += [("Aluminium", "AluminiumI", s, p, d) for s, p, d in rows]  # d1, d2, d3 (meaning not verified)

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
    out += [("Timber", k, s, p, d) for s, p, d, k in rows]
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


def main():
    header = ("// GENERATED by tools/gen_describir_data.py from docs/research/ui/data/describir_*.tsv (manual captures of the original).\n"
              "// Nominal numbers of the designations only (mm; mass in kg/m for the IPC). Do not edit by hand.\n")
    with open(os.path.join(OUT, "describir_catalogue_data.inc"), "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
        f.write("// {family, kind, series, profile, {dims}}\n")
        for fam, kind, series, profile, dims in library_rows():
            f.write('{SectionFamily::%s, ShapeKind::%s, "%s", "%s", {%s}},\n' % (fam, kind, series, profile, fmt(dims)))
    with open(os.path.join(OUT, "describir_tie_data.inc"), "w", encoding="utf-8", newline="\n") as f:
        f.write(header)
        f.write("// {kind, series, profile, {dims}}\n")
        for kind, series, profile, dims in tie_rows():
            f.write('{ShapeKind::%s, "%s", "%s", {%s}},\n' % (kind, series, profile, fmt(dims)))
    print("ok")


if __name__ == "__main__":
    sys.exit(main())
