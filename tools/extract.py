"""Extract all generated Genesis data into data.pkl (pure Python objects)."""
import re, os, pickle, sys
sys.path.insert(0, os.path.dirname(__file__))
import cparse
from cparse import nums, strings
GEN = cparse.GEN

def parse_all(fn):
    t = cparse._strip_comments(open(os.path.join(GEN, fn), encoding='latin1').read())
    res = {}
    for m in re.finditer(r'(?:static\s+)?const\s+([\w\s\*]+?)\s*\b(\w+)\s*((?:\[[^\]]*\])+)(?:\s*__attribute__\(\(\w+\(\d+\)\)\))?\s*=\s*\{', t):
        typ, name = m.group(1).strip(), m.group(2)
        start = m.end(); depth = 1; i = start
        while depth:
            c = t[i]
            if c == '{': depth += 1
            elif c == '}': depth -= 1
            elif c == '"':
                i += 1
                while t[i] != '"':
                    if t[i] == '\\': i += 1
                    i += 1
            i += 1
        res[name] = (typ, t[start:i-1])
    return res

D = {}
for fn in ['gfx.c', 'maps.c', 'texts.c', 'tables.c', 'scaletab.c', 'sounds.c', 'images.c']:
    r = parse_all(fn)
    for name, (typ, body) in r.items():
        if 'char' in typ:
            D[name] = strings(body)
        elif name in ('pic_table', 'map_defs', 'catalog_pages', 'snd_al_data'):
            D[name] = body
        else:
            D[name] = nums(body)
        print(fn, name, typ, len(D[name]) if not isinstance(D[name], str) else 'str')
pickle.dump(D, open(os.path.join(os.path.dirname(__file__), 'data.pkl'), 'wb'))
