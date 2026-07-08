#include "Zenith.h"
#include "Core/Zenith_Engine.h"
#include "Scripting/Zenith_GraphNodeRegistry.h"
#include "Scripting/Zenith_GraphBlackboard.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_AttachmentComponent.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Zenith_CameraResolve.h"
#include "EntityComponent/Zenith_GraphNodeHelpers.h"
#include "AssetHandling/Zenith_AssetRegistry.h"
#include "Prefab/Zenith_Prefab.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "ZenithECS/Zenith_Query.h"
#include "ZenithECS/Zenith_ComponentMeta.h"

#include <cmath>

//------------------------------------------------------------------------------
// Engine Behaviour Graph node library - Entity domain.
//
// Cross-entity queries and transforms built on the entity-targeting
// convention (m_strTargetVar / Zenith_GraphContext::ResolveTargetEntity;
// "" = self). Position-reference vars are polymorphic: a var may hold a
// VECTOR3 (world position) or a packed ENTITY_ID (resolved to that entity's
// transform position) - the ComputeDistance/Direction primitive every chase /
// range-gate chain builds on.
//------------------------------------------------------------------------------

namespace
{
	// Position references resolve through the shared helper
	// (Zenith_GraphNodeHelpers.h): "" = self, vec3 var = that position,
	// EntityID var = that entity's position.
	bool ResolvePositionRef(Zenith_GraphContext& xContext, const std::string& strVar, Zenith_Maths::Vector3& xOut)
	{
		return Zenith_GraphNode_ResolvePositionRef(xContext, strVar, xOut);
	}

