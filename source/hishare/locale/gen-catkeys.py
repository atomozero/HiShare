#!/bin/python3
# Generates Haiku Locale Kit catkeys/catalogs for BeShare from its built-in
# per-language string tables in ShareStrings.cpp.  BeShare's str() bridges to
# the Locale Kit (keyed by the English text, context "ShareStrings"), so these
# catalogs let the app follow the Haiku system language via standard tooling.
#
# Pipeline: parse the _<lang>Strings[] arrays -> emit a B_TRANSLATE source with
# every (non-\001) English string -> collectcatkeys to get the canonical catkeys
# (correct fingerprint) -> for each language rewrite the translation column ->
# linkcatkeys to a <code>.catalog.  The fingerprint depends only on the keys, so
# all languages share the English catkeys' fingerprint.
import os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
BESHARE = os.path.dirname(HERE)
SRC = os.path.join(BESHARE, "ShareStrings.cpp")
SIG = "application/x-vnd.HiShare"
CONTEXT = "ShareStrings"
OUT = os.path.join(HERE, "catalogs")

# BeShare language array -> Haiku catalog language code.
LANGS = [
    ("_englishStrings","en","English"), ("_spanishStrings","es","Spanish"),
    ("_germanStrings","de","German"), ("_dutchStrings","nl","Dutch"),
    ("_portugueseStrings","pt","Portuguese"), ("_frenchStrings","fr","French"),
    ("_italianStrings","it","Italian"), ("_russianStrings","ru","Russian"),
    ("_swedishStrings","sv","Swedish"), ("_esperantoStrings","eo","Esperanto"),
    ("_norwegianStrings","nb","Norwegian"), ("_serbianStrings","sr","Serbian"),
    ("_bulgarianStrings","bg","Bulgarian"), ("_koreanStrings","ko","Korean"),
    ("_hungarianStrings","hu","Hungarian"), ("_romanianStrings","ro","Romanian"),
    ("_schineseStrings","zh_Hans","Chinese"), ("_turkishStrings","tr","Turkish"),
    ("_japaneseStrings","ja","Japanese"), ("_finnishStrings","fi","Finnish"),
]

def unescape_c(lit):
    """Turn a C string literal body (without surrounding quotes) into its bytes-as-str."""
    out, i = [], 0
    while i < len(lit):
        c = lit[i]
        if c != '\\':
            out.append(c); i += 1; continue
        i += 1
        if i >= len(lit): break
        e = lit[i]
        simple = {'n':'\n','t':'\t','r':'\r','\\':'\\','"':'"',"'":"'",'0':'\0','a':'\a','b':'\b','f':'\f','v':'\v'}
        if e in ('0','1','2','3','4','5','6','7'):
            oct_ = ''
            while i < len(lit) and lit[i] in '01234567' and len(oct_) < 3:
                oct_ += lit[i]; i += 1
            out.append(chr(int(oct_,8))); continue
        if e == 'x':
            i += 1; hx=''
            while i < len(lit) and lit[i] in '0123456789abcdefABCDEF':
                hx += lit[i]; i += 1
            out.append(chr(int(hx,16))); continue
        out.append(simple.get(e,e)); i += 1
    return ''.join(out)

def extract_array(src, name):
    m = re.search(r'%s\s*\[NUM_STRINGS\]\s*=\s*\{(.*?)\n\};' % re.escape(name), src, re.S)
    if not m: return None
    body = m.group(1)
    # one string literal per entry line; grab each "..."(with escaped quotes) literal
    lits = re.findall(r'"((?:\\.|[^"\\])*)"', body)
    return [unescape_c(l) for l in lits]

def catkeys_escape(s):
    return s.replace('\\','\\\\').replace('\t','\\t').replace('\n','\\n').replace('\r','\\r')

def main():
    src = open(SRC, encoding='utf-8', errors='replace').read()
    arrays = {}
    for arr,_,_ in LANGS:
        a = extract_array(src, arr)
        if a is None: sys.exit("array %s not found" % arr)
        arrays[arr] = a
    english = arrays["_englishStrings"]
    n = len(english)
    print("parsed %d strings x %d languages" % (n, len(LANGS)))

    # indices that are real UI strings (skip \001-prefixed column keys and empties)
    ui_idx = [i for i in range(n) if english[i] and english[i][0] != '\001']
    print("  %d translatable UI strings (%d skipped as column keys/empty)" % (len(ui_idx), n-len(ui_idx)))

    os.makedirs(OUT, exist_ok=True)

    # 1) emit a B_TRANSLATE source and collectcatkeys -> canonical en.catkeys (real fingerprint)
    gensrc = os.path.join(HERE, "_catkeys_src.cpp")
    with open(gensrc,"w",encoding='utf-8') as f:
        f.write('#include <Catalog.h>\n#undef B_TRANSLATION_CONTEXT\n#define B_TRANSLATION_CONTEXT "%s"\n' % CONTEXT)
        f.write('const char* _beshare_catkeys() { return\n')
        seen=set()
        for i in ui_idx:
            e = english[i]
            if e in seen: continue
            seen.add(e)
            cesc = e.replace('\\','\\\\').replace('"','\\"').replace('\n','\\n').replace('\t','\\t').replace('\r','\\r')
            f.write('    B_TRANSLATE("%s")\n' % cesc)
        f.write('    ; }\n')
    pre = os.path.join(HERE,"_catkeys_src.i")
    subprocess.run("gcc -E -P -DB_COLLECTING_CATKEYS -I/boot/system/develop/headers/os/locale %s -o %s" % (gensrc,pre), shell=True, check=True)
    en_ck = os.path.join(OUT,"en.catkeys")
    subprocess.run("collectcatkeys -s '%s' -o '%s' '%s'" % (SIG,en_ck,pre), shell=True, check=True)
    header = open(en_ck,encoding='utf-8').readline().rstrip('\n')
    fields = header.split('\t')
    fingerprint = fields[3]
    print("canonical fingerprint =", fingerprint)

    # parse en.catkeys entries -> ordered list of (escaped_source, context, comment)
    entries=[]
    for line in open(en_ck,encoding='utf-8').read().split('\n')[1:]:
        if not line: continue
        parts = line.split('\t')
        if len(parts) >= 4: entries.append(parts[:3])   # source, context, comment
    print("  %d unique catalog keys" % len(entries))

    # english(unescaped) -> translation, per language (first index wins on dup english)
    def build_map(arr):
        m={}
        for i in ui_idx:
            e=english[i]
            if e not in m: m[e]=arrays[arr][i] if i < len(arrays[arr]) else e
        return m

    made=[]
    for arr,code,langname in LANGS:
        tmap = build_map(arr)
        ck = os.path.join(OUT, code+".catkeys")
        with open(ck,"w",encoding='utf-8') as f:
            f.write("1\t%s\t%s\t%s\n" % (langname, SIG, fingerprint))
            for (esc_src,ctx,com) in entries:
                eng = unescape_c(esc_src)   # catkeys escapes are a subset of C escapes we handle
                tr = tmap.get(eng, eng)
                f.write("%s\t%s\t%s\t%s\n" % (esc_src, ctx, com, catkeys_escape(tr)))
        cat = os.path.join(OUT, code+".catalog")
        subprocess.run("linkcatkeys -o '%s' -s '%s' -l %s '%s'" % (cat,SIG,langname,ck), shell=True, check=True)
        made.append(cat)
    print("generated %d catalogs in %s" % (len(made), OUT))
    print("english catalog (embed into the app):", os.path.join(OUT,"en.catalog"))

if __name__ == "__main__":
    main()
