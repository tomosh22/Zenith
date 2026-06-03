#pragma once

#ifdef ZENITH_TOOLS

// Engine-side, header-only friend-class wrapper that exposes the editor-only
// Zenith_SceneData verbs to editor code. Zenith_Editor is a NAMESPACE and so
// cannot itself be `friend`ed by Zenith_SceneData; this class can. Every verb
// below was moved to Zenith_SceneData's private section (it had no external,
// non-editor, non-friend caller), and the editor reaches it through these
// static inline forwarders instead of touching the private member directly.
//
// Zenith_SceneData grants this class friend access via
//   friend class Zenith_EditorSceneAccess;
// (declared inside its #ifdef ZENITH_TOOLS friend block).

#include "ZenithECS/Zenith_SceneData.h"
#include "ZenithECS/Zenith_Entity.h"
#include "Collections/Zenith_Vector.h"
#include <string>

class Zenith_EditorSceneAccess
{
public:
	static void RemoveEntity(Zenith_SceneData* pxData, Zenith_EntityID xID)
	{
		pxData->RemoveEntity(xID);
	}

	static void SaveToFile(Zenith_SceneData* pxData, const std::string& strFilename, bool bIncludeTransient = false)
	{
		pxData->SaveToFile(strFilename, bIncludeTransient);
	}

	static bool LoadFromFile(Zenith_SceneData* pxData, const std::string& strFilename)
	{
		return pxData->LoadFromFile(strFilename);
	}

	static u_int GetEntityCount(const Zenith_SceneData* pxData)
	{
		return pxData->GetEntityCount();
	}

	static void SetMainCameraEntity(Zenith_SceneData* pxData, Zenith_EntityID xEntity)
	{
		pxData->SetMainCameraEntity(xEntity);
	}

	template<typename T>
	static void GetAllOfComponentType(const Zenith_SceneData* pxData, Zenith_Vector<T*>& xOut)
	{
		pxData->GetAllOfComponentType<T>(xOut);
	}

	static void Editor_SetPath(Zenith_SceneData* pxData, const std::string& strPath)
	{
		pxData->Editor_SetPath(strPath);
	}

	static void Editor_SetBuildIndex(Zenith_SceneData* pxData, int iBuildIndex)
	{
		pxData->Editor_SetBuildIndex(iBuildIndex);
	}

	static void Editor_MarkEntityStarted(Zenith_SceneData* pxData, Zenith_EntityID xID)
	{
		pxData->Editor_MarkEntityStarted(xID);
	}
};

#endif // ZENITH_TOOLS
