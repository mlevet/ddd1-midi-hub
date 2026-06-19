---
name: project-ddd1-midi-hub
description: "DDD-1 MIDI Hub — current state, architecture, bugs fixed, Pattern mode editor, MIDI library imported, git repo created"
metadata: 
  node_type: memory
  type: project
  originSessionId: e5d32793-9250-43c1-9d6a-b1ad36817946
---

# DDD-1 MIDI Hub

JUCE Standalone app. 14-pad hub for Korg DDD-1 with per-pad modes: PassThrough / Keyboard / Arpeggiator / Pattern / GroupedTrigs. Delay is a per-pad overlay effect (`delayEnabled` toggle) available on all modes.

**Why:** [[project-ddd1-bass-translator]] handled one instrument at a time. Hub handles all 14 DDD-1 pads independently.

**How to apply:** When continuing, read current source files first — memory decays. Prioritize understanding the MIDI routing before touching processBlock.

---

## Key files

- `/Users/matthieulevet/PycharmProjects/DDD1MidiHub/Source/PluginProcessor.h`
- `/Users/matthieulevet/PycharmProjects/DDD1MidiHub/Source/PluginProcessor.cpp`
- `/Users/matthieulevet/PycharmProjects/DDD1MidiHub/Source/PluginEditor.h`
- `/Users/matthieulevet/PycharmProjects/DDD1MidiHub/Source/PluginEditor.cpp`
- `/Users/matthieulevet/PycharmProjects/DDD1MidiHub/Source/PatternBank.h/.cpp`
- `/Users/matthieulevet/PycharmProjects/DDD1MidiHub/Source/PatternSetBank.h/.cpp`
- Build: `xcodebuild -project build_xcode/DDD1MidiHub.xcodeproj -scheme DDD1MidiHub_Standalone -configuration Release`
- Install: `cp -R "...Release/Standalone/DDD-1 MIDI Hub.app" /Applications/`

---

## Architecture

### Three MIDI paths in processBlock

1. **`ddd1In`** → `ddd1InBuf` — DDD-1 MIDI OUT (pad hits + clock/start/stop if DDD-1 is master)
2. **`kbIn`** → `kbInBuf` — IAC Bus 1 (keyboard notes + Ableton clock/start/stop) + virtual piano injections
3. **`midi` buffer** — clock/start/stop only when no DDD-1 selected; noteOns ignored

### PadConfig fields

```cpp
int noteReceive; PadMode mode; bool muted;
int instKey, semitoneOffset, kbVelocity;
bool retriggEnabled; int retriggRate, retriggMax; float retriggDecay;
bool lfoEnabled; int lfoMode, lfoRate, lfoDepth, lfoShape;
int arpRate, arpDirection, arpOctaves; bool arpLatch;
juce::String selectedPatternId; int patternResolution, patternOffset; bool patternLoop;
bool delayEnabled; int delayRate, delayRepeats; float delayDecay;
std::vector<GroupTarget> groupTargets;
```

Default noteReceive/instKey: `{36,38,37,44,46,56,58,48,45,43,51,49,39,54}`

### PadMode enum

`PassThrough=0, Keyboard=1, Arpeggiator=2, PatternBank=3, GroupedTrigs=4`

### Clock / transport

- 24 PPQN from DDD-1 (primary) or Ableton via IAC (fallback when no DDD-1 selected)
- `globalPulseCounter`: incremented once per real clock pulse (triple-counting bug was fixed)
- `patternPlayingStep`: set in audio thread at exact step fire moment; exposed via `getPatternStep(padIdx)`

---

## Delay system

- **`delayQueue`** (sample-based, `DelayEvent` struct with `padIdx` field): PassThrough, KB, PatternBank direct hits
- **`clockEchos`** (pulse-based, per-pad): Pattern and Arp clock handler
- Decay formula: `decayFactor = jmap(delayDecay/100, 0→1, 1.0→0.5)` — full slider range musical, echoes never go silent
- First echo at full velocity (`powf(decayFactor, r-1)`)
- On new KB note: stale echoes for that pad are purged from delayQueue (mono-delay, prevents ghost triggers on fast playing)

---

## Pad click behavior (3-level, time-based)

