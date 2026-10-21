"""Parse the generated C arrays of the Genesis port (src/gen/*.c) into Python data."""
import re, os, pickle
GEN = os.path.expanduser('~/catacomb-abbyss-md/src/gen')

def _strip_comments(t):
    return re.sub(r'/\*.*?\*/', '', t, flags=re.S)

def c_unescape(s):
    out = []
    i = 0
    while i < len(s):
        ch = s[i]
        if ch == '\\':
            i += 1
            e = s[i]
            if e in '01234567':
                j = i
                while j < len(s) and j < i + 3 and s[j] in '01234567':
                    j += 1
                out.append(int(s[i:j], 8)); i = j; continue
            elif e == 'n': out.append(10)
            elif e == 't': out.append(9)
            elif e == 'r': out.append(13)
            elif e == 'x':
                j = i + 1
                while j < len(s) and s[j] in '0123456789abcdefABCDEF': j += 1
                out.append(int(s[i+1:j], 16)); i = j; continue
            else: out.append(ord(e))
            i += 1
        else:
            out.append(ord(ch)); i += 1
    return bytes(out)

def parse_file(fn):
    t = _strip_comments(open(os.path.join(GEN, fn), encoding='latin1').read())
    res = {}
    # numeric arrays
    for m in re.finditer(r'const\s+(\w+)\s+(\w+)\s*\[[^\]]*\](?:\s*__attribute__\(\(\w+\(\d+\)\)\))?\s*=\s*\{', t):
        typ, name = m.group(1), m.group(2)
        start = m.end()
        # find matching close brace
        depth = 1; i = start
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
        body = t[start:i-1]
        res[name] = (typ, body)
    return res

def nums(body):
    return [int(x, 0) for x in re.findall(r'-?(?:0x[0-9A-Fa-f]+|\d+)', body)]

def strings(body):
    """list of C string literals (adjacent literals concatenated) separated by commas, NULL->None"""
    out = []
    i = 0
    cur = None
    toks = re.finditer(r'"((?:[^"\\]|\\.)*)"|NULL|,|\{|\}', body, flags=re.S)
    for m in toks:
        tok = m.group(0)
        if tok.startswith('"'):
            b = c_unescape(m.group(1))
            cur = (cur or b'') + b
        elif tok == 'NULL':
            cur = None; out.append(None); cur = 'sep'
        elif tok == ',' or tok == '}':
            if cur is not None and cur != 'sep':
                out.append(cur)
            cur = None
    return out
