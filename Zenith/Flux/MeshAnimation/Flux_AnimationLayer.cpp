#include "Zenith.h"
#include "Flux_AnimationLayer.h"

//=============================================================================
// Flux_AnimationLayer
//=============================================================================

Flux_AnimationLayer::Flux_AnimationLayer(const std::string& strName)
	: m_strName(strName)
{
}

Flux_AnimationLayer::~Flux_AnimationLayer()
{
	delete m_pxStateMachine;
}

Flux_AnimationLayer::Flux_AnimationLayer(Flux_AnimationLayer&& xOther) noexcept
	: m_strName(std::move(xOther.m_strName))
	, m_fWeight(xOther.m_fWeight)
	, m_eBlendMode(xOther.m_eBlendMode)
	, m_bHasAvatarMask(xOther.m_bHasAvatarMask)
	, m_xAvatarMask(std::move(xOther.m_xAvatarMask))
	, m_pxStateMachine(xOther.m_pxStateMachine)
	, m_xOutputPose(std::move(xOther.m_xOutputPose))
{
	xOther.m_pxStateMachine = nullptr;
}

Flux_AnimationLayer& Flux_AnimationLayer::operator=(Flux_AnimationLayer&& xOther) noexcept
{
	if (this != &xOther)
	{
		delete m_pxStateMachine;

		m_strName = std::move(xOther.m_strName);
		m_fWeight = xOther.m_fWeight;
		m_eBlendMode = xOther.m_eBlendMode;
		m_bHasAvatarMask = xOther.m_bHasAvatarMask;
		m_xAvatarMask = std::move(xOther.m_xAvatarMask);
		m_pxStateMachine = xOther.m_pxStateMachine;
		m_xOutputPose = std::move(xOther.m_xOutputPose);

		xOther.m_pxStateMachine = nullptr;
	}
	return *this;
}

Flux_AnimationStateMachine& Flux_AnimationLayer::GetStateMachine()
{
	if (!m_pxStateMachine)
	{
		m_pxStateMachine = new Flux_AnimationStateMachine(m_strName);
	}
	return *m_pxStateMachine;
}

Flux_AnimationStateMachine* Flux_AnimationLayer::CreateStateMachine(const std::string& strName)
{
	delete m_pxStateMachine;
	m_pxStateMachine = new Flux_AnimationStateMachine(strName);
	return m_pxStateMachine;
}

void Flux_AnimationLayer::Update(float fDt, const Zenith_SkeletonAsset& xSkeleton)
{
	if (!m_pxStateMachine)
		return;

	m_pxStateMachine->Update(fDt, m_xOutputPose, xSkeleton);
}

void Flux_AnimationLayer::InitializePose(uint32_t uNumBones)
{
	m_xOutputPose.Initialize(uNumBones);
}

void Flux_AnimationLayer::WriteToDataStream(Zenith_DataStream& xStream) const
{
	xStream << m_strName;
	xStream << m_fWeight;
	xStream << static_cast<uint8_t>(m_eBlendMode);

	// Avatar mask
	m_xAvatarMask.WriteToDataStream(xStream);

	// State machine
	bool bHasSM = (m_pxStateMachine != nullptr);
	xStream << bHasSM;
	if (bHasSM)
	{
		m_pxStateMachine->WriteToDataStream(xStream);
	}
}

void Flux_AnimationLayer::ReadFromDataStream(Zenith_DataStream& xStream)
{
	xStream >> m_strName;
	xStream >> m_fWeight;

	uint8_t uBlendMode = 0;
	xStream >> uBlendMode;
	Zenith_Assert(uBlendMode <= LAYER_BLEND_ADDITIVE, "AnimationLayer: Invalid blend mode %u - possible corruption", uBlendMode);
	if (uBlendMode > LAYER_BLEND_ADDITIVE) uBlendMode = LAYER_BLEND_OVERRIDE;
	m_eBlendMode = static_cast<Flux_LayerBlendMode>(uBlendMode);

	// Avatar mask
	m_xAvatarMask.ReadFromDataStream(xStream);
	// Check if any weight is non-zero to determine if mask was explicitly set
	m_bHasAvatarMask = false;
	const std::vector<float>& xWeights = m_xAvatarMask.GetWeights();
	for (size_t j = 0; j < xWeights.size(); ++j)
	{
		if (xWeights[j] > 0.0f)
		{
			m_bHasAvatarMask = true;
			break;
		}
	}

	// State machine
	bool bHasSM = false;
	xStream >> bHasSM;
	if (bHasSM)
	{
		delete m_pxStateMachine;
		m_pxStateMachine = new Flux_AnimationStateMachine();
		m_pxStateMachine->ReadFromDataStream(xStream);
	}
}
