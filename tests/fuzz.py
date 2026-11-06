#!/usr/bin/env python3
"""Differential test: random patterns and subjects through rxtool and a
reference: Python's re (Perl syntax, leftmost-first, all groups) or the C
library's regcomp (POSIX ERE/BRE, leftmost-longest, whole match).
Usage: fuzz.py perl|ere|bre COUNT SEED. MIT, (c) 2026 AmiCradle Team."""
import random, re, subprocess, sys

mode, count, seed = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
extra = len(sys.argv) > 4 and sys.argv[4] == "extra"
rnd = random.Random(seed)
ALPHA = "abc"

def atom(d):
    r = rnd.random()
    if d > 3 or r < 0.45:
        more = []
        if extra:
            more = ["A", "B", "\n", "[A-Z]", "[^b-c]"]
            if mode == "perl":
                more += ["\\1", "(?=a)", "(?!b)", "\\B", "\\W", "[\\d\\s]", "(?i:a)", "\\A"]
            else:
                more += ["[[:alpha:]]", "[^[:space:]]"] + (["\\1"] if rnd.random() < 0.3 else [])
        return rnd.choice(list(ALPHA) + [".", "[ab]", "[^a]", "[a-c]"] + more +
                          (["\\w", "\\d", "\\s", "\\b"] if mode == "perl" else []))
    if r < 0.75:
        return group(d + 1)
    return atom(d + 1) + atom(d + 1)

def group(d):
    alts = [seq(d) for _ in range(rnd.randint(1, 3))]
    if mode == "bre":
        return "\\(" + "\\|".join(alts) + "\\)"
    if mode == "perl" and rnd.random() < 0.2:
        return "(?:" + "|".join(alts) + ")"
    return "(" + "|".join(alts) + ")"

def quant():
    q = rnd.choice(["", "", "", "*", "+", "?", "{2}", "{1,2}", "{0,1}", "{2,}"])
    if mode == "bre":
        q = {"+": "\\+", "?": "\\?", "{2}": "\\{2\\}", "{1,2}": "\\{1,2\\}",
             "{0,1}": "\\{0,1\\}", "{2,}": "\\{2,\\}"}.get(q, q)
    if mode == "perl" and q and rnd.random() < 0.3:
        q += "?"
    return q

def seq(d):
    s = "".join(atom(d) + quant() for _ in range(rnd.randint(1, 3)))
    return s

def pattern():
    p = seq(0)
    if rnd.random() < 0.15: p = "^" + p
    if rnd.random() < 0.15: p = p + "$"
    if mode != "bre" and rnd.random() < 0.3:
        p = p + "|" + seq(0)
    return p

def subject():
    return "".join(rnd.choice(ALPHA + " 1" + ("AB\n" if extra else "")) for _ in range(rnd.randint(0, 12)))

def esc(s):
    return s.replace("\\", "\\\\").replace("\t", "\\t").replace("\n", "\\n")

cases = [(pattern(), subject()) for _ in range(count)]
flag = {"perl": "P", "ere": "E", "bre": ""}[mode]
flags = []
for _ in cases:
    f = flag
    if extra:
        if rnd.random() < 0.3: f += "I"
        if mode == "perl":
            if rnd.random() < 0.3: f += "M"
            if rnd.random() < 0.3: f += "D"
        elif rnd.random() < 0.3: f += "N"
    if len(sys.argv) > 5: f += sys.argv[5]
    flags.append(f)
inp = "".join("%s\t%s\t%s\n" % (f, esc(p), esc(s)) for f, (p, s) in zip(flags, cases))
ours = subprocess.run(["build-host/rxtool"], input=inp.encode(), capture_output=True).stdout.decode().splitlines()
if mode == "perl":
    # Python's re in a child process, one case at a time, 2 s each (its
    # backtracking can run for ever on some back-reference cases)
    import json, select
    CHILD = r"""
import json, re, sys
for line in sys.stdin:
    f, p, s = json.loads(line)
    rf = (re.I if "I" in f else 0) | (re.M if "M" in f else 0) | (re.S if "D" in f else 0)
    try:
        m = re.search(p.encode("latin-1"), s.encode("latin-1"), rf)
    except re.error:
        print("ERR", flush=True); continue
    if not m: print("NOMATCH", flush=True); continue
    n = min(m.re.groups, 9)
    print(" ".join("%d,%d" % m.span(i) for i in range(n + 1)), flush=True)
"""
    def start():
        return subprocess.Popen([sys.executable, "-W", "ignore", "-c", CHILD], stdin=subprocess.PIPE,
                                stdout=subprocess.PIPE, text=True)
    child = start()
    ref = []
    for f, (p, s) in zip(flags, cases):
        child.stdin.write(json.dumps([f, p, s]) + "\n"); child.stdin.flush()
        r, _, _ = select.select([child.stdout], [], [], 2.0)
        if r:
            ref.append(child.stdout.readline().strip())
        else:
            child.kill(); child = start(); ref.append("TIMEOUT")
    child.kill()
else:
    ref = subprocess.run(["build-host/rxglibc"], input=inp.encode(), capture_output=True).stdout.decode().splitlines()

import os
vec = open(os.environ["VECTORS"], "a") if os.environ.get("VECTORS") else None
bad = sub = 0
for f, (p, s), o, r in zip(flags, cases, ours, ref):
    if vec and o == r and r != "TIMEOUT" and len(p) <= 120:
        vec.write("%s\t%s\t%s\t%s\n" % (f, esc(p), esc(s), r))
    if r == "TIMEOUT":
        continue
    if r.startswith("ERR") and o.startswith("ERR"):
        continue
    if r == "ERR 6" and mode != "perl":  # glibc refuses some back-references we take
        continue
        continue
    if mode == "perl":
        same = o == r
    else:
        same = o.split(" ")[0] == r.split(" ")[0]
        if same and o != r: sub += 1
    if not same:
        bad += 1
        if bad <= 15:
            print("DIFF %r on %r: ours %s, ref %s" % (p, s, o, r), flags[cases.index((p, s))])
print("%s: %d cases, %d differ%s" % (mode, len(cases), bad,
      ", %d differ only in groups" % sub if mode != "perl" else ""))
sys.exit(1 if bad else 0)
