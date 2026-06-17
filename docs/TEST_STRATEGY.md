# Testing GridSeq in a delivery cycle

A short note on how I would test an instrument like this inside an agile QA
pipeline: what I would automate, what stays human, and where each layer slots in.

## The testing pyramid for an instrument

| Layer | What it covers | Cost / speed | In this repo |
|---|---|---|---|
| **Unit** | Pure logic: timing maths, swing, pattern grid, state round-trip, the lock-free queue's semantics | Milliseconds; run on every save | [`tests/`](../tests) - Catch2, 29 cases, links only the engine |
| **Rendered-output regression** | The *real* engine's audio: do hits land on the expected sample grid? | ~1s; run on every push | [`tools/qa_render_check.py`](../tools/qa_render_check.py) driving [`gridseq_render`](../tools/render_cli.cpp) |
| **Plugin validation** | Does the built VST3/AU behave (params, state, threading, editor open/close, bus layouts)? | Seconds-minutes; run in CI | `pluginval` strictness 5 - wired into the Windows CI job |
| **Acceptance / feel** | Does it *sound* and *feel* right? Groove, swing, UI ergonomics | Human; per release | Manual - and this is where a player's ear earns its place |

The design rule that makes the bottom two layers cheap is the **engine/UI split**:
because the engine has no JUCE dependency, the fast layers need no audio device,
no plugin host and no GUI. The slow, host-dependent checks are reserved for the
things that genuinely require a host.

## What I automate vs. what stays human

- **Automate everything deterministic.** Timing, quantisation, swing, state
  persistence and the audio output's sample-accuracy are all deterministic, so
  they belong in CI as a gate. The render-and-analyse check means a one-sample
  timing regression fails a build instead of waiting to be noticed in QA.
- **Keep judgement human.** Whether the kit *sounds* good, whether the swing
  *feels* right at 90 vs 140 BPM, whether the grid is comfortable under your hands
  during a session - these are acceptance calls. As a working player I can do that
  acceptance pass credibly, which a pure software tester often can't.

## How it slots into an agile cycle

- **Per commit (local + CI):** unit suite + render regression. Fast enough to be a
  pre-merge gate; warnings are errors.
- **Per push / PR:** the full 3-OS matrix builds the plugin on MSVC/Xcode/GCC. A
  PR can't merge if any platform breaks - this already caught a Linux/macOS-only
  configure bug Windows had hidden.
- **Per release candidate:** plugin validation (pluginval) + a human acceptance
  pass on feel and sound, plus a quick DAW smoke test in a couple of hosts.
- **Each story is a vertical slice.** I built this the way I'd run a sprint
  ticket: write the spec as a failing test, implement to green, commit the concept.
  The git log is essentially a burned-down backlog.

## Next things I'd add (named, scoped, not yet built)

1. **Raise `pluginval` strictness to 10** and extend it to the macOS/Linux legs
   (Linux needs `xvfb` for the editor tests). It already runs at level 5 on
   Windows CI; level 10 adds more aggressive threading/timing checks.
2. **An audio-thread allocation/lock assertion** in debug builds, to fail loudly
   if anything ever allocates or locks inside `processBlock`.
3. **Golden-file audio diffing** - hash a rendered reference and diff on change,
   so any unintended DSP change is flagged for a human to approve.
4. **Fuzzing the state codec** with malformed blobs to harden `deserialize`.
```
