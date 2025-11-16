#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "Maths/Zenith_Maths.h"

namespace JPH
{
	class Body;
}

class Zenith_TransformComponent
{
public:
	Zenith_TransformComponent(const std::string& strName, Zenith_Entity& xEntity);
	~Zenith_TransformComponent();
	void Serialize(std::ofstream& xOut);

	void SetPosition(const Zenith_Maths::Vector3 xPos);
	void SetRotation(const Zenith_Maths::Quat xRot);
	void SetScale(const Zenith_Maths::Vector3 xScale);

	void GetPosition(Zenith_Maths::Vector3& xPos);
	void GetRotation(Zenith_Maths::Quat& xRot);
	void GetScale(Zenith_Maths::Vector3& xScale);

	Zenith_Maths::Vector3 m_xScale = { 1.,1.,1. };
	JPH::Body* m_pxRigidBody = nullptr;

	void BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut);

	std::string m_strName;

private:
	friend class Zenith_ColliderComponent;
	
	Zenith_Maths::Vector3 m_xPosition = { 0.0, 0.0, 0.0 };
	Zenith_Maths::Quat m_xRotation = { 1.0, 0.0, 0.0, 0.0 };

	Zenith_Entity m_xParentEntity;
};
