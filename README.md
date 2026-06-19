# DDD-1 MIDI Hub

JUCE standalone app — performance workstation for the Korg DDD-1 drum machine. One half of a two-repo product:

| Repo | Role |
|------|------|
| **ddd1-midi-hub** (this repo) | JUCE app — DDD-1 performance workstation |
| [drum-pattern-database](https://github.com/mlevet/drum-pattern-database) | Pattern corpus + import tools |

---

## What it does

14 independent pads, each configurable as:

| Mode | Description |
|------|-------------|
| **PassThrough** | DDD-1 hits pass through unmodified |
| **Keyboard** | IAC keyboard triggers pad with retrigger, LFO, and snap-to-grid quantization |
| **Arpeggiator** | Clock-synced arpeggio on incoming MIDI notes |
| **Pattern Bank** | Plays a drum pattern from the loaded bank, clock-synced |
| **GroupedTrigs** | One pad hit fans out to multiple pads with velocity/tune/delay offsets |

All modes support an independent **Delay** overlay (clock-synced echoes with musical decay).

**Capture mode**: record live DDD-1 playing into patterns (overdub-safe, suppresses playback during capture to prevent feedback loop).

---

## Pattern database

Load a bank via the Scenes panel → **Load Bank** button. Point it at the `drum-pattern-database/` directory for the full corpus, or at a single `patterns.json` for a specific genre/style.

On first launch, the app auto-discovers `~/PycharmProjects/drum-pattern-database` (and `~/Documents/`, `~/Desktop/` as fallbacks).

---

## Build

Requires JUCE (shared with DDD-1 Bass Translator at `../DDD1BassTranslator/JUCE/`).

```bash
xcodebuild -project build_xcode/DDD1MidiHub.xcodeproj \
           -scheme DDD1MidiHub_Standalone \
           -configuration Release -quiet

cp -R "build_xcode/DDD1MidiHub_artefacts/Release/Standalone/DDD-1 MIDI Hub.app" /Applications/
```

---

## MIDI routing (user's setup)

| Connection | Device |
|-----------|--------|
| DDD-1 MIDI OUT → hub | UM One MIDI IN |
| Hub → DDD-1 MIDI IN | UM One MIDI OUT |
| Keyboard + clock → hub | IAC Bus 1 (from Ableton) |

UM One monitoring in Ableton must be **OFF** to prevent THRU feedback.
