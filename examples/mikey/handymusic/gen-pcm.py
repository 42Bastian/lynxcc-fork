#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
#
# Lynx Game Development SDK example/template, (c) 2026 the lynxcc authors.
# Provided under the MIT License; copy it into your own projects freely.
# See the LICENSE file in the examples directory.
#
# Generate pcmsamples.h for the HandyMusic example: two short 8 kHz signed
# 8-bit one-shot blips fed to channel 0's direct-output register ($FD22) by the
# driver's timer-3 PCM IRQ (design/LYNX_HANDYMUSIC_DESIGN.md sec. 4.4).  The
# output is committed so the example builds with no Python dependency; rerun
# this only to change the samples.
#
#   python3 gen-pcm.py > pcmsamples.h

import math
import sys

SR = 8000


def emit(name, samples):
    out = [f"static const unsigned char {name}[] = {{"]
    line = "    "
    for v in samples:
        tok = f"0x{v & 0xFF:02X},"
        if len(line) + len(tok) > 76:
            out.append(line.rstrip())
            line = "    "
        line += tok
    out.append(line.rstrip())
    out.append("};")
    out.append(f"#define {name}_len ((unsigned){len(samples)})")
    return "\n".join(out)


def laser(n=900, f0=900.0, f1=300.0):
    s, phase = [], 0.0
    for i in range(n):
        t = i / n
        phase += (f0 + (f1 - f0) * t) / SR
        amp = int(60 * (1.0 - t))
        s.append(amp if (phase % 1.0) < 0.5 else -amp)
    return s


def blip(n=500, f=1400.0):
    s, phase = [], 0.0
    for i in range(n):
        t = i / n
        phase += f / SR
        amp = int(55 * math.exp(-3.0 * t))
        s.append(amp if (phase % 1.0) < 0.5 else -amp)
    return s


def main():
    out = [
        "/* Generated PCM sample data for the HandyMusic example (8 kHz, signed 8-bit).",
        "** Two short one-shot blips fed to channel 0's direct-output register.",
        "** Regenerate with the gen-pcm.py helper beside this file. */",
        "#ifndef HM_PCM_SAMPLES_H",
        "#define HM_PCM_SAMPLES_H",
        "",
        emit("pcm_laser", laser()),
        "",
        emit("pcm_blip", blip()),
        "",
        "#endif",
    ]
    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