- **Single click**: select pad
- **Double-click** (within 400ms): toggle mute (`pads[idx].muted`) — button shows "M", pad silent but still flashes on hit
- **Triple-click** (each within 400ms): reset to PassThrough + clear delay + clear groupTargets + unmute
- Switching to a different pad always resets click counter

### Pad flash on hit

- `padHitSeq[numPads]` (atomic uint32_t in processor): incremented on every note-on output (all modes)
- Editor timer (15Hz) detects change → sets `padFlashTicks[i] = 3` (~200ms)
- Flash color = full-alpha mode color (same as selected state), not white
- Muted pads still flash (signal visible even when silent)

---

## Keyboard mode

### Retrigger quantization (when clock sync active)

Snap-to-nearest grid boundary on note-on:
- **First half of period** (rem ≤ ppb/2): fire immediately, align next retrig to boundary
- **Second half** (rem > ppb/2): skip immediate fire, quantize forward to next boundary
- This lets you play on/near the beat and sound in sync; see [[feedback-kb-retrig-quantize]] for tuning notes

### Delay in KB mode

- Uses sample-based `scheduleDelayEchoes` (not clock echoes)
- Old pad echoes purged from delayQueue on each new note-on → clean mono-delay behavior

---

## Pattern mode

### PatternBank

- JSON file, `RhythmPattern`: id, name, instrument, styles[], steps[], resolution
- 1117 patterns, 267 unique names in user's bank; grouped by `name` field, routed by `instrument` field
- Instrument filter: 13 tags (kick, snare, rimshot, clap, closed_hihat, open_hihat, high_tom, mid_tom, low_tom, ride, crash, cowbell, tambourine)

### PatternSetBank (Scenes panel)

- Groups patterns by exact `name` field → auto-sets
- Routes by `instrument` field to pad noteReceive notes
- JSON at separate path; auto-sets built at runtime, not stored

### Direct DDD-1 hits in Pattern mode

When the user hits a DDD-1 pad that's in PatternBank mode, delay echoes now fire (same as PassThrough). Previously ignored.

---

## GroupedTrigs mode

- Pad hit fans out to multiple target pads with velocity scale, tune offset, and pulse delay
- `pendingGroupTrigs` vector (audio thread only): deferred fan-outs fire at exact pulse boundaries
- `groupLock` (public CriticalSection): protects `pads[i].groupTargets` (UI writes, audio reads)

---

## Pattern Sets / Scenes panel

- Right-side panel showing saved scenes + auto-generated sets
- Double-click to apply; Save Scene / Reset All / Refresh buttons
- `setsEntries` vector: `{ isAuto, idx, name, style }`

---

## Ableton routing (user's setup)

- kbIn = IAC Bus 1 (Ableton clock + keyboard)
- ddd1In = UM One MIDI IN (DDD-1 MIDI OUT)
- midiOut = UM One MIDI OUT (DDD-1 MIDI IN)
- UM One monitoring in Ableton must be OFF (otherwise THRU feedback loop)

---

## Bugs fixed (all sessions)

| Bug | Fix |
|-----|-----|
| DDD-1 crash during playback | Removed echo-back; added `delayEchoLastFired` 30ms THRU guard |
| Patterns not stopping on MidiStop | Explicit per-note noteOff before state reset |
| Playhead 1 step ahead | `patternPlayingStep` set at exact fire moment |
| Pattern too fast (2-3x speed) | Clock triple-counting fixed: midi buffer gated on `ddd1InputId.isEmpty()`, clock removed from kbIn |
| Delay echoes glitchy on fast KB playing | `DelayEvent` gains `padIdx`; stale echoes for pad purged on new note-on |
| Delay decay felt wrong (sudden silence) | Formula remapped: decay 0–100 → factor 1.0–0.5 (never silent) |
| PatternBank mode ignores direct DDD-1 hits for delay | Added delay scheduling in PatternBank hit handler case |
| KB/Arp retrig starts off-grid | `pulsesToNextRetrig/Arp` initialized to `ppb - (globalPulseCounter % ppb)` |
| Playing on beat always delayed by retrig quantize | Snap-to-nearest: first half of period fires immediately, second half quantizes forward |
| Mute had no effect | Mute check added to both DDD-1 hit handler AND clock per-pad loop AND kbIn loop |
| MidiDelay was a standalone mode | Removed; delay is now overlay. Old saves migrate |
| Pattern sync drift | Replaced pulsesToNextStep with `globalPulseCounter % pulsesPerStep` |
| Save Scene crash (double-free) | `ModalComponentManager::Callback` auto-deleted by JUCE after `modalStateFinished`; removed `delete this` from callback |
| Overdub on 2nd capture loop overwrites | See Capture section below |

