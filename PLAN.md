# M1 ARP: Project Plan

A macOS instrument plugin, aimed first at Ableton Live, that reproduces the voice,
combination and effects behaviour of the **Korg M1** (1988) and adds an
arpeggiator the hardware never had. Builds as an Audio Unit, a VST3 and a
Standalone app, the same way Juno 106-ARP does.

M1 ARP is a **clean-room implementation**. It copies published behaviour only.
Korg's PCM ROM, factory program and combination data, panel artwork, wordmark
and manual text are not part of this project. The plugin name is **M1 ARP**.
The manufacturer string is Alistair Rutherford. "Korg" does not appear in the
product name, the bundle id or the UI.

Sources for the behaviour: the M1 owner's manual (program, effect, combination
and global pages), the M1 service-manual specifications, and the published
waveform index. Where a number below comes from those, it is the hardware's
number. Where the manual gives only a 0–99 knob and no Hertz or milliseconds,
this plan defines the curve and versions it, the same way Juno 106-ARP versions
its filter calibration.

---

## 1. Goals

| # | Requirement | Acceptance criterion |
|---|-------------|----------------------|
| R1 | Loads in Live and other hosts as an instrument | AU + VST3 + Standalone, universal arm64 + x86_64. Passes `pluginval` at strictness 10. A Live track plays it from MIDI. |
| R2 | **Single program.** One multisample through a non-resonant low-pass (VDF) and an amp (VDA), with the hardware envelopes, velocity, keyboard tracking, pitch bend and aftertouch | 16 simultaneous notes. A known patch is silent outside its multisample's key range. Parameter curves match §4. |
| R3 | **Double program.** Two oscillators per note, the second with interval, detune and delay start | 8 simultaneous notes. OSC2 stays quiet until its delay has elapsed. Detune ±50 is ±50 cents. |
| R4 | **Drums.** Four kits. A drum program is one-shot, ignores note-off when Hold is on, and uses exclusive groups for hats | Open hat cuts the closed hat in the same group. 16 drum voices share the same pool as melodic voices. |
| R5 | **Effects.** Two effect processors, serial or parallel, covering the 33 hardware types | Dry/wet and the pairing rule in §6 hold. Hall, chorus, exciter, rotary and delay/hall are in the first playable build; the rest follow in order. |
| R6 | **Combinations.** Single, Layer, Split, Velocity Switch and 8-timbre Multi, sharing one 16-oscillator pool | Key windows, velocity windows and MIDI channels match §7. A Multi plays eight programs from eight MIDI channels. |
| R7 | **Arpeggiator** (not on the hardware) | Sample-accurate to the host grid, including loops and tempo changes. Modes, pattern, swing and per-timbre enable match §5. No stuck notes. |
| R8 | Programs, combinations and kits save and restore | A preset round-trip restores every parameter and still produces audio. Host state survives reload. |
| R9 | Real-time safe | No allocation and no lock on the audio thread. All-notes-off on stop, bypass, program change and plugin deletion. Correct at 32–2048 sample buffers. |

**Out of scope for v1:** the 8-track sequencer, PCM/RAM cards, individual outputs
3 and 4 as extra plugin buses, Windows, AAX, iOS/AUv3, and an online store.
See §13.

---

## 2. Signal path

```
MIDI in
  → Arpeggiator          (program: one arp; multi: one arp per timbre, shared clock)
  → Combination router   (key window, velocity window, MIDI channel, transpose)
  → Voice allocator      (16 oscillator slots)
       each slot:  Multisample player → Pitch EG + Pitch MG
                   → VDF (non-resonant low-pass) + VDF EG + VDF MG
                   → VDA + VDA EG
  → Effect 1 ── serial or parallel ── Effect 2
  → stereo out (hardware outs 1/L and 2/R)
```

Hardware facts the engine is built around:

- **16 oscillator slots.** Single mode uses one slot per note (16 notes). Double
  mode uses two (8 notes). Drum hits use one slot each. Service manual: "16
  voice, 16 oscillator (Single); 8 voice, 16 oscillator (Double)".