	// Target's world position -> vec3 blackboard var.
	class Zenith_GraphNode_ReadEntityPosition : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadEntityPosition)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "pos")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TransformComponent* pxTransform = xTarget.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xPosition;
			pxTransform->GetPosition(xPosition);
			Zenith_PropertyValue xValue;
			xValue.SetVector3(xPosition);
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadEntityPosition"; }
	};

	// Set the target's transform scale (constant or vec3 var override).
	class Zenith_GraphNode_SetEntityScale : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetEntityScale)
	public:
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xScale, Zenith_Maths::Vector3(1.0f, 1.0f, 1.0f))
		ZENITH_PROPERTY(std::string, m_strScaleVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TransformComponent* pxTransform = xTarget.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_Maths::Vector3 xScale = m_strScaleVar.empty()
				? m_xScale : xContext.m_pxBlackboard->GetVector3(m_strScaleVar, m_xScale);
			pxTransform->SetScale(xScale);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetEntityScale"; }
	};

	// Does the EntityID var resolve to a live entity? -> bool var (the BT
	// HasTarget gate; always SUCCESS - gate downstream with Branch/Gate).
	class Zenith_GraphNode_QueryEntityValid : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_QueryEntityValid)
	public:
		ZENITH_PROPERTY(std::string, m_strEntityVar, "target")
		ZENITH_PROPERTY(std::string, m_strResultVar, "hasTarget")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			const Zenith_Entity xEntity = xContext.ResolveTargetEntity(m_strEntityVar);
			Zenith_PropertyValue xValue;
			xValue.SetBool(xEntity.IsValid());
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "QueryEntityValid"; }
	};

	// Distance between two position refs (vec3-or-EntityID vars; "" = self).
	class Zenith_GraphNode_ComputeDistance : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ComputeDistance)
	public:
		ZENITH_PROPERTY(std::string, m_strFromVar, "")
		ZENITH_PROPERTY(std::string, m_strToVar, "target")
		ZENITH_PROPERTY(bool, m_bXZOnly, false)
		ZENITH_PROPERTY(std::string, m_strResultVar, "dist")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector3 xFrom, xTo;
			if (!ResolvePositionRef(xContext, m_strFromVar, xFrom) || !ResolvePositionRef(xContext, m_strToVar, xTo))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xDelta = xTo - xFrom;
			if (m_bXZOnly)
			{
				xDelta.y = 0.0f;
			}
			Zenith_PropertyValue xValue;
			xValue.SetFloat(glm::length(xDelta));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ComputeDistance"; }
	};

	// All entities within radius of a position ref, optionally filtered to
	// those having a component (by meta display name, e.g. "Collider") -> a
	// blackboard LIST of packed EntityIDs + a count var. Self is excluded.
	class Zenith_GraphNode_FindEntitiesInRadius : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_FindEntitiesInRadius)
	public:
		ZENITH_PROPERTY(std::string, m_strCenterVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fRadius, 10.0f, 0.0f, 100000.0f)
		ZENITH_PROPERTY(std::string, m_strComponentType, "")
		ZENITH_PROPERTY(std::string, m_strListVar, "found")
		ZENITH_PROPERTY(std::string, m_strCountVar, "foundCount")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector3 xCenter;
			if (!ResolvePositionRef(xContext, m_strCenterVar, xCenter))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_ComponentMeta* pxFilterMeta = m_strComponentType.empty()
				? nullptr : Zenith_ComponentMetaRegistry::Get().GetMetaByName(m_strComponentType);
			if (!m_strComponentType.empty() && (pxFilterMeta == nullptr || pxFilterMeta->m_pfnHasComponent == nullptr))
			{
				return GRAPH_NODE_STATUS_FAILURE;	// unknown filter type - fail loudly, not broadly
			}

			Zenith_Vector<Zenith_PropertyValue>& axFound = xContext.m_pxBlackboard->GetOrCreateList(m_strListVar);
			axFound.Clear();
			const float fRadiusSq = m_fRadius * m_fRadius;
			const Zenith_EntityID xSelfID = xContext.m_xSelf.GetEntityID();

			Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
			xScenes.QueryAllScenes<Zenith_TransformComponent>()
				.ForEach([&](Zenith_EntityID xID, Zenith_TransformComponent& xTransform)
			{
				if (xID == xSelfID)
				{
					return;
				}
				Zenith_Maths::Vector3 xPosition;
				xTransform.GetPosition(xPosition);
				const Zenith_Maths::Vector3 xDelta = xPosition - xCenter;
				if (glm::dot(xDelta, xDelta) > fRadiusSq)
				{
					return;
				}
				if (pxFilterMeta != nullptr)
				{
					Zenith_Entity xEntity = xScenes.ResolveEntity(xID);
					if (!xEntity.IsValid() || !pxFilterMeta->m_pfnHasComponent(xEntity))
					{
						return;
					}
				}
				Zenith_PropertyValue xValue;
				xValue.SetPackedEntityID(xID.GetPacked());
				axFound.PushBack(xValue);
			});

			if (!m_strCountVar.empty())
			{
				Zenith_PropertyValue xCount;
				xCount.SetInt32(static_cast<int32_t>(axFound.GetSize()));
				xContext.m_pxBlackboard->SetValue(m_strCountVar, xCount);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "FindEntitiesInRadius"; }
	};

	// Normalized from->to direction (vec3-or-EntityID vars; "" = self).
	class Zenith_GraphNode_ComputeDirection : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ComputeDirection)
	public:
		ZENITH_PROPERTY(std::string, m_strFromVar, "")
		ZENITH_PROPERTY(std::string, m_strToVar, "target")
		ZENITH_PROPERTY(bool, m_bXZOnly, false)
		ZENITH_PROPERTY(std::string, m_strResultVar, "dir")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector3 xFrom, xTo;
			if (!ResolvePositionRef(xContext, m_strFromVar, xFrom) || !ResolvePositionRef(xContext, m_strToVar, xTo))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xDelta = xTo - xFrom;
			if (m_bXZOnly)
			{
				xDelta.y = 0.0f;
			}
			const float fLength = glm::length(xDelta);
			Zenith_PropertyValue xValue;
			xValue.SetVector3(fLength > 0.0001f ? xDelta / fLength : Zenith_Maths::Vector3(0.0f));
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ComputeDirection"; }
	};

	// Instantiate dispatches OnAwake/OnEnable synchronously, so a prefab
	// whose own graph spawns prefabs recurses through this node - cut it at
	// the CallGraph-style depth cap instead of overflowing the stack.
	int g_iSpawnPrefabDepth = 0;

	// Instantiates a prefab asset at a position ref ("" = self) + offset into
	// self's scene (falls back to the active scene when self is gone) and
	// stores the root's packed EntityID. Synchronous: the spawned entity's
	// OnAwake/OnEnable (and any of its graphs) run inside this call.
	class Zenith_GraphNode_SpawnPrefab : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SpawnPrefab)
	public:
		ZENITH_PROPERTY(std::string, m_strPrefabPath, "")
		ZENITH_PROPERTY(std::string, m_strEntityName, "")
		ZENITH_PROPERTY(std::string, m_strPositionVar, "")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xOffset, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strResultVar, "spawned")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_strPrefabPath.empty())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (g_iSpawnPrefabDepth >= 8)
			{
				if (!m_bWarned)
				{
					m_bWarned = true;
					Zenith_Error(LOG_CATEGORY_CORE, "SpawnPrefab: depth cap hit spawning '%s' (prefab graph spawns prefabs recursively?)", m_strPrefabPath.c_str());
				}
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Prefab* pxPrefab = Zenith_AssetRegistry::GetView<Zenith_Prefab>(m_strPrefabPath);
			if (pxPrefab == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xPosition;
			if (!ResolvePositionRef(xContext, m_strPositionVar, xPosition))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_SceneData* pxSceneData = xContext.m_xSelf.IsValid()
				? xContext.m_xSelf.GetSceneData() : g_xEngine.Scenes().GetActiveSceneData();
			if (pxSceneData == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			++g_iSpawnPrefabDepth;
			Zenith_Entity xSpawned = pxPrefab->Instantiate(pxSceneData, m_strEntityName, xPosition + m_xOffset);
			--g_iSpawnPrefabDepth;
			if (!xSpawned.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (!m_strResultVar.empty())
			{
				Zenith_PropertyValue xValue;
				xValue.SetPackedEntityID(xSpawned.GetEntityID().GetPacked());
				xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SpawnPrefab"; }

	private:
		bool m_bWarned = false;
	};

	// First entity with the given name -> packed-EntityID var. Searches
	// self's scene, then the active scene when different (names are not
	// unique - first slot-order match wins). Not found = FAILURE.
	class Zenith_GraphNode_FindEntityByName : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_FindEntityByName)
	public:
		ZENITH_PROPERTY(std::string, m_strName, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "found")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			if (m_strName.empty())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_SceneData* pxSelfScene = xContext.m_xSelf.IsValid() ? xContext.m_xSelf.GetSceneData() : nullptr;
			Zenith_Entity xFound;
			if (pxSelfScene != nullptr)
			{
				xFound = pxSelfScene->FindEntityByName(m_strName);
			}
			if (!xFound.IsValid())
			{
				Zenith_SceneData* pxActiveScene = g_xEngine.Scenes().GetActiveSceneData();
				if (pxActiveScene != nullptr && pxActiveScene != pxSelfScene)
				{
					xFound = pxActiveScene->FindEntityByName(m_strName);
				}
			}
			if (!xFound.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xFound.GetEntityID().GetPacked());
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "FindEntityByName"; }
	};

	// Nearest entity within radius of a position ref, optionally filtered by
	// component meta display name -> packed-EntityID var (+ optional distance
	// var). Self excluded. None in range = FAILURE (the range-gate pattern).
	class Zenith_GraphNode_FindNearestEntity : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_FindNearestEntity)
	public:
		ZENITH_PROPERTY(std::string, m_strCenterVar, "")
		ZENITH_PROPERTY_RANGED(float, m_fRadius, 100.0f, 0.0f, 100000.0f)
		ZENITH_PROPERTY(std::string, m_strComponentType, "")
		ZENITH_PROPERTY(std::string, m_strResultVar, "nearest")
		ZENITH_PROPERTY(std::string, m_strDistanceVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Maths::Vector3 xCenter;
			if (!ResolvePositionRef(xContext, m_strCenterVar, xCenter))
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const Zenith_ComponentMeta* pxFilterMeta = m_strComponentType.empty()
				? nullptr : Zenith_ComponentMetaRegistry::Get().GetMetaByName(m_strComponentType);
			if (!m_strComponentType.empty() && (pxFilterMeta == nullptr || pxFilterMeta->m_pfnHasComponent == nullptr))
			{
				return GRAPH_NODE_STATUS_FAILURE;	// unknown filter type - fail loudly, not broadly
			}

			const float fRadiusSq = m_fRadius * m_fRadius;
			const Zenith_EntityID xSelfID = xContext.m_xSelf.GetEntityID();
			Zenith_EntityID xBestID;
			float fBestDistSq = fRadiusSq;
			bool bFound = false;

			Zenith_SceneSystem& xScenes = g_xEngine.Scenes();
			xScenes.QueryAllScenes<Zenith_TransformComponent>()
				.ForEach([&](Zenith_EntityID xID, Zenith_TransformComponent& xTransform)
			{
				if (xID == xSelfID)
				{
					return;
				}
				Zenith_Maths::Vector3 xPosition;
				xTransform.GetPosition(xPosition);
				const Zenith_Maths::Vector3 xDelta = xPosition - xCenter;
				const float fDistSq = glm::dot(xDelta, xDelta);
				if (fDistSq > fBestDistSq || (bFound && fDistSq >= fBestDistSq))
				{
					return;
				}
				if (pxFilterMeta != nullptr)
				{
					Zenith_Entity xEntity = xScenes.ResolveEntity(xID);
					if (!xEntity.IsValid() || !pxFilterMeta->m_pfnHasComponent(xEntity))
					{
						return;
					}
				}
				xBestID = xID;
				fBestDistSq = fDistSq;
				bFound = true;
			});

			if (!bFound)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_PropertyValue xValue;
			xValue.SetPackedEntityID(xBestID.GetPacked());
			xContext.m_pxBlackboard->SetValue(m_strResultVar, xValue);
			if (!m_strDistanceVar.empty())
			{
				Zenith_PropertyValue xDistance;
				xDistance.SetFloat(std::sqrt(fBestDistSq));
				xContext.m_pxBlackboard->SetValue(m_strDistanceVar, xDistance);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "FindNearestEntity"; }
	};

	// Attaches the target (the held ITEM - "" = self) to a named bone of the
	// skeleton entity in m_strSkeletonVar. Adds the AttachmentComponent on
	// demand; the follow runs in OnLateUpdate. Physics stays the caller's
	// concern (make held dynamic bodies kinematic game-side).
	class Zenith_GraphNode_AttachToBone : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_AttachToBone)
	public:
		ZENITH_PROPERTY(std::string, m_strSkeletonVar, "skeleton")
		ZENITH_PROPERTY(std::string, m_strBone, "RightHand")
		ZENITH_PROPERTY(Zenith_Maths::Vector3, m_xOffsetPosition, Zenith_Maths::Vector3(0.0f, 0.0f, 0.0f))
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xItem = xContext.ResolveTargetEntity(m_strTargetVar);
			Zenith_Entity xSkeleton = xContext.ResolveTargetEntity(m_strSkeletonVar);
			if (!xItem.IsValid() || !xSkeleton.IsValid() || m_strBone.empty())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_AttachmentComponent* pxAttachment = xItem.TryGetComponent<Zenith_AttachmentComponent>();
			if (pxAttachment == nullptr)
			{
				pxAttachment = &xItem.AddComponent<Zenith_AttachmentComponent>();
			}
			const Zenith_Maths::Matrix4 xOffset = glm::translate(Zenith_Maths::Matrix4(1.0f), m_xOffsetPosition);
			pxAttachment->AttachToBone(xSkeleton, m_strBone.c_str(), xOffset);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "AttachToBone"; }
	};

	// Detaches the target from its bone. Idempotent; no component = nothing
	// to detach = SUCCESS. The item stays wherever the last follow left it.
	class Zenith_GraphNode_DetachFromBone : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_DetachFromBone)
	public:
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xItem = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xItem.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			if (Zenith_AttachmentComponent* pxAttachment = xItem.TryGetComponent<Zenith_AttachmentComponent>())
			{
				pxAttachment->Detach();
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "DetachFromBone"; }
	};

	// Main-camera basis -> blackboard vars (forward/right, optional up +
	// position). m_bFlattenXZ projects forward/right onto the ground plane
	// (normalized) - the camera-relative-movement primitive. FAILURE when no
	// loaded scene has a resolvable main camera.
	class Zenith_GraphNode_ReadCameraBasis : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadCameraBasis)
	public:
		ZENITH_PROPERTY(bool, m_bFlattenXZ, false)
		ZENITH_PROPERTY(std::string, m_strForwardVar, "camForward")
		ZENITH_PROPERTY(std::string, m_strRightVar, "camRight")
		ZENITH_PROPERTY(std::string, m_strUpVar, "")
		ZENITH_PROPERTY(std::string, m_strPositionVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
			if (pxCamera == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xForward;
			pxCamera->GetFacingDir(xForward);
			if (m_bFlattenXZ)
			{
				xForward.y = 0.0f;
				const float fLength = glm::length(xForward);
				if (fLength < 0.0001f)
				{
					return GRAPH_NODE_STATUS_FAILURE;	// looking straight up/down - no XZ heading
				}
				xForward /= fLength;
			}
			const Zenith_Maths::Vector3 xRight = glm::normalize(glm::cross(Zenith_Maths::Vector3(0.0f, 1.0f, 0.0f), xForward));

			Zenith_PropertyValue xValue;
			if (!m_strForwardVar.empty())
			{
				xValue.SetVector3(xForward);
				xContext.m_pxBlackboard->SetValue(m_strForwardVar, xValue);
			}
			if (!m_strRightVar.empty())
			{
				xValue.SetVector3(xRight);
				xContext.m_pxBlackboard->SetValue(m_strRightVar, xValue);
			}
			if (!m_strUpVar.empty())
			{
				xValue.SetVector3(pxCamera->GetUpDir());
				xContext.m_pxBlackboard->SetValue(m_strUpVar, xValue);
			}
			if (!m_strPositionVar.empty())
			{
				Zenith_Maths::Vector3 xPosition;
				pxCamera->GetPosition(xPosition);
				xValue.SetVector3(xPosition);
				xContext.m_pxBlackboard->SetValue(m_strPositionVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadCameraBasis"; }
	};

	// Drives the main camera's pitch/yaw (DEGREES; the component stores
	// radians). Absolute mode sets; additive mode accumulates (mouse-look:
	// feed ReadMouseDelta y/x through pitch/yaw vars). Pitch clamps to
	// +/-89 degrees unless disabled.
	class Zenith_GraphNode_SetCameraPitchYaw : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_SetCameraPitchYaw)
	public:
		ZENITH_PROPERTY(float, m_fPitchDegrees, 0.0f)
		ZENITH_PROPERTY(std::string, m_strPitchVar, "")
		ZENITH_PROPERTY(float, m_fYawDegrees, 0.0f)
		ZENITH_PROPERTY(std::string, m_strYawVar, "")
		ZENITH_PROPERTY(bool, m_bAdditive, false)
		ZENITH_PROPERTY(bool, m_bClampPitch, true)

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_CameraComponent* pxCamera = Zenith_GetMainCameraAcrossScenes();
			if (pxCamera == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			const float fPitchDegrees = m_strPitchVar.empty()
				? m_fPitchDegrees : xContext.m_pxBlackboard->GetFloat(m_strPitchVar, m_fPitchDegrees);
			const float fYawDegrees = m_strYawVar.empty()
				? m_fYawDegrees : xContext.m_pxBlackboard->GetFloat(m_strYawVar, m_fYawDegrees);

			double fPitch = glm::radians(static_cast<double>(fPitchDegrees));
			double fYaw = glm::radians(static_cast<double>(fYawDegrees));
			if (m_bAdditive)
			{
				fPitch += pxCamera->GetPitch();
				fYaw += pxCamera->GetYaw();
			}
			if (m_bClampPitch)
			{
				const double fLimit = glm::radians(89.0);
				fPitch = fPitch < -fLimit ? -fLimit : (fPitch > fLimit ? fLimit : fPitch);
			}
			pxCamera->SetPitch(fPitch);
			pxCamera->SetYaw(fYaw);
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "SetCameraPitchYaw"; }
	};

	// Turns the target toward a direction (vec3 var, e.g. from
	// ComputeDirection) at a bounded rate; rate <= 0 snaps. m_bYawOnly
	// flattens the direction to the ground plane first (the character-facing
	// default). LH engine: quat * +Z equals the faced direction.
	class Zenith_GraphNode_RotateTowardDirection : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_RotateTowardDirection)
	public:
		ZENITH_PROPERTY(std::string, m_strDirectionVar, "dir")
		ZENITH_PROPERTY_RANGED(float, m_fDegreesPerSecond, 360.0f, 0.0f, 10800.0f)
		ZENITH_PROPERTY(bool, m_bYawOnly, true)
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TransformComponent* pxTransform = xTarget.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Vector3 xDirection = xContext.m_pxBlackboard->GetVector3(m_strDirectionVar);
			if (m_bYawOnly)
			{
				xDirection.y = 0.0f;
			}
			const float fLength = glm::length(xDirection);
			if (fLength < 0.0001f)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			xDirection /= fLength;

			const Zenith_Maths::Vector3 xUp(0.0f, 1.0f, 0.0f);
			if (std::fabs(glm::dot(xDirection, xUp)) > 0.999f)
			{
				return GRAPH_NODE_STATUS_FAILURE;	// degenerate vertical facing
			}
			const Zenith_Maths::Quat xGoal = glm::quatLookAt(xDirection, xUp);

			Zenith_Maths::Quat xCurrent;
			pxTransform->GetRotation(xCurrent);
			Zenith_Maths::Quat xResult = xGoal;
			if (m_fDegreesPerSecond > 0.0f)
			{
				const float fDot = std::fabs(glm::dot(xCurrent, xGoal));
				const float fAngle = 2.0f * std::acos(fDot > 1.0f ? 1.0f : fDot);
				if (fAngle > 0.0001f)
				{
					const float fMaxStep = glm::radians(m_fDegreesPerSecond) * xContext.m_fDt;
					const float fT = fMaxStep >= fAngle ? 1.0f : fMaxStep / fAngle;
					xResult = glm::slerp(xCurrent, xGoal, fT);
				}
			}
			pxTransform->SetRotation(glm::normalize(xResult));
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "RotateTowardDirection"; }
	};

	// Target's rotation -> blackboard. Forward (quat * +Z, the steering-safe
	// representation) and/or Euler degrees (display/storage - ambiguous near
	// gimbal poles). Empty vars skip.
	class Zenith_GraphNode_ReadEntityRotation : public Zenith_GraphNode
	{
	public:
		ZENITH_PROPERTIES_BEGIN(Zenith_GraphNode_ReadEntityRotation)
	public:
		ZENITH_PROPERTY(std::string, m_strForwardVar, "forward")
		ZENITH_PROPERTY(std::string, m_strEulerVar, "")
		ZENITH_PROPERTY(std::string, m_strTargetVar, "")

		GraphNodeStatus Execute(Zenith_GraphContext& xContext) override
		{
			Zenith_Entity xTarget = xContext.ResolveTargetEntity(m_strTargetVar);
			if (!xTarget.IsValid())
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_TransformComponent* pxTransform = xTarget.TryGetComponent<Zenith_TransformComponent>();
			if (pxTransform == nullptr)
			{
				return GRAPH_NODE_STATUS_FAILURE;
			}
			Zenith_Maths::Quat xRotation;
			pxTransform->GetRotation(xRotation);
			Zenith_PropertyValue xValue;
			if (!m_strForwardVar.empty())
			{
				xValue.SetVector3(xRotation * Zenith_Maths::Vector3(0.0f, 0.0f, 1.0f));
				xContext.m_pxBlackboard->SetValue(m_strForwardVar, xValue);
			}
			if (!m_strEulerVar.empty())
			{
				xValue.SetVector3(glm::degrees(glm::eulerAngles(xRotation)));
				xContext.m_pxBlackboard->SetValue(m_strEulerVar, xValue);
			}
			return GRAPH_NODE_STATUS_SUCCESS;
		}
		const char* GetTypeName() const override { return "ReadEntityRotation"; }
	};
}

