#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIButton.h"
#include "UI/Zenith_UICanvas.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"   // ComputeVisibleGlyphCount (the ONE reveal formula)

#include <cstring>

// ============================================================================
// ZM_UI_DialogueBox (S6 item 2 SC2). The queue + typewriter model, plus the
// best-effort presentation onto the persistent ZM_MenuRoot canvas. The reveal
// math (and its 45 glyphs/sec rate) is NOT duplicated here -- both the headless
// proxy and the on-screen reveal call ZM_UI_BattleHUD::ComputeVisibleGlyphCount.
// ============================================================================

namespace
{
	// Returned by GetCurrentLine() when the box is inactive: a stable empty string,
	// never a dangling temporary and never an out-of-range queue element.
	const std::string& ZM_EmptyDialogueLine()
	{
		static const std::string strEmpty;
		return strEmpty;
	}
}

// ---- PURE headless model ----------------------------------------------------

void ZM_UI_DialogueBox::Reset()
{
	for (u_int i = 0u; i < uMAX_QUEUED_LINES; ++i)
	{
		m_astrLines[i].clear();
	}
	m_strYesLabel.clear();
	m_strNoLabel.clear();
	m_uLineCount = 0u;
	m_uCurrentLine = 0u;
	m_fLineElapsedSeconds = 0.0f;
	m_bRevealInstant = false;
	m_bChoiceArmed = false;
	m_eChoiceAnswer = ZM_DIALOGUE_CHOICE_NONE;
}

bool ZM_UI_DialogueBox::QueueLine(const char* szLine)
{
	if (szLine == nullptr || szLine[0] == '\0' || m_uLineCount == uMAX_QUEUED_LINES)
	{
		return false;
	}
	m_astrLines[m_uLineCount] = szLine;
	++m_uLineCount;
	return true;
}

bool ZM_UI_DialogueBox::QueueLines(const char* const* paszLines, u_int uCount)
{
	// Validate the WHOLE batch first -- a half-queued conversation would show lines
	// the caller never got told were accepted.
	if (paszLines == nullptr || uCount == 0u || uCount > (uMAX_QUEUED_LINES - m_uLineCount))
	{
		return false;
	}
	for (u_int i = 0u; i < uCount; ++i)
	{
		if (paszLines[i] == nullptr || paszLines[i][0] == '\0')
		{
			return false;
		}
	}
	for (u_int i = 0u; i < uCount; ++i)
	{
		m_astrLines[m_uLineCount] = paszLines[i];
		++m_uLineCount;
	}
	return true;
}

void ZM_UI_DialogueBox::Tick(float fDeltaSeconds)
{
	if (!IsActive() || fDeltaSeconds <= 0.0f)
	{
		return;
	}
	m_fLineElapsedSeconds += fDeltaSeconds;
}

ZM_DIALOGUE_ADVANCE ZM_UI_DialogueBox::Confirm()
{
	if (IsAwaitingChoice())
	{
		// The lines are all read and the prompt is up. A bare confirm must NOT pick for
		// the player: the answer comes from ResolveChoice (by focused element NAME) or
		// CancelChoice, so this just re-reports the wait.
		return ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE;
	}
	if (!IsActive())
	{
		return ZM_DIALOGUE_ADVANCE_IGNORED;
	}
	if (!IsRevealComplete())
	{
		// First press snaps the typewriter to the end rather than advancing -- the
		// player never skips a line they have not seen.
		m_bRevealInstant = true;
		return ZM_DIALOGUE_ADVANCE_COMPLETED_REVEAL;
	}

	++m_uCurrentLine;
	m_fLineElapsedSeconds = 0.0f;
	m_bRevealInstant = false;
	if (m_uCurrentLine >= m_uLineCount)
	{
		if (m_bChoiceArmed && m_eChoiceAnswer == ZM_DIALOGUE_CHOICE_NONE)
		{
			// The ONE behavioural change SC8 makes to the SC2 advance: with a choice armed
			// the last line does not close the box, it opens the question. The queue is
			// deliberately NOT reset -- IsAwaitingChoice reads m_uCurrentLine against it, and
			// Present keeps showing the line being answered.
			return ZM_DIALOGUE_ADVANCE_AWAITING_CHOICE;
		}
		Reset();
		return ZM_DIALOGUE_ADVANCE_CLOSED;
	}
	return ZM_DIALOGUE_ADVANCE_NEXT_LINE;
}

// ---- The yes/no choice ------------------------------------------------------

bool ZM_UI_DialogueBox::ArmChoice(const char* szYesLabel, const char* szNoLabel)
{
	// A blank label would draw an unreadable button, and a SECOND arm would silently
	// replace the question the player is already looking at.
	if (szYesLabel == nullptr || szYesLabel[0] == '\0'
		|| szNoLabel == nullptr || szNoLabel[0] == '\0'
		|| m_bChoiceArmed)
	{
		return false;
	}
	m_strYesLabel = szYesLabel;
	m_strNoLabel = szNoLabel;
	m_bChoiceArmed = true;
	// A fresh prompt starts unanswered even when a PREVIOUS one left its answer behind.
	m_eChoiceAnswer = ZM_DIALOGUE_CHOICE_NONE;
	return true;
}

