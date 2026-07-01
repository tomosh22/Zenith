#pragma once

#include "Core/Zenith_Result.h"
#include "DataStream/Zenith_DataStream.h"

// Zenith_StreamEnvelope - a reusable binary header ("envelope") that prefixes a
// typed-asset DataStream payload. It generalises the bespoke magic+version
// blocks that already live inline in the scene path
// (Zenith_SceneData::uSCENE_MAGIC / ValidateSceneStream) and the .zdata path
// (Zenith_AssetRegistry::ZDATA_MAGIC). Reuses the shared Zenith_Result /
// Zenith_ErrorCode vocabulary (BAD_MAGIC, VERSION_MISMATCH) so the asset-load
// boundary speaks one error language.
//
// Layout on the wire (all u_int, little-endian — matches operator<< for PODs):
//   [uMagic][uEnvelopeVersion][uAssetTypeId][uSchemaVersion]
// followed immediately by the caller's payload.
//
// Back-compat: Zenith_ReadStreamHeader does a NON-DESTRUCTIVE peek (saves and
// restores the stream cursor on every return path, exactly like
// ValidateSceneStream). A legacy headerless stream therefore returns BAD_MAGIC
// with the cursor untouched, letting the caller rewind and read the old
// (version 0) layout. Kept dependency-light (Zenith_Result.h +
// Zenith_DataStream.h only) so it can be included from typed-asset .cpp files
// without dragging in extra headers.

struct Zenith_StreamHeader
{
	u_int m_uMagic;
	u_int m_uEnvelopeVersion;
	u_int m_uAssetTypeId;
	u_int m_uSchemaVersion;
};

// Shared envelope constants — single source of truth for every typed asset that
// adopts the envelope. Bump uENVELOPE_VERSION_CURRENT only when the envelope
// LAYOUT itself changes (not when a payload schema changes — that's the
// per-asset uSchemaVersion field).
static constexpr u_int uSTREAM_ENVELOPE_MAGIC           = 0x5A4E5448;  // "ZNTH" little-endian
static constexpr u_int uSTREAM_ENVELOPE_VERSION_CURRENT = 1;

// Writes the 4-field header at the current cursor. Call before the payload.
void Zenith_WriteStreamHeader(Zenith_DataStream& xStream, u_int uAssetTypeId, u_int uSchemaVersion);

// Non-destructive header read. On success, returns the parsed header AND leaves
// the cursor positioned immediately after the header (ready for the payload).
// On BAD_MAGIC / VERSION_MISMATCH, restores the cursor to its entry offset so a
// legacy headerless stream can be rewound and read by the old path.
//   - BAD_MAGIC        : magic mismatch (or stream too small to hold a header)
//   - VERSION_MISMATCH : envelope version newer than uSTREAM_ENVELOPE_VERSION_CURRENT
//   - INVALID_ARGUMENT : asset-type-id mismatch against uExpectedTypeId
Zenith_Result<Zenith_StreamHeader> Zenith_ReadStreamHeader(Zenith_DataStream& xStream, u_int uExpectedTypeId);

// Resolve a typed-asset stream's schema version — the read preamble every typed
// asset's ParseStream shares. Reads the shared envelope (validating uExpectedTypeId)
// and yields its schema version; on a legacy pre-envelope stream (BAD_MAGIC, cursor
// restored) reads the bare leading version word instead. Returns the envelope error
// (INVALID_ARGUMENT on a type-id mismatch, VERSION_MISMATCH on a newer envelope) on a
// genuine failure; the caller applies its own per-schema payload policy afterwards.
Zenith_Status Zenith_ReadAssetStreamVersion(Zenith_DataStream& xStream, u_int uExpectedTypeId, uint32_t& uOutVersion);
