#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_UIComponent.h"
#include <unordered_map>
#include <string>

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

// Force link function - call from Zenith_Scene.cpp to ensure static registration runs
void Zenith_ScriptComponent_ForceLink();

class Zenith_ScriptBehaviour {
	friend class Zenith_ScriptComponent;
public:
	virtual ~Zenith_ScriptBehaviour() {}

	//--------------------------------------------------------------------------
	// Lifecycle Hooks
	//--------------------------------------------------------------------------

	/**
	 * OnAwake - Called when behavior is first created/attached at RUNTIME.
	 * NOT called during scene deserialization.
	 * Use for: Initializing references, setting up state, procedural generation.
	 */
	virtual void OnAwake() {}

	/**
	 * OnStart - Called before the first OnUpdate, after all OnAwake calls.
	 * Called for ALL entities including those loaded from scene files.
	 * Use for: Initialization that depends on other components being ready.
	 */
	virtual void OnStart() {}

	/**
	 * OnUpdate - Called every frame.
	 */
	virtual void OnUpdate(float /*fDt*/) {}

	/**
	 * OnDestroy - Called when behavior is destroyed.
	 */
	virtual void OnDestroy() {}

	// Physics collision callbacks - override to handle collision events
	// xOther is the entity that was collided with
	virtual void OnCollisionEnter(Zenith_Entity /*xOther*/) {}
	virtual void OnCollisionStay(Zenith_Entity /*xOther*/) {}
	virtual void OnCollisionExit(Zenith_EntityID /*uOtherID*/) {}  // Exit only gets ID since body may be destroyed

	// Return the unique type name for this behavior (used for serialization)
	virtual const char* GetBehaviourTypeName() const = 0;

	// Editor UI - override to render behavior-specific properties
	// Default implementation does nothing
	virtual void RenderPropertiesPanel() {}

	// Serialization of behavior-specific parameters
	// Override these to save/load custom behavior state
	virtual void WriteParametersToDataStream(Zenith_DataStream& /*xStream*/) const {}
	virtual void ReadParametersFromDataStream(Zenith_DataStream& /*xStream*/) {}

	Zenith_Entity& GetEntity() { return m_xParentEntity; }

	std::vector<Zenith_GUID> m_axGUIDRefs;

protected:
	Zenith_Entity m_xParentEntity;
};

// Forward declaration
class Zenith_DataStream;

// Factory function type for creating behaviors
typedef Zenith_ScriptBehaviour* (*BehaviourFactoryFunc)(Zenith_Entity&);

// Behaviour registry for serialization support
class Zenith_BehaviourRegistry
{
public:
	static Zenith_BehaviourRegistry& Get()
	{
		static Zenith_BehaviourRegistry s_xInstance;
		return s_xInstance;
	}

	// Register a behavior type with the factory
	void RegisterBehaviour(const char* szTypeName, BehaviourFactoryFunc fnFactory)
	{
		m_xFactoryMap[szTypeName] = fnFactory;
	}

	// Create a behavior by type name
	Zenith_ScriptBehaviour* CreateBehaviour(const char* szTypeName, Zenith_Entity& xEntity)
	{
		auto it = m_xFactoryMap.find(szTypeName);
		if (it != m_xFactoryMap.end())
		{
			return it->second(xEntity);
		}
		return nullptr;
	}

	bool HasBehaviour(const char* szTypeName) const
	{
		return m_xFactoryMap.find(szTypeName) != m_xFactoryMap.end();
	}

	// Get all registered behavior names
	std::vector<std::string> GetRegisteredBehaviourNames() const
	{
		std::vector<std::string> xNames;
		for (const auto& pair : m_xFactoryMap)
		{
			xNames.push_back(pair.first);
		}
		return xNames;
	}

private:
	std::unordered_map<std::string, BehaviourFactoryFunc> m_xFactoryMap;
};