bool ZM_UI_DialogueBox::IsAwaitingChoice() const
{
	// "Every line read" is m_uCurrentLine having walked PAST the last one -- exactly the
	// state Confirm leaves behind when a choice is armed (it returns without resetting
	// there). A box with nothing queued has read nothing, so an armed choice on an empty
	// queue is not awaiting anything yet: the caller is still assembling the prompt.
	return m_bChoiceArmed
		&& m_eChoiceAnswer == ZM_DIALOGUE_CHOICE_NONE
		&& m_uLineCount > 0u
		&& m_uCurrentLine >= m_uLineCount;
}

ZM_DIALOGUE_CHOICE ZM_UI_DialogueBox::ResolveChoice(const char* szFocusedElementName)
{
	if (!IsAwaitingChoice() || szFocusedElementName == nullptr)
	{
		return ZM_DIALOGUE_CHOICE_NONE;
	}
	// DISPATCH BY THE FOCUSED ELEMENT'S NAME -- never SetOnClick(this): a `this` userdata
	// dangles when the ECS pool relocates the component that owns this box.
	ZM_DIALOGUE_CHOICE eAnswer = ZM_DIALOGUE_CHOICE_NONE;
	if (std::strcmp(szFocusedElementName, szYES_NAME) == 0)
	{
		eAnswer = ZM_DIALOGUE_CHOICE_YES;
	}
	else if (std::strcmp(szFocusedElementName, szNO_NAME) == 0)
	{
		eAnswer = ZM_DIALOGUE_CHOICE_NO;
	}
	if (eAnswer == ZM_DIALOGUE_CHOICE_NONE)
	{
		return ZM_DIALOGUE_CHOICE_NONE;   // a foreign name answers nothing; the box keeps waiting
	}
	StoreChoiceAnswer(eAnswer);
	return eAnswer;
}

ZM_DIALOGUE_CHOICE ZM_UI_DialogueBox::CancelChoice()
{
	if (!IsAwaitingChoice())
	{
		return ZM_DIALOGUE_CHOICE_NONE;
	}
	StoreChoiceAnswer(ZM_DIALOGUE_CHOICE_NO);
	return ZM_DIALOGUE_CHOICE_NO;
}

void ZM_UI_DialogueBox::StoreChoiceAnswer(ZM_DIALOGUE_CHOICE eAnswer)
{
	// Reset clears the answer along with everything else, so it is written back AFTER --
	// the caller acts on GetChoice() once the box has already gone inactive.
	Reset();
	m_eChoiceAnswer = eAnswer;
}

bool ZM_UI_DialogueBox::IsActive() const
{
	return m_uCurrentLine < m_uLineCount;
}

u_int ZM_UI_DialogueBox::GetRemainingLineCount() const
{
	return IsActive() ? (m_uLineCount - m_uCurrentLine) : 0u;
}

const std::string& ZM_UI_DialogueBox::GetCurrentLine() const
{
	return IsActive() ? m_astrLines[m_uCurrentLine] : ZM_EmptyDialogueLine();
}

const std::string& ZM_UI_DialogueBox::DisplayLine() const
{
	if (IsActive())
	{
		return m_astrLines[m_uCurrentLine];
	}
	if (IsAwaitingChoice())
	{
		// Awaiting implies m_uLineCount > 0, so the last read line is always in range.
		return m_astrLines[m_uLineCount - 1u];
	}
	return ZM_EmptyDialogueLine();
}

int ZM_UI_DialogueBox::GetCurrentLineGlyphTotal() const
{
	return static_cast<int>(GetCurrentLine().size());
}

int ZM_UI_DialogueBox::GetVisibleGlyphCount() const
{
	return ZM_UI_BattleHUD::ComputeVisibleGlyphCount(
		GetCurrentLineGlyphTotal(), m_fLineElapsedSeconds, m_bRevealInstant);
}

bool ZM_UI_DialogueBox::IsRevealComplete() const
{
	if (!IsActive())
	{
		return true;
	}
	return GetVisibleGlyphCount() >= GetCurrentLineGlyphTotal();
}

// ---- Presentation -----------------------------------------------------------

