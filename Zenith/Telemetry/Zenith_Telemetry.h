#pragma once

/**
 * Zenith_Telemetry - Engine-side recording of player + entity motion + events.
 *
 * Generic recorder used by gameplay code to log a structured stream of
 * frame samples + events to a binary file. The companion JSON exporter
 * walks the binary back out for human inspection / test assertions.
 *
 * Design (2026-05-16):
 *   - Binary primary via Zenith_DataStream (compact, fast, matches engine
 *     patterns); JSON export is opt-in alongside.
 *   - 10 Hz position samples by default (every 6 frames @ fixed-dt 1/60);
 *     callers can override.
 *   - Events recorded immediately when they fire (no buffering delay).
 *   - Caller owns the schema of EventType (uint16_t) -- this module is
 *     game-agnostic; DevilsPlayground defines its own enum on top.
 *
 * Threading: ALL recorder calls must come from the main thread. There is
 * no internal locking. This matches the engine convention that gameplay
 * code runs single-threaded on the main thread.
 *
 * File format (binary):
 *   Header                  (one)
 *   { uint8 RecordType byte + payload }* (interleaved samples + events)
 *   uint8 RecordType::End sentinel
 */

#include "DataStream/Zenith_DataStream.h"
#include "Collections/Zenith_Vector.h"
#include "Maths/Zenith_Maths.h"
#include "ZenithECS/Zenith_Entity.h"

#include <cstdint>
#include <string>

namespace Zenith_Telemetry
{
	// Leading byte on each record so the reader can dispatch.
	enum class RecordType : uint8_t
	{
		FrameSample = 1,
		Event       = 2,
		End         = 0xFF,
	};

