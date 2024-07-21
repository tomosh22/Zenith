#pragma once

namespace reactphysics3d {
	class Transform;
	class RigidBody;
}

class Zenith_TransformComponent
{
public:
	Zenith_TransformComponent(const std::string& strName);
	~Zenith_TransformComponent();
	void Serialize(std::ofstream& xOut);
	void SetPosition(const Zenith_Maths::Vector3& xPos);
	void SetRotation(const Zenith_Maths::Quat& xRot);
	void SetScale(const Zenith_Maths::Vector3& xScale);

	reactphysics3d::Transform* GetTransform();
	Zenith_Maths::Vector3 m_xScale = {0,0,0};
	reactphysics3d::RigidBody* m_pxRigidBody = nullptr;

	std::string m_strName;
		
private:
	friend class ColliderComponent;
	reactphysics3d::Transform* GetTransform_Unsafe();
	reactphysics3d::Transform* m_pxTransform = nullptr;
};
