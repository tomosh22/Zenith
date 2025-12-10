#include "Zenith.h"
#include "Physics/Zenith_Physics.h"
#include "EntityComponent/Components/Zenith_CameraComponent.h"
#include "EntityComponent/Components/Zenith_PhysicsMeshGenerator.h"
#include "Zenith_OS_Include.h"

JPH::TempAllocatorImpl* Zenith_Physics::s_pxTempAllocator = nullptr;
JPH::JobSystemThreadPool* Zenith_Physics::s_pxJobSystem = nullptr;
JPH::PhysicsSystem* Zenith_Physics::s_pxPhysicsSystem = nullptr;
double Zenith_Physics::s_fTimestepAccumulator = 0;
Zenith_Physics::PhysicsContactListener Zenith_Physics::s_xContactListener;

static void TraceImpl(const char* inFMT, ...)
{
	va_list list;
	va_start(list, inFMT);
	char buffer[1024];
	vsnprintf(buffer, sizeof(buffer), inFMT, list);
	va_end(list);

	std::cout << buffer << std::endl;
}

#ifdef JPH_ENABLE_ASSERTS
static bool AssertFailedImpl(const char* inExpression, const char* inMessage, const char* inFile, uint32_t inLine)
{
	std::cout << inFile << ":" << inLine << ": (" << inExpression << ") " << (inMessage != nullptr ? inMessage : "") << std::endl;

	__debugbreak();
	return true;
};
#endif

namespace Layers
{
	static constexpr JPH::ObjectLayer NON_MOVING = 0;
	static constexpr JPH::ObjectLayer MOVING = 1;
	static constexpr JPH::ObjectLayer NUM_LAYERS = 2;
}

