"""Extracts the "<id>text" UTF-16LE string groups of an original CYPE DLL and fills en_GB translations.

Usage: python -I tools/extract_dll_strings.py <dll> <out.tsv> [<ts file to fill>]
Each id has one string per language (ro, pl, tr, es, fr, pt, ca, it, en, de order, with gaps), so English is picked by a stop-word score, not by position.
"""
import re
import sys
import xml.etree.ElementTree as ET

RUN = re.compile(rb'(?:[\x09\x0a\x0d\x20-\x7e]\x00|[\xa0-\xff]\x00|[\x00-\xff][\x01-\x03]){4,}')
ENTRY = re.compile(r'<(\d{4,7})>(.*)', re.S)
EN_WORDS = {'the', 'of', 'to', 'and', 'show', 'hide', 'in', 'for', 'with', 'or', 'is', 'on', 'around', 'about', 'select',
            'section', 'planes', 'view', 'tool', 'activates', 'deactivates', 'displays', 'window', 'zoom', 'camera', 'rotation'}
ES_WORDS = {'el', 'la', 'de', 'los', 'las', 'del', 'para', 'con', 'una', 'un', 'en', 'por', 'mostrar', 'ocultar', 'activa'}


def groups(path):
    data = open(path, 'rb').read()
    out = {}
    order = []
    for m in RUN.finditer(data):
        try:
            text = m.group().decode('utf-16le')
        except UnicodeDecodeError:
            continue
        e = ENTRY.match(text)
        if not e:
            continue
        gid = int(e.group(1))
        if gid not in out:
            out[gid] = []
            order.append(gid)
        out[gid].append(e.group(2).rstrip('\x00'))
    return out, order


def english(cands):
    def score(s):
        w = re.findall(r"[a-zA-Z]+", s.lower())
        return sum(x in EN_WORDS for x in w) - 2 * sum(x in ES_WORDS for x in w)
    best = max(cands, key=score, default=None)
    return best if best is not None and score(best) > 0 else None


def main():
    dll, tsv = sys.argv[1], sys.argv[2]
    g, order = groups(dll)
    with open(tsv, 'w', encoding='utf-8', newline='\n') as f:
        for gid in order:
            f.write(str(gid) + '\t' + '\t'.join(s.replace('\t', ' ').replace('\n', '\\n') for s in g[gid]) + '\n')
    print(len(g), 'groups written to', tsv)
    if len(sys.argv) > 3:
        es = {t: v for v in g.values() for t in v}
        tree = ET.parse(sys.argv[3])
        done = 0
        for msg in tree.getroot().iter('message'):
            src_el, tr = msg.find('source'), msg.find('translation')
            if src_el is None or tr is None:
                continue
            src = src_el.text or ''
            if src in es and tr.get('type') == 'unfinished':
                en = english(es[src])
                if en:
                    tr.text = en
                    del tr.attrib['type']
                    done += 1
        print('filled', done)
        if done:
            tree.write(sys.argv[3], encoding='utf-8', xml_declaration=True)


main()
