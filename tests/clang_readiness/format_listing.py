#!/usr/bin/env python3
"""format_listing.py -- render an Open XL C/C++ "-mzos-listing=json" file
into a readable assembly listing. Independent of IBM's
ibm-clang-listing-formatter so you can run it on Linux/WSL after pulling
the JSON down from z/OS.

JSON structure (ref: IBM Open XL C/C++ 2.2 for z/OS Compiler Reference,
Chapter 14: "Listing file support" / "JSON listing file structure"):

    Top level:
        Compiler, Version, Filename, Timestamp,
        Pseudo Assembly Listing: { Version, Content: [Section, ...] }

    Section.Content    : [Function, ...]
    Function.Content   : [Comment | InlineAsm | Instruction | Label | Value]
    InlineAsm.Content  : [Comment | Value | Instruction]
    Instruction        : { Encoding, OpCode, Operands }      (binary + asm)
    Value              : { Encoding, OpCode='DC', Operands } (binary data)
    Label              : { Name }
    Comment            : { Text }

Run:
    format_listing.py <input.json>
    format_listing.py < input.json
    format_listing.py input.json -o output.txt

Output mirrors the layout of ibm-clang-listing-formatter (Offset / Object
code / Pseudo Assembly columns) but is more compact and lets us add diff-
oriented options later (e.g., strip offsets to make two listings diff-
clean for cross-compiler comparison).
"""

from __future__ import annotations

import argparse
import json
import sys
from typing import Any, TextIO


def emit(out: TextIO, off: int | None, encoding: str, body: str) -> None:
    """Emit one listing line. encoding is the hex byte string from JSON;
    we group it as 2-byte halfwords. `off` may be None for label/comment-
    only lines, in which case the offset column is blank."""
    off_col = f"{off:08x}" if off is not None else " " * 8
    enc_col = " ".join(encoding[i:i+4] for i in range(0, len(encoding), 4)) if encoding else ""
    out.write(f"{off_col}  {enc_col:<20}  {body}\n")


def render_inline_asm(out: TextIO, ia: dict, off: int) -> int:
    """Render an InlineAsm element. Returns updated offset."""
    out.write(f"          > Inline Assembly --- Begin\n")
    for el in ia.get("Content", []):
        kind = el.get("Element")
        if kind == "Comment":
            text = el.get("Comment", {}).get("Text", "").rstrip("\n")
            for line in text.splitlines() or [""]:
                emit(out, None, "", f"* {line}")
        elif kind == "Value" or kind == "Instruction":
            ins = el.get("Instruction", {})
            enc = ins.get("Encoding", "")
            opcode = ins.get("OpCode", "")
            ops = ins.get("Operands", "")
            body = f"{opcode:<8} {ops}" if (opcode or ops) else ""
            emit(out, off, enc, body.rstrip())
            off += len(enc) // 2
        else:
            emit(out, None, "", f"? unknown InlineAsm sub-element: {kind}")
    out.write(f"          > Inline Assembly --- End\n")
    return off


def render_function(out: TextIO, fn: dict) -> None:
    name = fn.get("Name", "?")
    out.write(f"\n          > Begin Function : {name}\n")
    if "DemangledName" in fn:
        out.write(f"          > Demangled Name : {fn['DemangledName']}\n")
    if "Demangled Name" in fn:
        out.write(f"          > Demangled Name : {fn['Demangled Name']}\n")
    out.write(f"                          {name}:\n")

    off = 0
    for el in fn.get("Content", []):
        kind = el.get("Element")
        if kind == "Comment":
            text = el.get("Comment", {}).get("Text", "").rstrip("\n")
            for line in text.splitlines() or [""]:
                emit(out, None, "", f"* {line}")
        elif kind == "Label":
            label = el.get("Label", {}).get("Name", "")
            out.write(f"                          {label}:\n")
        elif kind in ("Instruction", "Value"):
            ins = el.get("Instruction", {})
            enc = ins.get("Encoding", "")
            opcode = ins.get("OpCode", "")
            ops = ins.get("Operands", "")
            body = f"{opcode:<8} {ops}" if (opcode or ops) else ""
            emit(out, off, enc, body.rstrip())
            off += len(enc) // 2
        elif kind == "InlineAsm":
            off = render_inline_asm(out, el, off)
        else:
            emit(out, None, "", f"? unknown Function sub-element: {kind}")
    out.write(f"          > End Function : {name}\n")


def render(doc: dict, out: TextIO) -> None:
    out.write(f"Source File      : {doc.get('Filename', '?')}\n")
    out.write(f"Compilation Time : {doc.get('Timestamp', '?')}\n")
    out.write(f"Compiler         : {doc.get('Compiler', '?')}\n")
    out.write(f"Listing version  : {doc.get('Version', '?')}\n")

    pal = doc.get("Pseudo Assembly Listing", {})
    out.write("\n")
    out.write("Offset    Object code           Pseudo Assembly\n")
    out.write("--------  --------------------  -------------------------------\n")

    sections = pal.get("Content", []) or []
    for sec in sections:
        if sec.get("Element") != "Section":
            continue
        sec_name = sec.get("Name", "?")
        out.write(f"\n========== Section: {sec_name} ==========\n")
        for fn in sec.get("Content", []):
            if fn.get("Element") == "Function":
                render_function(out, fn)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("input", nargs="?",
                    help="JSON listing file (or stdin if omitted)")
    ap.add_argument("-o", "--output",
                    help="output file (default: stdout)")
    args = ap.parse_args()

    src: TextIO
    if args.input and args.input != "-":
        src = open(args.input, "r", encoding="utf-8")
    else:
        src = sys.stdin

    try:
        doc = json.load(src)
    except json.JSONDecodeError as e:
        print(f"format_listing: not valid JSON: {e}", file=sys.stderr)
        return 2
    finally:
        if src is not sys.stdin:
            src.close()

    out: TextIO
    if args.output:
        out = open(args.output, "w", encoding="utf-8")
    else:
        out = sys.stdout
    try:
        render(doc, out)
    finally:
        if out is not sys.stdout:
            out.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
