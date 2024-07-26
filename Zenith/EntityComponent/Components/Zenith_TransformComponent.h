#pragma once
#include "EntityComponent/Zenith_Entity.h"
#include "reactphysics3d/reactphysics3d.h"

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

	const reactphysics3d::Transform& const GetTransform();
	Zenith_Maths::Vector3 m_xScale = {1.,1.,1.};
	reactphysics3d::RigidBody* m_pxRigidBody = nullptr;

	void BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut);

	std::string m_strName;
		
private:
	friend class ColliderComponent;
	reactphysics3d::Transform& GetTransform_Unsafe();
	reactphysics3d::Transform m_xTransform;

	Zenith_Entity m_xParentEntity;
};
