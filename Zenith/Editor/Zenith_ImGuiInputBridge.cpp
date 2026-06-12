#include "Zenith.h"

#if defined(ZENITH_TOOLS) && defined(ZENITH_INPUT_SIMULATOR)

#include "Editor/Zenith_ImGuiInputBridge.h"
#include "Core/Zenith_ImGuiBridgeHook.h"
#include "Input/Zenith_InputSimulator.h"
#include "Input/Zenith_KeyCodes.h"

#include "Memory/Zenith_MemoryManagement_Disabled.h"
#include "imgui.h"
#include "Memory/Zenith_MemoryManagement_Enabled.h"

namespace
{
	// The simulated keys the bridge forwards. Editor-driving tests need text
	// entry + the common shortcuts; extend as tests require.
	struct BridgedKey
	{
		int m_iZenithKey;	// GLFW-compatible Zenith_KeyCode value
		ImGuiKey m_eImGuiKey;
		char m_cCharacter;	// 0 = not a printable character
	};

	constexpr BridgedKey kaxBRIDGED_KEYS[] =
	{
		{ 32,  ImGuiKey_Space,        ' '  },
		{ 45,  ImGuiKey_Minus,        '-'  },
		{ 46,  ImGuiKey_Period,       '.'  },
		{ 48,  ImGuiKey_0,            '0'  }, { 49, ImGuiKey_1, '1' }, { 50, ImGuiKey_2, '2' },
		{ 51,  ImGuiKey_3,            '3'  }, { 52, ImGuiKey_4, '4' }, { 53, ImGuiKey_5, '5' },
		{ 54,  ImGuiKey_6,            '6'  }, { 55, ImGuiKey_7, '7' }, { 56, ImGuiKey_8, '8' },
		{ 57,  ImGuiKey_9,            '9'  },
		{ 65,  ImGuiKey_A,            'a'  }, { 66, ImGuiKey_B, 'b' }, { 67, ImGuiKey_C, 'c' },
		{ 68,  ImGuiKey_D,            'd'  }, { 69, ImGuiKey_E, 'e' }, { 70, ImGuiKey_F, 'f' },
		{ 71,  ImGuiKey_G,            'g'  }, { 72, ImGuiKey_H, 'h' }, { 73, ImGuiKey_I, 'i' },
		{ 74,  ImGuiKey_J,            'j'  }, { 75, ImGuiKey_K, 'k' }, { 76, ImGuiKey_L, 'l' },
		{ 77,  ImGuiKey_M,            'm'  }, { 78, ImGuiKey_N, 'n' }, { 79, ImGuiKey_O, 'o' },
		{ 80,  ImGuiKey_P,            'p'  }, { 81, ImGuiKey_Q, 'q' }, { 82, ImGuiKey_R, 'r' },
		{ 83,  ImGuiKey_S,            's'  }, { 84, ImGuiKey_T, 't' }, { 85, ImGuiKey_U, 'u' },
		{ 86,  ImGuiKey_V,            'v'  }, { 87, ImGuiKey_W, 'w' }, { 88, ImGuiKey_X, 'x' },
		{ 89,  ImGuiKey_Y,            'y'  }, { 90, ImGuiKey_Z, 'z' },
		{ 256, ImGuiKey_Escape,       0    },
		{ 257, ImGuiKey_Enter,        0    },
		{ 258, ImGuiKey_Tab,          0    },
		{ 259, ImGuiKey_Backspace,    0    },
		{ 261, ImGuiKey_Delete,       0    },
		{ 340, ImGuiKey_LeftShift,    0    },
		{ 341, ImGuiKey_LeftCtrl,     0    },
	};
	constexpr u_int uBRIDGED_KEY_COUNT = sizeof(kaxBRIDGED_KEYS) / sizeof(kaxBRIDGED_KEYS[0]);
}

void Zenith_ImGuiInputBridge::PumpSimulatedInput()
{
	if (!Zenith_InputSimulator::IsEnabled() || ImGui::GetCurrentContext() == nullptr)
	{
		return;
	}

	ImGuiIO& xIO = ImGui::GetIO();

	// Mouse position - absolute, wins over any stale real-mouse position.
	Zenith_Maths::Vector2_64 xMousePos;
	Zenith_InputSimulator::GetMousePositionSimulated(xMousePos);
	xIO.AddMousePosEvent(static_cast<float>(xMousePos.x), static_cast<float>(xMousePos.y));

	// Mouse buttons - edge-triggered events from state diffs.
	static bool ls_abPreviousButtons[3] = { false, false, false };
	for (int iButton = 0; iButton < 3; ++iButton)
	{
		const bool bDown = Zenith_InputSimulator::IsKeyDownSimulated(static_cast<Zenith_KeyCode>(iButton));
		if (bDown != ls_abPreviousButtons[iButton])
		{
			xIO.AddMouseButtonEvent(iButton, bDown);
			ls_abPreviousButtons[iButton] = bDown;
		}
	}

	// Mouse wheel.
	const float fWheel = Zenith_InputSimulator::GetMouseWheelDeltaSimulated();
	if (fWheel != 0.0f)
	{
		xIO.AddMouseWheelEvent(0.0f, fWheel);
	}

	// Keys - edge events + printable characters on press.
	static bool ls_abPreviousKeys[uBRIDGED_KEY_COUNT] = {};
	for (u_int u = 0; u < uBRIDGED_KEY_COUNT; ++u)
	{
		const BridgedKey& xKey = kaxBRIDGED_KEYS[u];
		const bool bDown = Zenith_InputSimulator::IsKeyDownSimulated(static_cast<Zenith_KeyCode>(xKey.m_iZenithKey));
		if (bDown == ls_abPreviousKeys[u])
		{
			continue;
		}
		ls_abPreviousKeys[u] = bDown;

		// Modifier mods travel alongside their key events.
		if (xKey.m_eImGuiKey == ImGuiKey_LeftCtrl)
		{
			xIO.AddKeyEvent(ImGuiMod_Ctrl, bDown);
		}
		else if (xKey.m_eImGuiKey == ImGuiKey_LeftShift)
		{
			xIO.AddKeyEvent(ImGuiMod_Shift, bDown);
		}
		xIO.AddKeyEvent(xKey.m_eImGuiKey, bDown);

		if (bDown && xKey.m_cCharacter != 0)
		{
			xIO.AddInputCharacter(static_cast<unsigned int>(xKey.m_cCharacter));
		}
	}
}

namespace
{
	void PumpSimulatedInputThunk()
	{
		Zenith_ImGuiInputBridge::PumpSimulatedInput();
	}
}

// The Vulkan backend invokes this between the GLFW backend NewFrame and
// ImGui::NewFrame (see Core/Zenith_ImGuiBridgeHook.h).
Zenith_ImGuiSimulatedInputFn g_pfnZenithImGuiSimulatedInput = &PumpSimulatedInputThunk;

#endif // ZENITH_TOOLS && ZENITH_INPUT_SIMULATOR
