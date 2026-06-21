#pragma once
#include "ZenithECS/Zenith_Entity.h"
#include "Collections/Zenith_HashMap.h"
#include "Maths/Zenith_Maths.h"
#include <string>

class Zenith_DataStream;

#ifdef ZENITH_TOOLS
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"
#endif

// Reusable bone-attachment component.
//
// Lives on the ATTACHED entity (a tennis racket, or an FPS weapon) and drives
// that entity's own Zenith_TransformComponent each frame from a named bone of a
// DIFFERENT skeletal entity. The attached model stays a separate asset/entity —
// this is deliberately NOT skinning and NOT entity (slot) parenting: the slot
// hierarchy composes only rigid TRS and cannot insert a bone matrix between a
// parent and child, so a held item would lag the skeleton's pose. Instead the
// component reads the posed bone's model-space matrix (via
// Zenith_ModelComponent::GetBoneModelMatrix, which keeps the Flux skeleton
// headers out of this component).
//
// Update timing: the follow runs in OnLateUpdate, AFTER every OnUpdate, so the
// animator (component order 15) has already produced this frame's bone matrices.
// Running it in OnUpdate would read a one-frame-stale (or, on frame 0, identity)
// pose.
//
// Reuse / FPS weapons: the same component serves a racket-on-hand and an FPS gun
// pickup/drop — AttachToBone(player, "RightHand", gripOffset) to pick up,
// Detach() to drop; only the bone name and the offset matrix differ. The
// component is a pure transform-follow: if the attached entity has a dynamic
// physics body, the OWNER should make it kinematic / disable it while held (and
// re-enable + impulse it on drop) — physics ownership stays with the caller, so
// this component names no physics type.
class Zenith_AttachmentComponent
{
public:
	Zenith_AttachmentComponent(Zenith_Entity& xEntity) : m_xSelf(xEntity) {}

	// Attach this entity to szBone of xSkeletonEntity. xOffset is the mount
	// transform in BONE-LOCAL space — it bakes the grip position and the tool's
	// alignment (e.g. racket handle in the hand, muzzle forward).
	void AttachToBone(Zenith_Entity xSkeletonEntity, const char* szBone,
		const Zenith_Maths::Matrix4& xOffset);

	void Detach();

	bool IsAttached() const { return m_bAttached; }
	Zenith_Entity GetSkeletonEntity() const { return m_xSkeletonEntity; }
	const std::string& GetBoneName() const { return m_strBone; }
	const Zenith_Maths::Matrix4& GetOffset() const { return m_xOffset; }
	void SetOffset(const Zenith_Maths::Matrix4& xOffset) { m_xOffset = xOffset; }

	void OnLateUpdate(float fDt);

	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	// Scene-load hook (concept-detected by the component-meta registry). Re-binds
	// m_xSkeletonEntity from the serialized skeleton file-index once the loader's
	// file-index -> EntityID map is complete. Scene-load only — never runs in prefab
	// context, so a prefab-instantiated attachment stays inert until re-attached in code.
	void ResolveEntityReferences(const Zenith_HashMap<uint32_t, Zenith_EntityID>& xMap);

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		if (ImGui::CollapsingHeader("Attachment", ImGuiTreeNodeFlags_DefaultOpen))
		{
			ImGui::Text("Attached: %s", m_bAttached ? "true" : "false");
			ImGui::Text("Bone: %s", m_strBone.c_str());
		}
	}
#endif

private:
	Zenith_Entity m_xSelf;            // the entity this component is on (the racket/weapon)
	Zenith_Entity m_xSkeletonEntity;  // the entity whose bone we follow
	std::string   m_strBone;
	Zenith_Maths::Matrix4 m_xOffset = Zenith_Maths::Matrix4(1.0f);
	bool          m_bAttached = false;
	bool          m_bWarnedMissingBone = false;   // one-shot diagnostic latch

	// Deserialized binding awaiting resolution (scene v2+). ReadFromDataStream stashes
	// the serialized attached-flag + skeleton slot index here; ResolveEntityReferences
	// (scene-load only) consumes them, then clears both — so they are inert in any other
	// context (prefab instantiation, runtime). AttachToBone / Detach also clear them.
	uint32_t      m_uPendingSkeletonFileIndex = Zenith_EntityID::INVALID_INDEX;
	bool          m_bPendingAttached = false;
};