---

## Capture toggle (red button before pad buttons)

`captureToggleBtn` — 15th button in pad row, no label. Grey = off, red = on.

- **ON:** Sets all 14 pads to `PatternBank + overdubEnabled`, creates empty `__live_N__` patterns sized `patternLengthBars * 16` steps. Sets `proc.captureActive = true` → hub clock handler skips note output (record only, no playback to DDD-1).
- **OFF:** Keeps pads in PatternBank mode with recorded patterns, turns off `overdubEnabled`, sets `proc.captureActive = false` → hub starts playing captured pattern back. Turning ON again wipes patterns and starts fresh.

**Workflow:** Set bar count → red ON → DDD-1 plays loop(s) → red OFF → hub plays back captured pattern.

**Why capture suppresses output:** After the 1st loop, patterns are filled. Without suppression, hub sends notes to DDD-1 MIDI IN on loop 2; DDD-1 echoes them on MIDI OUT; hub receives them and overdubs at wrong step positions, corrupting the pattern.

---

## Overdub

`overdubEnabled` toggle (per-pad, PatternBank mode only). When on:
- DDD-1 hit on that pad → writes `hit=true` + velocity to `patternBank` at `padKb[padIdx].patternCurrentStep`
- `overdubDirty[padIdx]` atomic set → editor timer reloads grid from bank and repaints

### Two bugs fixed (2026-06-18)

| Bug | Fix |
|-----|-----|
| Empty live patterns rejected from bank (shared hash with other empty patterns) | `updateLivePattern` changed from `patternBank.add()` (hash-dedup) to unconditional `patternBank.insert()` |
| Overdub never fires if DDD-1 was running before hub opened | Added `if (!transportRunning) transportRunning = true;` at the top of `isMidiClock()` handler — auto-start on first received clock pulse |
| Capture overwrites on 2nd DDD-1 loop | Hub was playing recorded patterns back to DDD-1 MIDI IN; DDD-1 echoed them to MIDI OUT; hub overdubbed echoes at wrong steps. Fix: `captureActive` bool added to processor public fields; PatternBank clock handler gates `if (step.hit && !captureActive)` — no output during capture. Editor sets `proc.captureActive` in captureToggleBtn.onClick. |

### Key constraint

`transportRunning` is required for overdub (to know the current step). It's set by MidiStart or now by the first MidiClock received. It's cleared by MidiStop.

## Startup behavior (clean)

Pad configs NOT restored on launch (`setStateInformation` comment: "intentionally not restored"). Only restores: device IDs, bank paths, global length/steps. Everything else starts at defaults (PassThrough, no delay, no overdub). User sets up pads fresh each session.

---

## Git repo

DDD1MidiHub now has a git repo at `/Users/matthieulevet/PycharmProjects/DDD1MidiHub/`.
- `.gitignore` excludes `build_xcode/`
- Claude Code memory lives in `.claude/memory/` inside the repo
- **To preserve context: open Claude Code from `DDD1MidiHub/` going forward** (memory copied to `~/.claude/projects/-Users-matthieulevet-PycharmProjects-DDD1MidiHub/memory/`)
- Build command (from DDD1MidiHub/): `xcodebuild -project build_xcode/DDD1MidiHub.xcodeproj -scheme DDD1MidiHub_Standalone -configuration Release -quiet`
- Install: `cp -R "build_xcode/DDD1MidiHub_artefacts/Release/Standalone/DDD-1 MIDI Hub.app" /Applications/`

---

## Drum pattern database

Repo: `/Users/matthieulevet/PycharmProjects/drum-pattern-database/` (on GitHub, origin/main)

### Structure (as of 2026-06-19)

