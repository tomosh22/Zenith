#pragma once
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Zenith_Entity.h"

class Zenith_ScriptBehaviour {
	friend class Zenith_ScriptComponent;
public:
	virtual ~Zenith_ScriptBehaviour() {}
	virtual void OnCreate() = 0;
	virtual void OnUpdate(float fDt) = 0;
	//virtual void OnCollision(Zenith_Entity xOther, Physics::CollisionEventType eCollisionType) = 0;
	//virtual std::string GetBehaviourType() = 0;
	std::vector<Zenith_GUID> m_axGUIDRefs;

};
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

	void OnCreate() { m_pxScriptBehaviour->OnCreate(); }
	void OnUpdate(float fDt) { m_pxScriptBehaviour->OnUpdate(fDt); }
	//void OnCollision(Zenith_Entity xOther, Physics::CollisionEventType eCollisionType) { m_pxScriptBehaviour->OnCollision(xOther, eCollisionType); }

	template<typename T>
	void SetBehaviour() {
		m_pxScriptBehaviour = new T(*this);
	}
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
		if(xOther.GetGuid().m_uGuid == m_axGuidRefs[Terrain0_0
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



