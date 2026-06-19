---
name: feedback-kb-retrig-quantize
description: "Retrigger quantization behavior in Keyboard mode — snap-to-nearest logic, may need tuning"
metadata: 
  node_type: memory
  type: feedback
  originSessionId: e5d32793-9250-43c1-9d6a-b1ad36817946
---

Current implementation: "snap to nearest grid boundary" when retrig + clock sync active.

- First half of retrig period (rem <= ppb/2): fire immediately, align next retrig to boundary
- Second half (rem > ppb/2): skip immediate fire, quantize forward to next boundary
- Tolerance window: ppb/2 pulses (e.g. ~31ms at 1/16 + 120 BPM)

**Why:** Playing exactly on the beat gave a full-period delay (rem=0 → pulsesToNextRetrig=ppb). Snap-to-nearest fixes that — on-time or slightly-late playing fires immediately.

**How to apply:** User said "save this for later, we might need to adjust." The tolerance window (ppb/2) and the skip-vs-fire threshold are the main knobs. Could also make the window configurable, or try a fixed pulse count instead of ppb/2.

Code location: `PluginProcessor.cpp`, kbIn note-on handler, inside `if (clockSyncActive && pads[p].retriggEnabled)` block.
