"""Companion to aax-signtool.bat (from PACE's "Azure Digital Signing"
tutorial, adapted from the KoalaDSP guide).

wraptool hands the wrapper its legacy signtool argument list; the last
argument is the path of the binary to sign. Reduce the temp file written by
the .bat to just that path so the .bat can re-invoke the real signtool with
the Azure Artifact Signing arguments.

The temp file lives next to this script (the .bat writes it with %~dp0),
NOT in the current working directory - wraptool invokes the wrapper from
elsewhere, so resolve the path relative to this file.
"""
import os
import re

args_file = os.path.join(os.path.dirname(os.path.abspath(__file__)), "wraptool-args.tmp")

with open(args_file) as f:
    raw_args = f.read()

match = re.search(r'"?([A-Za-z]:\\.*?)"?\s*$', raw_args)
if not match:
    raise SystemExit("Could not find a binary path in wraptool's signtool arguments: " + raw_args)

with open(args_file, "w") as f:
    f.write(match.group(1))