class ObjectLayerPairFilterImpl : public JPH::ObjectLayerPairFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2) const override
	{
		switch (inObject1)
		{
		case Layers::NON_MOVING:
			return inObject2 == Layers::MOVING; // Non moving only collides with moving
		case Layers::MOVING:
			return true; // Moving collides with everything
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

namespace BroadPhaseLayers
{
	static constexpr JPH::BroadPhaseLayer NON_MOVING(0);
	static constexpr JPH::BroadPhaseLayer MOVING(1);
	static constexpr uint32_t NUM_LAYERS(2);
};

class BPLayerInterfaceImpl final : public JPH::BroadPhaseLayerInterface
{
public:
	BPLayerInterfaceImpl()
	{
		mObjectToBroadPhase[Layers::NON_MOVING] = BroadPhaseLayers::NON_MOVING;
		mObjectToBroadPhase[Layers::MOVING] = BroadPhaseLayers::MOVING;
	}

	virtual uint32_t GetNumBroadPhaseLayers() const override
	{
		return BroadPhaseLayers::NUM_LAYERS;
	}

	virtual JPH::BroadPhaseLayer GetBroadPhaseLayer(JPH::ObjectLayer inLayer) const override
	{
		JPH_ASSERT(inLayer < Layers::NUM_LAYERS);
		return mObjectToBroadPhase[inLayer];
	}

#if defined(JPH_EXTERNAL_PROFILE) || defined(JPH_PROFILE_ENABLED)
	virtual const char* GetBroadPhaseLayerName(JPH::BroadPhaseLayer inLayer) const override
	{
		switch ((JPH::BroadPhaseLayer::Type)inLayer)
		{
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::NON_MOVING:	return "NON_MOVING";
		case (JPH::BroadPhaseLayer::Type)BroadPhaseLayers::MOVING:		return "MOVING";
		default:														JPH_ASSERT(false); return "INVALID";
		}
	}
#endif

private:
	JPH::BroadPhaseLayer mObjectToBroadPhase[Layers::NUM_LAYERS];
};

class ObjectVsBroadPhaseLayerFilterImpl : public JPH::ObjectVsBroadPhaseLayerFilter
{
public:
	virtual bool ShouldCollide(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2) const override
	{
		switch (inLayer1)
		{
		case Layers::NON_MOVING:
			return inLayer2 == BroadPhaseLayers::MOVING;
		case Layers::MOVING:
			return true;
		default:
			JPH_ASSERT(false);
			return false;
		}
	}
};

static BPLayerInterfaceImpl s_xBroadPhaseLayerInterface;
static ObjectVsBroadPhaseLayerFilterImpl s_xObjectVsBroadPhaseLayerFilter;
static ObjectLayerPairFilterImpl s_xObjectLayerPairFilter;

void Zenith_Physics::Initialise()
{
	JPH::RegisterDefaultAllocator();

	JPH::Trace = TraceImpl;
	JPH_IF_ENABLE_ASSERTS(JPH::AssertFailed = AssertFailedImpl;)

	JPH::Factory::sInstance = new JPH::Factory();

	JPH::RegisterTypes();

	s_pxTempAllocator = new JPH::TempAllocatorImpl(10 * 1024 * 1024);

	s_pxJobSystem = new JPH::JobSystemThreadPool(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsBarriers, std::thread::hardware_concurrency() - 1);

	s_pxPhysicsSystem = new JPH::PhysicsSystem();
	s_pxPhysicsSystem->Init(s_uMaxBodies, s_uNumBodyMutexes, s_uMaxBodyPairs, s_uMaxContactConstraints,
		s_xBroadPhaseLayerInterface, s_xObjectVsBroadPhaseLayerFilter, s_xObjectLayerPairFilter);

	s_pxPhysicsSystem->SetGravity(JPH::Vec3(0.0f, -9.81f, 0.0f));

	s_pxPhysicsSystem->SetContactListener(&s_xContactListener);
}

void Zenith_Physics::Update(float fDt)
{
	s_fTimestepAccumulator += fDt;
	
	while (s_fTimestepAccumulator >= s_fDesiredFramerate)
	{
		s_pxPhysicsSystem->Update(static_cast<float>(s_fDesiredFramerate), 1, s_pxTempAllocator, s_pxJobSystem);
		s_fTimestepAccumulator -= s_fDesiredFramerate;
	}

	Zenith_PhysicsMeshGenerator::DebugDrawAllPhysicsMeshes();
}

void Zenith_Physics::Reset()
{
	Shutdown();
	Initialise();
}

void Zenith_Physics::Shutdown()
{
	if (s_pxPhysicsSystem)
	{
		delete s_pxPhysicsSystem;
		s_pxPhysicsSystem = nullptr;
	}

	if (s_pxJobSystem)
	{
		delete s_pxJobSystem;
		s_pxJobSystem = nullptr;
	}

	if (s_pxTempAllocator)
	{
		delete s_pxTempAllocator;
		s_pxTempAllocator = nullptr;
	}

	if (JPH::Factory::sInstance)
	{
		delete JPH::Factory::sInstance;
		JPH::Factory::sInstance = nullptr;
	}

	JPH::UnregisterTypes();
}

Zenith_Physics::RaycastInfo Zenith_Physics::BuildRayFromMouse(Zenith_CameraComponent& xCam)
{
	Zenith_Maths::Vector2_64 xMousePos;
	Zenith_Window::GetInstance()->GetMousePosition(xMousePos);

	double fX = xMousePos.x;
	double fY = xMousePos.y;

#if 0//def ZENITH_TOOLS
	//accounting for extra padding from imgui border
	fX -= 10;
	fY -= 45;

	//#TO_TODO: what happens on window resize?
	fX /= (float)VCE_GAME_WIDTH / float(VCE_GAME_WIDTH + VCE_EDITOR_ADDITIONAL_WIDTH);
	fY /= (float)VCE_GAME_HEIGHT / float(VCE_GAME_HEIGHT + VCE_EDITOR_ADDITIONAL_HEIGHT);
#endif

	glm::vec3 xNearPos = { fX, fY, 0.0f };
	glm::vec3 xFarPos = { fX, fY, 1.0f };

	glm::vec3 xOrigin = xCam.ScreenSpaceToWorldSpace(xNearPos);
	glm::vec3 xDest = xCam.ScreenSpaceToWorldSpace(xFarPos);

	Zenith_Maths::Vector3 xRayDirection = Zenith_Maths::Vector3(xDest.x - xOrigin.x, xDest.y - xOrigin.y, xDest.z - xOrigin.z);
	xRayDirection = glm::normalize(xRayDirection);

	RaycastInfo xInfo;
	xInfo.m_xOrigin = xOrigin;
	xInfo.m_xDirection = xRayDirection;
	return xInfo;
}

void Zenith_Physics::SetLinearVelocity(JPH::Body* pxBody, const Zenith_Maths::Vector3& xVelocity)
{
	if (!pxBody) return;
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetLinearVelocity(pxBody->GetID(), JPH::Vec3(xVelocity.x, xVelocity.y, xVelocity.z));
}

Zenith_Maths::Vector3 Zenith_Physics::GetLinearVelocity(JPH::Body* pxBody)
{
	if (!pxBody) return Zenith_Maths::Vector3(0, 0, 0);
	JPH::Vec3 xVel = pxBody->GetLinearVelocity();
	return Zenith_Maths::Vector3(xVel.GetX(), xVel.GetY(), xVel.GetZ());
}

void Zenith_Physics::SetAngularVelocity(JPH::Body* pxBody, const Zenith_Maths::Vector3& xVelocity)
{
	if (!pxBody) return;
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetAngularVelocity(pxBody->GetID(), JPH::Vec3(xVelocity.x, xVelocity.y, xVelocity.z));
}

void Zenith_Physics::AddForce(JPH::Body* pxBody, const Zenith_Maths::Vector3& xForce)
{
	if (!pxBody) return;
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.AddForce(pxBody->GetID(), JPH::Vec3(xForce.x, xForce.y, xForce.z));
}

void Zenith_Physics::SetGravityEnabled(JPH::Body* pxBody, bool bEnabled)
{
	if (!pxBody) return;
	JPH::BodyInterface& xBodyInterface = s_pxPhysicsSystem->GetBodyInterface();
	xBodyInterface.SetGravityFactor(pxBody->GetID(), bEnabled ? 1.0f : 0.0f);
}

JPH::ValidateResult Zenith_Physics::PhysicsContactListener::OnContactValidate(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	JPH::RVec3Arg inBaseOffset, const JPH::CollideShapeResult& inCollisionResult)
{
	return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
}

void Zenith_Physics::PhysicsContactListener::OnContactAdded(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
#if 0
	// Handle contact start event
	// You can access body user data like this:
	// void* userData1 = inBody1.GetUserData();
	// void* userData2 = inBody2.GetUserData();
	
	// Example: Trigger COLLISION_EVENT_TYPE_START callbacks
#endif
}

void Zenith_Physics::PhysicsContactListener::OnContactPersisted(
	const JPH::Body& inBody1, const JPH::Body& inBody2,
	const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings)
{
#if 0
	// Handle contact stay event
	// Example: Trigger COLLISION_EVENT_TYPE_STAY callbacks
#endif
}

void Zenith_Physics::PhysicsContactListener::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
{
#if 0
	// Handle contact exit event
	// Example: Trigger COLLISION_EVENT_TYPE_EXIT callbacks
#endif
}