void ZM_UI_DialogueBox::Present(Zenith_Entity& xRootEntity)
{
	Zenith_UIComponent* pxUI = xRootEntity.IsValid()
		? xRootEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	if (pxUI == nullptr)
	{
		return;   // best-effort: a missing UI component never crashes the dialogue
	}

	// Re-resolve by NAME every frame (never cache across frames).
	Zenith_UI::Zenith_UIRect* pxPanel = pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szPANEL_NAME);
	Zenith_UI::Zenith_UIText* pxText  = pxUI->FindElement<Zenith_UI::Zenith_UIText>(szTEXT_NAME);

	// A prompt awaiting its answer is INACTIVE (the queue has been read to the end) but
	// must stay on screen -- the question the two buttons answer is the line itself.
	// With no choice ever armed IsAwaitingChoice() is always false, so this is the SC2
	// behaviour unchanged.
	const bool bAwaitingChoice = IsAwaitingChoice();
	const bool bShown = IsActive() || bAwaitingChoice;

	// The buttons FIRST: the text element is best-effort and may be missing, and a
	// prompt must not lose its Yes/No to that.
	PresentChoiceButtons(*pxUI, bAwaitingChoice);

	if (pxPanel != nullptr)
	{
		pxPanel->SetVisible(bShown);
	}
	if (pxText == nullptr)
	{
		return;
	}
	pxText->SetVisible(bShown);
	if (!bShown)
	{
		return;
	}

	// SetText rebuilds the word wrap, so only write it when the line actually changed.
	const std::string& strLine = DisplayLine();
	if (pxText->GetText() != strLine)
	{
		pxText->SetText(strLine);
	}
	// Reveal against the engine's POST-WRAP glyph total (the authoritative on-screen
	// count), not the headless proxy. While the prompt waits, the line has already been
	// read to the end and stays WHOLE: the advance that opened the wait zeroed the clock,
	// so re-deriving the reveal would blank the question mid-answer.
	pxText->SetVisibleGlyphCount(bAwaitingChoice
		? pxText->GetTotalGlyphCount()
		: ZM_UI_BattleHUD::ComputeVisibleGlyphCount(
			pxText->GetTotalGlyphCount(), m_fLineElapsedSeconds, m_bRevealInstant));
}

void ZM_UI_DialogueBox::PresentChoiceButtons(Zenith_UIComponent& xUI, bool bAwaiting)
{
	Zenith_UI::Zenith_UIButton* pxYes = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szYES_NAME);
	Zenith_UI::Zenith_UIButton* pxNo  = xUI.FindElement<Zenith_UI::Zenith_UIButton>(szNO_NAME);

	// Only write the visibility when it actually CHANGES: SetVisible notifies the parent,
	// and this runs every frame the dialogue is the top screen (the SC5 lesson). A hidden
	// button must ALSO be non-focusable -- the engine nav collects visible + focusable
	// elements, so leaving it focusable would let the arrows park on an invisible answer.
	if (pxYes != nullptr)
	{
		if (pxYes->IsVisible() != bAwaiting)
		{
			pxYes->SetVisible(bAwaiting);
		}
		pxYes->SetFocusable(bAwaiting);   // a plain assignment (no notify) -- no guard needed
		if (bAwaiting && pxYes->GetText() != m_strYesLabel)
		{
			pxYes->SetText(m_strYesLabel);
		}
	}
	if (pxNo != nullptr)
	{
		if (pxNo->IsVisible() != bAwaiting)
		{
			pxNo->SetVisible(bAwaiting);
		}
		pxNo->SetFocusable(bAwaiting);
		if (bAwaiting && pxNo->GetText() != m_strNoLabel)
		{
			pxNo->SetText(m_strNoLabel);
		}
	}
	if (!bAwaiting)
	{
		return;
	}

	// Ensure ONE of the two holds the focus (default YES) so the arrow keys have
	// somewhere to start. Tested BY NAME rather than by pointer: with both elements
	// missing, a null focus would otherwise compare equal to a null Yes and read as
	// "already focused". Re-parked only when NEITHER holds it, so the player's own
	// left/right walk between them is never undone.
	Zenith_UI::Zenith_UICanvas& xCanvas = xUI.GetCanvas();
	Zenith_UI::Zenith_UIElement* pxFocused = xCanvas.GetFocusedElement();
	const char* szFocusedName = (pxFocused != nullptr) ? pxFocused->GetName().c_str() : nullptr;
	const bool bChoiceFocused = szFocusedName != nullptr
		&& (std::strcmp(szFocusedName, szYES_NAME) == 0
			|| std::strcmp(szFocusedName, szNO_NAME) == 0);
	if (bChoiceFocused)
	{
		return;
	}
	if (pxYes != nullptr)
	{
		xCanvas.SetFocusedElement(pxYes);
	}
	else if (pxNo != nullptr)
	{
		xCanvas.SetFocusedElement(pxNo);
	}
}

void ZM_UI_DialogueBox::Hide(Zenith_Entity& xRootEntity)
{
	Zenith_UIComponent* pxUI = xRootEntity.IsValid()
		? xRootEntity.TryGetComponent<Zenith_UIComponent>()
		: nullptr;
	if (pxUI == nullptr)
	{
		return;
	}
	if (Zenith_UI::Zenith_UIRect* pxPanel = pxUI->FindElement<Zenith_UI::Zenith_UIRect>(szPANEL_NAME))
	{
		pxPanel->SetVisible(false);
	}
	if (Zenith_UI::Zenith_UIText* pxText = pxUI->FindElement<Zenith_UI::Zenith_UIText>(szTEXT_NAME))
	{
		pxText->SetVisible(false);
	}
	// ...and the two choice buttons, hidden AND made unreachable by the nav.
	PresentChoiceButtons(*pxUI, /* bAwaiting */ false);
}
