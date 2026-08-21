# Companion to aax-signtool.bat: reduces the argument blob wraptool wrote
# to wraptool-args.tmp down to just the path of the binary being signed.
# From PACE's "Azure Digital Signing" tutorial (adapted from the KoalaDSP
# AAX code-signing guide).
import re

with open("wraptool-args.tmp") as f:
    raw_args = f.read()

match = re.search(r'"?([A-Za-z]:\\.*?)"?$', raw_args)
if not match:
    raise SystemExit("Could not find a binary path in wraptool's signtool arguments")

with open("wraptool-args.tmp", "w") as f:
    f.write(match.group(1))
