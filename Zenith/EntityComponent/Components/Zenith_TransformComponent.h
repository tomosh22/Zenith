#pragma once
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_SceneSystem.h"
#include "Maths/Zenith_Maths.h"
#include "Physics/Zenith_Physics_Fwd.h"   // Zenith_PhysicsBodyID (value type) — no Jolt

// Forward declarations for RegisterProperties() — full definition lives in
// Zenith_ComponentMeta.h, which we cannot include here without a cycle
// (ComponentMeta -> Scene -> SceneData -> TransformComponent).
template<typename T> class Zenith_Vector;
struct Zenith_PropertyDescriptor;

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#include "EntityComponent/Zenith_ComponentEditorRegistry.h"
#endif

class Zenith_TransformComponent
{
public:
	Zenith_TransformComponent(Zenith_Entity& xEntity);
	~Zenith_TransformComponent();

	// On-disk schema version of the Transform's serialized payload.
	//   < 7 : pos/rot/scale FOLLOWED BY the legacy hierarchy parent file-index
	//         (scene v3/4/5/6 + pre-v7 prefab blobs). The 2-arg ReadFromDataStream
	//         below migrates this: it consumes the parent and sinks it to the slot
	//         pending-parent so ResolvePendingParents rebuilds the hierarchy.
	//   >= 7: pos/rot/scale ONLY -- the parent moved to the entity record (scene v7).
	// The meta registry stamps this per-component value into the file and hands it
	// back to the 2-arg reader on load (see Zenith_ComponentMeta DeserializeEntityComponents).
	static constexpr u_int uSchemaVersion = 7;

	// Serialization methods for Zenith_DataStream. WriteToDataStream always emits the
	// CURRENT (v7) layout: pos/rot/scale, no parent. The 1-arg ReadFromDataStream is the
	// current-format reader (no parent); the 2-arg overload is the version-aware reader
	// the meta registry prefers -- it migrates the in-blob parent for uSchemaVersion < 7.
	void WriteToDataStream(Zenith_DataStream& xStream);
	void ReadFromDataStream(Zenith_DataStream& xStream);
	void ReadFromDataStream(Zenith_DataStream& xStream, u_int uSchemaVersion);

	// Property registration for prefab variant overrides. The reflection layer
	// in Zenith_ComponentMeta calls this once at component-type registration.
	static void RegisterProperties(Zenith_Vector<Zenith_PropertyDescriptor>& axProperties);

	void SetPosition(const Zenith_Maths::Vector3& xPos);
	void SetRotation(const Zenith_Maths::Quat& xRot);
	void SetScale(const Zenith_Maths::Vector3& xScale);

	void GetPosition(Zenith_Maths::Vector3& xPos);
	void GetRotation(Zenith_Maths::Quat& xRot);
	void GetScale(Zenith_Maths::Vector3& xScale) const;

	// Copy the live physics-body transform (if any) into the cached
	// m_xPosition/m_xRotation. Call this BEFORE destroying a body that the
	// transform reads from (e.g. Zenith_ColliderComponent::RebuildCollider),
	// so code reading GetPosition/GetRotation after the body is gone sees the
	// current world transform instead of a stale cached value.
	void CommitPhysicsTransformToCache();

	// Physics → transform-cache sync (Phase 1). If the owning entity has a live,
	// active physics body whose pose has drifted from the cached m_xPosition/
	// m_xRotation, commit the body pose into the cache and bump this entity's
	// hierarchy revision (invalidating the cached world matrix of this entity and
	// its descendants). The post-physics main-loop sweep calls this for every
	// collider entity to catch Jolt *simulation* moves (which go through no setter);
	// direct teleports invalidate immediately via the physics pose-change hook. No-op
	// for bodyless entities, when there is no active simulation, and for unchanged poses.
	void SyncPhysicsPoseAndInvalidate();

	Zenith_Maths::Vector3 m_xScale = { 1.,1.,1. };

	void BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut);

	//--------------------------------------------------------------------------
	// Parent/Child Hierarchy -- REMOVED (Phase 5b).
	//
	// The scene-graph hierarchy is owned exclusively by Zenith_EntitySlot (single
	// source of truth, Phase 5a). The Transform's former hierarchy navigation API
	// (SetParent / SetParentByID / TryGetParent / GetParentEntity / GetParentEntityID
	// / pending-parent accessors / GetChildEntityIDs / GetChildCount / ForEachChild /
	// TryGetChildAt / GetChildEntityAt / HasParent / IsRoot / DetachFromParent /
	// DetachAllChildren / IsDescendantOf) was a set of forwarding shims into
	// Zenith_Entity; all callers now use the Zenith_Entity slot API directly, so the
	// shims and the three backing members are gone. Use Zenith_Entity / Zenith_Scene
	// for hierarchy. BuildModelMatrix (above) still walks the slot parent chain;
	// Write/ReadFromDataStream still bridge the parent index through the slot.
	//--------------------------------------------------------------------------

	// The entity that owns this Transform (NOT the hierarchy parent). Retained so
	// callers can recover the owning Zenith_Entity from a Transform reference.
	Zenith_Entity& GetEntity() { return m_xOwningEntity; }
	const Zenith_Entity& GetEntity() const { return m_xOwningEntity; }


