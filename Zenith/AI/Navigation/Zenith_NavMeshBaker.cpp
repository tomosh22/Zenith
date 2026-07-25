#include "Zenith.h"
#include "AI/Navigation/Zenith_NavMeshBaker.h"

#include "AI/Navigation/Zenith_NavMesh.h"
#include "DataStream/Zenith_DataStream.h"
#include "FileAccess/Zenith_FileAccess.h"

#include <cstring>
#include <filesystem>

const char* Zenith_NavMeshBakeErrorToString(Zenith_NavMeshBakeError eError)
{
	switch (eError)
	{
	case NAVMESH_BAKE_ERROR_NONE:				return "NONE";
	case NAVMESH_BAKE_ERROR_EMPTY_PATH:			return "EMPTY_PATH";
	case NAVMESH_BAKE_ERROR_EMPTY_GEOMETRY:		return "EMPTY_GEOMETRY";
	case NAVMESH_BAKE_ERROR_GENERATION_FAILED:	return "GENERATION_FAILED";
	case NAVMESH_BAKE_ERROR_SERIALIZE_FAILED:	return "SERIALIZE_FAILED";
	case NAVMESH_BAKE_ERROR_WRITE_FAILED:		return "WRITE_FAILED";
	case NAVMESH_BAKE_ERROR_READBACK_MISMATCH:	return "READBACK_MISMATCH";
	}
	return "UNKNOWN";
}

namespace
{
	Zenith_NavMeshBakeResult MakeBakeFailure(Zenith_NavMeshBakeError eError)
	{
		Zenith_NavMeshBakeResult xResult;
		xResult.m_bSuccess = false;
		xResult.m_eError = eError;
		return xResult;
	}

	// Read the just-written file back and compare it byte-for-byte against the
	// bytes we handed the file layer. Returns NONE on a match.
	Zenith_NavMeshBakeError VerifyWrittenBytes(const std::string& strPath,
		const void* pExpected, uint64_t ulExpectedBytes)
	{
		if (!Zenith_FileAccess::FileExists(strPath.c_str()))
		{
			Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: no file on disk after write: %s", strPath.c_str());
			return NAVMESH_BAKE_ERROR_WRITE_FAILED;
		}

		Zenith_DataStream xVerify;
		xVerify.ReadFromFile(strPath.c_str());
		if (!xVerify.IsValid())
		{
			Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: written file is unreadable: %s", strPath.c_str());
			return NAVMESH_BAKE_ERROR_WRITE_FAILED;
		}

		// A file-loaded stream's capacity IS its byte length.
		if (xVerify.GetCapacity() != ulExpectedBytes)
		{
			Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: wrote %llu bytes but %s holds %llu",
				ulExpectedBytes, strPath.c_str(), xVerify.GetCapacity());
			return NAVMESH_BAKE_ERROR_WRITE_FAILED;
		}

		if (memcmp(xVerify.GetData(), pExpected, static_cast<size_t>(ulExpectedBytes)) != 0)
		{
			Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: bytes on disk differ from bytes written: %s",
				strPath.c_str());
			return NAVMESH_BAKE_ERROR_READBACK_MISMATCH;
		}

		return NAVMESH_BAKE_ERROR_NONE;
	}
}

Zenith_NavMeshBakeResult Zenith_NavMeshBaker::BakeToFile(
	const Zenith_Vector<Zenith_Maths::Vector3>& axVertices,
	const Zenith_Vector<uint32_t>& auIndices,
	const NavMeshGenerationConfig& xConfig,
	const std::string& strPath,
	Zenith_NavMesh** ppxMeshOut)
{
	if (ppxMeshOut != nullptr)
	{
		*ppxMeshOut = nullptr;
	}

	// ---- caller defects: loud, per the D6 doctrine --------------------------
	if (strPath.empty())
	{
		Zenith_Assert(false, "NavMesh bake: empty destination path");
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: empty destination path");
		return MakeBakeFailure(NAVMESH_BAKE_ERROR_EMPTY_PATH);
	}

	if (axVertices.GetSize() == 0u || auIndices.GetSize() == 0u)
	{
		Zenith_Assert(false, "NavMesh bake: empty geometry (%u vertices, %u indices)",
			axVertices.GetSize(), auIndices.GetSize());
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: empty geometry (%u vertices, %u indices)",
			axVertices.GetSize(), auIndices.GetSize());
		return MakeBakeFailure(NAVMESH_BAKE_ERROR_EMPTY_GEOMETRY);
	}

	// ---- generate ----------------------------------------------------------
	Zenith_NavMesh* pxNavMesh = Zenith_NavMeshGenerator::GenerateFromGeometry(axVertices, auIndices, xConfig);
	if (pxNavMesh == nullptr)
	{
		// DATA outcome, not a defect: geometry with no walkable span (an
		// all-vertical soup, or a cell size that voxelises nothing) legitimately
		// yields no mesh. Reported, not asserted.
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: generator produced no walkable mesh for %s",
			strPath.c_str());
		return MakeBakeFailure(NAVMESH_BAKE_ERROR_GENERATION_FAILED);
	}

	// ---- serialize ---------------------------------------------------------
	Zenith_DataStream xStream;
	pxNavMesh->WriteToDataStream(xStream);

	// GetCursor(), never GetCapacity(): an owned stream's capacity is its
	// doubling allocation, not the bytes written.
	const uint64_t ulBytes = xStream.GetCursor();
	if (ulBytes == 0ull)
	{
		Zenith_Error(LOG_CATEGORY_AI, "NavMesh bake: serializer emitted zero bytes for %s", strPath.c_str());
		delete pxNavMesh;
		return MakeBakeFailure(NAVMESH_BAKE_ERROR_SERIALIZE_FAILED);
	}

	// ---- write + read back -------------------------------------------------
	xStream.WriteToFile(strPath.c_str());

	const Zenith_NavMeshBakeError eVerify = VerifyWrittenBytes(strPath, xStream.GetData(), ulBytes);
	if (eVerify != NAVMESH_BAKE_ERROR_NONE)
	{
		// REMOVE the bad file. A bake writes straight into a COMMITTED asset, so
		// leaving a half-written one behind would hand every later load -- and
		// every CI run -- a corrupt navmesh that hard-asserts, long after the
		// bake that produced it. Absent is a recoverable state; corrupt is not.
		std::error_code xEC;
		std::filesystem::remove(strPath, xEC);

		// A failed write to a tracked asset is a DEFECT, not a data outcome --
		// same doctrine (and same condition) as Zenith_NavMesh::SaveToFile.
		Zenith_Assert(false, "NavMesh bake: %s writing %s",
			Zenith_NavMeshBakeErrorToString(eVerify), strPath.c_str());

		delete pxNavMesh;
		return MakeBakeFailure(eVerify);
	}

	Zenith_NavMeshBakeResult xResult;
	xResult.m_bSuccess = true;
	xResult.m_eError = NAVMESH_BAKE_ERROR_NONE;
	xResult.m_uPolygonCount = pxNavMesh->GetPolygonCount();
	xResult.m_uVertexCount = pxNavMesh->GetVertexCount();
	xResult.m_ulBytesWritten = ulBytes;

	Zenith_Log(LOG_CATEGORY_AI, "NavMesh bake: %s (%u vertices, %u polygons, %llu bytes)",
		strPath.c_str(), xResult.m_uVertexCount, xResult.m_uPolygonCount, ulBytes);

	if (ppxMeshOut != nullptr)
	{
		*ppxMeshOut = pxNavMesh;
	}
	else
	{
		delete pxNavMesh;
	}

	return xResult;
}
