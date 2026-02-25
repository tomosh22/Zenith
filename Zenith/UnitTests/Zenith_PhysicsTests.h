#pragma once

/**
 * Zenith_PhysicsTests - Unit tests for the physics system (Jolt integration)
 *
 * Tests cover:
 * - Gravity and free-fall
 * - Linear and angular velocity
 * - Forces and impulses
 * - Collision detection
 * - Collision events (OnCollisionEnter/Stay/Exit)
 * - Raycasting
 * - Body configuration (gravity toggle, rotation lock)
 * - Collider shape types (AABB, sphere, capsule)
 * - Physics timestep behaviour
 * - Scene cleanup and multi-scene physics
 */
class Zenith_PhysicsTests
{
public:
	static void RunAllTests();

private:
	//==========================================================================
	// Cat 1: Gravity & Free-Fall
	//==========================================================================
	static void TestDynamicBodyFallsUnderGravity();
	static void TestStaticBodyDoesNotFall();
	static void TestGravityDisabledBodyStaysStill();
	static void TestGravityReenabledBodyFalls();

	//==========================================================================
	// Cat 2: Velocity
	//==========================================================================
	static void TestSetLinearVelocity();
	static void TestGetLinearVelocityMatchesSet();
	static void TestSetAngularVelocity();
	static void TestGetAngularVelocityMatchesSet();
	static void TestZeroVelocityNoMovement();

	//==========================================================================
	// Cat 3: Forces & Impulses
	//==========================================================================
	static void TestAddForceAcceleratesBody();
	static void TestAddImpulseInstantVelocityChange();
	static void TestForceAccumulatesOverFrames();
	static void TestImpulseOnStaticBodyNoEffect();

	//==========================================================================
	// Cat 4: Collision Detection
	//==========================================================================
	static void TestDynamicHitsStaticFloor();
	static void TestTwoDynamicBodiesCollide();
	static void TestStaticStaticNoCollision();
	static void TestSphereOnBoxCollision();

	//==========================================================================
	// Cat 5: Collision Events
	//==========================================================================
	static void TestCollisionEnterCallback();
	static void TestCollisionStayCallback();
	static void TestCollisionExitCallback();
	static void TestCollisionEventBothEntitiesReceive();

	//==========================================================================
	// Cat 6: Raycasting
	//==========================================================================
	static void TestRaycastHitsSphere();
	static void TestRaycastMissesNoBody();
	static void TestRaycastReturnsHitPoint();
	static void TestRaycastReturnsHitEntity();
	static void TestRaycastMaxDistanceRespected();

	//==========================================================================
	// Cat 7: Body Configuration
	//==========================================================================
	static void TestLockRotationPreventsAngularVelocity();
	static void TestColliderHasValidBodyAfterAdd();
	static void TestColliderBodyIDMatchesJolt();
	static void TestRebuildColliderPreservesVelocity();

	//==========================================================================
	// Cat 8: Collider Shape Types
	//==========================================================================
	static void TestAABBColliderCreation();
	static void TestSphereColliderCreation();
	static void TestCapsuleColliderCreation();
	static void TestCapsuleExplicitDimensions();

	//==========================================================================
	// Cat 9: Physics Timestep
	//==========================================================================
	static void TestFixedTimestepOneStep();
	static void TestAccumulatorDoesNotOverStep();
	static void TestResetClearsPhysicsState();

	//==========================================================================
	// Cat 10: Scene Cleanup
	//==========================================================================
	static void TestUnloadSceneDestroysPhysicsBodies();
	static void TestMultipleScenePhysicsIndependence();

	//==========================================================================
	// Cat 11: Gravity Toggle + Impulse Launch
	//==========================================================================
	static void TestGravityOffThenImpulseLaunch();
};
