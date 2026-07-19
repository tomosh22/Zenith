#include "Zenith.h"

#include "Zenithmon/Source/UI/ZM_UI_DialogueBox.h"

#include "EntityComponent/Components/Zenith_UIComponent.h"
#include "UI/Zenith_UIElement.h"
#include "UI/Zenith_UIRect.h"
#include "UI/Zenith_UIText.h"
#include "ZenithECS/Zenith_Entity.h"
#include "ZenithECS/Zenith_Scene.h"
#include "Zenithmon/Source/UI/ZM_UI_BattleHUD.h"   // ComputeVisibleGlyphCount (the ONE reveal formula)

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
	m_uLineCount = 0u;
	m_uCurrentLine = 0u;
	m_fLineElapsedSeconds = 0.0f;
	m_bRevealInstant = false;
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
		Reset();
		return ZM_DIALOGUE_ADVANCE_CLOSED;
	}
	return ZM_DIALOGUE_ADVANCE_NEXT_LINE;
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

	const bool bActive = IsActive();
	if (pxPanel != nullptr)
	{
		pxPanel->SetVisible(bActive);
	}
	if (pxText == nullptr)
	{
		return;
	}
	pxText->SetVisible(bActive);
	if (!bActive)
	{
		return;
	}

	// SetText rebuilds the word wrap, so only write it when the line actually changed.
	if (pxText->GetText() != GetCurrentLine())
	{
		pxText->SetText(GetCurrentLine());
	}
	// Reveal against the engine's POST-WRAP glyph total (the authoritative on-screen
	// count), not the headless proxy.
	pxText->SetVisibleGlyphCount(ZM_UI_BattleHUD::ComputeVisibleGlyphCount(
		pxText->GetTotalGlyphCount(), m_fLineElapsedSeconds, m_bRevealInstant));
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
}
