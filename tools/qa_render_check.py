#!/usr/bin/env python3
"""
qa_render_check.py - offline render-and-analyse regression check for GridSeq.

WHY this exists: most plugin projects stop at unit tests. This script closes the
loop at the *rendered-audio* level. It drives the real engine through the
gridseq_render CLI, reads back the WAV, finds where every hit actually landed,
and asserts that those onsets fall on the mathematically-expected 16th-note grid
(with swing). If a future change shifts timing by even one sample, this fails, so
a timing regression is caught automatically before it ever reaches QA.

Deliberately uses only the Python standard library (wave, struct) so it runs
anywhere, including CI, with no pip install.

Exit code 0 = all checks passed; 1 = a regression was detected.
"""

import argparse
import os
import struct
import subprocess
import sys
import tempfile
import wave


def find_render_binary() -> str:
    """Locate the gridseq_render executable produced by CMake."""
    here = os.path.dirname(os.path.abspath(__file__))
    root = os.path.dirname(here)
    names = ("gridseq_render", "gridseq_render.exe")
    candidates = []
    for base in ("build", "build-plugin", "out"):
        for cfg in ("", "Release", "Debug"):
            for name in names:
                candidates.append(os.path.join(root, base, "bin", cfg, name))
                candidates.append(os.path.join(root, base, "bin", name))
    for c in candidates:
        if os.path.isfile(c):
            return c
    sys.exit("error: gridseq_render not found - build the project first "
             "(cmake -B build && cmake --build build).")


def read_wav_mono(path: str):
    """Return (samples [-1,1], sample_rate) from a 16-bit mono WAV."""
    with wave.open(path, "rb") as w:
        assert w.getnchannels() == 1, "expected mono"
        assert w.getsampwidth() == 2, "expected 16-bit"
        sr = w.getframerate()
        raw = w.readframes(w.getnframes())
    ints = struct.unpack("<" + "h" * (len(raw) // 2), raw)
    return [s / 32768.0 for s in ints], sr


def detect_onsets(samples, threshold=0.5):
    """Rising-edge detector: indices where the signal crosses `threshold`.

    In impulse mode each hit is a single non-zero sample, so this returns the
    exact onset positions.
    """
    onsets = []
    prev = 0.0
    for i, s in enumerate(samples):
        if abs(s) >= threshold > abs(prev):
            onsets.append(i)
        prev = s
    return onsets


def expected_onsets(cells_steps, sr, bpm, steps, bars, swing):
    """Compute where each programmed step *should* fire, in samples.

    Mirrors the engine's timing model: straight grid = step * samplesPerStep,
    odd (off-beat) steps pushed later by swing * samplesPerStep.
    """
    sps = (60.0 / bpm) / 4.0 * sr
    out = []
    for bar in range(bars):
        for step in sorted(set(cells_steps)):
            abs_index = bar * steps + step
            swing_off = swing * sps if (abs_index % 2) == 1 else 0.0
            out.append(round(abs_index * sps + swing_off))
    return sorted(out)


def main() -> int:
    ap = argparse.ArgumentParser(description="GridSeq offline render regression check")
    ap.add_argument("--bin", help="path to gridseq_render (auto-detected if omitted)")
    ap.add_argument("--sr", type=int, default=48000)
    ap.add_argument("--bpm", type=float, default=120.0)
    ap.add_argument("--swing", type=float, default=0.0)
    ap.add_argument("--steps", type=int, default=16)
    ap.add_argument("--bars", type=int, default=2)
    ap.add_argument("--tolerance", type=int, default=1, help="allowed onset error in samples")
    args = ap.parse_args()

    render_bin = args.bin or find_render_binary()

    # A recognisable on-track pattern: kick on the four downbeats.
    track = 0
    pattern_steps = [0, 4, 8, 12]

    with tempfile.TemporaryDirectory() as tmp:
        wav_path = os.path.join(tmp, "render.wav")
        cmd = [render_bin, "--out", wav_path, "--sr", str(args.sr),
               "--bpm", str(args.bpm), "--swing", str(args.swing),
               "--steps", str(args.steps), "--bars", str(args.bars)]
        for s in pattern_steps:
            cmd += ["--cell", f"{track}:{s}"]

        print("render:", " ".join(cmd))
        subprocess.run(cmd, check=True, capture_output=True, text=True)

        samples, sr = read_wav_mono(wav_path)

    got = detect_onsets(samples)
    want = expected_onsets(pattern_steps, sr, args.bpm, args.steps, args.bars, args.swing)

    print(f"\nexpected {len(want)} hits, detected {len(got)}")
    ok = len(got) == len(want)
    for i, exp in enumerate(want):
        actual = got[i] if i < len(got) else None
        err = abs(actual - exp) if actual is not None else None
        good = err is not None and err <= args.tolerance
        ok = ok and good
        mark = "ok " if good else "FAIL"
        print(f"  [{mark}] hit {i:2d}: expected {exp:>8} got "
              f"{actual if actual is not None else '----':>8}  "
              f"(err {err if err is not None else '?'} samples)")

    print("\nRESULT:", "PASS - timing is sample-accurate" if ok
          else "FAIL - timing regression detected")
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
