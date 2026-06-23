#pragma once

// RenderTest tennis -> Zenith_Telemetry glue.
//
// The autonomous tennis match (RenderTest_TennisMatchComponent, the passive
// referee) records analytics to disk through the engine's generic
// Zenith_Telemetry recorder: per-frame samples of both players + the ball, and
// a rich stream of discrete gameplay events (serves, contacts, bounces, points
// with their cause, games, faults, nav fallbacks, match end). The companion
// JSON/CSV exporters (Zenith_Telemetry::Reader) walk the binary back out for
// offline analysis.
//
// Layering mirrors DevilsPlayground's DPTelemetry: the engine owns the generic
// record/read/export; THIS header defines the game-specific event-type enum,
// the name resolvers the JSON exporter uses, the snapshot state-flag bit
// layout, and a small Emit() helper. Unlike DP, tennis has no event dispatcher
// — the referee resolves every point itself — so the referee calls Emit()
// directly at each milestone (Emit is a no-op unless the recorder is recording,
// so it is harmless when telemetry is disabled or during unit tests).

#include "Telemetry/Zenith_Telemetry.h"
#include "ZenithECS/Zenith_Entity.h"

#include <cstdint>

namespace RenderTest_TennisTelemetry
{
	// =========== Event-type enum ===========
	// Stable integer IDs — append new entries before _Count (file-format
	// compatibility depends on these not being reordered).
	enum class EventType : uint16_t
	{
		None          = 0,
		MatchStart    = 1,  // i0=server side; i1=jitter seed
		PointStart    = 2,  // i0=server, i1=attempt(0/1), i2=deuceCourt(1/0), i3=(nearPts<<8|farPts)
		ServeStruck   = 3,  // A=server; i0=shotType, i1=attempt; f0=pace, f1/f2=aim x/z, f3=spinMag
		Contact       = 4,  // A=striker; i0=shotType, i1=strikerSide, i2=inRange(1/0); f0=pace, f1/f2=aim x/z, f3=spinMag
		Bounce        = 5,  // i0=landSide, i1=bounceIdx, i2=inBounds(1/0), i3=isServeBounce(1/0); f0/f1=x/z, f2=incomingVy
		NetCross      = 6,  // i0=legal(1/0); f0=heightAboveTape
		PointResolved = 7,  // i0=winner, i1=PointReason, i2=rallyShots, i3=bouncesSinceHit; f0=crossedNet(1/0), f1=lastHitter
		GameWon       = 8,  // i0=winner, i1=gamesNear, i2=gamesFar
		Fault         = 9,  // i0=server, i1=attempt(the one that just faulted)
		DoubleFault   = 10, // i0=server, i1=receiver(awarded)
		NavFallback   = 11, // i0=agent side
		PhaseChange   = 12, // i0=fromPhase, i1=toPhase
		MatchOver     = 13, // i0=winner, i1=gamesNear, i2=gamesFar, i3=totalPointsPlayed
		_Count
	};

	// Why a point ended — the single most useful analytic dimension. Packed into
	// PointResolved.aiInts[1]. Stable IDs.
	enum class PointReason : int32_t
	{
		Unknown          = 0,
		ServeUnreturned  = 1,  // serve bounced twice; receiver never made contact (ace-like)
		DoubleFault      = 2,  // second serve also faulted
		OutOfRangeMiss   = 3,  // a swing fired too far from the ball (whiff)
		LandedOut        = 4,  // a struck ball cleared the net but landed out of bounds
		IntoNetOrOwnSide = 5,  // a struck ball failed to cross the net legally
		DoubleBounce     = 6,  // the receiver let a good ball bounce twice (rally winner)
		ServeTimeout     = 7,  // serve never struck within the watchdog
		RallyTimeout     = 8,  // shot watchdog / point timeout during a live rally
		_Count
	};

	// EntitySnapshot::uStateFlags layout for the two player entities.
	namespace PlayerFlags
	{
		static constexpr uint32_t IsPlayer     = 1u << 0;
		static constexpr uint32_t IsServer     = 1u << 1;
		static constexpr uint32_t IsNearSide   = 1u << 2;
		static constexpr uint32_t NavExternal  = 1u << 3; // nav agent owns XZ this frame
		static constexpr uint32_t NavFallback  = 1u << 4; // fell back to footwork (path failure)
		static constexpr uint32_t IsReceiver   = 1u << 5; // expected receiver this point
	}

