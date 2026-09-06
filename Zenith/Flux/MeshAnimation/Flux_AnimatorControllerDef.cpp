#include "Zenith.h"
#include "Flux_AnimatorControllerDef.h"
#include "AssetHandling/Zenith_AssetTypeIds.h"
#include "DataStream/Zenith_StreamEnvelope.h"

//=============================================================================
// Flux_AnimatorControllerLayerDef
//=============================================================================
Flux_AnimatorControllerLayerDef::Flux_AnimatorControllerLayerDef(const std::string& strName)
	: m_strName(strName)
{
}

void Flux_AnimatorControllerLayerDef::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_uLayerId;
	xStream << m_strName;
	xStream << m_fWeight;
	xStream << static_cast<uint8_t>(m_eBlendMode);
	xStream << m_bEmitEvents;
	xStream << m_strBoneMaskAssetPath;

	// EMBEDDED (D46). The def writes its own payload with no envelope of its own —
	// see Flux_AnimationStateMachineDef's class comment.
	m_xStateMachineDef.WriteToDataStream(xStream);
}

void Flux_AnimatorControllerLayerDef::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_uLayerId;
	xStream >> m_strName;
	xStream >> m_fWeight;

	uint8_t uBlendMode = 0;
	xStream >> uBlendMode;
	Zenith_Assert(uBlendMode <= LAYER_BLEND_ADDITIVE,
		"Flux_AnimatorControllerLayerDef: invalid blend mode %u - possible corruption", uBlendMode);
	if (uBlendMode > LAYER_BLEND_ADDITIVE)
	{
		uBlendMode = LAYER_BLEND_OVERRIDE;
	}
	m_eBlendMode = static_cast<Flux_LayerBlendMode>(uBlendMode);

	xStream >> m_bEmitEvents;
	xStream >> m_strBoneMaskAssetPath;

	m_xStateMachineDef.ReadFromDataStream(xStream);
}

//=============================================================================
// Flux_AnimatorControllerDef
//=============================================================================
Flux_AnimatorControllerDef::~Flux_AnimatorControllerDef()
{
	Clear();
}

void Flux_AnimatorControllerDef::Clear()
{
	delete m_pxStateMachineDef;
	m_pxStateMachineDef = nullptr;

	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		delete m_xLayers.Get(u);
	}
	m_xLayers.Clear();

	m_xClipPaths.Clear();
	m_strName.clear();
	m_uNextLayerId = 0;
}

void Flux_AnimatorControllerDef::CopyFrom(const Flux_AnimatorControllerDef& xSource)
{
	if (this == &xSource)
	{
		return;
	}

	Zenith_DataStream xStream(1);
	xSource.WriteToDataStream(xStream);
	xStream.SetCursor(0);
	// A stream this object just produced cannot fail its own envelope check; the
	// status is dropped deliberately rather than being turned into a second,
	// unreachable failure mode.
	ParseStream(xStream);
}

void Flux_AnimatorControllerDef::AddClipPath(const std::string& strPath)
{
	if (strPath.empty())
	{
		return;
	}
	for (u_int u = 0; u < m_xClipPaths.GetSize(); ++u)
	{
		if (m_xClipPaths.Get(u) == strPath)
		{
			return;
		}
	}
	m_xClipPaths.PushBack(strPath);
}

Flux_AnimationStateMachineDef& Flux_AnimatorControllerDef::GetOrCreateStateMachineDef()
{
	if (m_pxStateMachineDef == nullptr)
	{
		m_pxStateMachineDef = new Flux_AnimationStateMachineDef();
	}
	return *m_pxStateMachineDef;
}

void Flux_AnimatorControllerDef::ClearStateMachineDef()
{
	delete m_pxStateMachineDef;
	m_pxStateMachineDef = nullptr;
}

