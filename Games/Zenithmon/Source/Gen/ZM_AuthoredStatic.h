#pragma once

#ifdef ZENITH_TOOLS
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "AssetHandling/Zenith_ModelAsset.h"
#include "DataStream/Zenith_DataStream.h"
#include "../../../../Tools/Zenith_Tools_GlbImport.h"
#include <filesystem>

// Source ownership is decided before a procedural bake can write any files.
// A source with a failed import stays an error; it must not silently become a
// generated model. Without a source the existing generator remains available.
inline bool ZM_TryAuthoredStatic(const char* szModelRef, bool bForceImport, bool& bSuccess)
{
	const std::filesystem::path xModel(Zenith_AssetRegistry::ResolvePath(szModelRef));
	auto xSource = xModel; xSource.replace_extension(".glb");
	std::error_code xError;
	if (!std::filesystem::is_regular_file(xSource, xError)) { return false; }
	bSuccess = false;
	auto xMesh = xModel; xMesh.replace_extension(".zmesh");
	if (!bForceImport && std::filesystem::is_regular_file(xModel, xError) &&
		std::filesystem::is_regular_file(xMesh, xError) &&
		std::filesystem::last_write_time(xMesh, xError) >= std::filesystem::last_write_time(xSource, xError) && !xError)
	{
		Zenith_DataStream xStream;
		xStream.ReadFromFile(xModel.string().c_str());
		if (xStream.IsValid())
		{
			Zenith_ModelAsset xAsset;
			if (xAsset.ParseStream(xStream).IsOk() && xAsset.GetNumMeshes() == 1u && !xAsset.HasSkeleton())
			{
				const auto xBoundMesh = std::filesystem::path(Zenith_AssetRegistry::ResolvePath(
					xAsset.GetMeshBinding(0u).GetMeshPath()));
				bSuccess = std::filesystem::equivalent(xBoundMesh, xMesh, xError) && !xError;
				if (bSuccess) { return true; }
			}
		}
	}
	bSuccess = Zenith_Tools_GlbImport::ImportGlbFile(xSource.string()).m_bSuccess;
	return true;
}
#endif
