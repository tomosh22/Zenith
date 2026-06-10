#include "Zenith.h"
#include "EntityComponent/Zenith_UISystem.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_SceneSystem.h"

void Zenith_UISystem::Initialise(Zenith_SceneSystem& xScenes)
{
	m_pxScenes = &xScenes;
}

void Zenith_UISystem::Update(float fDt)
{
	// Collects from ALL loaded scenes (persistent entity UI + game scene UI).
	//
	// Two-pass: all Updates first (button clicks queue scene loads), then
	// the guard scope closes which drains any pending load, then we
	// re-collect components and Render. Without the split the click's
	// canvas would paint once more before the deferred load tears it
	// down — the "buttons persist for a frame" symptom.
	{
		Zenith_SceneUpdateDeferralGuard xUpdateGuard;
		Zenith_Vector<Zenith_UIComponent*> xUIComponents;
		xUIComponents.Clear();
		m_pxScenes->QueryAllScenes<Zenith_UIComponent>().ForEach([&xUIComponents](Zenith_EntityID, Zenith_UIComponent& xComp) { xUIComponents.PushBack(&xComp); });
		for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
		{
			xIt.GetData()->Update(fDt);
		}
	}
	// Guard destructor above drained any deferred LoadScene — scene may
	// have swapped here. Re-collect post-drain so we render the new
	// scene's UI, not the destroyed one.
	{
		Zenith_Vector<Zenith_UIComponent*> xUIComponents;
		xUIComponents.Clear();
		m_pxScenes->QueryAllScenes<Zenith_UIComponent>().ForEach([&xUIComponents](Zenith_EntityID, Zenith_UIComponent& xComp) { xUIComponents.PushBack(&xComp); });
		for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
		{
			xIt.GetData()->Render();
		}
	}
}