- **AI Synthesis.** A multisample is the oscillator. There is no analogue-style
  saw/pulse oscillator except where the waveform list itself contains a saw,
  square, pulse or sine multisample (indices 89–99).
- **The VDF has no resonance control.** The program page exposes Cutoff and EG
  Intensity only. Resonance is not added.
- **One pair of effects for the whole program, and one pair for a combination.**
  A combination's effects replace the programs' effects while that combination
  plays.
- **Dynamic voice allocation** across timbres. A late chord can steal a releasing
  voice from another timbre.

Steal order: a voice that has finished its release, then the releasing voice
with the lowest VDA level, then the oldest sounding voice. Stealing sends a
fast ramp to silence (5 ms) before reuse, so a stolen voice does not click.

---

## 3. Clean-room content

| We implement | We do not ship |
|---|---|
| The parameter set, ranges and routing in the owner's manual | Korg PCM, extracted ROMs, or samples converted from the ROM |
| A 100-slot multisample map and 44-slot drum map, filled with recordings we make or license | Factory program/combination banks, including any `.syx` dump of the internal presets |
| Original program names we write | The hardware's program names (Universe, and the rest of the 00–99 bank) |
| An original editor layout | The M1 panel, LCD graphic, logo or colours as a skin |
| A SysEx *parser* so a musician can load a dump from a unit they own | Korg's dump, embedded |

Multisample **indices** follow the published 00–99 list, so a program that asks
for multisound 00 asks for the piano slot and 31 asks for the strings slot.
The samples in those slots are ours. A slot with no sample yet is silent and
keeps its index. The UI labels a slot with its role (`Piano`, `Strings`,
`Saw`). The 00–99 index is the address, shown as a number next to the role.

---

## 4. Program model

A program is one of three oscillator modes, plus the pages below. Counts in
parentheses are the hardware ranges.

### 4.1 Oscillator

| Parameter | Range | Behaviour |
|---|---|---|
| Mode | Single, Double, Drums | Selects how many slots a note uses, and whether OSC1 is a multisample or a kit. |
| Assign | Poly, Mono | Mono retriggers pitch, filter and amp envelopes on each new note and plays one note. |
| Hold | Off, On | On ignores note-off. The voice stays until it is stolen or Hold is turned off. This is the drum-kit behaviour. |
| OSC1 multisound | 00–99, or kit 1–4 in Drums | Silent outside that multisample's key range. |
| OSC1 level | 0–99 | Linear amplitude, 0 silent, 99 unity. |
| OSC1 octave | 16', 8', 4' | −12, 0, +12 semitones from the multisample root. |
| OSC2 multisound, level, octave | same | Rendered only in Double. |
| OSC2 interval | −12…+12 semitones | Relative to OSC1, after octave. |
| OSC2 detune | −50…+50 | Cents. −50 is −50 cents. |
| OSC2 delay start | 0–99 | 0 starts with the note. 99 delays OSC2 by 500 ms. Exponential, so the bottom of the knob is a few milliseconds. |

### 4.2 Pitch EG (one per oscillator)

Start Level, Attack Time, Attack Level, Decay Time, Release Time, Release
Level, plus EG Level velocity sensitivity and EG Time velocity sensitivity.
Levels are −99…+99 and represent pitch in cents, limited to ±1 octave at the
velocity extremes, as the manual states. Times are 0–99.

There is no slope segment on the pitch EG. The contour is start → attack →
decay (sustain holds the attack-decay result) → release.

### 4.3 VDF and VDA

Both use a four-segment envelope: Attack, Decay, Slope, Release.

| | VDF | VDA |
|---|---|---|
| Times AT, DT, ST, RT | 0–99 | 0–99 |
| Levels | Attack, Break Point, Sustain, Release, each −99…+99, relative to the cutoff | Attack, Break Point, Sustain, Release, each 0–99, absolute amplitude |
| EG intensity | 0–99, scales every VDF level | — |
| Cutoff | 0–99 | — |

**Cutoff curve (calibration v1).** Exponential from 100 Hz at 0 to 16 kHz at
99. Keyboard tracking of 0 moves the cutoff by the same ratio as the note moves
from the centre key, which is what the manual means by "the change of cutoff
and the change of pitch are equal when set to 0". The centre key is C1–G9.
Positive tracking brightens notes above the centre; negative tracking does the
opposite.

