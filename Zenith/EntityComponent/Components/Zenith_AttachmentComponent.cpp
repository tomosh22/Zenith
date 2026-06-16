#include "Zenith.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "DataStream/Zenith_DataStream.h"

void Zenith_AttachmentComponent::AttachToBone(Zenith_Entity xSkeletonEntity, const char* szBone,
	const Zenith_Maths::Matrix4& xOffset)
{
	m_xSkeletonEntity = xSkeletonEntity;
	m_strBone = szBone ? szBone : "";
	m_xOffset = xOffset;
	m_bAttached = true;
}

void Zenith_AttachmentComponent::Detach()
{
	m_bAttached = false;
	m_xSkeletonEntity = Zenith_Entity();
}

void Zenith_AttachmentComponent::OnLateUpdate(float)
{
	if (!m_bAttached)
		return;
	if (!m_xSelf.IsValid() || !m_xSkeletonEntity.IsValid())
		return;
	if (!m_xSkeletonEntity.HasComponent<Zenith_ModelComponent>()
		|| !m_xSkeletonEntity.HasComponent<Zenith_TransformComponent>()
		|| !m_xSelf.HasComponent<Zenith_TransformComponent>())
		return;

	// Posed bone model-space matrix (resolved by ModelComponent so this TU names
	// no Flux skeleton type). Bails when the skeleton isn't ready or the bone name
	// is unknown — the attached entity is simply left where it is.
	Zenith_ModelComponent& xSkelModel = m_xSkeletonEntity.GetComponent<Zenith_ModelComponent>();
	Zenith_Maths::Matrix4 xBoneModel;
	if (!xSkelModel.GetBoneModelMatrix(m_strBone, xBoneModel))
	{
		// Warn once if the skeleton is present but the bone name is wrong (a real
		// misconfiguration); stay silent while the skeleton is merely not loaded yet.
		if (!m_bWarnedMissingBone && xSkelModel.HasSkeleton())
		{
			Zenith_Warning(LOG_CATEGORY_ECS,
				"[Attachment] bone '%s' not found on the target skeleton — attached entity left in place",
				m_strBone.c_str());
			m_bWarnedMissingBone = true;
		}
		return;
	}

	// world = skeletonEntityWorld * boneModelTransform * mountOffset.
	// The offset is on the RIGHT (bone-local), so it tracks the bone's frame.
	Zenith_Maths::Matrix4 xSkelWorld;
	m_xSkeletonEntity.GetComponent<Zenith_TransformComponent>().BuildModelMatrix(xSkelWorld);
	const Zenith_Maths::Matrix4 xWorld = xSkelWorld * xBoneModel * m_xOffset;

	Zenith_TransformComponent& xSelfT = m_xSelf.GetComponent<Zenith_TransformComponent>();
	xSelfT.SetPosition(Zenith_Maths::Vector3(xWorld[3]));

	// Orthonormalise the basis before extracting the rotation. quat_cast on a
	// matrix carrying scale/shear (a scaled skeleton entity or non-unit bone/bind
	// scale — both legal for the documented FPS-weapon reuse) yields a non-unit /
	// wrong quaternion. Gram-Schmidt the columns so SetRotation always gets a pure
	// rotation; the unscaled tennis rig is unaffected.
	Zenith_Maths::Matrix3 xBasis(xWorld);
	xBasis[0] = glm::normalize(xBasis[0]);
	xBasis[1] = glm::normalize(xBasis[1] - xBasis[0] * glm::dot(xBasis[0], xBasis[1]));
	xBasis[2] = glm::cross(xBasis[0], xBasis[1]);
	xSelfT.SetRotation(glm::normalize(glm::quat_cast(xBasis)));
}

void Zenith_AttachmentComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{
	// Runtime-only follow state — the skeleton-entity reference is re-established
	// by whoever spawns/attaches the entity (the attachment is set up in code,
	// not authored into a scene), so only the version tag persists.
	const u_int uVersion = 1;
	xStream << uVersion;
}

void Zenith_AttachmentComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0;
	xStream >> uVersion;
	m_bAttached = false;
}
