#pragma once

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

//------------------------------------------------------------------------------
// Simulated-input -> ImGui injection hook. The Vulkan backend calls this
// BETWEEN ImGui_ImplGlfw_NewFrame and ImGui::NewFrame so the simulator's
// events are queued AFTER the backend's real-OS-cursor events - last event
// wins, making simulated input deterministic even while the real mouse sits
// elsewhere (the GLFW backend polls the hardware cursor every focused frame).
//
// Constant-initialised to the bridge thunk in Zenith_ImGuiInputBridge.cpp
// (the g_pfnZenithLightGather pattern); null-safe at the call site.
//------------------------------------------------------------------------------
typedef void (*Zenith_ImGuiSimulatedInputFn)();
extern Zenith_ImGuiSimulatedInputFn g_pfnZenithImGuiSimulatedInput;

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
