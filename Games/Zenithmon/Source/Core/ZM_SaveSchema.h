#pragma once

#include "Core/Zenith_Result.h"
#include "DataStream/Zenith_DataStream.h"
#include "Zenithmon/Source/Party/ZM_GameState.h"

#include <cstdint>

// ============================================================================
// ZM_SaveSchema -- the pure Zenithmon game-payload codec.
//
// This is deliberately below Zenith_SaveData: it owns only the inner ZMSV
// payload and never names slots, files, ECS components, or runtime scenes.
// Writes append one complete payload transactionally. Reads consume one caller-
// bounded payload transactionally and publish a GameState only after every
// framed module validates.
// ============================================================================

namespace ZM_SaveSchema
{
	static constexpr uint32_t uMAGIC = 0x56534D5Au; // "ZMSV" little-endian
	static constexpr uint32_t uSCHEMA_VERSION_CURRENT = 1u;
	static constexpr uint32_t uMODULE_VERSION_CURRENT = 1u;
	static constexpr uint32_t uMODULE_COUNT = 11u;

	Zenith_Status Write(const ZM_GameState& xState, Zenith_DataStream& xOutStream);
	Zenith_Status Read(Zenith_DataStream& xInStream, uint64_t ulByteLength,
		ZM_GameState& xOutState);
}
