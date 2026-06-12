#pragma once

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

//------------------------------------------------------------------------------
// Zenith_ImGuiInputBridge - feeds Zenith_InputSimulator state into ImGui.
//
// ImGui normally receives input ONLY through the GLFW backend callbacks (real
// OS events) - simulated input from the automated-test harness never reaches
// it, so tests could not drive editor UI. This bridge converts the simulator's
// state into ImGui IO events (AddMousePosEvent / AddMouseButtonEvent /
// AddKeyEvent / AddInputCharacter) once per frame.
//
// Call PumpSimulatedInput() immediately BEFORE the backend's ImGui NewFrame
// block (Zenith_Editor::RenderImGuiFrame does this): events queued before
// ImGui::NewFrame are consumed by that same NewFrame - zero added latency.
//
// No-op while the simulator is disabled, so interactive editor sessions are
// untouched.
//------------------------------------------------------------------------------
class Zenith_ImGuiInputBridge
{
public:
	static void PumpSimulatedInput();
};

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
