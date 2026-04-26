# Exotic Players

C ports of the Amiga module replayers from
[NostalgicPlayer](https://github.com/neumatho/NostalgicPlayer).

Each player is a single header (`<player>.h`) that exposes:

```c
struct <player>_state *<player>_init(void *data, uint32_t len, int32_t sample_rate);
void <player>_free(struct <player>_state *s);
void <player>_get_audio(struct <player>_state *s, int16_t *output, int32_t frames);
```

`_init` mallocs internal state and references the caller's module buffer for
sample PCM. `_get_audio` accumulates int16 stereo frames at the rate given to
`_init`; the caller is responsible for clearing the buffer first. `_free`
releases the player's heap; the caller's module buffer is never touched.

A shared `paula.h` emulates a 32-channel virtual Paula (real Amiga has 4;
formats like 7-voice Hippel, 8-channel OctaMed/Oktalyzer and up to 32-channel
DigiBooster Pro use the additional virtual channels). Channel pan defaults to
LRRL repeating: 0 -> left, 1 -> right, 2 -> right, 3 -> left, 4 -> left, ...

The dispatcher (`test_player/test_player.c`) iterates a `player_api[]`
registry; extension match is preferred but each player's `init()` performs
content-based identification, so the wrong extension is harmless.

## Format support matrix

Format names are taken from the NostalgicPlayer matrix.

| Format | Extension(s) | Player file | Notes |
|---|---|---|---|
| Activision Pro | `.avp .mw` | `activisionpro.h` | M68k scanner identify |
| AHX 1.x / 2.x / HivelyTracker | `.hvl .ahx .thx` | `hivelytracker.h` | Custom internal mixer (ring mod, generated waveforms, filters) |
| AMOS Music Bank | `.abk` | `amosmusicbank.h` | LED filter is a no-op (TODO) |
| Art Of Noise (4v / 8v) | `.aon .aon4 .aon8` | `artofnoise.h` | |
| Ben Daglish | `.bd` | `bendaglish.h` | M68k scanner identify, sub-songs |
| David Whittaker | `.dw .dwold` | `davidwhittaker.h` | M68k scanner identify, periods v1/v2/v3 |
| Delta Music 1.0 | `.dm1` | `deltamusic10.h` | |
| Delta Music 2.0 | `.dm2` | `deltamusic20.h` | |
| DigiBooster 1.x | `.digi` | `digibooster.h` | 8-channel; backwards playback degrades to forward |
| DigiBooster Pro 2.x / 3.x | `.dbm` | `digiboosterpro.h` | DBM2/DBM3 frequency models. **Echo (V/W/X/Y/Z), volume/pan envelopes, backwards playback are not yet ported. TODO** |
| Digital Mugician (1 / 2) | `.dmu .mug` | `digitalmugician.h` | DMU1 4-ch and DMU2 7-ch (folded onto Paula) |
| Digital Sound Studio | `.dss` | `digitalsoundstudio.h` | LED filter is a no-op (TODO) |
| Face The Music | `.ftm` | `facethemusic.h` | 8 voices, SEL VM. **External-sample mode falls back to silent. TODO** |
| Fashion Tracker | `.ex` | `fashiontracker.h` | French demogroup format, ~14 modules total. Effect 1 (table-step pitch slide). No NostalgicPlayer reference; ported from EaglePlayer asm |
| Fred Editor | `.frd .fred` | `fred.h` | Header format only; `.fred` files in modland are m68k-embedded; not supported |
| Future Composer 1.0..1.4 | `.fc .fc14 .smod` | `futurecomposer.h` | Multi-sample (SSMP) handled. SMOD (FC 1.0..1.3) input is converted to FC14 in memory at load (FC1.3 player ROM wave-length and wave-table data is embedded) |
| Game Music Creator | `.gmc` | `gamemusiccreator.h` | |
| Hippel | `.hip .hipc .hip7` | `hippel.h` | Plain TFMX-marked, COSO-packed, and 7-voice variants. M68k scanner identify |
| IFF SMUS | `.smus` | `iffsmus.h` | Score engine ports cleanly. **External `.instr`/`.ss` files not loaded; voices play silent. TODO** |
| InStereo! 1.0 | `.is .is10` | `instereo10.h` | LED filter is a no-op (TODO) |
| InStereo! 2.0 | `.is .is20` | `instereo20.h` | |
| JamCracker | `.jam` | `jamcracker.h` | |
| MED 1.12 / 2.00 | `.med` | `med.h` | Versions 2 and 3 only (header byte). MED 3.x (version 4) -> see OctaMed (MMD0). MIDI events are skipped |
| Music Assembler | `.ma` | `musicassembler.h` | M68k scanner identify |
| OctaMED MMD0/1/2/3/MMDC | `.med .mmd0 .mmd1 .mmd2 .mmd3 .mmdc .omed .ocss .md0 .md1 .md2 .md3` | `octamed.h` | Multi-octave samples. MMDC RLE-decompressed. Synth + Hybrid playback ported (full SynthHandler), audited 1:1 against C# Player.cs. MIDI / 16-bit / stereo / delta-coded samples / filter-sweep / EffectMaster echo still TODO |
| Oktalyzer | `.okt .okta` | `oktalyzer.h` | 8-channel |
| PumaTracker | `.puma` | `pumatracker.h` | |
| QuadraComposer | `.emod` | `quadracomposer.h` | IFF EMOD |
| Ron Klaren | `.rk` | `ronklaren.h` | M68k scanner identify, sub-songs |
| Sample (8SVX / 16SV / AIFF / WAV) | `.iff .8svx .16sv .iff16 .aiff .aif .wav` | `sample.h` | IFF-8SVX (PCM + Fibonacci-Delta), IFF-16SV, AIFF (8/16/24/32-bit, IEEE-extended freq), RIFF-WAVE (PCM 8/16/24/32-bit, IEEE float 32/64) |
| SidMon 1.0 | `.sd1 .sid1 .sid` | `sidmon10.h` | M68k scanner identify, three period table variants |
| SidMon 2.0 | `.sd2 .sid2 .sid` | `sidmon20.h` | Magic at offset 58 |
| Sonic Arranger | `.sa .sonic` | `sonicarranger.h` | SOARV1.0 native, plus the m68k-embedded "Sonic Arranger Final" form (auto-converted at load). lh.library compressed `@OARV1.0` rejected |
| Sound Control (3.x / 4.0 / 5.0) | `.sc .sct` | `soundcontrol.h` | 4.0/5.0 disambiguation defaults to 5.0 |
| Sound Factory | `.psf` | `soundfactory.h` | |
| SoundFX 1.x / 2.0 | `.sfx .sfx2` | `soundfx.h` | Both `SO31` (31-sample) and `SONG` (15-sample, 1.x) variants. CIA-derived tick rate |
| SoundMon 1.1 / 2.2 | `.bp .bp2 .bp3` | `soundmon.h` | V.2 and V.3 detection |
| Soundtracker (15-sample) | `.mod` | `soundtracker.h` | UST + post-UST 15-sample variants. Effects 1 (arpeggio), 2 (pitch slide), 9 (sample offset), A (vol slide), B (pos jump), C (set vol), D (pat break), F (set speed). Variant-conflicting 0/1/2 effect numbers are UST-gated; 3/4/5/6/7/E not yet wired |
| Synthesis 4.0 / 4.2 | `.syn` | `synthesis.h` | All 16 synth FX, EGC double-buffered |
| TFMX 1.5 / Pro / 7v | `.tfx .mdat .tfm .tfmx` | `tfmx.h` | The test player auto-bundles paired `mdat.<name>` + `smpl.<name>` files into a TFHD container before init. Standalone TFHD/TFMXPAK/TFMX-MOD bundles are accepted directly |
| Voodoo Supreme Synthesizer | `.vss` | `voodoosupremesynthesizer.h` | Track jump-chain loops use a 256-byte visited-set bitmap (position index is `uint8_t`; functionally equivalent to the C# unbounded loop, with cycle termination made explicit) |

## Not yet ported

| Format | Reason |
|---|---|
| ModTracker | Use the user's existing `micromod.h` |
| Sawteeth | Skipped on user request |
| SidPlay | C64 SID; the C# is a libsidplayfp wrapper, would need a separate port |
| Xmp (libxmp wrapper) | Generic tracker engine; many of its formats are already covered by the dedicated ports above |
| MO3 / MPEG / Ogg / Opus / FFmpeg / WMA | Audio codecs, out of scope |
| ProWizard formats | Convert-and-replay system; would need both the converter and ModTracker |
| Module Converter formats (e.g. SC68, Epic UMX, Fred Editor Final, MED 2.10 MED4) | Need conversion to one of the supported native formats first. Future Composer 1.0..1.3 and Sonic Arranger Final are converted in-memory by their respective players |
| Future Composer BSI (`FUCO` magic, `.bsi`) | Tony Bybell's FC-derived variant; only two modules in modland and no reference port available |

## Known limitations / TODO

These are documented places where the port deliberately diverges from the
C# behaviour.

- **DigiBoosterPro**: DSP echo, volume/panning envelopes, backwards
  playback, global DSPE chunk all left out. Largest remaining gap.
- **OctaMed**: Synth/Hybrid playback verified 1:1 with C# Player.cs.
  MIDI handling, filter sweep effect, EffectMaster echo / stereo
  separation, and 16-bit / stereo / delta-coded / packed samples are
  still missing.
- **AMOS Music Bank / Digital Sound Studio / InStereo! 1.0**: Amiga LED
  low-pass filter (effects 0x0600 / 0x0700) is a no-op.
- **Face The Music**: External sample mode falls back to silent.
- **IFF SMUS**: External `.instr` / `.ss` files not loaded; voices silent.
- **Med (1.12/2.00)**: MIDI events silently skipped; the C# may use them
  for tempo control.
- **Voodoo Supreme Synthesizer**: jump-chain `while` loops use a
  256-byte visited-set bitmap to detect cycles. The C# original is
  unbounded but cannot loop forever in practice because the position
  index is `uint8_t`; the visited set is the same termination
  guarantee, expressed explicitly.

## Building the test player

```sh
cd test_player
./build.sh
./test_player path/to/module.<ext>
```

Linux (ALSA via `default` device). Ctrl+C exits cleanly. A Windows
cross-build (`build_win.sh`, mingw-w64) is included; the resulting
`test_player.exe` depends only on `kernel32.dll`, `user32.dll`, and
`winmm.dll`, so it runs on Windows 7 with no installed runtime.

## Style / conventions

All players follow the project house style:

- C99, hard-tab indentation (3-space display), snake_case identifiers.
- Every function `static`. No forward declarations: callees come first.
- Section banner comment above every function, padded to 120 columns.
- No `const` in internal code.
- Explicit C99 integer types (`uint32_t` etc.); never bare `int`.