void Zenith_RegisterEngineGraphNodes_Entity()
{
	Zenith_GraphNodeRegistry& xRegistry = Zenith_GraphNodeRegistry::Get();

	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadEntityPosition>("ReadEntityPosition", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetEntityScale>("SetEntityScale", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_QueryEntityValid>("QueryEntityValid", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ComputeDistance>("ComputeDistance", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ComputeDirection>("ComputeDirection", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_FindEntitiesInRadius>("FindEntitiesInRadius", GRAPH_EVENT_NONE, 1, false, "Entity");

	xRegistry.RegisterNodeType<Zenith_GraphNode_SpawnPrefab>("SpawnPrefab", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_FindEntityByName>("FindEntityByName", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_FindNearestEntity>("FindNearestEntity", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_AttachToBone>("AttachToBone", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_DetachFromBone>("DetachFromBone", GRAPH_EVENT_NONE, 1, false, "Entity");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadCameraBasis>("ReadCameraBasis", GRAPH_EVENT_NONE, 1, false, "Camera");
	xRegistry.RegisterNodeType<Zenith_GraphNode_SetCameraPitchYaw>("SetCameraPitchYaw", GRAPH_EVENT_NONE, 1, false, "Camera");
	xRegistry.RegisterNodeType<Zenith_GraphNode_RotateTowardDirection>("RotateTowardDirection", GRAPH_EVENT_NONE, 1, false, "Transform");
	xRegistry.RegisterNodeType<Zenith_GraphNode_ReadEntityRotation>("ReadEntityRotation", GRAPH_EVENT_NONE, 1, false, "Transform");
}
