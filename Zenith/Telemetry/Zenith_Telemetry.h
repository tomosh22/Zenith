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
#include "EntityComponent/Zenith_Entity.h"

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

	// Per-entity per-frame snapshot. uStateFlags is game-defined --
	// DevilsPlayground packs sprinting / walk-quiet / possessed / etc.
	struct EntitySnapshot
	{
		Zenith_EntityID       xId;
		Zenith_Maths::Vector3 xPos      = {0.0f, 0.0f, 0.0f};
		Zenith_Maths::Vector3 xForward  = {0.0f, 0.0f, 1.0f};
		uint32_t              uStateFlags = 0;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	struct FrameSample
	{
		uint32_t                       uFrameIdx = 0;
		float                          fTimeS    = 0.0f;
		Zenith_Vector<EntitySnapshot>  axEntities;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	// Event payload -- small variant. Most events fit in (4 floats + 4 ints
	// + 2 entity IDs + a 32-char label). Larger payloads should be split
	// across multiple events or written through a side-channel.
	struct EventPayload
	{
		float           afFloats[4]   = {0.0f, 0.0f, 0.0f, 0.0f};
		int32_t         aiInts[4]     = {0, 0, 0, 0};
		Zenith_EntityID xEntityA;
		Zenith_EntityID xEntityB;
		char            szLabel[32]   = {0};

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
	};

	struct Event
	{
		uint32_t     uFrameIdx  = 0;
		float        fTimeS     = 0.0f;
		uint16_t     uEventType = 0;       // game-defined enum value
		uint16_t     uReserved  = 0;
		EventPayload xPayload;

		void WriteToDataStream(Zenith_DataStream& xStream) const;
		void ReadFromDataStream(Zenith_DataStream& xStream);
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
		// Version 2 (2026-05-18): added axObstacles. Readers accept both
		// versions -- a v1 file just yields an empty obstacle list.
		uint32_t    uVersion      = 2;
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