**Filter.** Two cascaded one-pole low-passes, no resonance, no extra drive.
Cutoff is smoothed per block (one-pole, ~5 ms) so automation does not zipper.
This is the whole filter. A resonant ladder would be a different instrument.

**Envelope times (calibration v1).** Time value `t` (0–99) maps to seconds
`0.002 * exp(t / 99 * ln(5 / 0.002))`, so 0 is 2 ms and 99 is 5 s. The same
curve is used for pitch, VDF and VDA times. Velocity and keyboard time
modifiers scale that duration: a "+" polarity shortens it as velocity rises or
as the note moves above the centre key, a "−" lengthens it, "0" leaves it.
The three-way polarity controls (attack, decay, slope, release) match the
manual: −, 0, +.

**VDA level 99** is unity into the effect send. Levels in between are linear.
One oscillator at unity plays at `kVoiceGain` (0.45); in Double mode each
oscillator is scaled by a further 0.7 so a note keeps about the same level. The
processor ends with a Master parameter (−36…+6 dB) and a soft clipper that is
transparent below −2 dBFS and never exceeds 0 dBFS.

### 4.4 Velocity and keyboard tracking

Separate pages for VDF1, VDF2, VDA1, VDA2:

- EG intensity velocity, −99…+99 (VDF) or the matching VDA control.
- EG time velocity, 0–99, with a per-segment polarity −, 0, +.
- Keyboard centre key, cutoff (or level) amount −99…+99, EG time 0–99, and the
  same per-segment polarities.

VDF2 and VDA2 exist only in Double mode. Their parameters are stored anyway, so
switching Single → Double does not invent values.

### 4.5 Modulation generators

Pitch MG and VDF MG share one structure:

| Parameter | Range |
|---|---|
| Wave | Triangle, Saw up, Saw down, Square |
| Frequency | 0–99. 0 is 0.1 Hz, 99 is 20 Hz, exponential. |
| Delay | 0–99, same time curve as the envelopes. |
| Intensity | 0–99. Pitch intensity 99 is ±50 cents. VDF intensity 99 is ±2 octaves of cutoff. |
| OSC select | Off, OSC1, OSC2, Both |
| Key sync | Off: the MG free-runs. On: each voice starts at phase 0. |

### 4.6 Aftertouch and joystick

Channel aftertouch (0–127):

| Destination | Range | At full pressure |
|---|---|---|
| Pitch | −12…+12 | That many semitones, added as Pitch MG depth rather than a static offset. The manual ties the sign to Pitch MG. |
| Pitch MG depth | 0–99 | Scales the program's Pitch MG intensity. |
| VDF cutoff | −99…+99 | ±2 octaves at the extremes. |
| VDF MG depth | 0–99 | Scales VDF MG intensity. |
| VDA amplitude | −99…+99 | ±12 dB at the extremes, clamped so the voice cannot exceed unity by more than 6 dB. |

Joystick, from MIDI:

- Pitch bend → joystick X. Range −12…+12 semitones, symmetric.
- CC1 → joystick +Y. Raises Pitch MG depth and VDF MG depth by the program's
  amounts, and applies VDF Sweep (−99…+99, ±2 octaves at the extremes).
- CC11 (expression) is a separate level trim, default open. The hardware has no
  expression jack; this is so a Live controller still works.

### 4.7 Play-page offsets

The hardware's program-play page applies eight temporary offsets of −10…+10,
cleared on program change: oscillator balance, VDF cutoff, VDA level, keyboard
track, velocity sensitivity, attack time, release time, effect balance. They
offset the stored program; they are not written back unless the user saves.

---

## 5. Arpeggiator

The arp sits on MIDI, in front of the router. It keeps the held-note set,
passes pitch bend, aftertouch, CC and sustain through, and emits its own notes.
With the arp off, input MIDI reaches the router unchanged and any latched notes
are released on that block.

