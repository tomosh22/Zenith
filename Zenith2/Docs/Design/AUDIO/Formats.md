# AUDIO design: formats

## Scope of this file

This file is part of the AUDIO design family (entry: `Zenith2/Docs/Design/AUDIO.md`). It specifies the two cooked asset formats (`.zsnd`, `.zbank`), the audio codec identifiers, the authoring documents and import settings, and the in-memory records that cross threads (the mixer snapshot, the feedback record) or are written for diagnostics (the capture file, the consumption log). Every cooked file is an ASSET cooked file: the 64-byte header and the 40-byte section entries are ASSET's (`Zenith2/Docs/Design/ASSET/Formats.md` → "Header layout", `Zenith2/Docs/Design/ASSET/Formats.md` → "Section table"), with ASSET's checks applied before AUDIO's; AUDIO defines only the section kinds and their contents. All fields are little-endian; every section starts at a 16-byte aligned offset (ASSET's placement rule); every section is stored with section codec 0 (none). Version 1 of both formats is defined by this text; no file of either format exists before the Stage 3 ticket that writes the first cook step lands, so the corrections design review makes to the version 1 layouts (round 2's widened event enumeration fields and 64-bit mixer table counts among them) are not format version changes. From that ticket on, every layout change bumps the loader desc's `m_uFormatVersion` and re-cooks (`Zenith2/Docs/Design/AUDIO.md` → "Migration hooks").

## Sound file

### Sound file sections

A `.zsnd` file has magic `ZSND`, asset kind 10 (`ASSET_KIND_SOUND`), format version 1 (`Zenith_AssetKindLoaderDesc::m_uFormatVersion = 1`). Section kinds (the section entry's `uint16` kind, meaningful within kind 10 only):

| Section kind | Name | Count | Required | Loaded by the loader |
|---|---|---|---|---|
| 1 | `SOUND_SECTION_HEADER` | 1 | yes | always |
| 2 | `SOUND_SECTION_SAMPLES` | 1 | yes unless the frame count is 0 | RESIDENT modes only; STREAMED sounds read it with `ReadRange` |
| 3 | `SOUND_SECTION_SEEK_TABLE` | 0 or 1 | yes for codec sounds | always when present |
| 4 | `SOUND_SECTION_MARKERS` | 0 or 1 | no | always when present |
| 5 | `SOUND_SECTION_CODEC_SETUP` | 0 or 1 | yes for codec sounds | always when present |

Section order in the file is ascending kind; the samples section is the last section in the file, so a STREAMED sound's resident part (header, seek table, markers, setup) is one contiguous prefix.

### Sound header section

64 bytes:

| Offset | Type | Field | Rule |
|---|---|---|---|
| 0 | u32 | layout version | 1 |
| 4 | u8 | codec | a `Zenith_AudioCodec` value valid in this build |
| 5 | u8 | channel count | 1, 2, 4, 6 or 8 |
| 6 | u8 | channel layout | the `Zenith_AudioChannelLayout` matching the count |
| 7 | u8 | load mode | a `Zenith_AudioLoadMode` value |
| 8 | u32 | sample rate | 48,000 or 24,000 |
| 12 | u32 | flags | bit 0 loop, bit 1 has markers, bit 2 loudness normalised, bit 3 cache eligible (decoded size ≤ 4 s); other bits 0 |
| 16 | u64 | frame count | ≤ 2^40 |
| 24 | u64 | loop start frame | < loop end when bit 0 set, else 0 |
| 32 | u64 | loop end frame | ≤ frame count when bit 0 set, else 0 |
| 40 | f32 | integrated loudness | LUFS, finite, in [−70, 0], or −144 for silence |
| 44 | f32 | true peak | dBTP, finite, ≤ +6 |
| 48 | u32 | block or packet frames | ADPCM: 256; codec: the codec's maximum frames per packet; PCM16: 0 |
| 52 | u32 | codec delay frames | the codec pre-skip; 0 for PCM16 and ADPCM |
| 56 | u32 | seek interval frames | codec: 0.5 s at the sound's rate; others 0 |
| 60 | u32 | marker count | ≤ 64; equals the markers section's record count |

Channel order is the WAVE extensible order: mono C; stereo L R; quad L R Ls Rs; 5.1 L R C LFE Ls Rs; 7.1 L R C LFE Lb Rb Ls Rs.

### PCM16 sample section

`frame count × channel count` signed 16-bit samples, interleaved by frame. Section size = frame count × channels × 2 exactly.

### IMA ADPCM sample section

<!-- elaboration -->
Blocks of 256 frames, the last block zero-padded. Each block holds, per channel in channel order, a 4-byte channel header (`int16` predictor = the block's frame 0, `u8` step index in [0, 88], `u8` 0) followed by 128 bytes of 4-bit codes for frames 1 … 255 (low nibble first; the final nibble is 0). Block bytes = 132 × channels; section size = ceil(frame count / 256) × 132 × channels. Decoding follows the IMA ADPCM step and index tables exactly (`Zenith2/Docs/Design/AUDIO/Streaming.md` → "IMA ADPCM decoding"); a block decodes independently, which gives O(1) random access at 256-frame granularity.

### Compressed codec section

For `AUDIO_CODEC_OPUS` or `AUDIO_CODEC_VORBIS` (only the ruled one is valid): a sequence of packets, each a `u16` byte length (1 … 8,192) followed by the packet bytes, with no container framing. The codec setup section carries what the decoder needs before the first packet (the Opus identification fields: channel count, pre-skip, channel mapping family 0 or 1 and its table; or the three Vorbis header packets, each with a `u32` length prefix). The first packet decodes to frame −(codec delay); the decoder drops the delay frames.

### Seek table section

Present for codec sounds. 24-byte entries, strictly ascending by frame: the regular entries, one at every multiple of the seek interval (frames 0, I, 2I, … below the frame count, I = the header's seek interval), and, for a looping sound, one loop entry at the loop start frame (merged into the regular entry when the loop start is a multiple of I: that entry carries the loop flag). No other entry exists; in particular there is no loop-end entry, because a wrap only seeks to the loop start:

| Offset | Type | Field |
|---|---|---|
| 0 | u64 | frame: the first output frame of the decode run starting at this entry, after pre-roll |
| 8 | u64 | byte offset: of the packet length prefix within the samples section |
| 16 | u32 | pre-roll frames: frames decoded from the entry's packet and discarded to reach `frame` exactly |
| 20 | u32 | flags: bit 0 loop entry (at the loop start); other bits 0 |

A seek to frame f uses the last entry with frame ≤ f, decodes from its byte offset, discards its pre-roll plus (f − frame) frames, and so lands on f exactly; the loop entry makes a loop wrap a seek with zero additional decode beyond pre-roll.

### Markers and loop points section

64-byte records, ascending by frame:

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | marker ID: low 32 bits of `Zenith_Hash64` of the name |
| 4 | u32 | flags: bit 0 caption cue, bit 1 lip-sync phrase start, bit 2 music cue |
| 8 | u64 | frame |
| 16 | 48 bytes | name, UTF-8, NUL-padded, at most 47 bytes |

Markers come from the source file's cue points (CONTENT's importer passes them in the intermediate) or from the bank document; music cues reference markers by name.

### Sound file checks

Applied by `Zenith_AudioFileParse` before any field is used, in this order; the first failing check fails the load with `AUDIO_ERROR_CORRUPT_DATA` and the check number in `m_uDetail`, and the asset activates as the NULL_OBJECT fallback:

| Check | Rule |
|---|---|
| SND1 | exactly one header section of exactly 64 bytes; layout version 1 |
| SND2 | codec valid in this build; channel count in {1, 2, 4, 6, 8} and ≤ `uAUDIO_MAX_SOURCE_CHANNELS`; layout matches the count; sample rate 48,000 or 24,000 (≤ 192,000 is the Architecture parser limit; AUDIO's own cook emits only these two) |
| SND3 | load mode valid; STREAMED only with a codec or PCM16 |
| SND4 | flags carry no undefined bit; frame count ≤ 2^40 |
| SND5 | loop: start < end ≤ frame count when bit 0, both 0 otherwise |
| SND6 | loudness and peak finite and in range |
| SND7 | samples section present iff frame count > 0, and its size equals the codec's exact size rule (PCM16, ADPCM) or is ≥ 3 bytes (codec) |
| SND8 | codec sounds: seek table present and the interval > 0; entries strictly ascending by frame and every byte offset < the samples section size; the entries without bit 0, plus a bit-0 entry whose frame is a multiple of the interval, are exactly the frames k × interval for k = 0 … ceil(frame count / interval) − 1; at most one entry carries bit 0, present iff the loop flag is set and at the loop start frame; so the count is ceil(frame count / interval), plus 1 when the sound loops and its loop start is not a multiple of the interval |
| SND9 | codec sounds: setup section present and parsed by the codec's setup parser (channel count equal to the header's); the header's packet frames in [1, `uAUDIO_MAX_PACKET_FRAMES`] (8,192) |
| SND10 | markers section record count equals the header's marker count; frames ascending and ≤ frame count; names NUL-terminated within 48 bytes |
| SND11 | no section of an unknown kind; no duplicate kind |
| SND12 | codec packet walk (during decode, not at load): every length prefix in [1, 8,192] and within the section; a violation ends the voice (`AUDIO_ERROR_DECODE`) |

The fuzz target `Zenith_Fuzz_SoundFile` runs the parser over mutated files cooked from generated signals; no input may crash, hang or read out of bounds.

## Sound bank file

### Bank sections

A `.zbank` file has magic `ZBNK`, asset kind 11 (`ASSET_KIND_SOUND_BANK`), format version 1. Section kinds:

| Section kind | Name | Count | Required |
|---|---|---|---|
| 1 | `BANK_SECTION_HEADER` | 1 | yes |
| 2 | `BANK_SECTION_SOUND_REFS` | 0 or 1 | when any node plays a sound |
| 3 | `BANK_SECTION_EVENTS` | 0 or 1 | when the bank defines events |
| 4 | `BANK_SECTION_NODES` | 0 or 1 | with events |
| 5 | `BANK_SECTION_CURVES` | 0 or 1 | no |
| 6 | `BANK_SECTION_MUSIC` | 0 or 1 | no |
| 7 | `BANK_SECTION_DIALOGUE` | 0 or 1 | no |
| 8 | `BANK_SECTION_MIXER` | 0 or 1 | exactly in the master bank |
| 9 | `BANK_SECTION_STRINGS` | 1 | yes |
| 10 | `BANK_SECTION_LOCALE_VARIANTS` | 0 or 1 | in a locale-neutral bank whose document lists locale variants |

### Bank header section

64 bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | u32 | layout version, 1 |
| 4 | u32 | flags: bit 0 master bank, bit 1 dialogue bank, bit 2 locale variant (the bank's `locale` is set), bit 3 audition-only (a preview bank written by `Zenith_AudioCookBankPreview`: it never joins the bank list or the event index, `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Bank loader"; the `audio.bank` step never sets it); other bits 0 |
| 8 | u32 | sound reference count |
| 12 | u32 | event count |
| 16 | u32 | node count |
| 20 | u32 | curve count |
| 24 | u32 | music segment count |
| 28 | u32 | music state count |
| 32 | u32 | music transition count |
| 36 | u32 | dialogue line count |
| 40 | u32 | dialogue locale entry count |
| 44 | u32 | locale variant count (§ "Locale variant section") |
| 48 | u32 | locale string offset (into the string table; 0xFFFFFFFF for a locale-neutral bank) |
| 52 | u32 | string table size in bytes |
| 56 | 8 bytes | 0 |

Every count equals its section's record count (check BNK2).

### Sound reference section

24-byte records: the sound's `Zenith_AssetGUID` (16 bytes), `u8` reference flags (bit 0 hard: resident sound loaded with the bank; bit 1 soft: streamed or dialogue sound loaded on demand), `u8` the sound's load mode (copied from the sound's published property so the bank can decide streaming without resolving the sound), `u16` 0, `u32` 0. The cook declares each with `DeclareReference(guid, ASSET_DEPENDENCY_FLAG_HARD or ASSET_DEPENDENCY_FLAG_SOFT)` so the catalogue's dependency list matches the section.

### Event table section

96-byte records, sorted ascending by event ID (binary search at runtime):

| Offset | Type | Field |
|---|---|---|
| 0 | u64 | event ID |
| 8 | u32 | root node index |
| 12 | u16 | node count |
| 14 | u8 | priority (0 … 255) |
| 15 | u8 | spatial mode |
| 16 | u32 | output bus ID |
| 20 | f32 | volume dB [−96, 12] |
| 24 | f32 | pitch cents [−2400, 2400] |
| 28 | u8 | attenuation preset index into the mixer layout (255 = the layout default) |
| 29 | u8 | steal mode in bits 0–2 (`Zenith_AudioStealMode`, the enumerator value 1 … 4 stored as is), virtual behaviour in bits 3–5 (`Zenith_AudioVirtualBehaviour`, 1 … 4 as is), bits 6–7 0; a 3-bit field holds 0 … 7, so `AUDIO_STEAL_MODE_NONE` (4) and `AUDIO_VIRTUAL_BEHAVIOUR_KILL` (4) are storable, and BNK5 rejects 0 and 5 … 7 in either field |
| 30 | u16 | instance limit (0 = unlimited) |
| 32 | u32 | cooldown ms: a post within this time of the previous post of the event in the same world is refused (counted) |
| 36 | f32 | maximum distance override (0 = the preset's) |
| 40 | u8 | flags 2: bit 0 no fade-in (a frame-0 start is unfaded, `noFadeIn`), bit 1 ducks the music bus by 3 dB while playing (a stinger's `musicDuck`), bit 2 full-weight centre routing on 5.1 and 7.1 (`centreFull`, dialogue templates), bit 3 cone filtering (`coneFilter`: the cone lowers the cutoff toward 4 kHz); bits 4–7 0 |
| 41 | u8 | flags: bit 0 looping, bit 1 streamed, bit 2 occlusion enabled, bit 3 Doppler enabled, bit 4 time-scaled, bit 5 pauses with world, bit 6 zone reverb send enabled, bit 7 caption tag present |
| 42 | u16 | first curve index |
| 44 | u16 | curve count |
| 46 | u16 | caption string offset index (into the string table's offset list; 0xFFFF none) |
| 48 | f32 | Doppler factor [0, 5] |
| 52 | f32 | cone inner angle, degrees [0, 360] |
| 56 | f32 | cone outer angle, degrees [inner, 360] |
| 60 | f32 | cone outer gain dB [−96, 0] |
| 64 | f32 | send 0 level dB (−144 = no send) |
| 68 | f32 | send 1 level dB |
| 72 | u32 | send 0 aux bus ID |
| 76 | u32 | send 1 aux bus ID |
| 80 | f32 | spread degrees [0, 360] at minimum distance |
| 84 | f32 | low-pass cutoff Hz (20,000 = off) |
| 88 | f32 | high-pass cutoff Hz (10 = off) |
| 92 | u32 | duration ms (0 = indefinite) |

### Node table section

48-byte records; the children of a node are contiguous (`first child`, `child count`), every node index of an event lies in [root, root + node count), the root's parent is 0xFFFF:

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | node kind (`Zenith_AudioNodeKind`) |
| 1 | u8 | flags: bit 0 loop (SOUND), bit 1 randomise start offset (SOUND), bit 2 stream (SOUND) |
| 2 | u16 | first child index |
| 4 | u16 | child count (SOUND: 0) |
| 6 | u16 | parent index |
| 8 | u32 | sound reference index (SOUND) |
| 12 | f32 | volume dB |
| 16 | f32 | pitch cents |
| 20 | f32 | pitch randomisation ± cents [0, 1200] |
| 24 | f32 | volume randomisation − dB [0, 24] |
| 28 | u32 | start delay ms |
| 32 | u32 | kind field A: RANDOM no-repeat window (0 … child count − 1); SEQUENCE mode (0 step per post, 1 continuous playlist, 2 shuffle); SWITCH and BLEND parameter ID; SOUND loop count (0 = sound's flag, 0xFFFFFFFF = infinite) |
| 36 | f32 | kind field B, read from the child by its parent: RANDOM weight (> 0); SWITCH range minimum; BLEND crossfade curve index (into the bank's curves) |
| 40 | f32 | kind field C: SWITCH range maximum (exclusive); otherwise 0 |
| 44 | u32 | kind field D: SEQUENCE crossfade ms; LAYERED 0; otherwise 0 |

### Parameter curve section

80-byte records: offset 0 `u32` parameter ID; 4 `u8` target (`Zenith_AudioCurveTarget`); 5 `u8` point count (2 … 8); 6 `u8` interpolation (0 linear, 1 smoothstep); 7 `u8` 0; 8 `u16` node index (0xFFFF = the event as a whole); 10 `u16` 0; 12 `u32` 0; 16 eight points of (`f32` x, `f32` y), 64 bytes, unused points 0, with x strictly ascending over the used points; the value outside [x0, xn] clamps to the end points. Targets map y to: VOLUME dB, PITCH cents, LOW_PASS and HIGH_PASS Hz, SEND_0 and SEND_1 dB, BLEND_WEIGHT linear [0, 1].

### Music section

Three tables, in order:

| Table | Record | Fields |
|---|---|---|
| segments | 48 bytes | 0 `u32` sound reference index; 4 `f32` tempo BPM (20 … 300); 8 `u8` beats per bar (1 … 16); 9 `u8` flags (bit 0 loop); 10 `u16` grid in beats (1 … 64); 12 `u32` first music-cue marker index; 16 `u64` entry cue frame (where the downbeat of bar 1 is); 24 `u64` exit cue frame (where the segment may hand over at SEGMENT_END); 32 `u16` cue count; 34 `u16` 0; 36 `u32` 0; 40 `u64` 0 |
| states | 24 bytes | 0 `u32` state ID; 4 `u16` first segment; 6 `u16` segment count; 8 `u8` playlist mode (0 sequential loop, 1 shuffle, 2 hold last); 9 `u8` 0; 10 `u16` 0; 12 `u32` stinger bus ID; 16 `u64` 0 |
| transitions | 40 bytes | 0 `u32` from state ID (0 = any); 4 `u32` to state ID; 8 `u8` rule (`Zenith_AudioMusicTransitionRule`); 9 `u8` flags (bit 0 sync to the source's beat position, bit 1 play the transition segment); 10 `u16` transition segment index (0xFFFF none); 12 `u32` fade-out ms; 16 `u32` fade-in ms; 20 `f32` fade-out curve exponent (1 linear, 2 equal power); 24 `u64` stinger event ID (0 none); 32 `u64` 0 |

### Dialogue table section

Two tables. Lines (40 bytes, sorted by line ID): 0 `u64` line ID; 8 `u64` playback template event ID (an event in the same bank whose properties the line inherits: bus, attenuation, sends); 16 `u32` subtitle string offset; 20 `u32` speaker string offset; 24 `u32` first locale entry; 28 `u16` locale entry count; 30 `u8` priority; 31 `u8` flags (bit 0 positional, bit 1 ducks music and SFX); 32 `f32` duck amount dB; 36 `u32` 0. Locale entries (24 bytes): 0 `u32` locale tag string offset (BCP 47, e.g. `en`, `en-GB`, `fr`); 4 `u32` sound reference index (a soft reference); 8 `u32` duration ms; 12 `u32` caption-cue marker count; 16 `u64` 0.

### Embedded mixer section

Present only in the master bank. Nine tables in this order — buses, effects, attenuation presets, reverb presets, mix snapshots, ducking rules, global parameters, volume categories, bridges — each preceded by a `u64` count (little-endian; BNK9 checks each against its table's cap: buses 64, effects 256 (64 buses × 4), attenuation presets 255 (indices 0 … 254; 255 means the layout default), reverb presets 255, mix snapshots `uAUDIO_MAX_MIX_SNAPSHOTS`, ducking rules `uAUDIO_MAX_DUCKING_RULES`, global parameters `uAUDIO_MAX_GLOBAL_PARAMETERS`, volume categories exactly 6, bridges 64). The section starts 16-aligned, every count is 8 bytes and every record size below is a multiple of 8 (64, 64, 96, 40, 16 + 16 × overrides, 32, 24, 8, 48), so every table starts at an offset ≡ 0 (mod 8) and every `u64` field of every record (the bridges' match value at record offset 8 and target at 16) is naturally aligned in the file. A count that would place a table past the section end fails BNK2:

| Table | Record | Content |
|---|---|---|
| buses | 64 bytes | `u32` bus ID; `u8` parent index (0xFF for `Master`); `u8` volume category (0 none); `u8` flags (bit 0 pauses with world, bit 1 time-scaled, bit 2 aux return, bit 3 audition bus); `u8` aux kind (0 none, 1 reverb, 2 delay); `f32` volume dB; `f32` low-pass Hz; `f32` high-pass Hz; `u16` first effect; `u8` effect count (≤ 4); `u8` aux preset index; `u16` real-voice limit for voices output to this bus (0 unlimited); `u16` 0; then 36 bytes 0 |
| effects | 64 bytes | `u8` effect kind; `u8` bypass; `u16` 0; `f32` parameters [12] (the kind's parameter table of `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Bus effects"); `u32` 0; `u32` 0; `u32` 0 |
| attenuation presets | 96 bytes | 0 `u32` preset ID; 4 `u8` curve; 5 `u8` custom point count; 6 `u16` 0; 8 `f32` minimum distance; 12 `f32` maximum distance; 16 `f32` rolloff; 20 `f32` air absorption distance (0 off); 24 `f32` spread at maximum distance; 28 `u32` 0; 32 eight custom points of (`f32` normalised distance, `f32` dB), 64 bytes, unused points 0 |
| reverb presets | 40 bytes | `u32` preset ID; `f32` RT60 seconds (0.1 … 20); `f32` size (0.5 … 1.5); `f32` damping cutoff Hz; `f32` pre-delay ms (0 … 200); `f32` wet dB; `f32` diffusion (0 … 1); `f32` early level dB; `f32` low cut Hz; `u32` 0 |
| mix snapshots | 16 bytes + overrides | `u32` snapshot ID; `u16` override count; `u8` priority; `u8` 0; `u32` fade-in ms; `u32` fade-out ms; then per override 16 bytes: `u32` bus ID, `f32` volume dB offset, `f32` low-pass Hz, `f32` high-pass Hz |
| ducking rules | 32 bytes | `u32` sidechain bus ID; `u32` ducked bus ID; `f32` threshold dB; `f32` reduction dB; `u32` attack ms; `u32` hold ms; `u32` release ms; `u32` 0 |
| global parameters | 24 bytes | `u32` parameter ID; `f32` minimum; `f32` maximum; `f32` default; `u32` smoothing ms; `u32` 0 |
| volume categories | 8 bytes × 6 | `u8` category; 3 bytes 0; `u32` bus ID the category's user volume applies to |
| bridges | 48 bytes | 0 `u32` event type stable-name string offset; 4 `u32` match property name offset (0xFFFFFFFF = match every event); 8 `u64` match value (a string property's text hash or an integer); 16 `u64` target: the audio event ID (action 0) or the global parameter ID (actions 1 and 2); 24 `u32` property name offset: the time-into-tick property (action 0) or the value property (action 1), 0xFFFFFFFF none (always for action 2); 28 `u32` entity property name offset (0xFFFFFFFF none); 32 `f32` constant value (action 2; 0 otherwise); 36 `u32` ramp ms (actions 1 and 2); 40 `u8` flags (bit 0 follow entity, bit 1 schedule sub-tick); 41 `u8` action (0 post event, 1 set a global parameter from a numeric property, 2 set a global parameter to the constant); 42 6 bytes 0 |

### Locale variant section

Present in a locale-neutral bank whose document lists `localeVariants`. 24-byte records sorted by locale tag string: 0 `Zenith_AssetGUID` of the variant bank (16 bytes, declared as an `ASSET_DEPENDENCY_FLAG_SOFT` reference); 16 `u32` locale tag string offset (BCP 47); 20 `u32` 0. A variant bank is itself a `.zbank` with header flag bit 2 and its `locale` set; its events replace the base bank's events of the same ID while it is active (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Locale selection").

### String table section

UTF-8 strings, each NUL-terminated, referenced by byte offset; the first byte is NUL (offset 0 is the empty string). Event paths are stored in event order so `GetEventPath(i)` is an offset list lookup; the section ends with that `u32` offset list (one per event).

### Bank file checks

| Check | Rule |
|---|---|
| BNK1 | exactly one header of 64 bytes, layout version 1, no undefined flag bit |
| BNK2 | every count equals its section's size divided by its record size exactly |
| BNK3 | events strictly ascending by ID; no event ID 0 |
| BNK4 | every node index, child range, parent link, sound reference index, curve index and string offset within its table; every event's nodes form one tree rooted at its root, depth ≤ 8, count ≤ 64 |
| BNK5 | every enum field a valid value, including both 3-bit fields of event offset 29 (1 … 4 each); every float finite and in its documented range |
| BNK6 | RANDOM children weights > 0; SWITCH ranges non-empty; curves with 2 … 8 points and strictly ascending x |
| BNK7 | music: segment sound indices valid, tempo and grid in range, cues ≤ the sound's frame count is checked when the segment's sound resolves (a cue beyond it clamps and is counted) |
| BNK8 | dialogue lines strictly ascending by ID; locale entries within the table; tags valid BCP 47 syntax (letters, digits, hyphens, ≤ 35 bytes); locale variant records present iff the variant count is non-zero, only in a bank without flag bit 2, tags unique and valid, GUIDs non-null |
| BNK9 | mixer: present iff flag bit 0; every table's `u64` count within its cap (§ "Embedded mixer section"); ≤ 64 buses with exactly one root, parents precede children, depth ≤ 8; ≤ 4 effects per bus; ≤ 4 aux returns; IDs unique within each table; categories each map to an existing bus |
| BNK10 | string table first byte NUL, every string NUL-terminated within the section, UTF-8 valid |
| BNK11 | no unknown or duplicate section kind; every reserved field and every unused curve or attenuation point is 0 |

`Zenith_Fuzz_BankFile` fuzzes this parser.

## Codec identifiers

| ID | Codec | Status | Where decoded | Notes |
|---|---|---|---|---|
| 0 | invalid | never written | — | a header with codec 0 fails SND2 |
| 1 | `AUDIO_CODEC_PCM16` | valid | device thread (direct read) or decode thread (streams) | 16-bit signed LE interleaved |
| 2 | `AUDIO_CODEC_IMA_ADPCM` | valid | device thread (block decode, resident) or decode thread (streams, cache) | 4 bits per sample, 256-frame blocks |
| 3 | `AUDIO_CODEC_OPUS` | reserved for D-SEL-CODEC (open, Stage 2) | decode thread only | valid in loaders and the cook only if the ruling picks Opus |
| 4 | `AUDIO_CODEC_VORBIS` | reserved for D-SEL-CODEC (open, Stage 2) | decode thread only | valid only if the ruling picks Vorbis |
| 5 … 255 | unassigned | — | — | a new codec is a decision, a new ID and a format version bump |

Audio codecs are not `Zenith_Compression` codecs: COMPRESS's section codecs (zlib, zstd) are general-purpose and lossless and never apply to AUDIO sections (Z2-D-004 scopes COMPRESS to block compression), so an AUDIO section's ASSET codec field is always 0 and the audio codec ID lives in AUDIO's own header. The device thread never runs a codec decoder: only PCM16 and ADPCM, whose per-block cost is bounded and allocation-free, are decoded there.

## Authoring documents

### Common document conventions

Both documents follow the Zenith JSON document rules (Z2-D-038; the `zenith-doc` importer passes them through to the cook, `Zenith2/Docs/Design/ASSET/Import.md` → "Boundary with CONTENT importers"): top-level `"schema"` (the stable type name) and `"schemaVersion"` (integer), unknown members preserved by the migrator, assets referenced by GUID string, names as UTF-8 strings. Times in milliseconds, levels in dB, pitch in cents, distances in metres.

### Sound import settings

The per-asset settings the `audio.sound` step reads through `Zenith_CookContext::GetSettings`. The keys are declared in CONTENT's audio importer desc (`Zenith2/Docs/Design/ASSET/Import.md` → "Zenith_ImporterDesc"; cross-area row to CONTENT); their meaning is AUDIO's:

| Key | Type | Default | Values | Meaning |
|---|---|---|---|---|
| `audio.loadMode` | enum | `auto` | `auto`, `residentPcm`, `residentCompressed`, `streamed` | `auto` applies `Zenith2/Docs/Design/AUDIO/Streaming.md` → "Load mode rule" |
| `audio.codec` | enum | `auto` | `auto`, `pcm16`, `adpcm`, `codec` | `codec` = the D-SEL-CODEC codec; `auto` follows the load mode |
| `audio.sampleRate` | int | 48000 | 48000, 24000 | target rate; 24,000 halves memory for band-limited content |
| `audio.channels` | enum | `keep` | `keep`, `mono` | `mono` downmixes (equal-power sum) |
| `audio.normalise` | bool | false | — | gain to reach `audio.targetLufs` integrated loudness, limited so the true peak stays ≤ −1 dBTP |
| `audio.targetLufs` | float | −18.0 | [−36, −10] | normalisation target |
| `audio.bitrateKbps` | int | 96 | [16, 256] | codec bitrate for stereo (mono uses half) |
| `audio.loopStart`, `audio.loopEnd` | int | −1 | frames at the source rate; −1 = from the source's loop chunk, or no loop | explicit loop points |
| `audio.trimSilence` | bool | false | — | trim leading and trailing samples below −72 dBFS |
| `audio.streamThresholdSeconds` | float | 30.0 | [1, 600] | the `auto` rule's stream threshold |

### Bank document schema

`*.bank.json`, schema `Zenith.Audio.BankDocument`, version 1:

```json
{
  "schema": "Zenith.Audio.BankDocument",
  "schemaVersion": 1,
  "locale": "",
  "localeVariants": { "fr": "…guid…", "de": "…guid…" },
  "events": [
    {
      "path": "Footsteps/Grass",
      "bus": "Master/SFX/Footsteps",
      "priority": 96,
      "spatial": "positional",
      "attenuation": "Footstep",
      "volumeDb": -3.0,
      "pitchCents": 0.0,
      "instanceLimit": 6,
      "stealMode": "oldest",
      "cooldownMs": 40,
      "virtualBehaviour": "kill",
      "occlusion": true,
      "doppler": false,
      "noFadeIn": false,
      "musicDuck": false,
      "centreFull": false,
      "coneFilter": false,
      "sends": [ { "bus": "Master/Reverb/Zone", "levelDb": -12.0 } ],
      "caption": "",
      "root": {
        "kind": "random",
        "noRepeat": 2,
        "volumeRandomDb": 2.0,
        "pitchRandomCents": 80.0,
        "children": [
          { "kind": "sound", "sound": "6f1c2a9e-0d7b-4c11-9a35-2f0b8e1d4c77", "weight": 1.0 },
          { "kind": "sound", "sound": "0b3e77d2-55a1-4f0e-8c6b-9e2d10a4b6f3", "weight": 1.0 }
        ]
      },
      "curves": [
        { "parameter": "Speed", "target": "volume", "points": [ [0.0, -12.0], [1.0, 0.0] ] }
      ]
    }
  ],
  "music": {
    "segments": [ { "name": "Explore_A", "sound": "…", "bpm": 96, "beatsPerBar": 4, "gridBeats": 4, "entryCueFrame": 0, "exitCueFrame": 1843200, "loop": true } ],
    "states": [ { "name": "Explore", "segments": [ "Explore_A" ], "playlist": "sequential" } ],
    "transitions": [ { "from": "Explore", "to": "Combat", "rule": "nextBar", "fadeOutMs": 1500, "fadeInMs": 0, "stinger": "Music/CombatHit" } ]
  },
  "dialogue": {
    "lines": [
      { "id": "Intro.Mentor.001", "speaker": "Mentor", "subtitle": "dlg.intro.mentor.001", "template": "Dialogue/Default",
        "priority": 200, "ducks": true, "duckDb": -9.0,
        "locales": { "en": "…guid…", "fr": "…guid…" } }
    ]
  }
}
```

Member rules: `localeVariants` maps BCP 47 tags to variant bank GUIDs and is allowed only in a bank whose `locale` is empty; `noFadeIn`, `musicDuck`, `centreFull` and `coneFilter` are the event's flags 2 bits; `path` matches `[A-Za-z0-9_]+(/[A-Za-z0-9_]+)*`, ≤ 255 bytes, unique across every bank of the game (a cook error otherwise, `Zenith2/Docs/Design/AUDIO/Authoring.md` → "Cook validation rules"); `bus` names a bus path of the mixer document; node `kind` is `sound`, `random`, `sequence`, `switch`, `blend` or `layered`; a `switch` child carries `"range": [min, max]`, a `blend` child `"curve": [[x, w], …]`; enum members use the lower camel case of the C++ enumerators.

### Mixer document schema

`*.mixer.json`, schema `Zenith.Audio.MixerDocument`, version 1. Exactly one per game, referenced by the master bank document's `"mixer"` member (a GUID); the engine default is `Zenith2/Engine/ZenithAudio/Internal/Content/Audio/master.mixer.json`:

```json
{
  "schema": "Zenith.Audio.MixerDocument",
  "schemaVersion": 1,
  "buses": [
    { "path": "Master", "volumeDb": 0.0, "effects": [ { "kind": "limiter", "ceilingDb": -1.0, "releaseMs": 80 } ] },
    { "path": "Master/Music", "category": "music" },
    { "path": "Master/SFX", "category": "sfx", "pausesWithWorld": true, "timeScaled": true },
    { "path": "Master/SFX/Footsteps", "realVoiceLimit": 8 },
    { "path": "Master/Dialogue", "category": "dialogue" },
    { "path": "Master/Ambience", "category": "ambience", "pausesWithWorld": true },
    { "path": "Master/UI", "category": "ui" },
    { "path": "Master/Reverb/Zone", "aux": "reverb", "preset": "Room" },
    { "path": "Master/Audition" }
  ],
  "attenuation": [ { "name": "Footstep", "curve": "inverse", "minDistance": 1.0, "maxDistance": 30.0, "rolloff": 1.0, "airAbsorptionDistance": 0.0 } ],
  "reverbs": [ { "name": "Room", "rt60": 0.8, "size": 1.0, "dampingHz": 6000, "predelayMs": 12, "wetDb": 0.0, "diffusion": 0.7 } ],
  "snapshots": [ { "name": "PauseMenu", "priority": 10, "fadeInMs": 250, "fadeOutMs": 400, "overrides": [ { "bus": "Master/SFX", "volumeDb": -12.0, "lowPassHz": 1200 } ] } ],
  "ducking": [ { "sidechain": "Master/Dialogue", "ducked": "Master/Music", "thresholdDb": -40, "reductionDb": -9, "attackMs": 50, "holdMs": 200, "releaseMs": 400 } ],
  "parameters": [ { "name": "TimeOfDay", "min": 0.0, "max": 24.0, "default": 12.0, "smoothingMs": 0 } ],
  "bridges": [ { "eventType": "Zenith.Animation.Notify", "match": { "property": "NameHash", "value": "Footstep" }, "event": "Footsteps/Grass", "followEntity": true, "subTick": true },
    { "eventType": "Zenith.Environment.WeatherChanged", "match": { "property": "ToNameHash", "value": "Storm" }, "setParameter": "Rain", "value": 1.0, "rampMs": 4000 } ]
}
```

Rules: a bridge with `"setParameter"` and `"valueProperty"` is action 1, with `"setParameter"` and `"value"` (a number, plus an optional `"rampMs"`) action 2, otherwise (with `"event"`) action 0; a bridge's match `value` is text; the cook stores its `Zenith_Hash64` for a 64-bit hash property and the text's string-table form for a `STRING_ID` property; bus paths unique; every parent path present; exactly one root `Master`; the six categories each mapped once; `Master/Audition` present (the cook adds it when absent).

### Attenuation presets

A preset has a curve (`none`, `inverse`, `linear`, `logarithmic`, `custom`), minimum and maximum distance (0 < min < max ≤ 10,000 m), rolloff (0.1 … 10), an air absorption distance (0 off, else 10 … 10,000 m), the spread at maximum distance, and for `custom` up to 8 points of (normalised distance in [0, 1], dB). The formulas are `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Distance attenuation curves". The default preset (index 255 in events) is `inverse`, 1 m, 50 m, rolloff 1.

### Reverb presets

RT60, size, damping, pre-delay, wet, diffusion, early level and low cut as the embedded table defines; the engine default mixer ships `Room` (0.8 s), `Hall` (2.2 s), `Cave` (3.5 s), `Outdoor` (0.4 s, early level −12 dB). The algorithm is `Zenith2/Docs/Design/AUDIO/Mixer.md` → "Reverb".

## Runtime records

### Mixer snapshot record

`Zenith_AudioMixerSnapshot` (Internal), 53,504 bytes, trivially copyable, written by the control side into the triple buffer's back slot and published once per frame. Before the first publication all three slots hold the empty snapshot `Zenith_AudioSystem::Create` writes (epoch 0, voice count 0, bus count and layout pointer of the built-in master-only layout, master and category gain targets 1.0, every other byte 0; `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Creation steps"); the device applies no voice from epoch 0 and the first `Audio.Mix` publishes epoch 1:

| Part | Size | Content |
|---|---|---|
| header | 256 bytes | 0 `u64` epoch (strictly increasing from 1); 8 `u64` publish time (monotonic ns, diagnostic only); 16 `const Zenith_AudioRtLayout*` layout (8 bytes; immutable: bus topology, effect parameters, reverb presets); 24 `u32` voice count; 28 `u32` bus count; 32 `f32` master gain target; 36 `f32` category gains [8] (32 bytes); 68 `u8` output layout; 69 `u8` flags (bit 0 mixer paused, bit 1 capture enabled); 70 `u16` 0; 72 `u64` 0; 80 `f32` aux reverb wet targets [4] (16 bytes); 96 160 bytes 0 |
| voices | 256 × 192 bytes | § "Voice record" |
| buses | 64 × 64 bytes | § "Bus record" |

Only `voice count` voice records and `bus count` bus records are meaningful; the rest are not read. `Zenith_Fuzz_MixerSnapshot` fuzzes the device-side apply step with arbitrary snapshot bytes whose pointers are replaced by valid test buffers, asserting no out-of-bounds access: the apply step clamps every index and count, because a control-side defect must not become a device-thread crash.

### Voice record

192 bytes:

| Offset | Type | Field |
|---|---|---|
| 0 | u16 | device voice slot (0 … 255) |
| 2 | u16 | slot generation (a new generation is a new voice) |
| 4 | u8 | flags: bit 0 paused, bit 1 stop requested, bit 2 loop, bit 3 start scheduled, bit 4 positional |
| 5 | u8 | source kind: 1 resident PCM16, 2 resident ADPCM, 3 decoded cache (float or int16 per bit 7 of flags 2), 4 stream ring, 5 replay feed (test-only: written only by `Zenith_AudioSystemTestAccess::ReplayConsumptionLog`, never by `Audio.Mix`; offset 8 then points at a `Zenith_AudioReplayFeed`, `Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing"; outside a replay, which sets the mixer's replay flag, the device renders a kind-5 voice as silence, like any unknown kind) |
| 6 | u8 | source channels |
| 7 | u8 | output bus index |
| 8 | ptr | sample data base (resident or cache), the stream ring, or the replay feed (kind 5) |
| 16 | u64 | source frame count |
| 24 | u64 | loop start frame |
| 32 | u64 | loop end frame |
| 40 | u64 | scheduled start frame (absolute device frame; 0 = the next block) |
| 48 | u64 | start offset frame (seek position within the source) |
| 56 | f32 | gain target (linear) |
| 60 | f32 | pitch ratio target (source rate / 48,000 × pitch) |
| 64 | f32 | low-pass cutoff Hz target |
| 68 | f32 | high-pass cutoff Hz target |
| 72 | f32 | send gains [2] |
| 80 | u8 | send bus indices [2] |
| 82 | u16 | stream slot (0xFFFF none) |
| 84 | u32 | fade frames for a start (0 = none) or the requested stop fade |
| 88 | u64 | source epoch: the snapshot epoch at which the sample data was published |
| 96 | f32 | pan matrix [2 source channels][8 output channels] (64 bytes; sources with more than 2 channels use the layout's static downmix matrix) |
| 160 | u8 | flags 2: bit 0 time-locked, bit 1 LFE send, bit 7 cache data is float |
| 161 | u8 | 0 |
| 162 | u16 | 0 |
| 164 | f32 | LFE send gain |
| 168 | u64 | time-lock target frame (the source frame the voice should be at when the block whose start is the scheduled frame begins) |
| 176 | 16 bytes | 0 |

### Bus record

64 bytes: `u8` bus index; `u8` flags (bit 0 paused, bit 1 muted, bit 2 soloed); 2 bytes 0; `f32` gain target (category, snapshot and override combined, linear); `f32` low-pass Hz target; `f32` high-pass Hz target; `f32` duck gain target (from the control side's ducking state, applied as a sidechain target on the device for smoothness); `f32` time-scale pitch factor; 40 bytes 0.

### Feedback record

32 bytes, device → control through a `Zenith_SpscRing<Zenith_AudioFeedbackRecord>` over 1,024 preallocated records (`uAUDIO_FEEDBACK_RING_CAPACITY`):

| Offset | Type | Field |
|---|---|---|
| 0 | u8 | kind: 1 voice started, 2 voice ended, 3 stream underrun, 6 marker crossed, 8 clock sample; 4, 5 and 7 are reserved and never pushed (bus meters travel in the meter frame, § "Meter frame record"; the applied epoch and the callback timing are atomics, `Zenith2/Docs/Design/AUDIO/Device.md` → "Lock-free channels") |
| 1 | u8 | 0 |
| 2 | u16 | slot (voice slot or stream slot) |
| 4 | u16 | generation |
| 6 | u16 | 0 |
| 8 | u64 | device frame at which it happened |
| 16 | u64 | value A: voice ended → the source frame reached; marker → the marker ID; clock sample → `Zenith_GetMonotonicTimeNs` at the callback's start; otherwise 0 |
| 24 | u64 | value B: underrun → missing frames; clock sample → the frames the callback was asked for; otherwise 0 |

For a clock sample the device frame at offset 8 is the frame counter at the callback's start, so the clock fit receives one (frame, time) pair per callback as one record, never two torn atomics (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "The device clock").

With meters out of the ring, its steady traffic is one clock sample per callback: at most 48,000 / 96 = 500 per second for 96-frame AAudio bursts and 100 per second for 480-frame WASAPI periods, plus voice start, end, underrun and marker records. With no `Audio.Mix` for 300 ms (a main-thread stall) that is at most 150 clock samples plus the voice records, well under 1,024, so marker records (the source of `WhenMarker` and of caption cues, which the slot-state table cannot recover) are not dropped; only a stall of about 2 s on 96-frame bursts fills the ring.

### Meter frame record

`Zenith_AudioMeterFrame` (Internal), 1,040 bytes, trivially copyable, written by the device every 4th block into the back slot of a `Zenith_TripleBuffer<Zenith_AudioMeterFrame>` and published; `Audio.Mix` M1 acquires the latest one. A superseded frame is lost and counted in `audio.meters.dropped` (meters are display only: the peak hold and fall of `Zenith2/Docs/Design/AUDIO/PublicAPI.md` → "Status and meters" smooth over a lost frame). `Zenith_AudioSystem::Create` writes a frame with bus count 0 into all three slots.

| Offset | Type | Field |
|---|---|---|
| 0 | u64 | the device block index of the frame's last block |
| 8 | u32 | bus count (≤ 64) |
| 12 | u32 | 0 |
| 16 | 64 × 16 bytes | per bus in the snapshot's bus order: `f32` peak [2] and `f32` RMS [2], linear, over the 1,024 frames of the 4 blocks (left and right, or the first two channels) |

1,040 = 16 + 64 × 16; three slots are 3,120 bytes, allocated at create.

### Capture file

A RIFF WAVE file, `WAVE_FORMAT_IEEE_FLOAT` (3), 48,000 Hz, the output layout's channel count, 32-bit float, interleaved; for 5.1 and 7.1 the `WAVE_FORMAT_EXTENSIBLE` header with the channel mask of the layout. The device thread copies each finished block into a preallocated capture ring (seconds × 48,000 × channels × 4 bytes); the control thread drains it and writes the file through PLAT's atomic write on the user mount. A full capture ring drops blocks and records the gap count in the file's `LIST`/`INFO` `ICMT` chunk.

### Consumption log record

16 bytes, test-only (`Zenith_AudioSystemTestAccess::EnableConsumptionLog`), two kinds:

| Offset | Type | Kind 0: epoch applied | Kind 1: stream underrun |
|---|---|---|---|
| 0 | u64 | bits 0–55 the block index since start; bits 56–63 the kind (0) | bits 0–55 the block index; bits 56–63 the kind (1) |
| 8 | u64 | the snapshot epoch the block applied (0 when it found no new snapshot) | bits 32–47 the stream slot, bits 0–31 the missing frames, bits 48–63 0 |

The device thread writes one kind-0 record per block and one kind-1 record per underrun (in the block where it happened, after that block's kind-0 record) into a caller-provided array (no allocation; a full array stops logging and sets the test's overflow flag). The replay renders one block per `RenderBlocks` call, applies the archived snapshot of each recorded epoch at its block, and feeds every stream voice from a replay feed (voice source kind 5) whose underrun frames for the block are set from that block's kind-1 records, so the feed serves the available frames, then silence, and skips the missing frames once (`Zenith2/Docs/Design/AUDIO.md` → "Bit-exact offline mixing").
