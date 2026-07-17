#pragma once

class Zenith_Entity;

// The one full-canvas fade-overlay driver, shared by every ZM transition owner
// (ZM_GameStateManager's WarpFade and, from S5 item 3, ZM_BattleTransition's
// BattleFade). It REASSERTS the game-owned invariants on every call, because
// UIOverlay deserialization restores the generic centred modal-overlay default and
// the overlay's text clip must cover the canvas to hide lower-order UIText while
// the screen is black. Returns false when the named Overlay is absent from xEntity.
namespace ZM_FadeOverlay
{
	bool Apply(Zenith_Entity& xEntity, const char* szElementName, float fAlpha);
}
