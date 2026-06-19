---
name: project-ddd1-bass-translator
description: DDD-1 Bass Translator VST/standalone app — MIDI translator for Korg DDD-1 drum machine bass card
metadata: 
  node_type: memory
  type: project
  originSessionId: e5d32793-9250-43c1-9d6a-b1ad36817946
---

Built a MIDI translator for a **Korg DDD-1** drum machine with a bass synth ROM card.

**Why:** User wants to play the bass card chromatically from a MIDI keyboard. The DDD-1's MIDI implementation uses a non-standard note mapping.

**How to apply:** Project is complete and working as a standalone app. If the user wants to continue, the code is at `/Users/matthieulevet/PycharmProjects/DDD1BassTranslator/`.

---

## DDD-1 MIDI implementation (key facts from manual)

| Note range | Function |
|---|---|
| 9–24 | SEQ DECAY (decay control) |
| 25–71 | INST KEY (trigger instrument sound) |
| 72–96 | SEQ TUNE (pitch control, ±12 semitones) |

- **SEQ TUNE center = note 84** → 0 pitch change (matches TOTAL TUNE baseline set on DDD-1)
- Each step = 1 semitone, 100 cents
- Range = 2 octaves (72 to 96 = -12 to +12 semitones)
- To play a pitch: send SEQ TUNE note (72–96) first, then INST KEY note (25–71) to trigger
- INST KEY note is configurable via INST ASSIGN on the DDD-1

## Plugin architecture

**Final working form: standalone macOS app** (VST3/AU failed to load on macOS 26 Tahoe — too strict on unsigned plugins)

- Installed at: `/Applications/DDD-1 Bass Translator.app`
- Source: `/Users/matthieulevet/PycharmProjects/DDD1BassTranslator/`
- Built with: JUCE + CMake (Xcode generator), cloned JUCE at `/Users/matthieulevet/PycharmProjects/DDD1BassTranslator/JUCE/`

**Parameters:**
- `Inst Key` (25–71): MIDI note assigned to bass sound in DDD-1's INST ASSIGN (default 36)
- `Root Note` (0–127): keyboard key = center pitch / TOTAL TUNE baseline (default 60 = C4)
- `MIDI Ch` (1–16): channel DDD-1 listens on
- `Wobble` on/off + Rate (0.1–20 Hz) + Depth (0–12 semitones) + Shape (Sine/Square/Triangle/Saw)

**Wobble effect:** LFO periodically retriggles the bass hit at oscillating pitches — pitch wobble since DDD-1 is sample-based (no filter to sweep).

## Build commands (to rebuild)

```bash
cd ~/PycharmProjects/DDD1BassTranslator
cmake -B build_xcode -G Xcode
cmake --build build_xcode --config Release
cp -R "build_xcode/DDD1BassTranslator_artefacts/Release/Standalone/DDD-1 Bass Translator.app" /Applications/
xattr -cr /Applications/DDD-1\ Bass\ Translator.app
codesign --force --deep --sign - /Applications/DDD-1\ Bass\ Translator.app
```

## Usage

Connection: `Keyboard → MIDI interface → Mac → App → MIDI interface OUT → DDD-1 MIDI IN`

1. Launch app, click ↺ to refresh MIDI ports
2. Select DDD-1 port in "MIDI Out" dropdown
3. Click ⚙️ Settings → select keyboard as MIDI Input
4. Set Inst Key to match INST ASSIGN on DDD-1
5. Set Root Note to match TOTAL TUNE baseline on DDD-1
6. Play!

## v2 roadmap

- Boutons **octave +/-** pour décaler le Root Note d'une octave rapidement (la plage effective est limitée à ±12 demi-tons par la DDD-1, les boutons permettent de changer de registre sans toucher au slider)

## macOS 26 Tahoe note

VST3 and AU plugin formats failed to load in Ableton Live 12 on macOS 26 Tahoe — the OS blocks unsigned plugins. Standalone app works fine (user approves via Privacy & Security on first launch). VST3/AU code still exists in the project if this is revisited with a proper Apple developer certificate.
