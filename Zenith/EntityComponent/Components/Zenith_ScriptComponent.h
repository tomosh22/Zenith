#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include <unordered_map>
#include <string>

#ifdef ZENITH_TOOLS
#include "EntityComponent/Zenith_ComponentRegistry.h"
#endif

class Zenith_ScriptBehaviour {
	friend class Zenith_ScriptComponent;
public:
	virtual ~Zenith_ScriptBehaviour() {}
	virtual void OnCreate() = 0;
	virtual void OnUpdate(float fDt) = 0;

	// Physics collision callbacks - override to handle collision events
	// xOther is the entity that was collided with
	virtual void OnCollisionEnter(Zenith_Entity xOther) {}
	virtual void OnCollisionStay(Zenith_Entity xOther) {}
	virtual void OnCollisionExit(Zenith_EntityID uOtherID) {}  // Exit only gets ID since body may be destroyed

	// Return the unique type name for this behavior (used for serialization)
	virtual const char* GetBehaviourTypeName() const = 0;

	// Editor UI - override to render behavior-specific properties
	// Default implementation does nothing
	virtual void RenderPropertiesPanel() {}

	// Serialization of behavior-specific parameters
	// Override these to save/load custom behavior state
	virtual void WriteParametersToDataStream(Zenith_DataStream& xStream) const {}
	virtual void ReadParametersFromDataStream(Zenith_DataStream& xStream) {}

	std::vector<Zenith_GUID> m_axGUIDRefs;
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
		delete m_pxScriptBehaviour;
	}
	//void Serialize(std::ofstream& xOut);

	Zenith_ScriptBehaviour* m_pxScriptBehaviour = nullptr;

	Zenith_Entity m_xParentEntity;

	void OnCreate() { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnCreate(); }
	void OnUpdate(float fDt) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnUpdate(fDt); }

	// Physics collision event dispatch
	void OnCollisionEnter(Zenith_Entity xOther) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnCollisionEnter(xOther); }
	void OnCollisionStay(Zenith_Entity xOther) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnCollisionStay(xOther); }
	void OnCollisionExit(Zenith_EntityID uOtherID) { if(m_pxScriptBehaviour) m_pxScriptBehaviour->OnCollisionExit(uOtherID); }

	template<typename T>
	void SetBehaviour()
	{
		m_pxScriptBehaviour = new T(m_xParentEntity);
	}

	// Serialization methods for Zenith_DataStream
	void WriteToDataStream(Zenith_DataStream& xStream) const;
	void ReadFromDataStream(Zenith_DataStream& xStream);

#ifdef ZENITH_TOOLS
	// Editor UI
	void RenderPropertiesPanel();
#endif

public:
#ifdef ZENITH_TOOLS
	// Static registration function called by ComponentRegistry::Initialise()
	static void RegisterWithEditor()
	{
		Zenith_ComponentRegistry::Get().RegisterComponent<Zenith_ScriptComponent>("Script");
	}
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