This is the same job as the Juno 106-ARP arpeggiator, with a pattern row, swing,
a chord mode, and a per-timbre switch. Those four are the reason this project
has its own arp rather than a copy of the Juno one.

### 5.1 Parameters

| Parameter | Range | Notes |
|---|---|---|
| On | off / on | |
| Mode | Up, Down, Up/Down, Down/Up, Random, As Played, Chord, Pattern | Chord retriggers every held note together on each step. Pattern plays the step row. |
| Rate | 1/1, 1/2, 1/2T, 1/4, 1/4T, 1/8, 1/8T, 1/16, 1/16T, 1/32 | Quarter-note lengths: 4, 2, 4/3, 1, 2/3, 1/2, 1/3, 1/4, 1/6, 1/8. |
| Octaves | 1–4 | Repeats the pattern up by octaves, then returns to the bottom. |
| Gate | 1–100% | Fraction of the step a note sounds. 100% ties into the next step when that step is also on. |
| Swing | 50–75% | Delays every other step. 50% is straight. |
| Hold | off / on | Latch. Releasing Hold keeps only the keys still held, matching Juno 106-ARP. |
| Steps | 16 | Each step: on/off, octave −2…+2, velocity 1–127. Used by Pattern mode. |

### 5.2 Clock

While the host transport is running, step boundaries come from PPQ, so a loop
that restarts mid-step restarts the step. While stopped, the arp free-runs at
the host BPM from a sample counter. BPM below or equal to 0 is treated as 120.
Tempo changes inside a block take effect on the next step, which is the
audible choice and the one the tests lock down.

### 5.3 Where it runs

- **Program mode.** One arp, in front of the 16 slots.
- **Combination, every type.** One arp. Single, Layer, Split and Velocity
  Switch all hear the arp's notes and then apply their own windows.
- **Multi.** Each of the 8 timbres has its own on/off, mode, gate, octaves and
  pattern. They share rate, swing and the clock, so the parts stay aligned.
  A timbre with the arp off plays the incoming notes on its channel as held
  chords.

Random uses a per-voice xorshift seeded when the arp is prepared, so a test can
set the seed. As Played keeps insertion order. Sorting for Up and Down is by
note number, then by insertion order for ties.

### 5.4 Note hygiene

- A new step sends note-off for anything the previous step is not holding,
  before the new note-ons, on the same sample.
- Sustain (CC64) holds keys in the *input* set. It does not hold arp-generated
  notes past their gate.
- Transport stop, arp off, program change, bypass and `reset()` all send
  note-off for every note the arp generated.
- The on-screen keyboard draws the notes the voices receive, so a running
  pattern is visible. Clicks on that keyboard enter ahead of the arp.

MIDI output of the generated notes is a VST3-only follow-up (§13). Live drops
MIDI from AU instruments, which NoteCap and Chordism both ran into, so the AU
stays audio-only.

---

## 6. Effects

Two processors. Each program and each combination stores both, plus a
placement: **Serial** (effect 1 into effect 2) or **Parallel** (the dry voice
sum feeds both, and the two returns sum to stereo). Each effect has its own
dry:effect balance on the hardware scale 99:1 … 1:99.

**Balance law.** For effects that add a wet signal to the dry one (reverbs,
early reflections, delays, chorus, flanger, phaser, ensemble, the delay
combinations) the dry stays at full level up to 50:50 and the wet reaches full
level at 50:50, so adding a reverb never makes a program quieter. Effects that
carry the whole signal (EQ, overdrive, distortion, exciter, tremolo, rotary)
crossfade linearly. 99:1 is exactly dry and 1:99 exactly wet in both cases.
Parameter labels per type are in `effectParamName` (`Model.h`).

Effects 01–25 are stereo. Effects 26–33 are dual effects in one slot (delay
into hall, delay into chorus, and so on). Effect 24 (Symphonic Ensemble) and
effect 25 (Rotary Speaker) cannot be paired with a modulated effect, because
the hardware shares one modulation generator between them. The modulated types
are chorus 1/2, flanger, cross flanger, phaser 1/2, tremolo 1/2, ensemble,
rotary, and the delay combinations that include chorus, flanger, phaser or
tremolo. The UI disables the illegal partner and the engine forces it to No
Effect if a preset asks for it.

