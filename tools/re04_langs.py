"""T-RE-04: per-language split of the "<id>text" tables of CYPE DLLs (heuristic, see docs/research/ui/t_re_04_ejd3d_resources.md).

Usage: python -I -X utf8 tools/re04_langs.py <dll> <out.tsv>
Each id is stored once per language in the string pool of the DLL; identical strings are pooled by the linker, so a language
may be missing (typically pt-PT = pt-BR). Script-distinct languages (zh, ja, el, th, Cyrillic bg/ru) and diacritic-marked
ro/pl/tr are peeled off by character set; the remaining Latin strings follow the master order observed in the data
  es, fr, pt-BR, pt-PT, ca, it, en, gl, ro, de, sl, pl, tr
and are assigned by a monotone dynamic-programming alignment (word lists + diacritics, different skip costs per language).
Rows whose strings cannot be aligned are kept verbatim in 'unk' (no guessing). Treat the result as a lookup aid, not as truth.
"""
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from re04_extract import groups  # noqa: E402

MASTER = ['es', 'fr', 'pt_BR', 'pt_PT', 'ca', 'it', 'en', 'gl', 'ro', 'de', 'sl', 'pl', 'tr']
SKIP = {'es': 1.5, 'fr': 1.5, 'pt_BR': 1.0, 'pt_PT': 0.1, 'ca': 1.5, 'it': 1.5, 'en': 1.5, 'gl': 0.1, 'ro': 0.3, 'de': 1.5,
        'sl': 0.1, 'pl': 0.3, 'tr': 0.3}
COLS = ['es', 'fr', 'pt_BR', 'pt_PT', 'ca', 'it', 'en', 'gl', 'de', 'ro', 'sl', 'pl', 'tr', 'cs', 'hu', 'sk', 'bg', 'ru', 'zh', 'ja', 'el', 'th', 'unk']
WORDS = {
    'es': 'el la los las del para con una un en por que se es no de y al lo como sobre entre',
    'fr': 'le la les des du pour avec une un dans par que est pas de et au sur comme entre',
    'pt': 'o a os as do da dos das para com uma um em por que se é não de e ao no na como entre',
    'ca': 'el la els les del per amb una un en que és no de i al lo com sobre entre',
    'it': 'il lo la i gli le del della dei per con una un in che è non di e al sul come tra',
    'en': 'the of and to is in for with a an or on be are not by as from that this',
    'gl': 'o a os as do da dos das para con unha un en por que se é non de e ao no na como entre',
    'de': 'der die das den dem des und zu mit ist für nicht ein eine von im auf bei als nach',
    'ro': 'de și în pentru cu este nu la un o sau din pe se mai',
    'sl': 'je in na za se da ki ne ali pri po od iz kot lahko',
    'pl': 'w i na z do nie jest się dla oraz lub przez',
    'tr': 've bir bu için ile de da olarak veya değil'}
WS = {k: set(v.split()) for k, v in WORDS.items()}
FEAT = {'es': 'ñ¿¡', 'fr': 'èêàùçœ', 'pt': 'ãõçê', 'ca': '·çàòï', 'it': 'ìù', 'de': 'ßäöü', 'sl': 'čšž', 'gl': 'ñ',
        'ro': 'ăâîșț', 'pl': 'ąęłńśźż', 'tr': 'ğıİş'}


def script(t):
    if re.search(r'[぀-ヿ]', t):
        return 'ja'
    if re.search(r'[一-鿿]', t):
        return 'zh'
    if re.search(r'[Ͱ-Ͽ]', t):
        return 'el'
    if re.search(r'[฀-๿]', t):
        return 'th'
    if re.search(r'[Ѐ-ӿ]', t):
        return 'cyr'
    return None


def score(t, lang):
    key = 'pt' if lang.startswith('pt') else lang
    w = re.findall(r"[a-zà-ÿ·čšžłąęśźżăâîșțğış]+", t.lower())
    s = sum(x in WS[key] for x in w) - 0.5 * sum(x in WS['en'] and x not in WS[key] for x in w if key != 'en')
    s += 2 * sum(c in FEAT.get(key, '') for c in t.lower())
    return s


def classify_diacritic(t):
    lt = t.lower()
    if re.search(r'[ğıİş]', lt):
        return 'tr'
    if re.search(r'[řěů]', lt):
        return 'cs'
    if re.search(r'[őű]', lt):
        return 'hu'
    if re.search(r'[ľĺŕô]', lt):
        return 'sk'
    if re.search(r'[čšžđ]', lt):
        return 'sl'
    if re.search(r'[ăâîșț]', lt):
        return 'ro'
    if re.search(r'[ąęłńśźż]', lt):
        return 'pl'
    return None