#ifdef ZENITH_TOOLS
	//--------------------------------------------------------------------------
	// Editor UI - Renders component properties in the Properties panel
	//--------------------------------------------------------------------------
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen))
		{
			Zenith_Maths::Vector3 pos, scale;
			Zenith_Maths::Quat rot;
			GetPosition(pos);
			GetRotation(rot);
			GetScale(scale);
			
			// Position editing
			float position[3] = { pos.x, pos.y, pos.z };
			if (ImGui::DragFloat3("Position", position, 0.1f))
			{
				SetPosition({ position[0], position[1], position[2] });
			}
			
			// Rotation editing - convert quaternion to Euler angles for UI
			Zenith_Maths::Vector3 euler = glm::degrees(glm::eulerAngles(rot));
			float rotation[3] = { euler.x, euler.y, euler.z };
			if (ImGui::DragFloat3("Rotation", rotation, 1.0f))
			{
				Zenith_Maths::Vector3 newEuler = glm::radians(Zenith_Maths::Vector3(rotation[0], rotation[1], rotation[2]));
				SetRotation(Zenith_Maths::Quat(newEuler));
			}
			
			// Scale editing
			float scaleValues[3] = { scale.x, scale.y, scale.z };
			if (ImGui::DragFloat3("Scale", scaleValues, 0.1f))
			{
				SetScale({ scaleValues[0], scaleValues[1], scaleValues[2] });
			}
		}
	}

#endif

private:
	// Phase 1 scene-graph transform-cache tests read the private cache members
	// (m_xCachedWorld / m_uCachedHierRevision) directly to assert the worker-thread
	// cache-write contract. Test-only; mirrors the friend on Zenith_AnimatorComponent.
	friend class Zenith_UnitTests;

	// If the owning entity has a collider with a live physics body, return its
	// body ID. Single source of the "does this transform have an authoritative
	// physics body?" check the pose getters/setters share. Names no Jolt type.
	bool TryGetColliderBody(Zenith_PhysicsBodyID& xOutBodyID);

	// True iff (xPos,xRot) differs from the last-synced pose (m_xPosition/m_xRotation)
	// beyond the physics-sync epsilons (squared distance for position; quaternion
	// double-cover dot for rotation). The single change-test shared by the post-physics
	// sweep and CommitPhysicsTransformToCache, so committing a moved body pose can never
	// silently skip the hierarchy-revision bump that invalidates the cached world matrix.
	bool PhysicsPoseDiffersFromCache(const Zenith_Maths::Vector3& xPos, const Zenith_Maths::Quat& xRot) const;

	Zenith_Maths::Vector3 m_xPosition = { 0.0, 0.0, 0.0 };
	Zenith_Maths::Quat m_xRotation = { 1.0, 0.0, 0.0, 0.0 };

	// Phase 1 scene-graph transform cache. m_xCachedWorld holds the last computed
	// WORLD matrix (full ancestor chain folded in); m_uCachedHierRevision is the
	// owning slot's hierarchy revision it was built against (0 = never built — the
	// fresh-cache miss sentinel, since slot revisions start at 1). BuildModelMatrix
	// returns m_xCachedWorld when the slot revision still matches, else recomputes.
	// Only the main thread writes these (see the worker-thread cache-write contract
	// in BuildModelMatrix): a render-task worker that misses recomputes into its
	// out-param and leaves the cache untouched (no shared write → race-free).
	Zenith_Maths::Matrix4 m_xCachedWorld = Zenith_Maths::Matrix4(1.0f);
	uint64_t m_uCachedHierRevision = 0;

	// The entity that owns this component (NOT the hierarchy parent)
	Zenith_Entity m_xOwningEntity;

	// Phase 5b: the scene-graph hierarchy STORAGE (m_xParentEntityID /
	// m_xChildEntityIDs / m_uPendingParentFileIndex) was deleted here -- it lives on
	// Zenith_EntitySlot now (relocated in Phase 5a, single source of truth). The
	// position/rotation/scale cache above is the Transform's only remaining state.
};
