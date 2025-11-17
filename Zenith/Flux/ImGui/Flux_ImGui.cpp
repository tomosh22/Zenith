#include "Zenith.h"

#ifdef ZENITH_TOOLS

#include "Flux/ImGui/Flux_ImGui.h"
#include "Flux/Flux.h"
#include "Flux/Flux_CommandList.h"
#include "Flux/Flux_Graphics.h"

static Flux_CommandList s_xImGuiCommandList("ImGui");

void Flux_ImGui::SubmitRenderTask()
{
	// ImGui will be rendered separately in the swapchain EndFrame
	// Don't submit it through the normal command list pipeline to avoid render pass issues
}

#endif // ZENITH_TOOLS