	// Per-entity per-frame snapshot. The semantics of uStateFlags +
	// uAIIntent + uHeldItemTag + fSecondaryFloat are game-defined --
	// DevilsPlayground packs sprinting / walk-quiet / possessed / etc.
	// into the flags, the priest's current BT branch into uAIIntent,
	// the possessed villager's held item into uHeldItemTag, and the
	// villager's remaining life timer into fSecondaryFloat.
	//
	// v3 additions (2026-05-19): xAITargetPos / uAIIntent / uHeldItemTag /
	// fSecondaryFloat. Older recordings load with these defaulted; the
	// JSON exporter always emits them so downstream readers can rely on
	// the field being present.
	struct EntitySnapshot
	{
		Zenith_EntityID       xId;
		Zenith_Maths::Vector3 xPos              = {0.0f, 0.0f, 0.0f};
		Zenith_Maths::Vector3 xForward          = {0.0f, 0.0f, 1.0f};
		uint32_t              uStateFlags       = 0;
		// v3 fields:
		Zenith_Maths::Vector3 xAITargetPos      = {0.0f, 0.0f, 0.0f};
		uint8_t               uAIIntent         = 0;
		uint8_t               uHeldItemTag      = 0;
		uint16_t              uReserved         = 0;
		float                 fSecondaryFloat   = 0.0f;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream, uint32_t uVersion);
	};

	// v3 addition: per-frame camera pose for replay-style debugging --
	// "what was the player looking at when X happened". Sampled once per
	// FrameSample. xLookAt is the orbit target / focus point; xPos is the
	// camera's eye position.
	struct CameraState
	{
		Zenith_Maths::Vector3 xPos           = {0.0f, 0.0f, 0.0f};
		Zenith_Maths::Vector3 xLookAt        = {0.0f, 0.0f, 0.0f};
		float                 fOrbitYawRad   = 0.0f;
		float                 fOrbitDistance = 0.0f;
		float                 fFovRadians    = 0.0f;
		uint8_t               bValid         = 0;  // 0/1; non-bool so the
		                                           // serialised size stays
		                                           // platform-stable

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	struct FrameSample
	{
		uint32_t                       uFrameIdx     = 0;
		float                          fTimeS        = 0.0f;
		Zenith_Vector<EntitySnapshot>  axEntities;
		// v3 additions:
		CameraState                    xCamera;
		float                          fFrameWallMs  = 0.0f;  // wall-clock ms (0 if not measured)

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream, uint32_t uVersion);
	};

	// Event payload -- small variant. Most events fit in (4 floats + 4 ints
	// + 2 entity IDs + a 32-char label + a 24-char source tag). Larger
	// payloads should be split across multiple events or written through
	// a side-channel.
	//
	// szSource (v3): a short subsystem / call-site tag emitted by the
	// dispatcher. Lets readers attribute "two scripts emit the same
	// semantic event" without resorting to call-stack archaeology.
	struct EventPayload
	{
		float           afFloats[4]   = {0.0f, 0.0f, 0.0f, 0.0f};
		int32_t         aiInts[4]     = {0, 0, 0, 0};
		Zenith_EntityID xEntityA;
		Zenith_EntityID xEntityB;
		char            szLabel[32]   = {0};
		// v3 field:
		char            szSource[24]  = {0};

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream, uint32_t uVersion);
	};

	struct Event
	{
		uint32_t     uFrameIdx  = 0;
		float        fTimeS     = 0.0f;
		uint16_t     uEventType = 0;       // game-defined enum value
		uint16_t     uReserved  = 0;
		EventPayload xPayload;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream, uint32_t uVersion);
	};

	// Top-down oriented-bounding-box of a static scene obstacle (wall,
	// building, prop). Recorded once in the Header so the visualiser can
	// draw scene geometry under the entity trails -- a trail that
	// squeezes through a doorway or hugs a wall makes far more sense
	// against a backdrop of the actual obstacles.
	//
	// Convention: XZ-plane projection only (Y is discarded because the
	// visualiser is top-down). Half-extents are in the wall's LOCAL
	// space; fYawRadians rotates that local frame into world space.
	// Floors / non-blocking volumes are filtered by the caller before
	// they reach here -- this struct just stores whatever rectangles
	// the recording wants to overlay.
	struct SceneObstacle
	{
		float fCentreX     = 0.0f;
		float fCentreZ     = 0.0f;
		float fHalfExtentX = 0.0f;
		float fHalfExtentZ = 0.0f;
		float fYawRadians  = 0.0f;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	struct Header
	{
		uint32_t    uMagic        = 0x4D4C545A; // 'ZTLM' little-endian
		// Version 3 (2026-05-19): expanded EntitySnapshot (AI target /
		// intent / held item / life-timer), added CameraState +
		// fFrameWallMs to FrameSample, added szSource to EventPayload,
		// and added build-info / personality strings to the Header.
		// Reader accepts v1 / v2 / v3; older recordings load with
		// new fields defaulted.
		uint32_t    uVersion      = 3;
		uint64_t    uSeed         = 0;
		uint64_t    uStartUTCMs   = 0;
		std::string strSceneName;
		float       fFixedDt      = 1.0f / 60.0f;
		uint32_t    uSamplePeriodFrames = 6; // 10 Hz at fixed-dt 1/60
		// Static scene obstacles, top-down OBBs. Populated by the caller
		// before Recorder::Begin(); the recorder stores them as part of
		// the header and the JSON exporter mirrors them in the header
		// object so the visualiser can render walls under the trails.
		Zenith_Vector<SceneObstacle> axObstacles;
		// v3 build / personality metadata. Populated by the caller; empty
		// strings are emitted as empty JSON strings.
		std::string strBuildConfig;     // e.g. "vs2022_Debug_Win64_True"
		std::string strBuildHash;       // short git hash if available
		std::string strPersonalityName; // bot personality the run was driven by

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	// =========== Recorder ===========
	// Streaming writer. Begin -> NextFrame loop with RecordFrame /
	// RecordEvent -> End (flush to disk).
	class Recorder
	{
	public:
		Recorder();

		// Start a fresh recording. Resets internal state. The header is
		// copied; sample period comes from the header.
		void Begin(const Header& xHeader);

		bool IsRecording() const { return m_bRecording; }

		// Advance the internal frame counter. Call once per game frame.
		void NextFrame() { ++m_uFrameIdx; }

		// Returns true when the current frame index aligns with the
		// configured sample period. Callers use this to gate the
		// (potentially expensive) building of a FrameSample.
		bool ShouldSampleThisFrame() const
		{
			return m_bRecording && (m_uFrameIdx % m_xHeader.uSamplePeriodFrames) == 0;
		}

		uint32_t GetFrameIdx() const { return m_uFrameIdx; }

		// Record a position sample. Caller fills in xSample.uFrameIdx as
		// GetFrameIdx() (or the recorder overrides it).
		void RecordFrame(const FrameSample& xSample);

		// Record an event immediately. xEvt.uFrameIdx is overridden to
		// the current internal frame index for consistency.
		void RecordEvent(const Event& xEvt);

		// Finalize the recording and flush to disk. Optionally also write
		// a JSON export (pfnEventTypeToString is used by the JSON path so
		// game-defined event-type ints get human-readable labels).
		bool End(const char* szBinaryPath,
		         const char* szJsonPathOrNull = nullptr,
		         const char* (*pfnEventTypeToString)(uint16_t) = nullptr);

		// Write the FULL accumulated recording to disk WITHOUT ending it
		// (recording continues afterwards). Use to periodically checkpoint a
		// long-running recording so a process killed before End() still leaves
		// a complete, valid file on disk. Same output format as End(); the file
		// is overwritten each call. Returns false if not recording.
		bool FlushSnapshot(const char* szBinaryPath,
		                   const char* szJsonPathOrNull = nullptr,
		                   const char* (*pfnEventTypeToString)(uint16_t) = nullptr);

		// Telemetry can be paused (e.g. during pause overlay) without
		// ending the recording. NextFrame still ticks, but RecordFrame /
		// RecordEvent are no-ops while paused.
		void SetPaused(bool bPaused) { m_bPaused = bPaused; }
		bool IsPaused() const        { return m_bPaused; }

		// Read-only accessors used by tests + the in-process analyzer.
		const Header& GetHeader() const                       { return m_xHeader; }
		const Zenith_Vector<FrameSample>& GetFrames() const   { return m_axFrames; }
		const Zenith_Vector<Event>&       GetEvents() const   { return m_axEvents; }

	private:
		bool                          m_bRecording = false;
		bool                          m_bPaused    = false;
		uint32_t                      m_uFrameIdx  = 0;
		Header                        m_xHeader;
		Zenith_Vector<FrameSample>    m_axFrames;
		Zenith_Vector<Event>          m_axEvents;
	};

	// =========== Reader ===========
	// Loads a binary recording back into Header + sample/event arrays.
	class Reader
	{
	public:
		// Returns false on I/O error or corrupt magic / unknown version.
		bool LoadFromFile(const char* szBinaryPath);

		const Header&                     GetHeader() const { return m_xHeader; }
		const Zenith_Vector<FrameSample>& GetFrames() const { return m_axFrames; }
		const Zenith_Vector<Event>&       GetEvents() const { return m_axEvents; }

		// Write a JSON export to disk. The optional callback converts
		// game-defined event-type integers to human-readable names; pass
		// nullptr to fall back to the numeric form.
		bool ExportJson(const char* szJsonPath,
		                const char* (*pfnEventTypeToString)(uint16_t) = nullptr) const;

		// Write the recording as two CSV files for spreadsheet / pandas /
		// awk-style analysis. szFramesCsvPath gets one row per per-frame
		// entity sample (frame, t, entity_idx, entity_gen, pos_x/y/z,
		// fwd_x/y/z, flags, ai_intent, ai_target_x/y/z, held_item,
		// secondary_float, camera_*, frame_ms). szEventsCsvPath gets
		// one row per event (frame, t, type, type_name, payload_*).
		// Either path may be null to skip that file.
		bool ExportCsv(const char* szFramesCsvPath,
		               const char* szEventsCsvPath,
		               const char* (*pfnEventTypeToString)(uint16_t) = nullptr) const;

	private:
		Header                        m_xHeader;
		Zenith_Vector<FrameSample>    m_axFrames;
		Zenith_Vector<Event>          m_axEvents;
	};

	// =========== Module-level singleton accessor ===========
	// One global recorder per process. Game code calls
	// Zenith_Telemetry::GetRecorder() everywhere; switching between
	// scenes / runs is just Begin/End on the same singleton.
	Recorder& GetRecorder();
}
