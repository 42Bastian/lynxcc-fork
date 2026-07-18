#!/usr/bin/env python3
# SPDX-License-Identifier: MPL-2.0
#
# This Source Code Form is subject to the terms of the Mozilla Public License,
# v. 2.0. If a copy of the MPL was not distributed with this file, You can
# obtain one at https://mozilla.org/MPL/2.0/.
#
# Part of the Lynx Game Development SDK (lynxcc). See doc/licenses.html.

"""
Upstream fix mining (design/LYNX_COMPILER_AUDIT_DESIGN.md sec. T6).

Enumerates upstream cc65 commits after the V2.19 tag that touch compiler
files this fork still shares (compiler/cc65/*.c came from src/cc65/*.c)
and whose messages indicate a correctness fix. Emits a candidate table to
stdout; the curated classification lives in FIXES.md next to this script.

The upstream history is NOT vendored. Point the script at a local clone:

    git clone --filter=blob:none --no-checkout --single-branch \
        https://github.com/cc65/cc65 /tmp/cc65-upstream
    (cd /tmp/cc65-upstream && git fetch --filter=blob:none origin tag V2.19)
    python3 mine.py /tmp/cc65-upstream

Filtering is deliberately recall-biased: everything the keyword net
catches lands in the table, and classification (applies / doesn't apply /
already diverged) is a human step recorded in FIXES.md. Codegen-
correctness files are weighted; scanner/preproc/driver-only fixes are
listed but flagged LOW so effort goes where miscompiles live.
"""

import os
import subprocess
import sys

# Fork files that shipped from src/cc65 (see compiler/cc65/). Fixes that
# touch only files outside this set cannot apply to us.
SHARED = set("""
anonname.c asmcode.c asmlabel.c asmstmt.c assignment.c casenode.c codeent.c
codegen.c codeinfo.c codelab.c codeopt.c codeseg.c compile.c coptadd.c
coptc02.c coptcmp.c coptind.c coptneg.c coptptrload.c coptptrstore.c
coptpush.c coptshift.c coptsize.c coptstop.c coptstore.c coptsub.c copttest.c
dataseg.c datatype.c declare.c declattr.c error.c expr.c exprdesc.c
funcdesc.c function.c global.c goto.c hexval.c ident.c incpath.c input.c
lineinfo.c litpool.c loadexpr.c locals.c loop.c macrotab.c main.c opcodes.c
output.c pragma.c preproc.c reginfo.c scanner.c scanstrbuf.c segments.c
shiftexpr.c stackptr.c stdfunc.c stdnames.c stmt.c swstmt.c symentry.c
symtab.c testexpr.c typecmp.c typeconv.c util.c wrappedcall.c textseg.c
""".split())

# Where wrong-code bugs live; a fix touching one of these is HIGH priority.
CODEGEN = set("""
assignment.c codeent.c codegen.c codeinfo.c codelab.c codeopt.c codeseg.c
coptadd.c coptc02.c coptcmp.c coptind.c coptneg.c coptptrload.c
coptptrstore.c coptpush.c coptshift.c coptsize.c coptstop.c coptstore.c
coptsub.c copttest.c expr.c exprdesc.c loadexpr.c locals.c loop.c
shiftexpr.c stackptr.c stdfunc.c testexpr.c typeconv.c reginfo.c
""".split())

GREP = ("fix", "wrong", "incorrect", "bug", "bogus", "miscompil",
        "invalid", "regression", "clobber")


def git(repo, *args):
    return subprocess.run(["git", "-C", repo] + list(args),
                          capture_output=True, text=True, check=True).stdout


def main(argv):
    if len(argv) != 1:
        print(__doc__)
        return 2
    repo = argv[0]
    log = git(repo, "log", "--format=%H|%ad|%s", "--date=short",
              "V2.19..HEAD", "--", "src/cc65")
    rows = []
    for line in log.splitlines():
        sha, date, subj = line.split("|", 2)
        low = subj.lower()
        if not any(k in low for k in GREP):
            continue
        files = [os.path.basename(f) for f in
                 git(repo, "show", "--name-only", "--format=", sha).split()
                 if f.startswith("src/cc65/")]
        shared = sorted(set(files) & SHARED)
        if not shared:
            continue
        prio = "HIGH" if set(shared) & CODEGEN else "LOW"
        rows.append((date, prio, sha[:10], subj, ",".join(shared)))

    rows.sort(reverse=True)
    print("%d candidate fix commits (V2.19..HEAD touching shared files)"
          % len(rows))
    for date, prio, sha, subj, files in rows:
        print("%s %-4s %s %s\n           [%s]" % (date, prio, sha, subj[:110],
                                                  files))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
