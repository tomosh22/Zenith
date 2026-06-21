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

	// A live (re)attach supersedes any deserialized binding awaiting resolution.
	m_uPendingSkeletonFileIndex = Zenith_EntityID::INVALID_INDEX;
	m_bPendingAttached = false;
}

void Zenith_AttachmentComponent::Detach()
{
	m_bAttached = false;
	m_xSkeletonEntity = Zenith_Entity();

	m_uPendingSkeletonFileIndex = Zenith_EntityID::INVALID_INDEX;
	m_bPendingAttached = false;
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
	// v2: persist the bone binding so an attachment authored into a scene (a racket on
	// an NPC's hand, a jetpack on the player's spine) survives a load. The skeleton
	// reference is written as its slot index — which IS the scene file-index, exactly
	// like the main-camera entity — and re-resolved on load by ResolveEntityReferences.
	// v1 (version tag only) still loads back as detached.
	const u_int uVersion = 2;
	xStream << uVersion;

	xStream << m_bAttached;
	xStream << m_strBone;

	// Offset as 16 explicit floats (column-major, nested [i][j]) — an ABI-stable wire
	// format independent of the glm::mat4 in-memory layout (mirrors
	// Flux_AnimationController's matrix serialization).
	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			xStream << m_xOffset[i][j];
		}
	}

	const uint32_t uSkeletonFileIndex = (m_bAttached && m_xSkeletonEntity.IsValid())
		? m_xSkeletonEntity.GetEntityID().m_uIndex
		: Zenith_EntityID::INVALID_INDEX;
	xStream << uSkeletonFileIndex;
}

void Zenith_AttachmentComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{
	u_int uVersion = 0;
	xStream >> uVersion;

	// Reset to a clean detached state; the scene-load resolve pass re-establishes any
	// binding (and leaves it detached in non-scene contexts, e.g. prefab instantiation).
	m_bAttached = false;
	m_xSkeletonEntity = Zenith_Entity();
	m_uPendingSkeletonFileIndex = Zenith_EntityID::INVALID_INDEX;
	m_bPendingAttached = false;

	if (uVersion < 2)
	{
		return;   // v1 carried no binding.
	}

	xStream >> m_bPendingAttached;
	xStream >> m_strBone;

	for (int i = 0; i < 4; ++i)
	{
		for (int j = 0; j < 4; ++j)
		{
			xStream >> m_xOffset[i][j];
		}
	}

	xStream >> m_uPendingSkeletonFileIndex;
}

void Zenith_AttachmentComponent::ResolveEntityReferences(const Zenith_HashMap<uint32_t, Zenith_EntityID>& xMap)
{
	// Single-use pending state: capture then clear, so a failed resolve (or a
	// non-scene context that never calls this) leaves the component cleanly detached.
	const bool bWantAttach = m_bPendingAttached;
	const uint32_t uPendingIndex = m_uPendingSkeletonFileIndex;
	m_uPendingSkeletonFileIndex = Zenith_EntityID::INVALID_INDEX;
	m_bPendingAttached = false;

	if (!bWantAttach)
	{
		return;   // authored detached (or v1) — nothing to bind.
	}

	// The attachment and its skeleton target were loaded from the same scene, so the
	// owner's SceneData resolves the remapped EntityID into a live handle.
	Zenith_SceneData* pxSceneData = m_xSelf.GetSceneData();
	if (pxSceneData != nullptr && uPendingIndex != Zenith_EntityID::INVALID_INDEX)
	{
		if (const Zenith_EntityID* pxID = xMap.TryGet(uPendingIndex))
		{
			Zenith_Entity xTarget = pxSceneData->GetEntity(*pxID);
			if (xTarget.IsValid())
			{
				m_xSkeletonEntity = xTarget;
				m_bAttached = true;
				return;
			}
		}
	}

	// Unresolved: the target was transient-excluded, cross-scene, or missing. Stay
	// detached so OnLateUpdate no-ops rather than chasing a stale handle.
	Zenith_Warning(LOG_CATEGORY_ECS,
		"[Attachment] could not resolve skeleton entity for bone '%s' on scene load — left detached",
		m_strBone.c_str());
}

#include "EntityComponent/Components/Zenith_AttachmentComponent.Tests.inl"
