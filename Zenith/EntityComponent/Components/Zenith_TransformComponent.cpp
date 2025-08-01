#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"

Zenith_TransformComponent::Zenith_TransformComponent(const std::string& strName, Zenith_Entity& xEntity)
	: m_strName(strName)
	, m_xParentEntity(xEntity)
{
}

Zenith_TransformComponent::~Zenith_TransformComponent() {
}

void Zenith_TransformComponent::SetPosition(const Zenith_Maths::Vector3 xPos)
{
	if (m_pxRigidBody)
	{
		//#TO making a copy
		reactphysics3d::Transform xTrans = m_pxRigidBody->getTransform();
		xTrans.setPosition({ xPos.x, xPos.y, xPos.z });
		m_pxRigidBody->setTransform(xTrans);
	}
	else
	{
		m_xTransform.setPosition({ xPos.x, xPos.y, xPos.z });
	}
}

void Zenith_TransformComponent::SetRotation(const Zenith_Maths::Quat xRot)
{
	if (m_pxRigidBody)
	{
		//#TO making a copy
		reactphysics3d::Transform xTrans = m_pxRigidBody->getTransform();
		xTrans.setOrientation({ xRot.x, xRot.y, xRot.z, xRot.w });
		m_pxRigidBody->setTransform(xTrans);
	}
	else
	{
		m_xTransform.setOrientation({ xRot.x, xRot.y, xRot.z, xRot.w });
	}
}

void Zenith_TransformComponent::SetScale(const Zenith_Maths::Vector3 xScale)
{
	m_xScale = xScale;
}

void Zenith_TransformComponent::GetPosition(Zenith_Maths::Vector3& xPos)
{
	if (m_pxRigidBody)
	{
		//#TO making a copy
		reactphysics3d::Transform xTrans = m_pxRigidBody->getTransform();
		reactphysics3d::Vector3 xInternalPos = xTrans.getPosition();
		xPos.x = xInternalPos.x;
		xPos.y = xInternalPos.y;
		xPos.z = xInternalPos.z;
	}
	else
	{
		reactphysics3d::Vector3 xInternalPos = m_xTransform.getPosition();
		xPos.x = xInternalPos.x;
		xPos.y = xInternalPos.y;
		xPos.z = xInternalPos.z;
	}
}

void Zenith_TransformComponent::GetRotation(Zenith_Maths::Quat& xRot)
{
	if (m_pxRigidBody)
	{
		//#TO making a copy
		reactphysics3d::Transform xTrans = m_pxRigidBody->getTransform();
		reactphysics3d::Quaternion xInternalRot = xTrans.getOrientation();
		xRot.x = xInternalRot.x;
		xRot.y = xInternalRot.y;
		xRot.z = xInternalRot.z;
		xRot.w = xInternalRot.w;
	}
	else
	{
		reactphysics3d::Quaternion xInternalRot = m_xTransform.getOrientation();
		xRot.x = xInternalRot.x;
		xRot.y = xInternalRot.y;
		xRot.z = xInternalRot.z;
		xRot.w = xInternalRot.w;
	}
}

void Zenith_TransformComponent::GetScale(Zenith_Maths::Vector3& xScale)
{
	xScale = m_xScale;
}

const reactphysics3d::Transform& Zenith_TransformComponent::GetTransform()
{
	return m_pxRigidBody ? m_pxRigidBody->getTransform() : m_xTransform;
}

void Zenith_TransformComponent::BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut)
{
	GetTransform().getOpenGLMatrix(&xMatOut[0][0]);
	xMatOut *= glm::scale(glm::identity<glm::highp_mat4>(), m_xScale);

	Zenith_GUID xParentGUID = m_xParentEntity.m_xParentEntityGUID;
	while ((GUIDType)xParentGUID != (GUIDType)Zenith_GUID::Invalid)
	{
		glm::mat4 xToMultiply;
		Zenith_Entity xEntity = m_xParentEntity.m_pxParentScene->GetEntityByGUID(xParentGUID);
		xEntity.GetComponent<Zenith_TransformComponent>().GetTransform().getOpenGLMatrix(&xToMultiply[0][0]);
		xMatOut *= xToMultiply;
		//#TO_TODO: why is the minus necessary?
		xMatOut *= -glm::scale(glm::identity<glm::highp_mat4>(), xEntity.GetComponent<Zenith_TransformComponent>().m_xScale);
		xParentGUID = xEntity.m_xParentEntityGUID;
	}
}

reactphysics3d::Transform& Zenith_TransformComponent::GetTransform_Unsafe()
{
	return m_xTransform;
}

void Zenith_TransformComponent::Serialize(std::ofstream& xOut)
{
	STUBBED
#if 0
		xOut << "TransformComponent\n";
	reactphysics3d::Transform* pxTrans = GetTransform();
	xOut << pxTrans->getPosition().x << ' ' << pxTrans->getPosition().y << ' ' << pxTrans->getPosition().z << '\n';
	xOut << pxTrans->getOrientation().x << ' ' << pxTrans->getOrientation().y << ' ' << pxTrans->getOrientation().z << ' ' << pxTrans->getOrientation().w << '\n';
	xOut << m_xScale.x << ' ' << m_xScale.y << ' ' << m_xScale.z << '\n';
#endif
}