| # | Type | v1 parameters we honour |
|---|---|---|
| 00 | No Effect | — |
| 01–06 | Hall, Ensemble Hall, Concert Hall, Room, Large Room, Live Stage | Time, pre-delay, early level, high damp, EQ low, EQ high, dry:effect. The six types differ by early/late mix and damping defaults, not by six unrelated algorithms. |
| 07–09 | Early reflection 1–3 | Early time, pre-delay, EQ, dry:effect. Three tap patterns. |
| 10–11 | Stereo delay, cross delay | Left time, right time, feedback, high damp, EQ, dry:effect. Cross feeds each side into the other. |
| 12–13 | Stereo chorus 1–2 | Depth, speed, delay, waveform (sine/triangle), EQ, dry:effect. |
| 14–15 | Stereo flanger, cross flanger | Depth, speed, delay, feedback, waveform, EQ, dry:effect. |
| 16–17 | Phaser 1–2 | Manual, speed, depth, feedback, waveform, dry:effect. Four all-pass stages. |
| 18–19 | Stereo tremolo 1–2 | Depth, speed, waveform, shape. Type 1 pans with inverted phases; type 2 does not. |
| 20 | Equalizer | Low gain, low frequency (250 / 500 / 1 kHz), high gain, high frequency (1 / 2 / 4 kHz), dry:effect. |
| 21–22 | Overdrive, distortion | Drive, level, EQ, dry:effect. Two curves, both band-limited enough not to alias at 48 kHz. |
| 23 | Exciter | Blend, emphatic, EQ low, EQ high, dry:effect. A high-shelf of the distorted top end, added back. |
| 24 | Symphonic ensemble | Depth, EQ, dry:effect. A slow multi-voice chorus. |
| 25 | Rotary speaker | Depth, speed ratio, dry:effect. Two speeds, ramped, with a horn and a drum. |
| 26–33 | Delay/Hall, Delay/Room, Delay/Early, Delay/Delay, Delay/Chorus, Delay/Flanger, Delay/Phaser, Delay/Tremolo | The delay parameters plus the partner's parameters, in one slot. |

Reverbs are Schroeder-style (parallel combs, series all-passes) with a
modulated delay line on Ensemble Hall only. They run at the host rate. No
convolution of a hardware impulse, because that would be a recording of Korg's
program.

Effect tail is reported to the host as 8 seconds when either slot is a reverb
or a delay, and 0.1 seconds otherwise, so Live does not chop a hall on stop.
The real tail follows the program.

---

## 7. Combinations

Five types, matching edit-combination mode.

| Type | What plays |
|---|---|
| Single | One program. |
| Layer | Two programs, each with a level, a pan and a damper switch. |
| Split | Lower and upper program, a split key, level, pan, damper. |
| Velocity Switch | Soft and loud program, a velocity threshold, level, pan, damper. |
| Multi | Eight timbres. |

Each Multi timbre has: program, level, pan, damper on/off, MIDI channel 1–16,
key bottom, key top, velocity bottom, velocity top, transpose −24…+24,
detune −50…+50, and the arp enable from §5.3. Output pan is the stereo position
into the combination's effect pair. Hardware outs 3 and 4 are stored (off, or a
balance) and ignored by the mixer until the extra-bus follow-up.

Timbres share the 16 slots. A timbre with a Double program spends two slots per
note it wins. The allocator does not reserve slots per timbre; the hardware's
dynamic allocation does not either.

The combination stores the effect pair. Switching combination reloads both
effects and sends all-notes-off.

---

## 8. Multisamples, kits, programs

### 8.1 Multisample file

A bank is a directory of `zones.json` plus WAV files, packed by
`Tools/pack_bank.py` into one binary loaded at prepare time.

```json
{
  "index": 31,
  "role": "Strings",
  "zones": [
    { "wav": "strings-c2.wav", "root": 36, "lo": 36, "hi": 48,
      "tuneCents": 0, "loopStart": 12000, "loopEnd": 48000 }
  ]
}
```

