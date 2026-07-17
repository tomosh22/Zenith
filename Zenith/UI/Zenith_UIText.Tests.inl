#include "Core/Zenith_TestFramework.h"

// ============================================================================
// Zenith_UIText typewriter-reveal unit tests (E3).
//
// Lock the visible-glyph-count reveal property added to Zenith_UIText:
//   * ClipToVisibleGlyphs (the pure, static reveal helper) over boundaries,
//     spaces, and newlines.
//   * The default (-1 = fully revealed) so existing widgets are byte-identical.
//   * The setter stores the RAW value (no clamp; ClipToVisibleGlyphs owns bounds).
//   * GetTotalGlyphCount mirrors the display-string length.
//   * v3 serialization round-trips the new field, AND a hand-built v2 blob still
//     loads and defaults to fully revealed (the additive back-compat contract).
//
// This file is textually included at the bottom of Zenith_UIText.cpp (under
// ZENITH_TESTING) AFTER `} // namespace Zenith_UI`, so the class is qualified as
// Zenith_UI::Zenith_UIText and Zenith_DataStream is already in scope.
//
// HEADLESS-SAFE — every test touches CPU-side string/serialization state ONLY.
// Render() (which needs a live canvas + GPU text queue) is NEVER called.
// ============================================================================

ZENITH_TEST(UIText, ClipToVisibleGlyphs_Boundaries)
{
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("hello", 0), std::string(""),
        "clip to 0 glyphs must reveal nothing");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("hello", 3), std::string("hel"),
        "clip to 3 glyphs must reveal the first three");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("hello", 5), std::string("hello"),
        "clip to exact length must reveal the whole string");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("hello", 99), std::string("hello"),
        "clip beyond length must reveal the whole string");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("hello", -1), std::string("hello"),
        "negative count must reveal the whole string");
}

ZENITH_TEST(UIText, ClipToVisibleGlyphs_CountsSpacesAndNewlines)
{
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("ab cd\nef", 4), std::string("ab c"),
        "space must count as one glyph (glyph 3)");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("ab\ncd", 3), std::string("ab\n"),
        "newline must count as one glyph (glyph 3)");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("ab\ncd", 4), std::string("ab\nc"),
        "the glyph after a newline must reveal correctly");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs("", 5), std::string(""),
        "clipping an empty string must stay empty");
}

ZENITH_TEST(UIText, DefaultVisibleGlyphCount_IsFullyRevealed)
{
    Zenith_UI::Zenith_UIText t("hello");
    ZENITH_ASSERT_EQ(t.GetVisibleGlyphCount(), -1,
        "a freshly-constructed UIText must default to -1 (fully revealed)");
    ZENITH_ASSERT_EQ(Zenith_UI::Zenith_UIText::ClipToVisibleGlyphs(t.GetText(), t.GetVisibleGlyphCount()),
        std::string("hello"),
        "the default reveal must show the whole string (no behaviour change)");
}

ZENITH_TEST(UIText, SetGetVisibleGlyphCount_RoundTripsRawValue)
{
    Zenith_UI::Zenith_UIText t("hello");

    t.SetVisibleGlyphCount(3);
    ZENITH_ASSERT_EQ(t.GetVisibleGlyphCount(), 3, "setter must store 3 verbatim");

    t.SetVisibleGlyphCount(-1);
    ZENITH_ASSERT_EQ(t.GetVisibleGlyphCount(), -1, "setter must store -1 verbatim");

    // No clamp in the setter — a value far beyond the string length round-trips raw
    // (ClipToVisibleGlyphs handles the out-of-range case at render time).
    t.SetVisibleGlyphCount(1000);
    ZENITH_ASSERT_EQ(t.GetVisibleGlyphCount(), 1000, "setter must not clamp large values");
}

ZENITH_TEST(UIText, GetTotalGlyphCount_MatchesTextLengthNoWrap)
{
    // Default m_fMaxWidth == 0 -> no wrapping -> display string == raw text.
    Zenith_UI::Zenith_UIText t("hello world");
    ZENITH_ASSERT_EQ(t.GetTotalGlyphCount(), 11,
        "total glyph count must equal the (unwrapped) display-string length");

    Zenith_UI::Zenith_UIText tEmpty;
    ZENITH_ASSERT_EQ(tEmpty.GetTotalGlyphCount(), 0,
        "a default-empty widget must report zero glyphs");
}

ZENITH_TEST(UIText, Serialization_RoundTripsVisibleGlyphCount)
{
    // Positive value survives a v3 write/read cycle.
    {
        Zenith_UI::Zenith_UIText src("abc");
        src.SetVisibleGlyphCount(4);

        Zenith_DataStream xStream;
        src.WriteToDataStream(xStream);
        xStream.SetCursor(0);

        Zenith_UI::Zenith_UIText dst;
        dst.ReadFromDataStream(xStream);
        ZENITH_ASSERT_EQ(dst.GetVisibleGlyphCount(), 4,
            "a positive visible-glyph-count must survive serialization");
    }

    // The sentinel -1 (fully revealed) also round-trips.
    {
        Zenith_UI::Zenith_UIText src("abc");
        src.SetVisibleGlyphCount(-1);

        Zenith_DataStream xStream;
        src.WriteToDataStream(xStream);
        xStream.SetCursor(0);

        Zenith_UI::Zenith_UIText dst;
        dst.ReadFromDataStream(xStream);
        ZENITH_ASSERT_EQ(dst.GetVisibleGlyphCount(), -1,
            "the -1 fully-revealed sentinel must survive serialization");
    }
}

ZENITH_TEST(UIText, Serialization_PreV3BlobDefaultsToRevealed)
{
    // Back-compat proof: a hand-built v2 blob (no glyph-count field) must still
    // load, defaulting the new property to -1 (fully revealed). We reproduce the
    // EXACT v2 wire layout: base-element write, then version 2, then the five
    // text-payload groups in WriteToDataStream order — but NO glyph count.
    Zenith_UI::Zenith_UIText src("legacy");

    Zenith_DataStream xStream;
    src.Zenith_UI::Zenith_UIElement::WriteToDataStream(xStream);   // base element blob
    xStream << (uint32_t)2u;                                       // UI_TEXT_VERSION == 2
    xStream << std::string("legacy");                              // m_strText
    xStream << 24.0f;                                              // m_fFontSize
    xStream << (uint32_t)0u;                                       // m_eAlignment (Left)
    xStream << (uint32_t)0u;                                       // m_eVerticalAlignment (Top)
    xStream << false;                                              // m_bShadowEnabled
    xStream << 0.0f; xStream << 0.0f; xStream << 0.0f; xStream << 0.5f;  // m_xShadowColor
    xStream << 2.0f; xStream << 2.0f;                              // m_xShadowOffset
    // (v2 stops here — no visible-glyph-count field)

    xStream.SetCursor(0);

    Zenith_UI::Zenith_UIText dst;
    dst.ReadFromDataStream(xStream);

    ZENITH_ASSERT_EQ(dst.GetVisibleGlyphCount(), -1,
        "a pre-v3 blob must default the visible-glyph-count to -1 (fully revealed)");
    ZENITH_ASSERT_EQ(dst.GetText(), std::string("legacy"),
        "the v2 payload must be consumed in the correct order (text reads back intact)");
}
