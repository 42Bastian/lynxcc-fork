#!/usr/bin/env python3
# SPDX-License-Identifier: CC-BY-4.0
# Lynx Game Development SDK documentation, (c) 2026 the lynxcc authors,
# licensed under Creative Commons Attribution 4.0 International.
#
# Harvest every anchored section heading (<h2>/<h3>/<h4 id="...">) from the
# doc/*.html pages and splice a compact SEARCH_INDEX array into the fenced
# region of doc.js.  Only the region between the two marker comments is
# rewritten; the hand-written code around it is left untouched.
#
# Run via `make doc-search-index`.  `make doc-search-index-check` runs this in
# --check mode and fails if doc.js would change (i.e. the index is stale).
#
# See design/DOC_SEARCH_DESIGN.md.

import glob
import html
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
DOCJS = os.path.join(HERE, "doc.js")
BEGIN = "// === SEARCH_INDEX"
END = "// === END SEARCH_INDEX ==="

HEADING_RE = re.compile(r'<h[234]\s+id="([^"]+)"[^>]*>(.*?)</h[234]>', re.S)
TAG_RE = re.compile(r"<[^>]+>")
WS_RE = re.compile(r"\s+")


def clean(text):
    """Strip inner tags, unescape entities, collapse whitespace."""
    text = TAG_RE.sub("", text)
    text = html.unescape(text)
    return WS_RE.sub(" ", text).strip()


def harvest():
    rows = []
    for path in sorted(glob.glob(os.path.join(HERE, "*.html"))):
        page = os.path.basename(path)
        with open(path, encoding="utf-8") as fh:
            src = fh.read()
        for hid, inner in HEADING_RE.findall(src):
            label = clean(inner)
            if label:
                rows.append((page, hid, label))
    return rows


def js_str(s):
    return '"' + s.replace("\\", "\\\\").replace('"', '\\"') + '"'


def render(rows):
    lines = ["var SEARCH_INDEX = ["]
    for page, hid, label in rows:
        lines.append(
            "  [%s,%s,%s]," % (js_str(page), js_str(hid), js_str(label))
        )
    lines.append("];")
    return "\n".join(lines)


def splice(doc, block):
    b = doc.index(BEGIN)
    # keep the whole BEGIN marker line
    b_line_end = doc.index("\n", b) + 1
    e = doc.index(END, b_line_end)
    e_line_end = doc.index("\n", e) + 1
    return doc[:b_line_end] + block + "\n" + doc[e:e_line_end] + doc[e_line_end:]


def main():
    check = "--check" in sys.argv
    rows = harvest()
    block = render(rows)
    with open(DOCJS, encoding="utf-8") as fh:
        doc = fh.read()
    if BEGIN not in doc or END not in doc:
        sys.exit("error: SEARCH_INDEX markers not found in doc.js")
    updated = splice(doc, block)
    if check:
        if updated != doc:
            sys.exit(
                "error: doc.js SEARCH_INDEX is stale; run `make doc-search-index`"
            )
        print("doc-search-index: up to date (%d headings)" % len(rows))
        return
    if updated != doc:
        with open(DOCJS, "w", encoding="utf-8") as fh:
            fh.write(updated)
        print("doc-search-index: wrote %d headings" % len(rows))
    else:
        print("doc-search-index: unchanged (%d headings)" % len(rows))


if __name__ == "__main__":
    main()
