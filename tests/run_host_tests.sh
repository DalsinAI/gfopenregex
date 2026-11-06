#!/bin/sh
# GFOpenRegex PC tests: the core built for the PC (with AddressSanitizer
# when the compiler has it) against tests/cases.txt (hand-written answers)
# and tests/vectors.txt (answers from Python's re for Perl syntax and the
# C library's regcomp for POSIX syntax, kept where they agree with ours).
# FUZZ=n also runs n new random cases per syntax against those references.
# MIT, Copyright (c) 2026 AmiCradle Team.
set -eu
HERE=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
OUT=${OUT:-"$HERE/build-host"}
CC=${HOST_CC:-cc}
mkdir -p "$OUT"
SAN="-fsanitize=address,undefined"
echo 'int main(void){return 0;}' > "$OUT/san.c"
$CC $SAN -o "$OUT/san" "$OUT/san.c" 2>/dev/null || SAN=
$CC -O1 -g $SAN -Wall -Wextra -o "$OUT/rxtool" "$HERE/tests/rxtool.c" "$HERE"/src/rx_compile.c \
  "$HERE"/src/rx_exec.c "$HERE"/src/rx_unicode.c 2>&1 | grep -v "declared inside parameter list" || true
fail=0
for f in cases vectors; do
    python3 - "$OUT/rxtool" "$HERE/tests/$f.txt" <<'PY' || fail=1
import subprocess, sys
lines = [l.rstrip("\n") for l in open(sys.argv[2]) if l.strip() and not l.startswith("#")]
inp = "".join("\t".join(l.split("\t")[:3]) + "\n" for l in lines)
out = subprocess.run([sys.argv[1]], input=inp.encode(), capture_output=True, check=True).stdout.decode().splitlines()
bad = 0
for l, o in zip(lines, out):
    want = l.split("\t")[3]
    if o != want:
        bad += 1
        if bad <= 20: print("DIFF %r: got %s" % (l, o))
print("%s: %d cases, %d differ" % (sys.argv[2].rsplit("/", 1)[-1], len(lines), bad))
sys.exit(1 if bad else 0)
PY
done
if [ -n "${FUZZ:-}" ]; then
    cc -O1 -DUSE_GLIBC -o "$OUT/rxglibc" "$HERE/tests/rxtool.c" 2>/dev/null
    (cd "$HERE" && for m in perl ere bre; do python3 tests/fuzz.py $m "$FUZZ" "$(date +%s)" extra || true; done)
fi
exit $fail
