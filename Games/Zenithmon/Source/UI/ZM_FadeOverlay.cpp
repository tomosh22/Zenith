#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_FadeOverlay.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"

namespace
{
	constexpr float fFADE_TRANSPARENT = 0.0f;
}

bool ZM_FadeOverlay::Apply(
	Zenith_Entity& xEntity,
	const char* szElementName,
	float fAlpha)
{
	Zenith_UIComponent* pxUI = xEntity.IsValid()
		? xEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	Zenith_UI::Zenith_UIElement* pxElement = pxUI != nullptr
		? pxUI->FindElement(szElementName)
		: nullptr;
	if (pxElement == nullptr
		|| pxElement->GetType() != Zenith_UI::UIElementType::Overlay)
	{
		return false;
	}

	Zenith_UI::Zenith_UIOverlay* pxFadeOverlay =
		static_cast<Zenith_UI::Zenith_UIOverlay*>(pxElement);
	pxFadeOverlay->SetContentSize(0.0f, 0.0f);
	pxFadeOverlay->SetAnchorAndPivot(Zenith_UI::AnchorPreset::StretchAll);
	pxFadeOverlay->SetGroupAlpha(fAlpha);
	if (fAlpha > fFADE_TRANSPARENT)
	{
		pxFadeOverlay->Show();
	}
	else
	{
		pxFadeOverlay->Hide();
	}
	return true;
}