def assign_latin(latin):
    """Monotone alignment of the Latin strings to MASTER; returns {lang: text} or None if impossible (too many strings)."""
    n, m = len(latin), len(MASTER)
    if n == 0:
        return {}
    if n > m:
        return None
    NEG = -1e9
    best = [[NEG] * (m + 1) for _ in range(n + 1)]
    back = [[None] * (m + 1) for _ in range(n + 1)]
    best[0][0] = 0.0
    for i in range(n + 1):
        for j in range(m):
            cur = best[i][j]
            if cur == NEG:
                continue
            v = cur - SKIP[MASTER[j]]  # language j absent
            if v > best[i][j + 1]:
                best[i][j + 1], back[i][j + 1] = v, (i, j, 0)
            if i < n:  # string i belongs to language j
                v = cur + 1.0 + score(latin[i], MASTER[j])
                if v > best[i + 1][j + 1]:
                    best[i + 1][j + 1], back[i + 1][j + 1] = v, (i, j, 1)
    i, j = n, m
    out = {}
    while i > 0 or j > 0:
        pi, pj, took = back[i][j]
        if took:
            out[MASTER[pj]] = latin[pi]
        i, j = pi, pj
    if 'pt_BR' in out and 'pt_PT' not in out:
        out['pt_PT'] = out['pt_BR']  # identical strings are pooled
    return out


LATIN_CHECK = ['es', 'fr', 'pt_BR', 'ca', 'it', 'en', 'de', 'ro']
CORE = ['es', 'fr', 'pt_BR', 'ca', 'it', 'en', 'de']


def confidence(row):
    """Independent plausibility check of an aligned row: 'A' = all core languages present and no cell is clearly written
    in another language, 'B' = a core language is missing or evidence is too weak to tell, 'C' = at least one cell
    contradicted (another language scores >= 2 points higher), duplicate text in non-Portuguese columns, or unaligned text."""
    if row.get('unk'):
        return 'C'
    bad = 0
    weak = 0
    seen = {}
    for lang in LATIN_CHECK:
        t = row.get(lang)
        if not t:
            continue
        own = score(t, lang)
        other = max(score(t, o) for o in LATIN_CHECK if o != lang and o != ('pt_PT' if lang == 'pt_BR' else ''))
        if other >= own + 2 and other >= 2:
            bad += 1
        elif own < 1:
            weak += 1
        if lang not in ('pt_BR', 'pt_PT') and seen.setdefault(t, lang) != lang and len(t) > 12:
            bad += 1
    if bad:
        return 'C'
    if any(not row.get(c) for c in CORE) or weak > 2:
        return 'B'
    return 'A'


def split_group(items):
    """Return (row, n_latin) for the (offset, text) items of one id."""
    texts = [t for _, t in items]
    row = {}
    spill = []
    cyr = [t for t in texts if script(t) == 'cyr']
    if len(cyr) == 1:
        row['ru' if re.search(r'[ыэёЫЭЁ]|\bдля\b|\bэто\b|\bчто\b', cyr[0]) else 'bg'] = cyr[0]
    elif len(cyr) == 2:
        row['bg'], row['ru'] = cyr
    else:
        spill += cyr
    latin = []
    for t in texts:
        sc = script(t)
        if sc == 'cyr':
            continue
        c = sc or classify_diacritic(t)
        if c and c not in row:
            row[c] = t
        elif sc:
            spill.append(t)
        else:
            latin.append(t)
    n = len(latin)
    assigned = assign_latin(latin)
    if assigned is None:
        spill += latin
    else:
        for k, v in assigned.items():
            row.setdefault(k, v)
    if spill:
        row['unk'] = ' | '.join(spill)
    return row, n


def main():
    g = groups(sys.argv[1])
    with open(sys.argv[2], 'w', encoding='utf-8', newline='\n') as f:
        f.write('id\tn_latin\t' + '\t'.join(COLS) + '\n')
        for gid, items in g.items():
            row, n = split_group(items)
            cells = [row.get(c, '').replace('\t', ' ').replace('\r', '').replace('\n', '\\n') for c in COLS]
            f.write(str(gid) + '\t' + str(n) + '\t' + '\t'.join(cells) + '\n')
    print(len(g), 'ids written to', sys.argv[2])


if __name__ == '__main__':
    main()
