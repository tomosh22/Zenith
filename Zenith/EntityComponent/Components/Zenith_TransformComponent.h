#pragma once

namespace reactphysics3d {
	class Transform;
	class RigidBody;
}

class Transform
{
public:
	Transform() {};
	Transform(Zenith_Maths::Vector3 xPos, Zenith_Maths::Vector3 xScale, Zenith_Maths::Quat xRot /*= glm::quat_identity<float, glm::defaultp>()*/) : m_xPosition(xPos), m_xScale(xScale), m_xRotationQuat(xRot) {}

	void SetPosition(const Zenith_Maths::Vector3& pos);
	void SetRotationQuat(const Zenith_Maths::Quat& rot);
	void SetScale(const Zenith_Maths::Vector3& xScale);
	inline void UpdateMatrix();
	void UpdateRotation();
	Zenith_Maths::Matrix4 m_xMatrix /*= glm::identity<float>()*/;

	Zenith_Maths::Quat m_xRotationQuat /*= glm::quat_identity<float, glm::packed_highp>()*/;
	float m_fRoll = 0, m_fYaw = 0, m_fPitch = 0;
	Zenith_Maths::Vector3 m_xPosition = { 0,0,0 };
	Zenith_Maths::Vector3 m_xScale = { 1,1,1 };

	static Zenith_Maths::Matrix4 RotationMatFromQuat(const Zenith_Maths::Quat& quat);
	static Zenith_Maths::Matrix4 RotationMatFromVec3(float degrees, const Zenith_Maths::Vector3& axis);
	static Zenith_Maths::Quat EulerAnglesToQuat(float roll, float yaw, float pitch);
private:
	float m_fPrevRoll = 0, m_fPrevYaw = 0, m_fPrevPitch = 0;
};

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
