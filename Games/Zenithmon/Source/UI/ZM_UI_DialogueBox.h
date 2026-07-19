#pragma once

#include <string>

class Zenith_Entity;
class Zenith_UIComponent;

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
// The headless MODEL (queue + clock + advance + the SC8 yes/no choice) names no
// scene / graphics type and is unit-tested verbatim; Present() is the only part
// that touches the UI.
// ============================================================================

// The outcome of a Confirm() press -- the frozen contract the unit tests and the
// menu stack's input routing both read. APPEND-ONLY (the four SC2 values keep their
// numbering; SC8's wait is the fifth).
enum ZM_DIALOGUE_ADVANCE : u_int
{
	ZM_DIALOGUE_ADVANCE_IGNORED = 0u,       // not active -> nothing happened
	ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL,   // first press finished the typewriter
	ZM_DIALOGUE_ADVANCE_NEXT_LINE,          // moved on to the next queued line
	ZM_DIALOGUE_ADVANCE_CLOSED,             // consumed the last line -> inactive
	ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE,    // last line read + a choice is armed -> the box holds
};

// The two-way answer a PROMPT dialogue carries (S6 item 2 SC8). NONE is both "no
// choice has been armed" and "the armed choice has not been answered yet" -- the two
// are told apart by IsChoiceArmed() / IsAwaitingChoice().
enum ZM_DIALOGUE_CHOICE : u_int
{
	ZM_DIALOGUE_CHOICE_NONE = 0u,
	ZM_DIALOGUE_CHOICE_YES,
	ZM_DIALOGUE_CHOICE_NO,
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
	// The SC8 yes/no buttons. They live on the SAME panel as the line (a prompt the
	// player answers must not float over the world), authored hidden + non-focusable and
	// raised only while a choice is awaiting an answer.
	static constexpr const char* szYES_NAME   = "Menu_DialogueYes";
	static constexpr const char* szNO_NAME    = "Menu_DialogueNo";

	// ---- Choice-button geometry, shared with the ONE placement site
	//      (ZM_ConfigureMenuRoot in Zenithmon.cpp) so the two can never drift. ----
	// Offsets are BottomCenter-anchored, matching the panel + text: +Y is DOWN, so a
	// NEGATIVE Y is that many pixels ABOVE the bottom of the screen. The panel spans
	// 32..192 and the text box 56..176; the buttons sit at 40..76, inside the panel and
	// below where a one/two-line prompt actually renders.
	static constexpr float fCHOICE_WIDTH   = 140.0f;
	static constexpr float fCHOICE_HEIGHT  = 36.0f;
	static constexpr float fCHOICE_Y       = -40.0f;
	static constexpr float fCHOICE_YES_X   = 200.0f;
	static constexpr float fCHOICE_NO_X    = 360.0f;

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
	// Consume a confirm press; see ZM_DIALOGUE_ADVANCE for the five outcomes.
	ZM_DIALOGUE_ADVANCE Confirm();

	// ---- The yes/no CHOICE (S6 item 2 SC8) ----
	//
	// A prompt is a normal line queue PLUS an armed choice: the player reads the lines
	// exactly as they read any conversation, and the box then HOLDS instead of closing
	// until the question is answered. The answer is given BY THE FOCUSED ELEMENT'S NAME
	// (ResolveChoice) -- never a SetOnClick userdata, which dangles when the ECS pool
	// relocates the owning component -- or by cancelling, which resolves NO.

	// Arm the yes/no prompt with the two button labels. Rejects a null / empty label and
	// a SECOND arm while one is already armed (no mutation on reject). Arming is
	// independent of the queue: a caller arms it alongside QueueLines, in either order.
	bool ArmChoice(const char* szYesLabel, const char* szNoLabel);
	// A choice has been armed and not yet answered.
	bool IsChoiceArmed() const { return m_bChoiceArmed; }
	// ...and every queued line has been read, so the box is waiting on the answer.
	bool IsAwaitingChoice() const;
	// The ANSWER. NONE until one is actually given, and it SURVIVES the resolve (which
	// otherwise resets the box) so the caller can act on it after the fact.
	ZM_DIALOGUE_CHOICE GetChoice() const { return m_eChoiceAnswer; }
	// Answer the prompt from the canvas's focused element NAME (szYES_NAME / szNO_NAME).
	// A null or unknown name leaves the box awaiting and returns NONE. On a match the
	// answer is stored, the box is reset (queue, clock, arm and labels) and left
	// INACTIVE, and the answer is returned.
	ZM_DIALOGUE_CHOICE ResolveChoice(const char* szFocusedElementName);
	// Cancel while awaiting == NO. The LINES stay modal (cancel can never skip them), but
	// a prompt the player can neither answer nor escape would be a dead end. Inert (NONE,
	// nothing changed) when no choice is awaiting.
	ZM_DIALOGUE_CHOICE CancelChoice();
	const std::string& GetYesLabel() const { return m_strYesLabel; }
	const std::string& GetNoLabel() const { return m_strNoLabel; }

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

	// Show / refresh the panel + text for the current line, and the two choice buttons
	// while a choice is awaiting an answer. A missing UI component or element is skipped
	// silently -- presentation never crashes the dialogue.
	void Present(Zenith_Entity& xRootEntity);
	// Hide all four elements. Does NOT touch the queue (Reset is the caller's job).
	void Hide(Zenith_Entity& xRootEntity);

private:
	// Show / hide + label the two choice buttons and park the focus on one of them
	// (default YES) while bAwaiting. Split out of Present so the (long) element pass
	// stays readable, and called BEFORE the text element's null-guard return so a
	// missing text element can never cost the prompt its buttons.
	void PresentChoiceButtons(Zenith_UIComponent& xUI, bool bAwaiting);
	// The line the BOX is showing: the current one while active, and -- while a choice
	// waits -- the last one read (the question being answered must stay on screen).
	// GetCurrentLine() deliberately keeps its narrower SC2 contract.
	const std::string& DisplayLine() const;
	// Reset everything EXCEPT the answer, which is then stored back.
	void StoreChoiceAnswer(ZM_DIALOGUE_CHOICE eAnswer);

	std::string m_astrLines[uMAX_QUEUED_LINES];
	std::string m_strYesLabel;
	std::string m_strNoLabel;
	u_int       m_uLineCount = 0u;
	u_int       m_uCurrentLine = 0u;
	float       m_fLineElapsedSeconds = 0.0f;
	bool        m_bRevealInstant = false;
	bool        m_bChoiceArmed = false;
	ZM_DIALOGUE_CHOICE m_eChoiceAnswer = ZM_DIALOGUE_CHOICE_NONE;
};