```
factory/
  allen/      3,796 patterns (curated, Allen source)
  badness/      122 patterns
  bardet/     1,117 patterns
  roland/       659 patterns
imported/           ← gitignored (regenerable)
  midi_archive/
    rock/
      rock/       8,145 patterns
      metal/      3,804 patterns
      shuffle/    1,001 patterns
      indie_rock/ 5,649 patterns
      hard_rock/  2,829 patterns
      … (14 rock sub-styles)
    electronic/
      electronic_dance/  17,066 patterns
      progressive/          464 patterns
      house/                394 patterns
      … (7 electronic sub-styles)
    world/
      world/      1,495 patterns
      africa/       791 patterns
      asia/         708 patterns
      middle_east/  311 patterns
      acoustic/     226 patterns
    blues/, jazz/, funk/, latin/, pop/, dance/, disco/, reggae/, rnb/, ska/
    … 51 total files across 13 genres, 15,113 imported patterns
registry/
  import_registry.json  (SHA-256 dedup, ~15k entries)
user/
  patterns.json, pattern_sets.json, ratings.json  (user's working files)
```

### Scripts

- `import_midi.py` — imports 800k MIDI archive; skips SD2 (425k) and GM pack (360k, with --skip-gm); SHA-256 dedup; deepest-match genre/style extraction; outputs `genre/style/patterns.json` two-level structure; coverage field tracks unknown notes
- Run command: `python3 import_midi.py "/path/to/archive" --skip-gm`
- `reorganize.py` — one-time migration that created the factory/ structure

### Open issues in database

1. Superior Drummer 2 (425k files, single-instrument per file) skipped — needs a separate grouping pass to reconstruct multi-part patterns.
2. GM MIDI Pack (360k files of full arrangements) always excluded with --skip-gm — not a drum pattern library.

---

## PatternBank — extended data model (new format)

`RhythmPattern` now has:
- `source` — collection slug (bardet, allen, roland, badness, or genre for imported)
- `group` — groups related patterns (same groove, different instruments)
- `genre` — top-level genre string
- `style` — sub-style string
- `isFill()` — checks group name for `_break_`, `_fill_`, `_ending_` OR legacy styles array

`PatternBank::loadDirectory(root)` walks:
- `root/factory/*/patterns.json`
- `root/imported/midi_archive/<genre>/patterns.json` (flat, for any legacy flat files)
- `root/imported/midi_archive/<genre>/<style>/patterns.json` (two-level, current structure)
- `root/user/patterns.json`

`filter()` checks `p.genre` and `p.style` directly (not only `p.styles[]`).

Load Bank button now accepts a **directory** (loads all sub-collections) or a **single `.json` file** (loads one collection). Directory load of the full 150 MB takes ~30s — for daily use load a single genre file.

---

## Browser UI (PatternBank mode)

Filters: Instrument / Genre / Style
Buttons: Clear Steps, Undo (restores before last Clear Steps), Unassign (removes pad assignment)

Instrument list includes: kick, snare, rimshot, clap, closed_hihat, open_hihat, pedal_hihat, high_tom, mid_tom, low_tom, ride, crash, cowbell, tambourine, shaker, claves, conga, bongo, perc, bass

---

## Scenes panel

- Load Bank button moved here (always visible, not buried in PatternBank mode)
- Filters: Genre, Source, Fill/Groove toggle
- Single-click loads a scene (was double-click)
- Row shows name left, source right (muted); saved scenes show ★

---

## UI bugs status (plan file)

1. Playhead off by one step — FIXED: `patternPlayingStep` set at exact fire moment; editor uses `proc.getPatternStep(selectedPad)` with `proc.transportRunning` guard
2. Stop doesn't silence last note — FIXED: explicit noteOff loop in MidiStop handler before state reset
3. Tune not visible in grid cell — PARTIAL: green dot shown when tune≠0; toggle gridViewBtn ("Vel"/"Tune") to switch to full tune bar view
4. Pattern list back in top panel — DONE: `patternListBox` + genre/style/instr filters are in top panel; BottomZoneState has no Browse state

## RhythmPattern coverage field

`float coverage = 1.0f` — fraction of note_on events that mapped to known GM instruments.
- World percussion folders: all 100% coverage (GM notes 60-81)
- Patterns with coverage < 1.0 get `coverage_XXpct` tag
- World sub-folder names (djembe_set, darabuka_set) added as tags for instrument identity
