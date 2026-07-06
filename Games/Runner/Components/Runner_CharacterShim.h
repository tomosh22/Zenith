#pragma once
#include "Core/Zenith_Engine.h"
/**
 * Runner_CharacterShim - the static-scope -> shim-wrapper pilot (W1).
 *
 * Runner's character modules (Runner_CharacterController,
 * Runner_AnimationDriver) are static-scope singletons. Behaviour-graph nodes
 * resolve their target through the CONTEXT ENTITY, not through globals - this
 * thin component sits on the runtime-spawned character entity and forwards
 * the graph-facing surface to the statics, so the Runner_CharacterActions
 * graph stays doctrine-clean (self-resolved seams, no nodes naming statics).
 *
 * Added by Runner_GameComponent::CreateCharacter together with the
 * GraphComponent (AddGraphByAssetPath - runtime-spawned entity pattern).
 */

#include "DataStream/Zenith_DataStream.h"
#include "ZenithECS/Zenith_Entity.h"

#include "Runner_CharacterController.h"
#include "Runner_AnimationDriver.h"

#ifdef ZENITH_TOOLS
#include "imgui.h"
#endif

class Runner_CharacterShim
{
public:
	Runner_CharacterShim() = delete;
	Runner_CharacterShim(Zenith_Entity& /*xParentEntity*/)
	{
	}
	~Runner_CharacterShim() = default;

	// Action gates (Try* pattern: true = the action was taken).
	bool TrySwitchLane(int32_t iDirection) { return Runner_CharacterController::TrySwitchLane(iDirection); }
	bool TryJump() { return Runner_CharacterController::TryJump(); }
	bool TrySlide() { return Runner_CharacterController::TrySlide(); }

	// Slide-expiry systems body + the duration tunable (read live from the
	// controller config by the RunnerTrySlide node).
	void EndSlide() { Runner_CharacterController::EndSlide(); }
	float GetSlideDuration() const { return Runner_CharacterController::GetSlideDuration(); }

	// Frame facts for the animation decision chain.
	int32_t GetCharacterState() const { return static_cast<int32_t>(Runner_CharacterController::GetState()); }
	float GetCurrentSpeed() const { return Runner_CharacterController::GetCurrentSpeed(); }

	// Animation transition (the decision arrives from the graph).
	void SetAnimState(int32_t iAnimState)
	{
		if (iAnimState >= 0 && iAnimState <= static_cast<int32_t>(RunnerAnimState::SLIDE))
		{
			Runner_AnimationDriver::SetStateFromGraph(static_cast<RunnerAnimState>(iAnimState));
		}
	}

#ifdef ZENITH_TOOLS
	void RenderPropertiesPanel()
	{
		const char* szCharStates[] = { "RUNNING", "JUMPING", "SLIDING", "DEAD" };
		ImGui::Text("Character: %s", szCharStates[GetCharacterState()]);
		ImGui::Text("Speed: %.1f", GetCurrentSpeed());
		ImGui::TextWrapped("Graph-facing shim over the static character/animation "
			"modules (Runner_CharacterActions resolves this component).");
	}
#endif

	void WriteToDataStream(Zenith_DataStream& xStream) const
	{
		// Component-contract leading version. Stateless (all state is in the
		// static modules / the graph blackboard); the character entity lives
		// in the procedural "Run" scene and is never saved to disk.
		const u_int uComponentVersion = 1;
		xStream << uComponentVersion;
	}

	void ReadFromDataStream(Zenith_DataStream& xStream)
	{
		u_int uComponentVersion = 0;
		xStream >> uComponentVersion;
	}
};