// Helper macro to define a behavior with serialization support
#define ZENITH_BEHAVIOUR_TYPE_NAME(TypeName) \
	virtual const char* GetBehaviourTypeName() const override { return #TypeName; } \
	static Zenith_ScriptBehaviour* CreateInstance(Zenith_Entity& xEntity) { return new TypeName(xEntity); } \
	static void RegisterBehaviour() { Zenith_BehaviourRegistry::Get().RegisterBehaviour(#TypeName, &TypeName::CreateInstance); }
class Zenith_ScriptComponent
{
public:

	Zenith_ScriptComponent(Zenith_Entity& xEntity) : m_xParentEntity(xEntity) {};
	~Zenith_ScriptComponent() {
		if (m_pxScriptBehaviour)
		{
			m_pxScriptBehaviour->OnDestroy();
			delete m_pxScriptBehaviour;
		}
	}

	// Move semantics - required for component pool operations
	Zenith_ScriptComponent(Zenith_ScriptComponent&& xOther) noexcept
		: m_pxScriptBehaviour(xOther.m_pxScriptBehaviour)
		, m_xParentEntity(xOther.m_xParentEntity)
	{
		xOther.m_pxScriptBehaviour = nullptr;  // Nullify source
	}

	Zenith_ScriptComponent& operator=(Zenith_ScriptComponent&& xOther) noexcept
	{
		if (this != &xOther)
		{
			// Clean up our existing behaviour
			if (m_pxScriptBehaviour)
			{
				m_pxScriptBehaviour->OnDestroy();
				delete m_pxScriptBehaviour;
			}

			// Take ownership from source
			m_pxScriptBehaviour = xOther.m_pxScriptBehaviour;
			m_xParentEntity = xOther.m_xParentEntity;

			// Nullify source
			xOther.m_pxScriptBehaviour = nullptr;
		}
		return *this;
	}

	// Disable copy semantics - component should only be moved
	Zenith_ScriptComponent(const Zenith_ScriptComponent&) = delete;
	Zenith_ScriptComponent& operator=(const Zenith_ScriptComponent&) = delete;

	Zenith_ScriptBehaviour* m_pxScriptBehaviour = nullptr;

	Zenith_Entity m_xParentEntity;

	/**
	 * OnAwake - Called at RUNTIME when behavior is attached.
	 * NOT called during scene deserialization.
	 */
	void OnAwake() { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnAwake(); }

	/**
	 * OnStart - Called before first update, for ALL entities (including loaded ones).
	 * This is dispatched by Zenith_Scene during the first frame an entity is active.
	 */
	void OnStart() { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnStart(); }

	/**
	 * OnUpdate - Called every frame.
	 */
	void OnUpdate(float fDt) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnUpdate(fDt); }

	/**
	 * OnDestroy - Called when component is destroyed.
	 */
	void OnDestroy() { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnDestroy(); }

	// Physics collision event dispatch
	void OnCollisionEnter(Zenith_Entity xOther) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnCollisionEnter(xOther); }
	void OnCollisionStay(Zenith_Entity xOther) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnCollisionStay(xOther); }
	void OnCollisionExit(Zenith_EntityID uOtherID) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnCollisionExit(uOtherID); }

	/**
	 * SetBehaviour - Attach a behaviour to this component at runtime.
	 * Calls OnAwake() immediately and marks entity as awoken.
	 * Use this when creating entities during gameplay.
	 */
	template<typename T>
	void SetBehaviour()
	{
		m_pxScriptBehaviour = new T(m_xParentEntity);
		m_pxScriptBehaviour->m_xParentEntity = m_xParentEntity;
		m_pxScriptBehaviour->OnAwake();

		// Mark entity as awoken to prevent duplicate dispatch in Scene::Update()
		if (m_xParentEntity.IsValid())
		{
			Zenith_Scene::GetCurrentScene().MarkEntityAwoken(m_xParentEntity.GetEntityID());
		}
	}

	/**
	 * SetBehaviourForSerialization - Attach a behaviour for scene setup/serialization.
	 * Does NOT call OnAwake() - lifecycle hooks will be called when Play mode is entered.
	 * Use this in Project_LoadInitialScene to set up behaviors that will be serialized.
	 */
	template<typename T>
	void SetBehaviourForSerialization()
	{
		m_pxScriptBehaviour = new T(m_xParentEntity);
		m_pxScriptBehaviour->m_xParentEntity = m_xParentEntity;
		// OnAwake is NOT called - will be dispatched when entering Play mode
	}

	template<typename T>
	T* GetBehaviour()
	{
		return static_cast<T*>(m_pxScriptBehaviour);
	}

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	// Editor UI
	void RenderPropertiesPanel();
#endif

};

#if 0

class PlayerController : public ScriptBehaviour {
public:
	PlayerController(ScriptComponent* pxScriptComponent) : m_xScriptComponent(*pxScriptComponent) {}
	~PlayerController() override {
	}

	enum GuidRefernceIndices {
		Terrain0_0
	};
	ScriptComponent& m_xScriptComponent;
	bool m_bIsOnGround;
	virtual void OnCreate() override {
		m_bIsOnGround = false;
	}
	virtual void OnUpdate(float fDt) override {
	}
	virtual void OnCollision(Entity xOther, Physics::CollisionEventType eCollisionType) override {
		if (xOther.GetGuid().m_uGuid == m_axGuidRefs[Terrain0_0
		].m_uGuid) {
			if (eCollisionType == Physics::CollisionEventType::Exit)
				m_bIsOnGround = false;
			else if (eCollisionType == Physics::CollisionEventType::Start || eCollisionType == Physics::CollisionEventType::Stay)
				m_bIsOnGround = true;
		}
	}
	virtual std::string GetBehaviourType() override { return "PlayerController"; }
};
#endif
