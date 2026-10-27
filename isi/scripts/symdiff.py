"""Compare two firmware ELFs by symbol: what changed size, what was added or removed.

usage: python3 -I isi/scripts/symdiff.py REF.elf NEW.elf
Needs arm-none-eabi-nm on PATH. Only names unique in both ELFs are compared (static
duplicates such as allowed_args.0 are skipped). Same-size symbols that merely moved are
not listed: addresses shift whenever anything before them changes size.
"""
import collections
import subprocess
import sys


def symbols(elf):
    out = subprocess.check_output(["arm-none-eabi-nm", "-S", "--defined-only", elf], text=True)
    seen = collections.defaultdict(list)
    for line in out.splitlines():
        p = line.split()
        if len(p) == 4:
            seen[p[3]].append((int(p[1], 16), p[2]))
    return {name: v[0] for name, v in seen.items() if len(v) == 1}


ref, new = symbols(sys.argv[1]), symbols(sys.argv[2])
common = set(ref) & set(new)
resized = sorted((n, ref[n][0], new[n][0]) for n in common if ref[n][0] != new[n][0])
added = sorted(set(new) - set(ref))
removed = sorted(set(ref) - set(new))

print(f"unique symbols: {len(common)} common, {len(added)} added, {len(removed)} removed, "
      f"{len(resized)} resized")
for name, a, b in resized:
    print(f"  resized  {name}: {a} -> {b} ({b - a:+d})")
for name in added:
    print(f"  added    {name} ({new[name][0]} B)")
for name in removed:
    print(f"  removed  {name} ({ref[name][0]} B)")
