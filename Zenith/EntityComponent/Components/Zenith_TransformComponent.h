#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "EntityComponent/Zenith_Scene.h"
#include "Maths/Zenith_Maths.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

class Zenith_TransformComponent
{
public:
	Zenith_TransformComponent(Zenith_Entity& xEntity);
	~Zenith_TransformComponent();

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

	void SetPosition(const Zenith_Maths::Vector3& xPos);
	void SetRotation(const Zenith_Maths::Quat& xRot);
	void SetScale(const Zenith_Maths::Vector3& xScale);

	void GetPosition(Zenith_Maths::Vector3& xPos);
	void GetRotation(Zenith_Maths::Quat& xRot);
	void GetScale(Zenith_Maths::Vector3& xScale) const;

	Zenith_Maths::Vector3 m_xScale = { 1.,1.,1. };

	void BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut);

	//--------------------------------------------------------------------------
	// Parent/Child Hierarchy (Unity-style - hierarchy owned by Transform)
	// Uses EntityIDs instead of raw pointers for safety (survives pool relocations)
	//--------------------------------------------------------------------------

	void SetParent(Zenith_TransformComponent* pxParent);
	void SetParentByID(Zenith_EntityID uParentID);

	// Returns parent transform, or nullptr if no parent or parent entity doesn't exist
	// NOTE: Named "Try" to indicate it may return nullptr - always check return value
	Zenith_TransformComponent* TryGetParent() const;

	// Returns parent entity by value (safe - no dangling pointers)
	// Check IsValid() on returned entity before use
	Zenith_Entity GetParentEntity() const;

	Zenith_EntityID GetParentEntityID() const { return m_xParentEntityID; }

	// Pending parent file index (for scene loading - maps old file index to new EntityID)
	void SetPendingParentFileIndex(uint32_t uIndex) { m_uPendingParentFileIndex = uIndex; }
	uint32_t GetPendingParentFileIndex() const { return m_uPendingParentFileIndex; }
	void ClearPendingParentFileIndex() { m_uPendingParentFileIndex = Zenith_EntityID::INVALID_INDEX; }

	// Child access - use ForEachChild for safe iteration
	// Non-const overload is needed for scene deserialization to rebuild child lists
	const Zenith_Vector<Zenith_EntityID>& GetChildEntityIDs() const { return m_xChildEntityIDs; }
	Zenith_Vector<Zenith_EntityID>& GetChildEntityIDs() { return m_xChildEntityIDs; }
	uint32_t GetChildCount() const { return static_cast<uint32_t>(m_xChildEntityIDs.GetSize()); }

	// Safe child iteration - handles invalid entity IDs gracefully
	template<typename Func>
	void ForEachChild(Func&& func);

	// Get child transform by index (returns nullptr if invalid)
	// NOTE: Named "Try" to indicate it may return nullptr - always check return value
	Zenith_TransformComponent* TryGetChildAt(uint32_t uIndex) const;

	// Returns child entity by value (safe - no dangling pointers)
	// Check IsValid() on returned entity before use
	Zenith_Entity GetChildEntityAt(uint32_t uIndex) const;

	bool HasParent() const { return m_xParentEntityID.IsValid(); }
	bool IsRoot() const { return !m_xParentEntityID.IsValid(); }
	void DetachFromParent();
	void DetachAllChildren();
	Zenith_Entity& GetEntity() { return m_xOwningEntity; }
	const Zenith_Entity& GetEntity() const { return m_xOwningEntity; }

	// Hierarchy safety: Check if this transform is a descendant of the given entity
	// Used to prevent circular hierarchies (e.g., parenting A to its own child)
	bool IsDescendantOf(Zenith_EntityID uAncestorID) const;

	// Unsafe version for internal use when scene mutex is already held
	// ONLY call this when you already hold Zenith_Scene::m_xMutex!
	bool IsDescendantOfUnsafe(Zenith_EntityID uAncestorID, Zenith_Scene& xScene) const;


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
	Zenith_Maths::Vector3 m_xPosition = { 0.0, 0.0, 0.0 };
	Zenith_Maths::Quat m_xRotation = { 1.0, 0.0, 0.0, 0.0 };

	// The entity that owns this component (NOT the hierarchy parent)
	Zenith_Entity m_xOwningEntity;

	// Parent-child hierarchy (Unity-style) - uses EntityIDs for pointer safety
	// EntityIDs survive component pool relocations (swap-and-pop removal)
	// NOTE: This is the HIERARCHY parent, not the owning entity above
	Zenith_EntityID m_xParentEntityID = INVALID_ENTITY_ID;
	Zenith_Vector<Zenith_EntityID> m_xChildEntityIDs;

	// Pending parent file index - used during scene loading to map old indices to new EntityIDs
	uint32_t m_uPendingParentFileIndex = Zenith_EntityID::INVALID_INDEX;
};

// Template implementation for ForEachChild - must be in header
template<typename Func>
void Zenith_TransformComponent::ForEachChild(Func&& func)
{
	Zenith_Scene& xScene = Zenith_Scene::GetCurrentScene();

	// Copy child IDs to local vector to prevent invalidation during iteration
	// This fixes the case where the callback modifies the child list (e.g., reparenting)
	Zenith_Vector<Zenith_EntityID> xChildIDsCopy;
	xChildIDsCopy.Reserve(m_xChildEntityIDs.GetSize());
	for (u_int u = 0; u < m_xChildEntityIDs.GetSize(); ++u)
	{
		xChildIDsCopy.PushBack(m_xChildEntityIDs.Get(u));
	}

	// Now iterate the copy safely
	for (u_int u = 0; u < xChildIDsCopy.GetSize(); ++u)
	{
		Zenith_EntityID uChildID = xChildIDsCopy.Get(u);
		if (xScene.EntityExists(uChildID))
		{
			Zenith_Entity xChildEntity = xScene.GetEntity(uChildID);
			Zenith_TransformComponent& xChildTransform = xChildEntity.GetComponent<Zenith_TransformComponent>();
			func(xChildTransform);
		}
	}
}
