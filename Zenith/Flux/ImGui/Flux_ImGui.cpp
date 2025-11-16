#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Flux/ImGui/Flux_ImGui.h"
#include "Flux/Flux.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux_Graphics.h"

static Flux_CommandList s_xImGuiCommandList("ImGui");

void Flux_ImGui::SubmitRenderTask()
{
	s_xImGuiCommandList.Reset(false);
	s_xImGuiCommandList.AddCommand<Flux_CommandRenderImGui>();
	
	Flux::SubmitCommandList(&s_xImGuiCommandList, Flux_Graphics::s_xFinalRenderTarget_NoDepth, RENDER_ORDER_IMGUI);
}

#endif // ZENITH_TOOLS
