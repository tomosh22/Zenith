#pragma once

#include "Zenithmon/Source/Data/ZM_SpeciesData.h"   // ZM_SPECIES_ID (by value in the static formatters)

#include <string>

class Zenith_Entity;
class ZM_BattleDirectorCore;
struct ZM_BattleEvent;

// ============================================================================
// ZM_UI_BattleHUD (S5 item 4 SC4) -- the first VISIBLE battle UI: a text log
// revealed by the E3 typewriter plus two HP panels (species / level / HP text +
// an HP bar). It is a small NON-ECS presentation class OWNED BY VALUE by
// ZM_BattleDirector (seam Option B): NO order, NO component registration, NO
// editor mirror. Its Zenith_UIComponent + elements are authored at bake time onto
// the existing BattleDirector entity (mirroring how BattleFade is authored on the
// ZM_BattleTransitionRoot entity), and the director drives it Setup / Update /
// Hide. Still AI-vs-AI, NO interaction (that lands SC5).
//
// The instance holds only the latched log line + its typewriter clock, so it is
// trivially movable (the director move-constructs via the ECS component pool).
// The static formatters are pure (no scene / graphics / core) and unit-tested.
// ============================================================================
class ZM_UI_BattleHUD
{
public:
	// --- Instance drive (called only by ZM_BattleDirector) ---

	// Reveal all five HUD elements on the director entity's Zenith_UIComponent and
	// seed the two HP panels from the core's opening state. A missing UI component
	// skips gracefully. Resets the latched log line + typewriter clock.
	void Setup(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore);
	// Re-resolve the elements each frame (never cache pointers across frames): latch
	// a new log line when the current event carries text and differs, advance the
	// typewriter reveal, and refresh both HP panels.
	void Update(Zenith_Entity& xDirectorEntity, const ZM_BattleDirectorCore& xCore, float fDeltaSeconds);
	// Hide all five HUD elements (so the end-fade never shows the HUD over black).
	void Hide(Zenith_Entity& xDirectorEntity);

	// --- PURE statics (no scene / graphics / core -- unit-tested) ---

	// The total event->log-line mapping. Defined for EVERY ZM_BATTLE_EVENT kind in
	// [0, ZM_BATTLE_EVENT_COUNT); framing events return "" (never spam the log); an
	// out-of-range value falls through the default to "". Never asserts / crashes.
	// The subject species for MOVE_USED / FAINT is the active species of the event's
	// side; enemy-side subjects are prefixed "Foe ".
	static std::string FormatBattleLogLine(const ZM_BattleEvent& xEvent,
	                                       ZM_SPECIES_ID ePlayerActiveSpecies,
	                                       ZM_SPECIES_ID eEnemyActiveSpecies);
	// "<SpeciesName>  Lv<level>  HP <cur>/<max>" -- contains the species name, the
	// level as decimal, and the "<cur>/<max>" substring.
	static std::string FormatHpPanel(ZM_SPECIES_ID eSpecies, u_int uLevel, u_int uCurHp, u_int uMaxHp);
	// cur/max clamped to [0,1]; 0 when max == 0 (avoids a divide-by-zero).
	static float       ComputeHpFraction(u_int uCurHp, u_int uMaxHp);
	// Glyphs revealed so far: the whole line when bInstant, else floor(elapsed *
	// fCHARS_PER_SEC) clamped to [0, total]. Negative / huge elapsed clamp to 0 / total.
	static int         ComputeVisibleGlyphCount(int iTotalGlyphs, float fLineElapsedSeconds, bool bInstant);

private:
	std::string m_strShownLine;
	float       m_fLineElapsedSeconds = 0.0f;
};
