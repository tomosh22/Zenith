#pragma once

#include <string>

class Zenith_Entity;

// ============================================================================
// ZM_UI_DialogueBox (S6 item 2 SC2) -- the overworld dialogue box: a queue of
// lines revealed one at a time by the E3 typewriter, advanced by confirm.
//
// It is a small NON-ECS presentation class OWNED BY VALUE by ZM_UI_MenuStack
// (the same seam ZM_BattleDirector uses for ZM_UI_BattleHUD): NO order, NO
// component registration, NO editor mirror. Its panel + text elements are
// authored at bake time onto the persistent ZM_MenuRoot entity's
// Zenith_UIComponent (ZM_ConfigureMenuRoot), and the menu stack drives it
// Tick / Confirm / Present / Hide.
//
// Members are only std::string + PODs (no Zenith_Vector, no owning pointers, no
// references). std::string's own move is noexcept, so the owning component's
// defaulted noexcept move stays well-formed when the ECS pool relocates it.
//
// The headless MODEL (queue + clock + advance) names no scene / graphics type
// and is unit-tested verbatim; Present() is the only part that touches the UI.
// ============================================================================

// The outcome of a Confirm() press -- the frozen contract the unit tests and the
// menu stack's input routing both read.
enum ZM_DIALOGUE_ADVANCE : u_int
{
	ZM_DIALOGUE_ADVANCE_IGNORED = 0u,       // not active -> nothing happened
	ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL,   // first press finished the typewriter
	ZM_DIALOGUE_ADVANCE_NEXT_LINE,          // moved on to the next queued line
	ZM_DIALOGUE_ADVANCE_CLOSED,             // consumed the last line -> inactive
};

class ZM_UI_DialogueBox
{
public:
	static constexpr u_int uMAX_QUEUED_LINES = 8u;

	// The authored element names -- the SINGLE SOURCE of the ZM_ConfigureMenuRoot
	// contract in Zenithmon.cpp (create/place these) and of this class's runtime
	// re-resolution (never cache element pointers -- the canvas may relocate them).
	static constexpr const char* szPANEL_NAME = "Menu_DialoguePanel";
	static constexpr const char* szTEXT_NAME  = "Menu_DialogueText";

	// ---- PURE headless model (no scene / graphics -- unit-tested directly) ----

	// Drop every queued line and the in-flight reveal state.
	void Reset();
	// Append one line. Rejects null / empty / a full queue (no mutation on reject).
	// Appending WHILE active is legal and never disturbs the in-flight reveal.
	bool QueueLine(const char* szLine);
	// Append a batch, ALL-OR-NOTHING: the queue is only touched when every entry is
	// accepted (non-null, non-empty, and the whole batch fits).
	bool QueueLines(const char* const* paszLines, u_int uCount);
	// Advance the current line's typewriter clock (no-op when inactive or dt <= 0).
	void Tick(float fDeltaSeconds);
	// Consume a confirm press; see ZM_DIALOGUE_ADVANCE for the four outcomes.
	ZM_DIALOGUE_ADVANCE Confirm();

	bool  IsActive() const;
	u_int GetQueuedLineCount() const { return m_uLineCount; }
	// Lines still to be read INCLUDING the current one; 0 when inactive.
	u_int GetRemainingLineCount() const;
	u_int GetCurrentLineIndex() const { return m_uCurrentLine; }
	// The line being revealed, or a shared empty string when inactive.
	const std::string& GetCurrentLine() const;
	// The HEADLESS proxy for the total glyph count: the raw line's character count.
	// It is CONSERVATIVE -- always >= the engine's post-wrap glyph total (wrapping
	// drops whitespace), so the model reports "reveal complete" no EARLIER than the
	// visual does, which is the safe direction for gating an advance. The real
	// post-wrap total (Zenith_UIText::GetTotalGlyphCount) is used only in Present().
	int   GetCurrentLineGlyphTotal() const;
	bool  IsRevealComplete() const;
	int   GetVisibleGlyphCount() const;

	// ---- Presentation (best-effort; re-resolves elements by NAME every frame) ----

	// Show / refresh the panel + text for the current line. A missing UI component or
	// element is skipped silently -- presentation never crashes the dialogue.
	void Present(Zenith_Entity& xRootEntity);
	// Hide both elements. Does NOT touch the queue (Reset is the caller's job).
	void Hide(Zenith_Entity& xRootEntity);

private:
	std::string m_astrLines[uMAX_QUEUED_LINES];
	u_int       m_uLineCount = 0u;
	u_int       m_uCurrentLine = 0u;
	float       m_fLineElapsedSeconds = 0.0f;
	bool        m_bRevealInstant = false;
};