Playback is 16-bit linear interpolation, forward loop. A zone with
`loopStart == loopEnd` is a one-shot. Pitch is a phase increment from the note,
the octave, the interval, the detune, the pitch EG and the MG. No sinc, and no
oversampling: that is the character of this kind of engine, and it is cheap
enough for 16 voices with the effects still in budget.

The first bank that must exist before the plugin is useful:

| Index | Role | Why this one is early |
|---|---|---|
| 00 | Piano | The patch the instrument is recognised by, once hall and exciter exist. |
| 01 | Electric piano | Velocity-to-filter behaviour. |
| 05 | Organ | Rotary speaker. |
| 14 | Pick bass | Short attack, musical arp target. |
| 30 | Choir | Long loop, delay-start doubles. |
| 31 | Strings | Keyboard tracking. |
| 89–94, 98, 99 | Square, saw, pulse, digital, triangle, sine | Steady tones for filter and envelope tests. |
| drum 0–8 | Kick, snare, closed hat, open hat, toms, clap, ride | One kit. |

Until recorded WAVs exist, every index is filled by the **built-in bank**
(`Source/BankBuilder.h`): each multisound is synthesised from a recipe (partial
ratios with attack and held amplitudes, decay times, detuned copies, a noise
transient and an optional looped breath band), rendered as one band-limited
zone per octave with a seamless inverse-FFT loop. Drums 0–43 are synthesised
the same way. The bank takes about a second to build in a Release build, so
`BankCache` builds it once per process on a background thread and caches it at
`~/Library/Caches/com.alistairrutherford.m1arp/builtin-v<N>.m1bk`; bump
`kBuiltinBankVersion` whenever a recipe changes. A `bank.m1bk` in the preset
folder still replaces slots on top. Adding a sample is a content change, not an
engine change.

`tuneCents` in a zone is a **correction**: a zone whose PCM is 3 cents sharp of
its root stores −3.

### 8.2 Drum kit

Four kits. Each kit has 30 keys (MIDI 35–64 to start; the global page can move
a key). Per key: sample index 0–43, tune −50…+50 cents scaled to ±1 octave at
the extremes, level 0–99, decay 0–99 (scales the one-shot playback rate of the
amplitude), pan, exclusive group 0–7 (0 means no group). A new note in a group
note-offs the previous one in that group.

### 8.3 Factory programs

About 24 programs and 8 combinations, written for this bank, with our names.
They exist to prove each page: a single piano with hall, a double choir, a
mono bass, a drum program, a split, a velocity switch, an 8-part multi, and
four arp patterns. They are ordinary presets, not a recreation of the 1988
bank.

---

## 9. Plugin, state, build

Same toolchain as Juno 106-ARP.

- JUCE at `~/Development/JUCE`. Projucer resaves `M1Arp.jucer`. No CMake, no
  Homebrew dependency.
- Formats: AU, VST3, Standalone. macOS deployment target 12.0. Universal binary.
- `pluginIsSynth`, MIDI in, no MIDI out in v1.
- Bundle id `com.alistairrutherford.m1arp`. Plugin code `M1ap`. Manufacturer
  code `ARut`.
- Parameters live in `AudioProcessorValueTreeState`. Program and combination
  blobs that are too wide for a useful automation lane (the 16 arp steps, the
  kit, the multisample choice per program) live in the state XML and are still
  recalled by the host. Automatable lanes are the play-page offsets, the arp
  on/rate/gate, cutoff, release, and the effect dry:wet. Everything else is in
  the preset.
- State root carries `calibration="1"`. A future curve change bumps the number
  and writes a migrator, as in Juno 106-ARP. Unknown newer versions load and
  leave the new fields alone.
- Preset files are `.m1arp` (XML). Save and Load on the editor. The host sees
  factory programs as `getNumPrograms`.

Audio thread rules:

- Sample data, effect delay lines and voice objects are allocated in
  `prepareToPlay`.
- `processBlock` only reads the parameter snapshot taken at the start of the
  block, runs the arp, allocates voices and renders.
- GUI edits cross via APVTS or a lock-free request flag (arp reset, bank
  reload). A bank reload finishes on the message thread and swaps a
  `shared_ptr` the audio thread reads once per block.

