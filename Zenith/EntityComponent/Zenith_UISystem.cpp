#include "Zenith.h"
#include "EntityComponent/Zenith_UISystem.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "Input/Zenith_InputActions.h"
#include "ZenithECS/Zenith_SceneSystem.h"

void Zenith_UISystem::Initialise(Zenith_SceneSystem& xScenes)
{
	m_pxScenes = &xScenes;
}

void Zenith_UISystem::UpdateInput(Zenith_InputActions& xActions, Zenith_Pointers& xPointers, float fDt)
{
	// Same deferral guard the visual pass uses: a click callback that queues a
	// LoadScene must not tear the scene down underneath this walk. The guard
	// drains it when the scope closes. It brackets BOTH walks below, because a
	// focus-navigation confirm fires exactly the same callbacks a click does.
	Zenith_SceneUpdateDeferralGuard xInputGuard;

	Zenith_Vector<Zenith_UIComponent*> xUIComponents;
	xUIComponents.Clear();
	m_pxScenes->QueryAllScenes<Zenith_UIComponent>().ForEach([&xUIComponents](Zenith_EntityID, Zenith_UIComponent& xComp) { xUIComponents.PushBack(&xComp); });

	// 10b: close the engine-reserved UI actions FIRST. They are claim-
	// independent by contract, so they must be readable before the capture walk
	// below can take a claim that would otherwise be able to suppress them.
	xActions.FinalizeReservedUI();

	// ...and let every visible canvas consume them. Focus navigation lives in
	// the INPUT phase, not the visual pass: a frame that submits no render work
	// still has to answer an arrow key.
	for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_UIComponent* pxComponent = xIt.GetData();
		if (pxComponent->IsVisible())
		{
			pxComponent->GetCanvas().UpdateFocusNavigation(xActions);
		}
	}

	// 10c: the standard-control capture walk. Claims are taken here.
	for (Zenith_Vector<Zenith_UIComponent*>::Iterator xIt(xUIComponents); !xIt.Done(); xIt.Next())
	{
		Zenith_UIComponent* pxComponent = xIt.GetData();
		if (pxComponent->IsVisible())
		{
			pxComponent->GetCanvas().UpdatePointerInput(xPointers, fDt);
		}
	}

	// 10e: everything else closes AFTER the capture walk, so a pointer-sourced
	// binding sees the final claim state. (The WP3a virtual-control publish step
	// belongs between the walk above and this call.)
	xActions.FinalizeGameplay();
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