	// EntitySnapshot::uStateFlags layout for the ball entity.
	namespace BallFlags
	{
		static constexpr uint32_t IsBall   = 1u << 0;
		static constexpr uint32_t InFlight = 1u << 1; // gravity on / live
		static constexpr uint32_t HasSpin  = 1u << 2; // |angVel| above a threshold
	}

	// =========== name resolvers (used by the JSON/CSV exporters) ===========
	inline const char* EventTypeToString(uint16_t uType)
	{
		switch (static_cast<EventType>(uType))
		{
		case EventType::None:          return "None";
		case EventType::MatchStart:    return "MatchStart";
		case EventType::PointStart:    return "PointStart";
		case EventType::ServeStruck:   return "ServeStruck";
		case EventType::Contact:       return "Contact";
		case EventType::Bounce:        return "Bounce";
		case EventType::NetCross:      return "NetCross";
		case EventType::PointResolved: return "PointResolved";
		case EventType::GameWon:       return "GameWon";
		case EventType::Fault:         return "Fault";
		case EventType::DoubleFault:   return "DoubleFault";
		case EventType::NavFallback:   return "NavFallback";
		case EventType::PhaseChange:   return "PhaseChange";
		case EventType::MatchOver:     return "MatchOver";
		case EventType::_Count:
		default:                       return nullptr;
		}
	}

	inline const char* PointReasonToString(int32_t iReason)
	{
		switch (static_cast<PointReason>(iReason))
		{
		case PointReason::Unknown:          return "Unknown";
		case PointReason::ServeUnreturned:  return "ServeUnreturned";
		case PointReason::DoubleFault:      return "DoubleFault";
		case PointReason::OutOfRangeMiss:   return "OutOfRangeMiss";
		case PointReason::LandedOut:        return "LandedOut";
		case PointReason::IntoNetOrOwnSide: return "IntoNetOrOwnSide";
		case PointReason::DoubleBounce:     return "DoubleBounce";
		case PointReason::ServeTimeout:     return "ServeTimeout";
		case PointReason::RallyTimeout:     return "RallyTimeout";
		default:                            return "?";
		}
	}

	// =========== Emit helper ===========
	// Records one event on the global recorder. A no-op unless a recording is
	// active (Recorder::RecordEvent early-returns when not recording), so the
	// referee can call this unconditionally.
	inline void Emit(EventType eType,
		Zenith_EntityID xA = Zenith_EntityID{}, Zenith_EntityID xB = Zenith_EntityID{},
		int32_t i0 = 0, int32_t i1 = 0, int32_t i2 = 0, int32_t i3 = 0,
		float f0 = 0.0f, float f1 = 0.0f, float f2 = 0.0f, float f3 = 0.0f,
		const char* szLabel = nullptr)
	{
		Zenith_Telemetry::Event xE;
		xE.uEventType = static_cast<uint16_t>(eType);
		xE.xPayload.xEntityA = xA;
		xE.xPayload.xEntityB = xB;
		xE.xPayload.aiInts[0] = i0;
		xE.xPayload.aiInts[1] = i1;
		xE.xPayload.aiInts[2] = i2;
		xE.xPayload.aiInts[3] = i3;
		xE.xPayload.afFloats[0] = f0;
		xE.xPayload.afFloats[1] = f1;
		xE.xPayload.afFloats[2] = f2;
		xE.xPayload.afFloats[3] = f3;
		// Fixed-buffer label copy (avoids std::strncpy /W4 deprecation).
		if (szLabel != nullptr)
		{
			const size_t uMax = sizeof(xE.xPayload.szLabel) - 1u;
			size_t uI = 0;
			for (; uI < uMax && szLabel[uI] != '\0'; ++uI)
				xE.xPayload.szLabel[uI] = szLabel[uI];
			xE.xPayload.szLabel[uI] = '\0';
		}
		// Subsystem tag for the timeline.
		const char szSrc[] = "TennisReferee";
		for (size_t uI = 0; uI < sizeof(szSrc) && uI < sizeof(xE.xPayload.szSource); ++uI)
			xE.xPayload.szSource[uI] = szSrc[uI];

		Zenith_Telemetry::GetRecorder().RecordEvent(xE);
	}
}
