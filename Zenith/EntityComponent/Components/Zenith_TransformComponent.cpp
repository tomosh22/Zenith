#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
//#include "reactphysics3d/reactphysics3d.h"

Zenith_TransformComponent::Zenith_TransformComponent(const std::string& strName) : m_strName(strName) {
	//m_pxTransform = new reactphysics3d::Transform;
}

Zenith_TransformComponent::~Zenith_TransformComponent() {
}

void Zenith_TransformComponent::SetPosition(const Zenith_Maths::Vector3& xPos) {
	STUBBED
	//m_pxTransform->setPosition({xPos.x, xPos.y, xPos.z});
}

void Zenith_TransformComponent::SetRotation(const Zenith_Maths::Quat& xRot) {
	STUBBED
	//m_pxTransform->setOrientation({ xRot.x, xRot.y, xRot.z, xRot.w });
}

void Zenith_TransformComponent::SetScale(const Zenith_Maths::Vector3& xScale) {
	m_xScale = xScale;
}

reactphysics3d::Transform* Zenith_TransformComponent::GetTransform() {
	STUBBED
	return nullptr;
	//return m_pxRigidBody ? const_cast<reactphysics3d::Transform*>(&m_pxRigidBody->getTransform()) : m_pxTransform;
}

reactphysics3d::Transform* Zenith_TransformComponent::GetTransform_Unsafe() {
	return m_pxTransform;
}

void Zenith_TransformComponent::Serialize(std::ofstream& xOut) {
	STUBBED
#if 0
	xOut << "TransformComponent\n";
	reactphysics3d::Transform* pxTrans = GetTransform();
	xOut << pxTrans->getPosition().x << ' ' << pxTrans->getPosition().y << ' ' << pxTrans->getPosition().z << '\n';
	xOut << pxTrans->getOrientation().x << ' ' << pxTrans->getOrientation().y << ' ' << pxTrans->getOrientation().z << ' ' << pxTrans->getOrientation().w << '\n';
	xOut << m_xScale.x << ' ' << m_xScale.y << ' ' << m_xScale.z << '\n';
#endif
}
