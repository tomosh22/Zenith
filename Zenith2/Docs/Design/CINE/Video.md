# CINE video, capture and export

## Scope of this file

Video playback (Z2-I-CINE-10), capture (Z2-I-CINE-11) and export (Z2-I-CINE-12): the decoder selection D-SEL-VIDEO (open, Stage 2) that CINE owns, the internal decoder interface and its backends, the video thread, the players, frame selection, the GPU side in `ZenithFlux`, the composite pass, texture surfaces, audio and video sync, failure handling, the Android lifecycle, and the capture and export algorithms. Video is presentation only: no decoder state, decode timing, frame availability or memory pressure ever reaches the simulation (Z2-D-019, Z2-D-050; that video is isolated is CINE's elaboration of Z2-D-019). The cooked video format is `Zenith2/Docs/Design/CINE/Formats.md` → "Cooked video file".

## Video pipeline overview

| Stage | Thread | Owner | Input | Output |
|---|---|---|---|---|
| 1 sequence evaluation | job worker (phase 1 and phase 12) | `Cinematics.Evaluate`, `Cinematics.ExtractRender` | the instance's time | one `Flux_VideoDrawDesc` per active video section in the render world |
| 2 player control | render-submit | `Zenith_VideoPlayerTable` (CINE, behind `Flux_VideoFrameSource`) | the consumed render world's draw records | open, seek and close commands to the video thread |
| 3 demux and decode | `Zenith.VideoDec` (CINE's dedicated thread) | the decoder backend | bitstream ranges read through `Zenith_AssetReader::ReadRange` | decoded CPU frames (NV12, P010 or I420) in the player's frame pool |
| 4 frame hand-off | video thread ↔ render-submit | per player a frame ring and a return ring, per module an event ring (`Zenith_SpscRing`) | decoded frames; released buffer indices; stream events | `Flux_VideoFrameSource::AcquireFrame` results; free buffers; player state changes |
| 5 upload | render-submit | `Flux_VideoSystem` | acquired frame planes | plane textures through the upload ring |
| 6 conversion | GPU, common prologue row 2b | `VideoConvert` compute pass | plane textures (the crop rectangle) | per-player linear RGBA16F texture (the surface) |
| 7 composite | GPU, common epilogue row 6b | `VideoComposite` pass | surfaces, overlay records | the display-referred output |

## D-SEL-VIDEO candidates and criteria

D-SEL-VIDEO (open, Stage 2) selects the decoder backend. CINE owns the selection; it is decided by two tickets (Z2-D-084: a harness ticket plus a `needs-human` ruling ticket):

| Ticket | Kind | Content |
|---|---|---|
| "D-SEL-VIDEO decoder harness" | worker, Stage 2 | builds `Zenith2/Benchmarks/VideoDecoderSelection/` with `zenith_add_benchmark(VideoDecoderSelection DEPENDS ZenithCore ZenithPlatform THIRD_PARTY ffmpeg-libavcodec SUPPORTED ZENITH_WINDOWS ZENITH_ANDROID)`: the executable `Zenith_Bench_VideoDecoderSelection` and its CTest `--smoke` run (`Zenith2/Docs/Architecture/BuildSystem.md` → "Tools, benchmarks and integration suites"). Its sources: `Zenith_VideoDecoderSelection.cpp` (the measurement driver of V-C1 … V-C9 below, one JSON report per run under `${ZENITH_OUT}`), `Internal/Zenith_SelectionVideoDecoder.h` (an interface with exactly the members and contract of "Zenith_VideoDecoder interface", so the backends move into ZenithCinematics unchanged), `Internal/Zenith_SelectionMp4.h/.cpp` (the sample-table reader of the box subset of "Video importer", `Zenith2/Docs/Design/CINE/Authoring.md`), `Internal/Windows/Zenith_SelectionDecoderMF.cpp`, `Internal/Android/Zenith_SelectionDecoderMediaCodec.cpp`, `Internal/FFmpeg/Zenith_SelectionDecoderFFmpeg.cpp`. The same diff adds the manifest entry `ffmpeg-libavcodec` (LGPL 2.1 build, libavcodec and libavutil only, no GPL components; `owner` `Zenith_Bench_VideoDecoderSelection`, `usage` `tools`, the one benchmark owner a selection harness may have, `Zenith2/Docs/Architecture/BuildSystem.md` → "The manifest dependencies section"). The smoke run decodes a clip the setup step synthesises (the `TestCodecClip` shape of `Zenith2/Docs/Design/CINE/Tests.md` → "Fixtures" is not decodable by real decoders, so the smoke checks only that every backend opens, reports its caps and closes); it records no measurement (WORKER-EVIDENCE) |
| "D-SEL-VIDEO ruling" | `needs-human`, Stage 2 | runs the harness on the reference desktop and the S24 Ultra (`Zenith2/Docs/Budgets.md` → "Reference hardware") with the CinematicSlice video set, writes `Zenith2/Docs/Research/CINE/VideoDecoderSelection.md` (per platform, per candidate, per criterion: the measured value and pass or fail), applies the acceptance rule below, appends the ruling to `Zenith2/Docs/DecisionLog.md` with the next free `Z2-D` number in its own diff (Z2-D-097), and sets `cine.video.backend`'s admitted values per platform |

The Stage 4 platform-decoder ticket (slice 20 of `Zenith2/Docs/Design/CINE.md` → "Ticket slices and order") moves the harness's platform backends into `Zenith2/Engine/ZenithCinematics/Internal/Windows/` and `Internal/Android/` (a move of the files, renamed `Zenith_VideoDecoderMF.cpp` and `Zenith_VideoDecoderMediaCodec.cpp`, not a rewrite) and deletes `Zenith2/Benchmarks/VideoDecoderSelection/` with its manifest entry unless the ruling admitted candidate B, whose backend then moves to `Internal/FFmpeg/` and whose entry's owner becomes `ZenithCinematics`. The importer ticket (slice 17) writes ZenithCook's MP4 reader starting from the harness's reader.

| Candidate | Windows | Android (S24) | Licence | Lives in after the ruling |
|---|---|---|---|---|
| A: platform decoders | the synchronous Microsoft H.264/HEVC decoder MFT (`IMFTransform`, enumerated with `MFT_ENUM_FLAG_SYNCMFT`) driven directly with CINE's demuxed samples, hardware-accelerated by DXVA through the DXGI device manager of a D3D11 device the backend creates, output copied to system memory NV12/P010 ("Platform decoder backend"); asynchronous vendor MFTs are not part of candidate A | `AMediaCodec` (NDK, minSdk 34) in synchronous mode, output to an `AImageReader` (`AIMAGE_FORMAT_YUV_420_888`), copied into I420 or NV12 | OS components, no redistribution | `Engine/ZenithCinematics/Internal/Windows/`, `Internal/Android/` |
| B: FFmpeg (libavcodec only, LGPL build, no GPL components, static link per Architecture's dependency table) with platform hardware acceleration where libavcodec offers it (`d3d11va`, `mediacodec`) | libavcodec | libavcodec | LGPL 2.1 — admitted only if the ruling records the relinking obligation as met for static linking | `Engine/ZenithCinematics/Internal/FFmpeg/`, third-party confinement (`Zenith2/Docs/Contracts.md` → "Third-party confinement") |

Criteria, each with its pass rule, measured per platform:

| # | Criterion | Pass rule |
|---|---|---|
| V-C1 | hardware decode of the CinematicSlice video set (1080p30 and 1080p60 H.264 High, one 2160p30 clip) | every clip decodes at ≥ 1.25 × real time with ≤ 1 dropped frame per minute |
| V-C2 | CPU cost of decode plus copy on the video thread | within the pending row "Video decode CPU" (CN12) at the proposed value |
| V-C3 | memory for decode-ahead buffers and decoder internals | within `Z2-B-CINE-001`'s proposed CPU share |
| V-C4 | output usable by the GPU path | frames reach the conversion pass with ≤ 2 frame periods of latency from decode completion |
| V-C5 | seek | a seek to any time lands on the target frame in ≤ 250 ms (1080p) |
| V-C6 | A/V sync | "Audio and video sync" holds |
| V-C7 | licence | candidate A always passes; B passes only with the human ruling on LGPL static linking |
| V-C8 | build and package cost | Android package growth ≤ 1 MB (A: 0; B: measured), within `Z2-B-PACK-001` |
| V-C9 | export encode on the Windows host (optional) | an H.264 encoder is available to `Zenith_ExportCinematicCapture` without GPL components (A: the MF H.264 encoder MFT; B: none in an LGPL build) |

Acceptance rule, per platform (Windows, Android):

1. A candidate is eligible on a platform when it passes V-C1 to V-C8 there. The ruling selects, per platform, candidate A when it is eligible, otherwise B when B is eligible; so A on one platform and B on the other is a valid outcome, and `cine.video.backend`'s admitted values and default are recorded per platform.
2. When no candidate passes V-C1 on a platform but one passes V-C2 to V-C8 there, the ruling records the reduced video set (resolution and frame rate) that passes V-C1 with that candidate, and CinematicSlice's video content is cooked to it.
3. When no candidate passes some criterion other than V-C1 on a platform, D-SEL-VIDEO stays open for that platform: the ruling records the failing criterion and the measured values, and video stays on candidate A's Stage 4 baseline there until a new ruling.
4. V-C9 never decides the backend; it decides only whether `Zenith_CinematicExportDesc::m_bEncodeVideo` is admitted (the recorded encoder, or refusal).

Until the ruling, Stage 4 ships candidate A, which is the configuration default; candidate B is not compiled into ZenithCinematics.

## Zenith_VideoDecoder interface

Internal to `ZenithCinematics` (`Engine/ZenithCinematics/Internal/Video/Zenith_VideoDecoder.h`); every backend implements it; the video thread is its only caller.

```cpp
/// @contract interface Zenith_VideoDecoder
/// Ownership: Created as Zenith_Owned<Zenith_VideoDecoder> at module Start step 6 by the module desc's factory
///   (Zenith_CreatePlatformVideoDecoder in production, Zenith_CreateTestVideoDecoder in tests); destroyed by the module
///   after the video thread is joined.
/// Thread affinity: OWNER_THREAD — the Zenith.VideoDec thread after creation (or the test's thread under PumpOnce).
/// Invalidation: A stream handle is invalid after CloseStream.
/// Units: Microseconds; pixels; bytes.
/// Errors: Zenith_CinematicError: VIDEO_UNSUPPORTED (codec, profile, bit depth, size), VIDEO_DECODE (a stream error),
///   VIDEO_DEVICE (D3D11 device removal, a reclaimed codec), OUT_OF_MEMORY.
/// Reentrancy: NOT_APPLICABLE. It invokes no callback.
/// Version: since Z2 Stage 4. Internal; not a public header.
/// @endcontract
class Zenith_VideoDecoder
{
public:
	virtual ~Zenith_VideoDecoder() = default;
	virtual Zenith_Result<Zenith_VideoCaps, Zenith_CinematicError> QueryCaps(Zenith_VideoCodec eCodec) = 0;
	virtual Zenith_Result<uint32, Zenith_CinematicError> OpenStream(const Zenith_VideoInfo& xInfo,
		Zenith_ByteSpan xDecoderConfig) = 0;                                       // returns a stream handle
	// Submits one access unit (one frame's NAL units, length-prefixed) with its presentation time; false when the decoder
	// cannot accept input now (resubmitted at the next loop iteration).
	virtual Zenith_Result<bool, Zenith_CinematicError> Submit(uint32 uStream, Zenith_ByteSpan xAccessUnit,
		uint64 ulPtsUs, bool bKeyframe) = 0;
	// Writes the next decoded frame in presentation order into xOut (planes into xOut's buffer, cropped as below); false
	// when none is ready.
	virtual Zenith_Result<bool, Zenith_CinematicError> Receive(uint32 uStream, Zenith_VideoDecodedFrame& xOut) = 0;
	virtual void Flush(uint32 uStream) = 0;                                      // drops queued input and output (seek)
	virtual void Drain(uint32 uStream) = 0;                                      // end of stream: outputs every held frame
	virtual void CloseStream(uint32 uStream) = 0;
	virtual void Suspend() = 0;                                                   // Android background: release codecs
	virtual Zenith_Result<void, Zenith_CinematicError> Resume() = 0;
};

struct Zenith_VideoCaps { uint32 m_uMaxWidth = 0; uint32 m_uMaxHeight = 0; uint32 m_uMaxFps = 0; bool m_bHardware = false; bool m_b10Bit = false; };
struct Zenith_VideoDecodedFrame
{
	Flux_VideoPixelLayout m_eLayout;
	uint32 m_uWidth, m_uHeight;             // the stored plane dimensions in the buffer
	uint32 m_auCrop[4];                     // x, y, width, height of the displayed region, even values (Flux_VideoFrameView::m_auCrop)
	uint32 m_auStrides[3];
	Zenith_MutableSpan<uint8> m_xBuffer;    // the frame pool buffer the caller provides, ≥ the layout's size
	uint64 m_ulPtsUs;
};
```

At most `uCINE_MAX_VIDEO_PLAYERS` (4) streams are open at a time. A test backend `Zenith_TestVideoDecoder` (`Engine/ZenithCinematics/Tests/Support/Zenith_TestVideoDecoder.h`, header-only per X184, with the factory `Zenith_CreateTestVideoDecoder`) decodes the test codec of "Fixtures" in `Zenith2/Docs/Design/CINE/Tests.md` (frames whose pixels encode their own index), and offers fault points (stall N frames, fail frame k, report device loss) so every test above the decoder runs on CI hosts without hardware decoders. It is selected only through the module's test construction seam (`Zenith2/Docs/Design/CINE/PublicAPI.md` → "Zenith_Module_Cinematics header"); it implements an interface, never a double of a concrete class (FINAL-DOUBLES).

## Platform decoder backend

**Windows** (`Internal/Windows/Zenith_VideoDecoderMF.cpp`):

1. **Backend creation.** `MFStartup(MF_VERSION, MFSTARTUP_LITE)`; `D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, …, D3D11_CREATE_DEVICE_VIDEO_SUPPORT | D3D11_CREATE_DEVICE_BGRA_SUPPORT, …)`; `ID3D10Multithread::SetMultithreadProtected(TRUE)`; `MFCreateDXGIDeviceManager(&uToken, &pxManager)` and `ResetDevice(pxDevice, uToken)`. The D3D11 device is private to the decoder; nothing is shared with the RHI device, so device loss of either never affects the other. Destruction releases in reverse and calls `MFShutdown`.
2. **Stream open (one model: synchronous MFT with DXVA).** Hardware decoder MFTs registered by GPU vendors are asynchronous (`MF_TRANSFORM_ASYNC` = TRUE): they return `MF_E_TRANSFORM_ASYNC_LOCKED` until a client sets `MF_TRANSFORM_ASYNC_UNLOCK` and are driven by `IMFMediaEventGenerator` events, not by polled `ProcessInput`/`ProcessOutput`. Candidate A does not use them. It calls `MFTEnumEx(MFT_CATEGORY_VIDEO_DECODER, MFT_ENUM_FLAG_SYNCMFT | MFT_ENUM_FLAG_LOCALMFT | MFT_ENUM_FLAG_SORTANDFILTER, {MFMediaType_Video, MFVideoFormat_H264 or MFVideoFormat_HEVC}, nullptr, …)`, which returns the synchronous Microsoft decoders, and activates the first whose attributes do not report `MF_TRANSFORM_ASYNC` = TRUE (an asynchronous entry is skipped, logged once). That decoder is D3D11-aware (`MF_SA_D3D11_AWARE` = TRUE): `ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, (ULONG_PTR)pxManager)` with the backend's DXGI device manager makes it decode in hardware through DXVA, and its success sets `Zenith_VideoCaps::m_bHardware`; when the message fails (`E_NOTIMPL`, or no DXVA profile for the stream), the same MFT decodes in software into system-memory buffers, and V-C1's measurement records which path ran.
3. **Input type.** `MF_MT_MAJOR_TYPE` video, `MF_MT_SUBTYPE` H264/HEVC, `MF_MT_FRAME_SIZE` (coded size from the SPS), `MF_MT_FRAME_RATE` (num/den), `MF_MT_INTERLACE_MODE` progressive, and `MF_MT_MPEG_SEQUENCE_HEADER` = the parameter sets in Annex B form (`00 00 00 01` before each SPS and PPS of the `avcC` record; VPS, SPS and PPS of `hvcC`). The same Annex B parameter sets are also prefixed to the first IDR access unit after open and after every seek, for decoders that ignore the sequence header.
4. **Output type and streaming.** Enumerate `GetOutputAvailableType(0, i)` and set the first with subtype `MFVideoFormat_NV12` (8-bit) or `MFVideoFormat_P010` (10-bit) through `SetOutputType`; read `GetOutputStreamInfo(0)` (whether the MFT provides samples, `MFT_OUTPUT_STREAM_PROVIDES_SAMPLES`, and otherwise the sample size `cbSize` for a caller-allocated `MFCreateMemoryBuffer` sample); then `ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0)` and `ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0)`. `ProcessOutput` returning `MF_E_TRANSFORM_STREAM_CHANGE` re-runs this step's type negotiation and re-reads the output frame size and the aperture. The interface's members map to messages as follows: `Flush` = `MFT_MESSAGE_COMMAND_FLUSH` (then `NOTIFY_START_OF_STREAM` before the next input); `Drain` = `MFT_MESSAGE_NOTIFY_END_OF_STREAM`, then `MFT_MESSAGE_COMMAND_DRAIN`, and sets the stream's draining flag; it returns at once, because it has no output parameter. The held frames then come out through `Receive` like any other (`ProcessOutput`), and `MF_E_TRANSFORM_NEED_MORE_INPUT` while draining marks the stream's end (`Receive` returns false and the stream is at end until the next seek); a SEEK or a loop after a drain begins with `MFT_MESSAGE_COMMAND_FLUSH` and `MFT_MESSAGE_NOTIFY_START_OF_STREAM`, which clear the flag. `CloseStream` = `MFT_MESSAGE_NOTIFY_END_STREAMING`, then the transform is released.
5. **Crop.** The displayed rectangle is the output type's `MF_MT_MINIMUM_DISPLAY_APERTURE` (an `MFVideoArea`: offsets `OffsetX.value`, `OffsetY.value`, size `Area.cx`, `Area.cy`), else `MF_MT_GEOMETRIC_APERTURE`, else the whole frame; each value is rounded down to even. Hardware decoders output aligned heights (1088 for 1080p), so without it the frame would carry garbage rows.
6. **Feed.** Each access unit is converted to Annex B (each 4-byte length prefix replaced by `00 00 00 01`) into an `IMFSample` with `SetSampleTime(pts × 10)` (100 ns units) and `MFSampleExtension_CleanPoint` on keyframes; `ProcessInput` returning `MF_E_NOTACCEPTING` makes `Submit` return false (outputs are drained first).
7. **Output.** `ProcessOutput`: `MF_E_TRANSFORM_NEED_MORE_INPUT` → `Receive` returns false. For a D3D11-aware MFT (`MFT_OUTPUT_STREAM_PROVIDES_SAMPLES`) the sample's buffer is queried for `IMFDXGIBuffer`: `GetResource(IID_ID3D11Texture2D)` gives the decoder's texture array and `GetSubresourceIndex(&uSubresource)` the slice; `ID3D11DeviceContext::CopySubresourceRegion(pxStaging, 0, 0, 0, 0, pxDecoderTexture, uSubresource, &xCropBox)` copies only the crop (`D3D11_BOX { left, top, 0, right, bottom, 1 }` of step 5) into a staging texture of the crop size (format NV12 or P010, `D3D11_USAGE_STAGING`, `D3D11_CPU_ACCESS_READ`, created once per stream and recreated when the crop changes); `Map(pxStaging, 0, D3D11_MAP_READ, 0, &xMapped)` then copies the Y rows (`xMapped.RowPitch`) and the CbCr rows (which follow at `RowPitch × crop height`) into the pool buffer, and `Unmap`. The crop is then the whole buffer (`m_auCrop = {0, 0, w, h}`). The software MFT's buffer is read with `IMF2DBuffer::Lock2D` and the crop applied while copying. The sample time divided by 10 is the frame's presentation time.
8. **Errors.** `DXGI_ERROR_DEVICE_REMOVED` or `DXGI_ERROR_DEVICE_RESET` from `Map` or `ProcessOutput`, and `MF_E_DXGI_DEVICE_NOT_INITIALIZED`, are `CINEMATIC_ERROR_VIDEO_DEVICE`; any other failing `HRESULT` is `CINEMATIC_ERROR_VIDEO_DECODE`.

**Android** (`Internal/Android/Zenith_VideoDecoderMediaCodec.cpp`), synchronous mode only, so no NDK callback thread exists:

1. **Stream open.** `AImageReader_new(w, h, AIMAGE_FORMAT_YUV_420_888, uMaxImages = 4, &pxReader)` with the coded size; `AImageReader_getWindow` gives the output surface. `AMediaCodec_createDecoderByType("video/avc" | "video/hevc")`; an `AMediaFormat` with `AMEDIAFORMAT_KEY_MIME`, `AMEDIAFORMAT_KEY_WIDTH`, `AMEDIAFORMAT_KEY_HEIGHT`, `csd-0` = the Annex B SPS (AVC) or VPS + SPS + PPS (HEVC) and `csd-1` = the Annex B PPS (AVC); `AMediaCodec_configure(codec, format, window, nullptr, 0)`; `AMediaCodec_start`. Only 8-bit content plays: `AIMAGE_FORMAT_YUV_420_888` is 8-bit, so a 10-bit stream is `CINEMATIC_ERROR_VIDEO_UNSUPPORTED` here (and refused at cook for an Android game, `cine.cook.video_10bit_android`).
2. **Input.** `AMediaCodec_dequeueInputBuffer(codec, 0)` (0 µs timeout): an index ≥ 0 → `AMediaCodec_getInputBuffer`, copy the access unit converted to Annex B, `AMediaCodec_queueInputBuffer(codec, i, 0, size, ptsUs, bKeyframe ? AMEDIACODEC_BUFFER_FLAG_KEY_FRAME : 0)`; −1 (`AMEDIACODEC_INFO_TRY_AGAIN_LATER`) makes `Submit` return false. End of stream queues an empty buffer with `AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM`.
3. **Output.** Only while the player's frame pool has a free buffer and no image is pending in the reader: `AMediaCodec_dequeueOutputBuffer(codec, &xInfo, 0)`: an index ≥ 0 → push `xInfo.presentationTimeUs` into the stream's 8-entry pending-timestamp FIFO and `AMediaCodec_releaseOutputBuffer(codec, i, true)` (render to the reader surface); `AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED` (−2) → read the new format's display crop (`AMEDIAFORMAT_KEY_DISPLAY_CROP`) for the stream; `AMEDIACODEC_INFO_OUTPUT_BUFFERS_CHANGED` (−3) and −1 → nothing. Holding output buffers is the back-pressure: the codec stalls while CINE has no free buffer.
4. **Image.** `AImageReader_acquireNextImage(pxReader, &pxImage)`: `AMEDIA_OK` → `AImage_getTimestamp(pxImage, &iNs)` gives the presentation time in ns, and `iNs / 1000` must equal the FIFO's head (the surface timestamp is the buffer's `presentationTimeUs × 1000`), which is popped; `AImage_getCropRect` gives the crop (rounded down to even); `AImage_getPlaneData`, `AImage_getPlaneRowStride` and `AImage_getPlanePixelStride` give the planes, and the crop is copied into the pool buffer as I420 (chroma pixel stride 1) or NV12 (chroma pixel stride 2 with the V plane at U + 1); `AImage_delete` at once, so at most one image is ever acquired (`AMEDIA_IMGREADER_MAX_IMAGES_ACQUIRED` cannot occur); `AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE` → `Receive` returns false. A timestamp that matches no FIFO entry drops the image and counts `cine.video.timestamp_mismatch_delta`. **Flush** (a seek): `AMediaCodec_flush`, then clear the pending-timestamp FIFO, then acquire and delete every image already rendered to the reader (`AImageReader_acquireNextImage` and `AImage_delete` until `AMEDIA_IMGREADER_NO_BUFFER_AVAILABLE`), so no frame decoded before the seek reaches the frame ring; **Drain** queues the empty end-of-stream input buffer of step 2 and sets the draining flag, and `AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM` on an output buffer marks the stream's end.
5. **Errors and reclaim.** A `media_status_t` below 0 from `queueInputBuffer`, `releaseOutputBuffer`, `start` or `flush`, or an index below −3 from a dequeue — which is how a codec reclaimed by the system (`AMEDIACODEC_ERROR_RECLAIMED`, `AMEDIACODEC_ERROR_INSUFFICIENT_RESOURCE`) or invalidated (`AMEDIA_ERROR_INVALID_OBJECT`) reports in synchronous mode — is `CINEMATIC_ERROR_VIDEO_DEVICE`: the player suspends, the thread recreates the stream (steps 1 to 4) and the player seeks to its media time ("Video players"); a second device error within 2 s moves the player to `FAILED`. `AMEDIA_ERROR_MALFORMED` is `CINEMATIC_ERROR_VIDEO_DECODE`.

## FFmpeg decoder backend

Compiled only when the D-SEL-VIDEO ruling admits candidate B: `Internal/FFmpeg/Zenith_VideoDecoderFFmpeg.cpp`, `avcodec_send_packet` / `avcodec_receive_frame`, hardware device contexts `d3d11va` and `mediacodec` when present, the crop from the frame's `crop_*` fields, `sws` not used (the GPU converts). The library is confined to `Internal/FFmpeg/` and its manifest entry is the harness's, re-owned by the ruling's diff.

## Video thread

`Zenith.VideoDec`, one `Zenith_Thread` (`Zenith2/Docs/Design/CORE/Threading.md` → "Threads") created at module Start step 6 and joined at module shutdown (row X293 of the entry file adds it to `Zenith2/Docs/Contracts.md` → "Shutdown ownership order"). It takes no lock, runs no job and touches no simulation state. Its channels are CORE's `Zenith_SpscRing<T>`, the one single-producer single-consumer primitive of the programme (the Q80 ruling; the contract is `Zenith2/Docs/Design/AUDIO.md` → "Assumed interfaces pending other areas", row AU6: capacity a power of two in [2, 2²⁰], no peek, `TryPush` counts a drop when full, `linux-tsan` stress tests in CORE):

| Ring | Element | Capacity | Producer | Consumer | Full policy |
|---|---|---|---|---|---|
| command ring (one per module) | `Zenith_VideoThreadCommand` (OPEN with the asset, the target, pool and ring pointers; SEEK with the target and the `uint16` seek epoch; CLOSE; 48 bytes; the target is the presentation time of the frame due at the media time m, `Zenith_VideoAsset::FindFrameAtOrBefore(m)`'s pts, computed by the render-submit thread) | 64 | render-submit thread | video thread | the producer checks `GetWritableCount() > 0` first; with no room it keeps the command for the next frame and counts `cine.video.command_full_delta` (the ring's own dropped count stays 0) |
| lifecycle ring (one per module) | `Zenith_VideoLifecycleCommand` (SUSPEND, RESUME; 4 bytes) | 4 | main thread (the frame listener) | video thread | cannot fill: at most one SUSPEND and one RESUME are outstanding, coalesced by the producer |
| event ring (one per module) | `Zenith_VideoThreadEvent` (`OPENED`, `OPEN_FAILED`, `CLOSED`, `SUSPENDED`, `RESUMED`, `DEVICE_LOST`, `FAILED`; player index (1), event (1), error code (1), reserved (1), presentation time (8); 16 bytes) | 128 | video thread | render-submit thread | never full: the video thread does a unit of work that can produce events (pop a command or a lifecycle command, handle a stream error) only while `GetWritableCount()` ≥ 5 (one event per player plus one), and the render-submit thread drains the ring at the start of each frame's player control |
| frame ring (one per player) | `Zenith_VideoFrameSlot` (buffer index, presentation time, crop, layout, strides, seek epoch `uint16`; 48 bytes) | `NextPow2(cine.video.decodeAheadFrames)` (6 → 8) | video thread | render-submit thread | the producer pushes only while `GetCapacity() − GetWritableCount() < m_uDecodeAheadLimit`, so the ring never refuses a push |
| return ring (one per player) | `uint32` buffer index | `NextPow2(pool size)` (16 for a pool of 9) | render-submit thread (Flux's `ReleaseFrame`, frame selection's superseded slots, the seek drain) | video thread | cannot fill: there are pool-size buffers in total, and each is pushed once per use (the single-release rule of "Frame selection") |

**One owner per piece of state.** Only the render-submit thread changes a player's state (the table of "Video players"); the video thread keeps its own per-player stream state (`CLOSED`, `OPEN`, `SUSPENDED`, `FAILED`, the latest seek epoch it popped, its free list) and reports every change through the event ring, in the order it happened. The frame and return rings of a player slot are constructed with `Zenith_Construct` in the slot's storage when the player enters `OPENING` (before the OPEN command that carries them is pushed) and destroyed with `Zenith_Destroy` when it enters `FREE` (after the video thread reported `CLOSED`, so it no longer touches them), so each `Zenith_SpscRing` is initialised exactly once per construction (AU6's single `Init`) and a reused slot gets fresh rings.

Each player's frame pool holds `decodeAheadFrames + 3` buffers: the decode-ahead in the frame ring, the look-ahead slot `m_xPending`, the frame `AcquireFrame` returned and Flux has not yet released (within one render-submit call sequence), and one being decoded; so a pool of 9 for the default 6, and a return ring of `NextPow2(9)` = 16. `m_uDecodeAheadLimit` is an atomic `uint32` per player, written by the render-submit thread (`cine.video.decodeAheadFrames`, or 2 at memory WARNING, `Zenith2/Docs/Design/CINE/Playback.md` → "Memory consumer") and read with acquire by the video thread.

Loop, `Zenith_VideoThread::PumpOnce()` then `Zenith_SleepUs(2000)` when it did nothing (no busy wait):

1. Pop every lifecycle command (SUSPEND: `Suspend` the decoder, mark every open stream suspended, push `SUSPENDED` per player; RESUME: `Resume`, push `RESUMED` per player, or `FAILED` with the error when `Resume` fails) and every command of the command ring (OPEN: initialise the player's free list with every pool index, open the stream with the carried rings and pool, push `OPENED` or `OPEN_FAILED`, then decode from the keyframe at or before the target; SEEK: `Flush`, adopt the command's seek epoch, seek to the keyframe at or before the target; CLOSE: `CloseStream` when the stream is open, then push `CLOSED` — a CLOSE of a stream that is closed, failed or never opened pushes `CLOSED` without calling `CloseStream`, so the interface's handle rule is never broken), each only while the event ring has the room stated above. A device error pushes `DEVICE_LOST` (then the stream is recreated, step 5 of the Android backend, or `FAILED` on a second loss within 2 s); a stream error pushes `FAILED` when "Video failure handling" says the player fails.
2. Pop every buffer index of each player's return ring into that player's free list (owned by the video thread).
3. Per open player, in player order: if a bitstream read is in flight, poll its operation handle (`Zenith_OperationHandle::IsComplete()`; `GetStatus()` `OPERATION_STATUS_FAILED` is a stream error); otherwise, if the stream is not at end and fewer than `reorder depth + 2` access units are submitted and not yet received, issue `Zenith_AssetReader::ReadRange` for the next run of consecutive frames (≤ 1 MiB) into the player's read buffer. With bytes available, `Submit` the next access unit (validated against its frame record first, "Video load validation" of `Zenith2/Docs/Design/CINE/Formats.md`). After the last access unit of the bitstream is submitted, call `Drain` once, so the reorder-depth frames at the end of the clip are received. Then, while the free list is non-empty and the frame ring is below the limit, `Receive` into a free buffer and push its slot stamped with the stream's latest seek epoch.
4. A frame whose presentation time is below the stream's target (OPEN's or the latest SEEK's: the pts of the frame due at the requested media time) is received into a buffer and returned to the free list without being pushed (decode-and-discard to the target). The first pushed frame is therefore the frame due at m, with p ≤ m, so frame selection presents it at once even when m lies between two frames (an odd tick at 60 Hz for 30 fps content), a paused scrub with a constant m shows its frame, and a capture held at m finds it ready.

`PumpOnce` is the whole loop body and is public to the module's tests: the state-machine and player tests construct the thread in test mode (no OS thread) and call `PumpOnce` on the test's thread between frames, so they are deterministic under `win-asan`; only `Test_VideoThread_SpscHandOff_Stress` runs a real thread.

## Video players

A player plays one video section of one instance. The player table (8 entries, owned by the render-submit thread) maps a (world tag, instance ID, track, section) key from the consumed render world's draw records to a player slot. A "frame" below is one render world consumed by the render-submit thread; a paused world that produces the same draw records keeps its players open.

| State | Event | Next | Action |
|---|---|---|---|
| `FREE` | a draw record with a new key | `OPENING` | allocate the frame pool (`decodeAhead + 3` buffers of the layout's size), construct the rings, epoch 0; command OPEN with the asset, the media time and the pool and rings |
| `OPENING` | event `OPENED` | `RUNNING` | — |
| `OPENING` | event `OPEN_FAILED` (`VIDEO_UNSUPPORTED`, `CORRUPT`) | `FAILED` | log once per asset `cine.video: '<asset>' cannot play: <error>`; draws nothing |
| `RUNNING` | the draw record's media time is before the reference presentation time by more than one frame, or after it by more than 1 s, the reference being `m_xPending`'s pts when it is valid, else the last returned frame's (no seek is triggered before the first frame of the epoch arrives) | `SEEKING` | the seek protocol below |
| `SEEKING` | the first slot of the current epoch is popped | `RUNNING` | — |
| `OPENING`, `RUNNING`, `SEEKING`, `SUSPENDED` | no draw record with the key in 2 consecutive consumed render worlds (the instance ended, the section ended, or the world detached) | `CLOSING` | command CLOSE (the thread answers CLOSE whatever its stream state) |
| `RUNNING` | the media time passes the video's duration | `RUNNING` | hold the last frame |
| any but `FREE` | event `SUSPENDED` or `DEVICE_LOST` | `SUSPENDED` | frames held; the last upload stays on screen |
| `SUSPENDED` | event `RESUMED`, or `OPENED` after a recreation | `SEEKING` | the seek protocol to the current media time |
| any but `FREE` | event `FAILED` | `FAILED` | logged once with the error |
| any but `FREE` | event `CLOSED` (a close that crossed a `FAILED`, or the answer to the `FAILED` row's CLOSE) | `FREE` | destroy the rings, free the frame pool |
| `FAILED` | no draw record for 2 frames | `CLOSING` | command CLOSE (the stream may still be open on the video thread) |

**Seek protocol** (render-submit thread, the frame ring's consumer and the return ring's producer): increment the player's `uint16` seek epoch; pop every slot of the frame ring and the look-ahead slot `m_xPending` and push their buffer indices into the return ring (the drain); push SEEK {player, target, epoch}, the target being the pts of `FindFrameAtOrBefore(m)` for the record's media time m. From then on frame selection returns at once to the return ring every popped slot whose epoch differs from the player's current one, so a backward seek strands no buffer in the ring (the pool cannot run dry) and a forward seek presents no frame decoded before it (`Test_VideoPlayer_BackwardSeekFullPool_NoDeadlock`: a full pool, a backward seek to an earlier keyframe, the player reaches `RUNNING` at the target within the pumped iterations of one decode-and-discard, and every buffer index is returned exactly once).

A fifth concurrent video section beyond `uCINE_MAX_VIDEO_PLAYERS` draws nothing and counts `cine.video.player_limit_delta`. The player table, the pools and the rings belong to the module, not to a world: a world that detaches stops writing draw records and its players close by absence, so `DetachWorld` on the main thread never touches them.

## Frame selection

Per player the render-submit thread keeps a one-slot look-ahead `m_xPending` (a `Zenith_VideoFrameSlot` with a valid bit), because the ring has no peek, the selected slot `m_xSelected` (valid bit) and the metadata of the last frame returned (presentation time, token). The player table fills each draw record's `m_uPlayer` before Flux calls `AcquireFrame(player, m)`, m the record's media time (already interpolated by the extraction alpha, `Zenith2/Docs/Design/CINE/Playback.md` → "Video tracks"):

1. If `m_xPending` is empty, pop one slot into it (or stop when the ring is empty); a popped slot of a stale epoch is pushed back into the return ring at once and the pop repeats.
2. While `m_xPending` is valid and its presentation time ≤ m: when `m_xSelected` is valid it is superseded and its buffer is pushed into the return ring (it was popped and never returned, so this is its only release); `m_xPending` becomes `m_xSelected`, and the next slot is popped into `m_xPending` as in step 1.
3. When `m_xSelected` is valid: fill `xOutFrame` from it, record it as the last returned frame, clear `m_xSelected` and return true; Flux copies its planes and calls `ReleaseFrame` with its token in the same frame, which pushes its buffer into the return ring (its only release). Otherwise return false; when the pending slot's time is > m and the last returned frame's time is more than one frame period before m, count `cine.video.late_delta`.

Each buffer index is therefore pushed into the return ring exactly once per decode: by step 1 (stale epoch), step 2 (superseded), Flux's `ReleaseFrame` (returned) or the seek drain — never by two of them, so the return ring's bound holds and no two decodes share a buffer. The returned frame's presentation time p satisfies `m − frame period < p ≤ m` whenever the decoder keeps up, so the displayed frame lags the ideal by less than one frame period. Complexity O(frames popped).

## GPU side

`Flux_VideoSystem` (CINE-owned files in `ZenithFlux`, `Engine/ZenithFlux/Internal/Video/`) holds, per active player: the plane textures (NV12: R8 W×H and RG8 W/2×H/2; P010: R16 and RG16; I420: three R8, each at the crop size), a converted texture RGBA16F at the crop size (the player's surface), and the current frame's metadata. Textures are created on the first frame of a player and when the crop size changes, and destroyed when it frees (deferred destruction through the renderer's retire list), counted toward the VRAM side of `Z2-B-CINE-001`; a 1080p player uses 3.1 MB of plane textures and 16.6 MB of RGBA16F, a 2160p player four times that.

- **Upload.** When `AcquireFrame` returns a new frame, the crop region of each plane is copied into the upload ring (`Zenith2/Docs/Rendering/GpuResources.md` → "Upload ring") and a copy into the plane textures is recorded; a frame that does not fit the ring's frame budget is skipped (the previous frame stays on screen) and counts `flux.video.upload_skipped_delta`. Either way Flux then calls `ReleaseFrame` with the frame's token in the same frame (the single-release rule of "Frame selection").
- **Conversion** (`VideoConvert`, common prologue row 2b, queue G, `Flux_AddVideoConvert`, one dispatch of 8 × 8 threads per player whose frame changed): read the planes, expand the range (limited: Y 16–235, C 16–240 for 8-bit; scaled for 10-bit), apply the matrix (BT.709, BT.601 or BT.2020 non-constant from the metadata), convert R'G'B' to linear with the transfer's inverse (BT.709/BT.1886 as a 2.4 power; PQ and HLG through their EOTFs, scaled so that 1.0 is 203 nits, the ITU-R BT.2408 reference white; SDR content maps reference white to 1.0), write RGBA16F linear, alpha 1. Chroma is upsampled bilinearly at the chroma location (left, co-sited vertically per H.273 chroma location 0).
- **Composite** (`VideoComposite`, "VideoComposite pass").

The video shaders live in `Zenith2/Shaders/Video/` (`VideoConvert.slang`, `VideoComposite.slang`) with generated parameter structs (Z2-D-015).

<!-- assumption: pending RPROD, REND -->
`Zenith2/Docs/Rendering/FrameAnatomy.md` → "Common prologue" gains row 2b `VideoConvert` (`Flux_AddVideoConvert`, queue G, reads the video plane textures, writes the video surfaces) and "Common epilogue" gains row 6b `VideoComposite`; both are recorded only when `Flux_FrameShared::m_pxVideo` is non-null and has an active player or an overlay. Rows X292 and X313 (e).
<!-- /assumption -->

## VideoComposite pass

Per view that is the main view (the game view in the editor), after `TonemapAndGrade`, before the passes TWOD records at `FLUX_EXTENSION_POINT_BEFORE_UI`, and before `UIComposite` (so 2D canvases draw over cinematic video and under the UI; X313 (e) orders the two in the common epilogue): one full-screen pass blending, in ascending layer, each video draw record without a surface name onto the display-referred output:

1. Placement: the record's rectangle (normalised output coordinates) or full screen; fit `letterbox` (scale to fit, bars transparent so the scene shows unless the layer is opaque), `fill` (scale to cover, crop), `stretch`.
2. Colour: the surface's linear value is encoded for the output with `Flux_EncodeOutput(linear, fNitsForOne = the output's paper white, mode)` (LIGHT's colour management, "UI and HDR"), the same path UI uses, so SDR video looks identical on SDR and HDR displays.
3. Blend: `out = lerp(out, video, opacity)`.
4. Overlay, after every layer: `out = lerp(out, encode(fade colour), fade alpha)`, then bars of the letterbox fraction at the top and the bottom set to the encoded black.

The pass writes only the output; it reads the surfaces and the overlay record. GPU cost is the pending row "Video conversion and composite GPU" (CN12).

## Texture surfaces

A video section with a `surface` name writes no composite layer; its player's converted texture is published under the surface name hash, and `Flux_VideoSystem::FindSurface(hash)` returns it for the frame (null when the player has no frame yet). Two consumers read it:

- UI image elements whose image source is `video:<surface>` (row CN10 asks UI for the image-source scheme).
- Materials whose texture slot is bound to a video surface through `Flux_MaterialInstance`'s dynamic texture binding (row CN8 asks RPROD for the binding; until it exists only UI consumes surfaces).

A surface without a playing section samples as transparent black (UI) or the material's fallback (materials).

## Audio and video sync

Both clocks are the simulation: the video's media time is derived from the instance's sequence time (presentation interpolation by the extraction alpha), and the video's audio is a time-locked AUDIO voice posted at the section start, kept on simulation time by AUDIO's drift correction (`Zenith2/Docs/Design/AUDIO/Mixer.md` → "Time-locked voices": |e| ≤ 5 ms ignored, nudge to 43 ms, seek beyond). The presented frame lags the ideal by less than one frame period ("Frame selection"); the audio voice deviates by at most 5 ms in the steady state. So the presented audio-video offset is bounded by one frame period plus 5 ms (38.3 ms at 30 fps, 21.7 ms at 60 fps), inside the ±45 ms detectability threshold of ITU-R BT.1359 for audio leading; the end-to-end measurement adds the display path's latency, which the device run reports. `Test_VideoSync_OfflineOffsetWithinOneFrame` asserts the bound in `Zenith_Integration_Cinematics` with the offline audio backend, the test decoder through the seam, the pumped video thread and a WARP or lavapipe device (not `needs-gpu`, X170); the S24 device run of the CinematicSlice acceptance measures it with a light sensor and microphone recording (`needs-human` evidence in `Games/CinematicSlice/Acceptance/BugBar.md`).

## Video failure handling

| Failure | Behaviour |
|---|---|
| the video asset fails to load | never fails the start: the section draws nothing, `cine.video` logs once per asset (`Zenith2/Docs/Design/CINE/Playback.md` → "Asset binding" step 3) |
| unsupported codec, profile, bit depth or size on this device | player `FAILED`; logged once per asset with the caps; the sequence plays on |
| a stream error mid-play | the player seeks to the next keyframe after the failing frame; three errors within 2 s move it to `FAILED` |
| decoder device loss (Windows D3D11 removed, Android codec reclaimed) | the stream is recreated on the video thread; players `SUSPENDED` → `SEEKING`; a second loss within 2 s → `FAILED` |
| RHI device loss | the surfaces and plane textures are recreated by the renderer's device-loss path; players keep decoding; the first frame after recovery re-uploads |
| frame pool or texture allocation failure | the player draws nothing; `OUT_OF_MEMORY` logged; at memory CRITICAL invisible players are stopped first (`Zenith2/Docs/Design/CINE/Playback.md` → "Memory consumer") |
| bitstream read failure | as a stream error |

None of these is visible to simulation: no event, no state change of the instance, no counter that simulation reads.

## Android lifecycle

PLAT's lifecycle reaches modules through the module frame listener (`Zenith2/Docs/Architecture/CompositionRoot.md` → "Module interfaces": `OnStopAudio` inside `Platform.Pump` before `OnEnterBackground`, `OnResumeAudio` after `OnRecreateSurface`, on the main thread). CINE's `Zenith_CinematicsFrameListener`, registered at Start step 6, pushes SUSPEND into the lifecycle ring from `OnStopAudio` and RESUME from `OnResumeAudio`. On SUSPEND the video thread calls `Suspend` (every MediaCodec stopped and released: the system reclaims codec resources from background apps) and reports `SUSPENDED` per player through the event ring, so the render-submit thread moves every player to `SUSPENDED`; on RESUME it calls `Resume`, which recreates the codecs, and reports `RESUMED`, so every suspended player runs the seek protocol to its current media time; the simulation, paused or not by the game's pause policy, is unaffected. A `Resume` that fails leaves the players `FAILED` and logs the error. The decoder's `AImageReader` surface is independent of the window surface, so `PLATFORM_EVENT_SURFACE_LOST` needs no video action. `Test_VideoPlayer_SuspendResume_Seeks` drives the listener calls with the pumped thread.

## Capture state machine

| State | Event | Next | Action |
|---|---|---|---|
| `IDLE`, `FINISHED`, `FAILED` | `Begin` with a valid description, a non-null development file writer and the capture configuration | `RECORDING` | require `cine.capture.enabled`, `sim.ticksPerFrame = 1` (the clock in `SIM_CLOCK_MODE_EXTERNAL_TICKS`, `Zenith2/Docs/Design/JOBS/PublicAPI.md` → "Zenith_SimulationClock header"), a non-null capture tick gate, and, with `m_bCaptureAudio`, `audio.device.backend = offline` (`Zenith2/Docs/Design/AUDIO/Device.md` → "Backend selection") and a non-null audio capture bridge, else `CINEMATIC_ERROR_CAPTURE_STATE`; record the development file writer's completion-failure count; call the bridge's `Begin()` |
| `RECORDING` | each frame | `RECORDING` | "Capture algorithm" |
| `RECORDING` | `End` | `DRAINING` | request no new tick; the gate stays held |
| `DRAINING` | the last requested frame and audio chunk submitted | `FINISHED` | write `subtitles.json`, then `manifest.json`; `End()` on the bridge; release the gate |
| `RECORDING`, `DRAINING` | a refused write (`DEV_FILE_ERROR_BAD_NAME`, `_TOO_LARGE`, `_IO`, `_NOT_IN_SHIPPING`), the writer's completion-failure count rose, or 300 consecutive frames without video readiness ("Capture algorithm" step 2) | `FAILED` | log the path and error; cancel the pending readback; release the gate |

The audio backend cannot switch at run time (`Zenith_AudioOfflineRenderer::Create` returns `AUDIO_ERROR_INVALID_DESC` unless the system was created with `AUDIO_BACKEND_OFFLINE`), so a capture with audio runs in a process started with `--set audio.device.backend=offline --set sim.ticksPerFrame=1 --set cine.capture.enabled=true`; the device stays silent for the whole run. The editor's capture dialog starts the game's Player in such a child process and drives it over DEVT's remote console (`COMMAND` messages `cine.play`, `cine.capture.begin`, `cine.capture.end`; `Zenith2/Docs/Design/DEVT/Console.md` → "Remote messages").

## Capture algorithm

The capture is lockstep: tick t + 1 is simulated only after the frame of tick t has been read back, showing every video section at its ready frame, and its file submitted, so a refused or failed readback is retried on the same tick and no tick goes uncaptured (`Test_CineCapture_DeferredFrame_RetriedNoGap`). The capture's state is the main thread's; the render-submit thread sees only its **render channel** `Zenith_CaptureRenderChannel`, owned by the capture and borrowed by the module's frame source implementation:

| Member | Type | Writer | Reader |
|---|---|---|---|
| `m_ulTickToRequest` | `std::atomic<uint64>` (release store, acquire load; `UINT64_MAX` none) | main thread (the gate) | render-submit thread |
| `m_xReports` | `Zenith_SpscRing<Zenith_CaptureFrameReport>`, capacity 4 | render-submit thread | main thread |

`Zenith_CaptureFrameReport` (32 bytes): the render world's tick (8), outcome (1: `REQUESTED` with a readback, `VIDEO_NOT_READY`), reserved (7), `Flux_ReadbackHandle` (16; null for `VIDEO_NOT_READY`). The render-submit thread pushes a report only when `GetWritableCount()` > 0 (else it requests nothing this frame, and the gate keeps holding), and the main thread drains the ring every frame, so the ring never refuses a push.

Per frame while `RECORDING`, the main-thread work runs in the frame listener's `OnPlatformEvents` of the next frame (after prologue step 1, before the clock advance of step 2), in the order audio, outcome, gate, because a module has no main-thread hook after `RunFrame`:

1. **Audio** (main thread): when the previous frame advanced the clock to tick t (the gate did not hold it, so its `AdvanceExternal` returned 1), the bridge's `RenderTick` renders `FramesForNextTick(1/H)` frames into the capture's 1-second buffer (384,000 bytes); a full buffer is submitted as `captures/<name>/audio/<second, 6 digits>.pcm`, the last partial one at `End`. A held frame renders no audio, so the audio contains exactly one tick's frames per captured tick, and no time-locked voice sees a paused world. CINE reaches AUDIO's offline renderer only through the runtime's audio capture bridge, because it cannot include ZenithAudio headers. The cumulative audio frame count is capped at `ulCINE_CAPTURE_MAX_AUDIO_FRAMES` = 536,870,907 (the most whose `audio.wav` data size, 8 bytes per frame, plus 36 fits the `uint32` RIFF size; about 3.1 hours at 48 kHz): the tick whose audio would exceed it forces `End`, logged `cine.capture: audio limit reached`.
2. **Outcome** (main thread): pop every report. `VIDEO_NOT_READY` for tick t: count `cine.capture.video_wait_delta` and the capture's consecutive wait; at `uCINE_CAPTURE_VIDEO_WAIT_FRAMES` (300) consecutive frames the capture moves to `FAILED` with `CINEMATIC_ERROR_CAPTURE_STATE`, logged `cine.capture: video not ready for tick <t>`. `REQUESTED`: keep the handle and poll it. A readback `READY` → convert to BGRA8 and submit `captures/<name>/frames/<t − firstTick, 6 digits>.tga` with `Zenith_DevFileWriter::Write` through the writer `Begin` received; `DEV_FILE_ERROR_BUSY` keeps the frame for the next frame (the gate stays held); success marks tick t written and resets the wait. `FAILED` with `FLUX_READBACK_ERROR_POOL_FULL`, `FLUX_READBACK_ERROR_FRAME_DEFERRED`, `FLUX_READBACK_ERROR_FRAME_FAILED`, or after device loss ([Z2-N-REND-501]) → drop the handle; the tick is requested again (step 3). Still pending → hold.
3. **Gate** (main thread): when the frame of the current tick t is not yet written, call `HoldNextTick()` on the capture tick gate, so this frame's clock advance is `AdvanceExternal(0)` and the frame renders tick t again (alpha stays 0 in external-ticks mode, so the render world is identical), and, when no readback of tick t is pending, store t into `m_ulTickToRequest`; otherwise do nothing, so the frame advances to t + 1.
4. **Request** (render-submit thread, while the frame's graph is recorded): when the consumed render world's tick equals `m_ulTickToRequest` and no capture readback is in flight, decide video readiness for this frame: the frame is ready when every video draw record's player is `RUNNING` and the frame it shows satisfies `m − frame period < p ≤ m` ("Frame selection") and was uploaded this frame or earlier (not skipped by the upload ring). Ready → request a readback of the main view's offscreen output at the capture size after `VideoComposite` (or after `UIComposite` with `m_bIncludeUI`) and push `REQUESTED` with its handle; not ready → push `VIDEO_NOT_READY` and request nothing. Readback memory comes from the pool of `Z2-B-RHI-011` ([Z2-N-REND-499]); at most one capture readback is in flight. The captured frames therefore never depend on decode timing: a section's first frames wait while the player is `OPENING`, a seek waits while it is `SEEKING`, and a decoder that lags only lengthens the run (`Test_CineCapture_VideoSectionStart_WaitsForFirstFrame`).
5. **Subtitles**: the caption cues and cancels **accepted** in tick t — the outbox entries whose `Zenith.Cinematics.Emission` bit is set for t (`Zenith2/Docs/Design/CINE/Playback.md` → "Emission acceptance"), read in the next frame's step 2 — are appended to the subtitle list with the frame index t − firstTick, so a group the phase-8 merge rejected and re-emitted at t + 1 is logged once, at t + 1.
6. **Failure check**: the writer's completion-failure count above its value at `Begin` moves the capture to `FAILED`.

<!-- assumption: pending AUDIO, MOD, RPROD, DEVT (rows CN8 (c), CN9 (a), CN21; the capture tick gate (c) landed with X313 (d)) -->
(a) ZenithRuntime exposes an audio capture bridge to modules in development builds (`Zenith_ModuleStartContext::GetAudioCapture() → Zenith_AudioCaptureBridge*`, null when no Audio module, when the audio backend is not offline, or in Shipping) with `Begin()`, `RenderTick(Zenith_MutableSpan<float>) → frames`, `End()`, implemented over `Zenith_AudioOfflineRenderer` (its `ServiceBeforeFrame`, the frame's mix, then `Render`, in that order); (b) RPROD lets a development tool render the main view into an offscreen output texture of a requested size and request a readback of it after a named epilogue pass; (c) the composition root's development-only capture tick gate `Zenith_CaptureTickGate* Zenith_ModuleStartContext::GetCaptureTickGate()` (null in Shipping and when `sim.ticksPerFrame` ≠ 1) with `void HoldNextTick()` (MAIN_THREAD), making the next clock advance `AdvanceExternal(0)` (X313 (d)); (d) DEVT's `Zenith_DevFileWriter` gains `uint64 GetCompletionFailureCount() const` (MAIN_THREAD, cumulative), so a completion failure, which the writer logs and does not report to the caller, reaches the capture (row CN21). The writer itself reaches the capture through `Zenith_DevCommandOutput::GetFileWriter()` in the `cine.capture.begin` handler (`Zenith2/Docs/Design/DEVT/Console.md` → "Zenith_DevConsole header"), which passes it to `Begin`; DEVT's `Zenith_DevTools` owns it for the process's life.
<!-- /assumption -->

The capture exists only in development builds and the editor: the module creates `Zenith_CinematicCapture` at Start step 7 only when `!Zenith_IsShippingBuild()` (an `if constexpr`, no layout-changing conditional, Non-negotiable 11), holding it as a `Zenith_Owned` that is null in Shipping and `ShippingTest`; its code compiles in every configuration and is unreferenced, so unlinked, in Shipping; the `cine.capture.*` keys are not registered there.

## Export algorithm

`Zenith_ExportCinematicCapture` (tool side, `Zenith2/Tools/Authoring/Internal/Cinematics/Zenith_CinematicExport.cpp`):

1. Read and validate `manifest.json` (schema `zenith2.cinecapture` version 1; frame files present and contiguous; audio chunks present and their total equal to `audio.frames` × 8 bytes); `CINEMATIC_ERROR_CORRUPT` names the first failing item.
2. Frames: when `m_bWritePng`, convert each TGA to PNG (IHDR 8-bit RGBA, one IDAT of zlib level 6 through COMPRESS, filter 0 per row, IEND; CRCs per the PNG specification), else copy; frames processed in parallel jobs of 16 frames, written in index order.
3. Audio: concatenate the chunks into `audio.wav` behind the header of "Export output" in `Zenith2/Docs/Design/CINE/Formats.md`.
4. Subtitles: when `m_bWriteWebVtt`, write `subtitles.vtt` from `subtitles.json`.
5. Video: when `m_bEncodeVideo`: refused with `CINEMATIC_ERROR_VIDEO_UNSUPPORTED` unless the D-SEL-VIDEO ruling admitted an encoder (V-C9); otherwise encode H.264 High at the capture's rate through it and mux an MP4 with the WAV as AAC-LC (encoded by the same platform encoder).
6. Write `export.json` last, so its presence marks a complete export.

Export is deterministic for identical inputs (PNG bytes, WAV bytes, VTT text, JSON), which `Test_CineExport_PngAndVttDeterministic` asserts; the MP4 is not covered by the determinism rule (platform encoders are not bit-exact).