---

## 10. Editor

One window, about 980×640, with a page tab. Original layout, plain type, no
hardware skin.

| Page | Contents |
|---|---|
| Play | Program / combination name, the eight offsets, effect balance, a keyboard. |
| Osc | Mode, assign, hold, both oscillators, delay start. |
| Pitch | Both pitch EGs, drawn. |
| Filter | Cutoff, intensity, both VDF EGs, velocity, keyboard track. |
| Amp | Both VDA EGs, velocity, keyboard track. |
| Mod | Both MGs, aftertouch, joystick ranges. |
| Arp | On, mode, rate, octaves, gate, swing, hold, and the 16 steps. This page is the feature the hardware does not have, so it is a tab of its own rather than a buried row. |
| FX | Two type menus, placement, the parameters of the selected type. Illegal pairs are greyed. |
| Combi | Type, the timbre rows, key and velocity windows. |
| Kit | 30 rows for the current kit. |

The keyboard shows notes after the arp. Editing a step while the pattern plays
is heard on the next pass.

**Info View** (2026-11-18, as NoteCap's and Live's own): a strip along the
bottom, under the keyboard, 54 px with a 10 px gap, so the window is 64 px
taller (1080 × 794 by default, 960 × 704 at its smallest). Pointing at any
control shows a bold title and a line or two on what it does and what its
values mean; elsewhere a hint. It replaces the tooltips (no `TooltipWindow`).
The table is in `PluginEditor.cpp`, keyed `<page>:<section>/<control>` (each
page's controls are tagged when it is built, `tagPage`), with `*` for any page
(the arp's controls on Play and Arp, the polarity switches) or any control of a
section (an effect type's own parameters), and `header.…` / `tab.…` for the
header. `Tests/run_integration.sh` checks that every control on every page has
its own entry, in PROG and COMBI mode, for a Single, a Double and a Drums
program, and writes the README's screenshot to `Tests/build/editor-info.png`.

---

## 11. Tests

`Tests/run_tests.sh` compiles with the Xcode clang against the JUCE modules, the
same approach as Juno 106-ARP. No test depends on a plugin host.

| Test | What it locks |
|---|---|
| `test_env` | Pitch EG has no slope segment. VDF levels are bipolar and scaled by intensity. VDA levels stay 0–99. Time 0 is 2 ms and time 99 is 5 s, within one sample at 48 kHz. |
| `test_filter` | Cutoff 0 and 99 land on 100 Hz and 16 kHz. Tracking 0 shifts cutoff by a octave when the note moves an octave from the centre key. No resonance coefficient exists. |
| `test_voice` | Single plays 16 notes and the 17th steals. Double plays 8. OSC2 delay is silent until the delay elapses. Detune −50 is −50 cents at the phase increment. A note outside the zone is silent. |
| `test_drums` | Open hat replaces closed hat in the same exclusive group. Hold ignores note-off. |
| `test_arp` | Every mode against a fake transport: grid position, gate length, swing, octave wrap, chord mode note count, pattern steps, hold-release behaviour, loop restart, tempo change, all-notes-off. Seeded random is repeatable. |
| `test_fx` | Dry:effect 99:1 equals dry. Serial and parallel routing. Effect 24 refuses a chorus partner. |
| `test_combi` | Split point, velocity switch threshold, multi MIDI channel and key window, 16-slot sharing between a double timbre and a single timbre. |
| `test_state` | Save and load round-trip, including arp steps and a combination. Calibration migrator from a missing version field. |

`Tests/run_integration.sh` loads the built plugin, applies a factory program,
renders a second of audio, and checks the output is non-silent and that a
preset round-trip still is.

Manual check before calling v1 done: the AU and the VST3 both load in Live, the
piano program responds to a clip, the arp locks to the grid with the transport
running and free-runs with it stopped, and a multi on eight MIDI channels plays
eight programs.

---

## 12. Phases

Each phase builds on the previous one and leaves the plugin loadable.

### Phase 1 — Scaffold and one voice

Projucer project, processor, a single multisample voice, VDA envelope, 16-slot
allocator, one piano zone, Standalone makes sound. Tests: voice count, zone
bounds, envelope times.

### Phase 2 — The Single program

VDF, VDF EG, pitch EG, velocity, keyboard tracking, aftertouch, pitch bend, MG.
Play-page offsets. Tests: filter curve, tracking, pitch EG limits.

### Phase 3 — Arpeggiator

§5 in full, for a Single program. Editor arp page. `test_arp` complete. This
lands before double mode and effects, because a bass multisample and a pattern
are enough to hear it, and it is the feature this project adds.

### Phase 4 — Double mode

OSC2, interval, detune, delay start, the second filter and amp. Polyphony drops
to 8. Tests for delay and detune.

### Phase 5 — Drums

Kit data, exclusive groups, Hold, drum editor page, one kit of nine sounds.

### Phase 6 — Effects, the set a patch needs

No effect, the hall family, early reflections, stereo and cross delay, both
choruses, exciter, EQ, overdrive, rotary. Serial and parallel. Tail length.

### Phase 7 — The remaining effects

Flanger, phaser, tremolo, distortion, ensemble, and the eight combination
effects. The pairing rule.

### Phase 8 — Combinations

All five types, shared allocator, combination effects, per-timbre arp enable.

### Phase 9 — Presets and the rest of the bank

`.m1arp` save/load, factory programs and combinations, host programs, the
remaining multisample slots that have WAVs ready. SysEx import of a program
dump, mapping multisound indices onto our slots, as a tool rather than a
runtime dependency.

### Phase 10 — Editor completion and host validation

The pages in §10, `pluginval`, the Live pass in §11.

### Release 1.1.0 (2026-11-18)

Version 1.1.0 (0x10100) in `M1Arp.jucer` (`738d45d`, "Release 1.1.0"); `CHANGELOG.md` (new: 1.1.0,
the Info View; 1.0.0, not released) and the README's version line (`8e85c7b`). Tag **`v1.1.0`** on
the changelog commit `8e85c7b`. **Checks:** `Tests/run_tests.sh` and `Tests/run_integration.sh`
pass; **pluginval strictness 10 passes** on the VST3 and `auval -v aumu M1ap ARut` on the AU; the
installed AU, VST3 and Standalone report 1.1.0. **GitHub release**
[M1 ARP 1.1.0](https://github.com/alistairrutherford/m1-arp/releases/tag/v1.1.0):
`M1Arp-1.1.0-macOS-adhoc.zip` (12.7 MB; the AU, the VST3, the Standalone, README, CHANGELOG and
licence; universal) and its `.sha256`, signed ad hoc and not notarized (no Developer ID on this
Mac), downloaded back and checked. Not run in Live.

---

## 13. Explicitly later

- **Sequencer.** The hardware's 8-track, 10-song, 100-pattern sequencer is a
  second product. Live already sequences. Nothing in phases 1–10 builds a
  timeline.
- **VST3 MIDI out** of the arp notes, so a clip can record the pattern. AU
  stays without MIDI out.
- **Outputs 3 and 4** as extra buses. The pan fields are stored from day one so
  this does not rewrite preset files.
- **PCM card slots and the EXK-M1 expansion map.** A second bank file can be
  loaded later; the engine already addresses slots by index.
- **Resonance.** It was not a hardware control. Adding it means a new filter,
  which is a different synth.

---

## 14. Sources

- Korg M1 owner's manual: program pages (OSC basic through effect placement),
  effect parameter chart (types 01–33 and the pairing note), combination types,
  global drum kits.
- Korg M1 service manual specifications: 16 / 8 voices, 16-bit 2 Mword PCM,
  two digital multi-effects, 100 programs, 100 combinations, 8-part multi.
- Published waveform index: multisounds 00–99 and 44 drum sounds. The "144
  waveforms" figure is those two counts added together.
- Juno 106-ARP in `../Juno106AU`: Projucer layout, header-style DSP, arp as a
  MIDI stage, calibration version, `Tests/run_tests.sh`.
- Chordism plan in `../chordism/PLAN.md`: clean-room rule, and the note that
  Live does not take MIDI from an AU instrument.
