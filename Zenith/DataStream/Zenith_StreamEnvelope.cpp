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
	// Non-destructive: every failure path restores the cursor to its entry offset,
	// so a rejected stream is left exactly as it was handed over and the caller's
	// error handling cannot be confused by a half-consumed cursor. On success the
	// cursor is intentionally left positioned just past the header, ready for the
	// payload.
	const uint64_t ulSavedCursor = xStream.GetCursor();

	// A stream too small to hold a full header cannot carry one, which is the same
	// answer as a mismatched magic: this is not an asset stream.
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
		// Not our envelope. Nothing else is accepted.
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

Zenith_Status Zenith_ReadAssetStreamVersion(Zenith_DataStream& xStream, u_int uExpectedTypeId, uint32_t& uOutVersion)
{
	// ★★ THE ENVELOPE IS MANDATORY. This used to fall back to reading a bare
	// leading version word when the magic did not match, so a pre-envelope file
	// still loaded. That path is gone: a stream without the envelope is not an
	// asset this engine reads, and BAD_MAGIC is returned to the caller like any
	// other failure rather than being caught and worked around.
	//
	// ★ NOTHING IS STRANDED BY THIS, and that is why it can simply be deleted
	// rather than migrated. Every asset file lives under the `**/Assets/**`
	// gitignore and is BAKE OUTPUT: a file in an older layout is a stale bake, and
	// the fix is to delete it and let the tools boot rewrite it — which is the
	// same thing a fresh clone does unconditionally. There is no such file
	// anywhere that is not reproducible from its source.
	Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uExpectedTypeId);
	if (!xHeader.IsOk())
	{
		return xHeader.Error();
	}
	uOutVersion = xHeader.Value().m_uSchemaVersion;
	return true;
}