Flux_AnimatorControllerLayerDef* Flux_AnimatorControllerDef::AddLayer(const std::string& strName)
{
	Flux_AnimatorControllerLayerDef* pxLayer = new Flux_AnimatorControllerLayerDef(strName);
	pxLayer->SetLayerId(m_uNextLayerId++);
	pxLayer->GetStateMachineDef().SetName(strName);
	m_xLayers.PushBack(pxLayer);
	return pxLayer;
}

Flux_AnimatorControllerLayerDef* Flux_AnimatorControllerDef::GetLayer(u_int uIndex)
{
	return (uIndex < m_xLayers.GetSize()) ? m_xLayers.Get(uIndex) : nullptr;
}

const Flux_AnimatorControllerLayerDef* Flux_AnimatorControllerDef::GetLayer(u_int uIndex) const
{
	return (uIndex < m_xLayers.GetSize()) ? m_xLayers.Get(uIndex) : nullptr;
}

Flux_AnimatorControllerLayerDef* Flux_AnimatorControllerDef::FindLayerById(u_int uLayerId)
{
	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		if (m_xLayers.Get(u)->GetLayerId() == uLayerId)
		{
			return m_xLayers.Get(u);
		}
	}
	return nullptr;
}

const Flux_AnimatorControllerLayerDef* Flux_AnimatorControllerDef::FindLayerById(u_int uLayerId) const
{
	for (u_int u = 0; u < m_xLayers.GetSize(); ++u)
	{
		if (m_xLayers.Get(u)->GetLayerId() == uLayerId)
		{
			return m_xLayers.Get(u);
		}
	}
	return nullptr;
}

void Flux_AnimatorControllerDef::AssignLayerId(Flux_AnimatorControllerLayerDef& xLayer, u_int uLayerId)
{
	xLayer.SetLayerId(uLayerId);
	if (uLayerId >= m_uNextLayerId)
	{
		m_uNextLayerId = uLayerId + 1u;
	}
}

void Flux_AnimatorControllerDef::RemoveLayer(u_int uIndex)
{
	if (uIndex >= m_xLayers.GetSize())
	{
		return;
	}
	// ★ m_uNextLayerId is NOT rewound. An id a removed layer held must never come
	// back: anything holding it (WU-6.3's addressing, an editor selection) would
	// silently start naming a different layer.
	delete m_xLayers.Get(uIndex);
	m_xLayers.Remove(uIndex);
}

//=============================================================================
// Serialization
//=============================================================================
void Flux_AnimatorControllerDef::WriteToDataStream(Zenith_DataStream& xStream) const
{
	Zenith_WriteStreamHeader(xStream, uZENITH_ANIMCTRL_ASSET_TYPE_ID, uZENITH_ANIMCTRL_SCHEMA_CURRENT);

	xStream << m_strName;
	xStream << m_uNextLayerId;

	// Clip paths
	const u_int uNumClips = m_xClipPaths.GetSize();
	xStream << uNumClips;
	for (u_int u = 0; u < uNumClips; ++u)
	{
		xStream << m_xClipPaths.Get(u);
	}

	// Top-level state machine (optional)
	const bool bHasStateMachine = (m_pxStateMachineDef != nullptr);
	xStream << bHasStateMachine;
	if (bHasStateMachine)
	{
		m_pxStateMachineDef->WriteToDataStream(xStream);
	}

	// Layers
	const u_int uNumLayers = m_xLayers.GetSize();
	xStream << uNumLayers;
	for (u_int u = 0; u < uNumLayers; ++u)
	{
		m_xLayers.Get(u)->WriteToDataStream(xStream);
	}
}

