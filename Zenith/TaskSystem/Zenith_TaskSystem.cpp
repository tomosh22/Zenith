#include "Zenith.h"

#include "TaskSystem/Zenith_TaskSystem.h"
#include "TaskSystem/Zenith_TaskSystemImpl.h"

// Phase 3b: per-Engine TaskSystem state lives on Zenith_TaskSystemImpl,
// allocated by Zenith_Engine::Initialise(). The static facade methods
// stay as thin forwarders so the existing 26 call sites compile
// unchanged. Phase 9's static-API removal sweep deletes these
// forwarders once the codemod to g_xEngine.Tasks().X() is done.

void Zenith_TaskSystem::Inititalise()
{
	g_xEngine.Tasks().Initialise();
}

void Zenith_TaskSystem::Shutdown()
{
	g_xEngine.Tasks().Shutdown();
}

void Zenith_TaskSystem::SubmitTask(Zenith_Task* const pxTask)
{
	g_xEngine.Tasks().SubmitTask(pxTask);
}

void Zenith_TaskSystem::SubmitTaskArray(Zenith_TaskArray* const pxTaskArray)
{
	g_xEngine.Tasks().SubmitTaskArray(pxTaskArray);
}
