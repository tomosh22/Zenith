#include "Zenith.h"

// ============================================================================
// ZM_CreatureAnimCommon -- shared procedural-clip kit implementation. See the
// header for the unit-duality + rotation-only contract. ZM_AnimAddRotCurve is a
// header-inline template (it takes a captureless/small-capture callable); only
// the non-template atoms live here.
// ============================================================================

#include "Zenithmon/Source/Gen/ZM_CreatureAnimCommon.h"

namespace
{
	// Local axis constants (unit axes for angleAxis).
	const Zenith_Maths::Vector3 xZM_ANIM_AXIS_X(1.0f, 0.0f, 0.0f);
	const Zenith_Maths::Vector3 xZM_ANIM_AXIS_Y(0.0f, 1.0f, 0.0f);
	const Zenith_Maths::Vector3 xZM_ANIM_AXIS_Z(0.0f, 0.0f, 1.0f);
}

Zenith_Maths::Quat ZM_AnimRotX(float fDeg)
{
	return glm::angleAxis(glm::radians(fDeg), xZM_ANIM_AXIS_X);
}

Zenith_Maths::Quat ZM_AnimRotY(float fDeg)
{
	return glm::angleAxis(glm::radians(fDeg), xZM_ANIM_AXIS_Y);
}

Zenith_Maths::Quat ZM_AnimRotZ(float fDeg)
{
	return glm::angleAxis(glm::radians(fDeg), xZM_ANIM_AXIS_Z);
}

Zenith_Maths::Quat ZM_AnimRotCompose(const Zenith_Maths::Quat& xA, const Zenith_Maths::Quat& xB)
{
	return xA * xB;
}

void ZM_AnimAddRotKeys(Flux_AnimationClip& xClip, const char* szBone,
	const ZM_AnimRotKey* pxKeys, u_int uCount)
{
	Zenith_Assert(pxKeys != nullptr && uCount >= 1u,
		"ZM_AnimAddRotKeys: need >= 1 key for bone '%s'", szBone);

	Flux_BoneChannel xChannel;
	for (u_int u = 0; u < uCount; ++u)
	{
		xChannel.AddRotationKeyframe(pxKeys[u].m_fTick, pxKeys[u].m_xRot);
	}
	xChannel.SortKeyframes();
	xClip.AddBoneChannel(szBone, std::move(xChannel));
}
