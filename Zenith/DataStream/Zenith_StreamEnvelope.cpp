#include "Zenith.h"
#include "DataStream/Zenith_StreamEnvelope.h"

void Zenith_WriteStreamHeader(Zenith_DataStream& xStream, u_int uAssetTypeId, u_int uSchemaVersion)
{
	xStream << uSTREAM_ENVELOPE_MAGIC;
	xStream << uSTREAM_ENVELOPE_VERSION_CURRENT;
	xStream << uAssetTypeId;
	xStream << uSchemaVersion;
}

Zenith_Result<Zenith_StreamHeader> Zenith_ReadStreamHeader(Zenith_DataStream& xStream, u_int uExpectedTypeId)
{
	// Non-destructive: every failure path restores the cursor to its entry
	// offset so a legacy headerless stream can be rewound and read by the old
	// path. Mirrors Zenith_SceneData::ValidateSceneStream. On success the cursor
	// is intentionally left positioned just past the header, ready for the payload.
	const uint64_t ulSavedCursor = xStream.GetCursor();

	// A stream too small to contain a full header is treated as headerless
	// (legacy). Report BAD_MAGIC so the caller rewinds and reads the old layout.
	static constexpr uint64_t ulHEADER_SIZE = sizeof(u_int) * 4;
	if (!xStream.IsValid() || (xStream.GetCapacity() - ulSavedCursor) < ulHEADER_SIZE)
	{
		xStream.SetCursor(ulSavedCursor);
		return Zenith_ErrorCode::BAD_MAGIC;
	}

	Zenith_StreamHeader xHeader;
	xStream >> xHeader.m_uMagic;
	xStream >> xHeader.m_uEnvelopeVersion;
	xStream >> xHeader.m_uAssetTypeId;
	xStream >> xHeader.m_uSchemaVersion;

	if (xHeader.m_uMagic != uSTREAM_ENVELOPE_MAGIC)
	{
		// Not our envelope — almost certainly a legacy headerless payload.
		xStream.SetCursor(ulSavedCursor);
		return Zenith_ErrorCode::BAD_MAGIC;
	}

	if (xHeader.m_uEnvelopeVersion > uSTREAM_ENVELOPE_VERSION_CURRENT)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Zenith_ReadStreamHeader: envelope version %u is newer than supported (%u)",
			xHeader.m_uEnvelopeVersion, uSTREAM_ENVELOPE_VERSION_CURRENT);
		xStream.SetCursor(ulSavedCursor);
		return Zenith_ErrorCode::VERSION_MISMATCH;
	}

	if (xHeader.m_uAssetTypeId != uExpectedTypeId)
	{
		Zenith_Error(LOG_CATEGORY_ASSET, "Zenith_ReadStreamHeader: asset-type-id mismatch (got %u, expected %u)",
			xHeader.m_uAssetTypeId, uExpectedTypeId);
		xStream.SetCursor(ulSavedCursor);
		return Zenith_ErrorCode::INVALID_ARGUMENT;
	}

	// Success: cursor is left immediately after the header for the payload read.
	return xHeader;
}
