#include "Zenith.h"
#include "EntityComponent/Components/Zenith_TransformComponent.h"
#include "EntityComponent/Components/Zenith_ColliderComponent.h"
#include "EntityComponent/Components/Zenith_ModelComponent.h"
#include "Physics/Zenith_Physics.h"
#include "DataStream/Zenith_DataStream.h"
#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include <Jolt/Jolt.h>
#include <Jolt/Physics/Body/Body.h>
#include "Memory/Zenith_MemoryManagement_Enabled.h"

Zenith_TransformComponent::Zenith_TransformComponent(Zenith_Entity& xEntity)
	: m_xParentEntity(xEntity)
{
}

Zenith_TransformComponent::~Zenith_TransformComponent() {
}

void Zenith_TransformComponent::SetPosition(const Zenith_Maths::Vector3 xPos)
{
	if (m_pxRigidBody)
	{
		JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
		JPH::Vec3 xJoltPos(xPos.x, xPos.y, xPos.z);
		xBodyInterface.SetPosition(m_pxRigidBody->GetID(), xJoltPos, JPH::EActivation::Activate);
	}
	else
	{
		m_xPosition = xPos;
	}
}

void Zenith_TransformComponent::SetRotation(const Zenith_Maths::Quat xRot)
{
	if (m_pxRigidBody)
	{
		JPH::BodyInterface& xBodyInterface = Zenith_Physics::s_pxPhysicsSystem->GetBodyInterface();
		JPH::Quat xJoltRot(xRot.x, xRot.y, xRot.z, xRot.w);
		xBodyInterface.SetRotation(m_pxRigidBody->GetID(), xJoltRot, JPH::EActivation::Activate);
	}
	else
	{
		m_xRotation = xRot;
	}
}

void Zenith_TransformComponent::SetScale(const Zenith_Maths::Vector3 xScale)
{
	// Check if scale actually changed
	if (m_xScale.x == xScale.x && m_xScale.y == xScale.y && m_xScale.z == xScale.z)
	{
		return;
	}

	m_xScale = xScale;

	// If entity has a model component, regenerate physics mesh with new baked scale
	if (m_xParentEntity.HasComponent<Zenith_ModelComponent>())
	{
		Zenith_ModelComponent& xModel = m_xParentEntity.GetComponent<Zenith_ModelComponent>();
		if (xModel.HasPhysicsMesh())
		{
			xModel.GeneratePhysicsMesh();
		}
	}

	// If entity has a collider component, rebuild it to reflect new scale
	if (m_xParentEntity.HasComponent<Zenith_ColliderComponent>())
	{
		Zenith_ColliderComponent& xCollider = m_xParentEntity.GetComponent<Zenith_ColliderComponent>();
		xCollider.RebuildCollider();
	}
}

void Zenith_TransformComponent::GetPosition(Zenith_Maths::Vector3& xPos)
{
	if (m_pxRigidBody)
	{
		JPH::Vec3 xJoltPos = m_pxRigidBody->GetPosition();
		xPos.x = xJoltPos.GetX();
		xPos.y = xJoltPos.GetY();
		xPos.z = xJoltPos.GetZ();
	}
	else
	{
		xPos = m_xPosition;
	}
}

void Zenith_TransformComponent::GetRotation(Zenith_Maths::Quat& xRot)
{
	if (m_pxRigidBody)
	{
		JPH::Quat xJoltRot = m_pxRigidBody->GetRotation();
		xRot.x = xJoltRot.GetX();
		xRot.y = xJoltRot.GetY();
		xRot.z = xJoltRot.GetZ();
		xRot.w = xJoltRot.GetW();
	}
	else
	{
		xRot = m_xRotation;
	}
}

void Zenith_TransformComponent::GetScale(Zenith_Maths::Vector3& xScale)
{
	xScale = m_xScale;
}

void Zenith_TransformComponent::BuildModelMatrix(Zenith_Maths::Matrix4& xMatOut)
{
	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	GetPosition(xPos);
	GetRotation(xRot);

	glm::mat4 xTranslation = glm::translate(glm::identity<glm::mat4>(), xPos);
	glm::mat4 xRotation = glm::mat4_cast(xRot);
	glm::mat4 xScaleMat = glm::scale(glm::identity<glm::mat4>(), m_xScale);
	
	xMatOut = xTranslation * xRotation * xScaleMat;

	Zenith_EntityID uParentGUID = m_xParentEntity.m_uParentEntityID;
	while (uParentGUID != -1)
	{
		Zenith_Entity xEntity = m_xParentEntity.m_pxParentScene->GetEntityByID(uParentGUID);
		Zenith_TransformComponent& xParentTransform = xEntity.GetComponent<Zenith_TransformComponent>();
		
		Zenith_Maths::Vector3 xParentPos;
		Zenith_Maths::Quat xParentRot;
		xParentTransform.GetPosition(xParentPos);
		xParentTransform.GetRotation(xParentRot);

		glm::mat4 xParentTranslation = glm::translate(glm::identity<glm::mat4>(), xParentPos);
		glm::mat4 xParentRotation = glm::mat4_cast(xParentRot);
		glm::mat4 xParentScale = glm::scale(glm::identity<glm::mat4>(), xParentTransform.m_xScale);
		glm::mat4 xParentMatrix = xParentTranslation * xParentRotation * xParentScale;
		
		xMatOut = xParentMatrix * xMatOut;
		uParentGUID = xEntity.m_uParentEntityID;
	}
}

void Zenith_TransformComponent::WriteToDataStream(Zenith_DataStream& xStream) const
{

	// Write position, rotation, and scale
	// Note: We get current values from physics if rigid body exists
	Zenith_Maths::Vector3 xPos;
	Zenith_Maths::Quat xRot;
	const_cast<Zenith_TransformComponent*>(this)->GetPosition(xPos);
	const_cast<Zenith_TransformComponent*>(this)->GetRotation(xRot);

	xStream << xPos;
	xStream << xRot;
	xStream << m_xScale;

	// Note: m_pxRigidBody is not serialized as it's a runtime-only physics handle
	// Physics will be reconstructed from ColliderComponent on load
}

void Zenith_TransformComponent::ReadFromDataStream(Zenith_DataStream& xStream)
{

	// Read position, rotation, and scale
	xStream >> m_xPosition;
	xStream >> m_xRotation;
	xStream >> m_xScale;

	// m_pxRigidBody remains nullptr - will be set by ColliderComponent if needed
}