Zenith_Status Flux_AnimatorControllerDef::ParseStream(Zenith_DataStream& xStream)
{
	// The envelope is MANDATORY and the schema must be EXACTLY current — the same
	// contract .zanim carries, and for the same reason: a .zanimctrl is authored
	// or baked output, so an unrecognised layout is a stale file to rewrite rather
	// than a layout to guess at. Zenith_ReadStreamHeader restores the cursor on
	// every failure path, so a refused stream is handed back untouched.
	//
	// ★ EVERY REFUSAL RETURNS ITS ERROR CODE AS WELL AS ASSERTING, and leaves this
	// def CLEARED. The assert is the developer-time signal; the status is what
	// Zenith_AnimatorControllerAsset::LoadFromFile turns into a failed load, so a
	// refused file cannot be cached as an empty-but-successful controller.
	Zenith_Result<Zenith_StreamHeader> xHeader = Zenith_ReadStreamHeader(xStream, uZENITH_ANIMCTRL_ASSET_TYPE_ID);
	if (!xHeader.IsOk())
	{
		Zenith_Assert(false, "Flux_AnimatorControllerDef::ParseStream: stream carries no valid " ZENITH_ANIMCTRL_EXT " envelope");
		Clear();
		return xHeader.Error();
	}
	if (xHeader.Value().m_uSchemaVersion != uZENITH_ANIMCTRL_SCHEMA_CURRENT)
	{
		Zenith_Assert(false, "Flux_AnimatorControllerDef::ParseStream: " ZENITH_ANIMCTRL_EXT " schema %u is not the current %u",
			xHeader.Value().m_uSchemaVersion, uZENITH_ANIMCTRL_SCHEMA_CURRENT);
		Clear();
		return Zenith_ErrorCode::VERSION_MISMATCH;
	}

	Clear();

	xStream >> m_strName;
	xStream >> m_uNextLayerId;

	u_int uNumClips = 0;
	xStream >> uNumClips;
	constexpr u_int uMAX_CLIPS = 4096u;
	Zenith_Assert(uNumClips <= uMAX_CLIPS,
		"Flux_AnimatorControllerDef: clip count %u exceeds limit - possible corruption", uNumClips);
	if (uNumClips > uMAX_CLIPS)
	{
		Clear();
		return Zenith_ErrorCode::CORRUPT_DATA;
	}
	m_xClipPaths.Reserve(uNumClips);
	for (u_int u = 0; u < uNumClips; ++u)
	{
		std::string strPath;
		xStream >> strPath;
		m_xClipPaths.PushBack(strPath);
	}

	bool bHasStateMachine = false;
	xStream >> bHasStateMachine;
	if (bHasStateMachine)
	{
		m_pxStateMachineDef = new Flux_AnimationStateMachineDef();
		m_pxStateMachineDef->ReadFromDataStream(xStream);
	}

	u_int uNumLayers = 0;
	xStream >> uNumLayers;
	constexpr u_int uMAX_LAYERS = 256u;
	Zenith_Assert(uNumLayers <= uMAX_LAYERS,
		"Flux_AnimatorControllerDef: layer count %u exceeds limit - possible corruption", uNumLayers);
	if (uNumLayers > uMAX_LAYERS)
	{
		Clear();
		return Zenith_ErrorCode::CORRUPT_DATA;
	}
	for (u_int u = 0; u < uNumLayers; ++u)
	{
		Flux_AnimatorControllerLayerDef* pxLayer = new Flux_AnimatorControllerLayerDef();
		pxLayer->ReadFromDataStream(xStream);
		m_xLayers.PushBack(pxLayer);
	}

	return true;
}

void Flux_AnimatorControllerDef::ReadFromDataStream(Zenith_DataStream& xStream)
{
	ParseStream(xStream);
}

bool Flux_AnimatorControllerDef::Export(const std::string& strPath) const
{
	if (strPath.empty())
	{
		Zenith_Assert(false, "Flux_AnimatorControllerDef::Export: empty path");
		return false;
	}

	Zenith_DataStream xStream;
	WriteToDataStream(xStream);
	xStream.WriteToFile(strPath.c_str());

	Zenith_Log(LOG_CATEGORY_ANIMATION, "[AnimatorControllerDef] Exported '%s' to: %s",
		m_strName.c_str(), strPath.c_str());
	return true;
}

#ifdef ZENITH_TESTING
#include "Flux/MeshAnimation/Flux_AnimatorControllerDef.Tests.inl"
#endif
