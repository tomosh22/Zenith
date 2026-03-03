# Paws & Pins - Required Art Assets

Complete inventory of art assets required for Google Play / App Store release.

Each asset includes:
- **Specs**: Dimensions, format, technical requirements
- **Where used**: Location in the game
- **Priority**: Must-Have or Nice-to-Have
- **Procedural feasibility**: How much can be generated without a human artist
- **How to generate**: Specific techniques and tools
- **Effort estimate**: Relative effort (Low / Medium / High)

---

## 1. Store Listing Assets

### 1.1 App Icon

| Field | Value |
|-------|-------|
| **Specs** | 512x512 PNG (Google Play), 1024x1024 PNG (iOS) |
| **Where used** | Store listing, home screen, app drawer |
| **Priority** | Must-Have |
| **Procedural** | Requires artist but AI-assistable |
| **How to generate** | Use AI image generation (Midjourney/DALL-E) to draft cat + puzzle piece composition, then refine in vector editor (Figma/Illustrator). Must be distinctive at 48x48px launcher size. |
| **Effort** | Medium |

### 1.2 Feature Graphic

| Field | Value |
|-------|-------|
| **Specs** | 1024x500 PNG (Google Play) |
| **Where used** | Google Play store listing header |
| **Priority** | Must-Have |
| **Procedural** | Requires artist but AI-assistable |
| **How to generate** | Composite of gameplay screenshot + logo + cat illustrations. AI-generate base scene, overlay title text and UI mockup in image editor. |
| **Effort** | Medium |

### 1.3 Screenshots (Gameplay)

| Field | Value |
|-------|-------|
| **Specs** | Min 2, max 8. 16:9 or 9:16 aspect ratio, min 320px, max 3840px per side |
| **Where used** | Store listing gallery |
| **Priority** | Must-Have (min 2) |
| **Procedural** | Fully procedural |
| **How to generate** | Capture directly from game at target resolution. Add text overlays (feature callouts) via image editor or script. Use the engine's screenshot capability or OS-level capture. |
| **Effort** | Low |

### 1.4 Promo Video

| Field | Value |
|-------|-------|
| **Specs** | 30-120 seconds, landscape, 1920x1080 or higher |
| **Where used** | Store listing (optional but recommended) |
| **Priority** | Nice-to-Have |
| **Procedural** | Partially procedural + hand-tweaked |
| **How to generate** | Screen-record gameplay sessions via OBS/game capture. Edit in video editor (DaVinci Resolve free). Add music, text overlays, and transitions. |
| **Effort** | Medium |

---

## 2. In-Game UI Icons

### 2.1 Star Icon (Filled + Empty)

| Field | Value |
|-------|-------|
| **Specs** | 64x64 PNG with alpha, or SDF shader |
| **Where used** | Level complete overlay, level select grid, cat cafe cards |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | Render as SDF (engine has `Flux_SDFs` system). 5-pointed star via signed distance function. Gold fill (#FFD700) for earned, gray outline for empty. Alternatively, generate as texture via compute shader. |
| **Effort** | Low |

### 2.2 Coin Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | HUD coin counter, shop, rewards display |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF circle with embossed edge (inner shadow + highlight gradient). Gold color (#FFD700 to #B8860B). Can render via `Flux_SDFs` or bake as texture using compute shader with distance field math. |
| **Effort** | Low |

### 2.3 Heart / Life Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | HUD lives counter, lives refill dialog |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF heart shape (two circles + triangle union). Red fill (#E63C3C). Render via `Flux_SDFs` or bake to texture. |
| **Effort** | Low |

### 2.4 Undo Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | Gameplay HUD undo button |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF curved arrow (arc + arrowhead). Standard undo iconography. White on transparent, tinted at runtime. Render via SDF or vector-to-texture pipeline. |
| **Effort** | Low |

### 2.5 Hint (Lightbulb) Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | Gameplay HUD hint button |
| **Priority** | Must-Have |
| **Procedural** | Partially procedural + hand-tweaked |
| **How to generate** | SDF lightbulb shape (circle top + trapezoid base + rays). More complex shape may benefit from manual SVG creation then rasterize to texture. |
| **Effort** | Low-Medium |

### 2.6 Skip (Forward Arrow) Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | Skip level button (shown after 3 resets) |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF double right-pointing triangle (standard skip/forward icon). Render via SDF math. |
| **Effort** | Low |

### 2.7 Lock Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | Level select (locked levels), conditional shapes |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF padlock (rectangle body + rounded-rect shackle). Standard lock iconography. |
| **Effort** | Low |

### 2.8 Menu / Hamburger Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | In-game menu button |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | Three horizontal SDF rectangles, evenly spaced. Trivial to generate procedurally. |
| **Effort** | Low |

### 2.9 Back Arrow Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | Cat cafe back button, level select back |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF left-pointing chevron or arrow. Single shape, trivial math. |
| **Effort** | Low |

### 2.10 Settings Gear Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | Settings button (main menu) |
| **Priority** | Must-Have |
| **Procedural** | Partially procedural + hand-tweaked |
| **How to generate** | SDF gear (circle + periodic teeth via polar modulation). Achievable procedurally but teeth spacing needs tuning. |
| **Effort** | Low-Medium |

### 2.11 Sound On/Off Icons

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha, 2 variants |
| **Where used** | Settings toggle |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF speaker cone + sound waves (arcs). Off variant adds diagonal line. Standard audio iconography. |
| **Effort** | Low |

### 2.12 Cat Silhouette (Unknown Cat)

| Field | Value |
|-------|-------|
| **Specs** | 64x64 PNG with alpha |
| **Where used** | Cat cafe - uncollected cat placeholder |
| **Priority** | Must-Have |
| **Procedural** | Partially procedural + hand-tweaked |
| **How to generate** | SDF cat head outline (two triangle ears + circle head). Dark gray silhouette with question mark overlay. Could also use a simple SVG rasterized to texture. |
| **Effort** | Low-Medium |

### 2.13 Reset Icon

| Field | Value |
|-------|-------|
| **Specs** | 48x48 PNG with alpha |
| **Where used** | Gameplay HUD reset button |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF circular arrow (arc + arrowhead, similar to undo but full circle). Standard refresh iconography. |
| **Effort** | Low |

---

## 3. Gameplay Visual Assets

### 3.1 Cat Mesh (3D Model)

| Field | Value |
|-------|-------|
| **Specs** | Low-poly .zmesh, ~200-500 triangles, with UV mapping |
| **Where used** | Cat entities on puzzle grid, cat cafe display |
| **Priority** | Must-Have (upgrade from sphere placeholder) |
| **Procedural** | Partially procedural + hand-tweaked |
| **How to generate** | Option A: Procedural mesh generation in code (sphere + cone ears + cylinder tail). Option B: Model in Blender (10 min for simple cat shape), export via Assimp pipeline to .zmesh. Option C: AI-assisted (Meshy.ai, Tripo3D) then clean up in Blender. Needs 5 color material variants (Red, Green, Blue, Yellow, Purple). |
| **Effort** | Medium |

### 3.2 Cat Face Textures (5 colors)

| Field | Value |
|-------|-------|
| **Specs** | 256x256 .ztex per color variant, RGBA8 |
| **Where used** | Applied to cat mesh face UV region |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | Compute shader or offline script: draw two oval eyes (white + black pupil), small triangle nose, whisker lines, mouth curve. Color-tint the base per variant. Can be generated entirely in code using 2D SDF primitives rendered to texture. |
| **Effort** | Low-Medium |

### 3.3 Floor Tile Texture

| Field | Value |
|-------|-------|
| **Specs** | 128x128 .ztex, tileable, RGBA8 |
| **Where used** | Floor cell material diffuse map |
| **Priority** | Nice-to-Have (solid color works) |
| **Procedural** | Fully procedural |
| **How to generate** | Compute shader: subtle grid pattern, slight color variation noise, thin border lines. Tileable by construction. Apply as diffuse on floor material. Could also use a simple checkerboard or cross-hatch pattern. |
| **Effort** | Low |

### 3.4 Blocker Texture

| Field | Value |
|-------|-------|
| **Specs** | 128x128 .ztex, tileable, RGBA8 |
| **Where used** | Static blocker shape material |
| **Priority** | Nice-to-Have (solid dark color works) |
| **Procedural** | Fully procedural |
| **How to generate** | Compute shader: dark cross-hatch or diagonal stripe pattern to visually distinguish from floor. Darker tones (#503C1E base). |
| **Effort** | Low |

### 3.5 Shape Material Variants (5 colors + blocker)

| Field | Value |
|-------|-------|
| **Specs** | 6 .zmat files with PBR properties |
| **Where used** | Draggable shape entities (per color), static blockers |
| **Priority** | Must-Have (basic versions exist) |
| **Procedural** | Fully procedural |
| **How to generate** | Create material assets via code: set base color per palette entry, metallic=0.0, roughness=0.6-0.8 for matte look. Emissive variant for selection highlight (same color, higher intensity). Engine `Zenith_MaterialAsset` API supports all needed properties. |
| **Effort** | Low |

### 3.6 Elimination Particle Effect

| Field | Value |
|-------|-------|
| **Specs** | GPU particle emitter config (`Flux_ParticleEmitterConfig`) |
| **Where used** | Cat elimination event (sparkle burst) |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | Configure `Flux_ParticleEmitterConfig`: burst mode (20-30 particles), short lifetime (0.5-1.0s), radial velocity outward, color over lifetime (cat's color -> white -> transparent), small size (0.02-0.05 units), gravity=-2.0 for upward drift. No texture needed (point sprites or small quads). |
| **Effort** | Low |

### 3.7 Victory Confetti Particle Effect

| Field | Value |
|-------|-------|
| **Specs** | GPU particle emitter config |
| **Where used** | Level complete celebration |
| **Priority** | Nice-to-Have |
| **Procedural** | Fully procedural |
| **How to generate** | Configure emitter: burst (50-100 particles), longer lifetime (2-3s), spawn from top of screen, gravity=5.0 downward, random X velocity spread, multi-color (cycle through puzzle palette), size 0.03-0.08 units, slight drag=0.5 for flutter effect. |
| **Effort** | Low |

### 3.8 Lock Indicator (Conditional Shape Overlay)

| Field | Value |
|-------|-------|
| **Specs** | 3D floating text or icon above locked shapes |
| **Where used** | Gameplay - shapes with uUnlockThreshold > 0 |
| **Priority** | Must-Have (text number exists, icon upgrade) |
| **Procedural** | Fully procedural |
| **How to generate** | Currently renders as floating yellow number via `Flux_Text`. Upgrade option: add small lock icon (SDF) next to the number. Both are fully procedural using existing engine text and SDF systems. |
| **Effort** | Low |

### 3.9 Pinball Ball Texture/Material

| Field | Value |
|-------|-------|
| **Specs** | 1 .zmat with metallic PBR properties |
| **Where used** | Pinball ball entity |
| **Priority** | Nice-to-Have (sphere with material works) |
| **Procedural** | Fully procedural |
| **How to generate** | Material: silver/chrome look (metallic=0.9, roughness=0.1, base color=#C0C0C0). Alternatively, golden ball (metallic=0.8, roughness=0.2, base color=#FFD700). |
| **Effort** | Low |

### 3.10 Pinball Peg Material

| Field | Value |
|-------|-------|
| **Specs** | 2 .zmat variants (unhit + hit flash) |
| **Where used** | Pinball peg entities |
| **Priority** | Must-Have (basic exists) |
| **Procedural** | Fully procedural |
| **How to generate** | Unhit: blue/cyan matte (roughness=0.6, metallic=0.0). Hit flash: same color with emissive boost (emissive intensity 2.0+), driven by `m_afPegFlashTimer` (0.3s flash duration). Materials created via `Zenith_MaterialAsset` API. |
| **Effort** | Low |

### 3.11 Pinball Target Material

| Field | Value |
|-------|-------|
| **Specs** | 1 .zmat with emissive glow |
| **Where used** | Pinball target entity |
| **Priority** | Must-Have (basic exists) |
| **Procedural** | Fully procedural |
| **How to generate** | Bright green with emissive glow (base color=#3CE63C, emissive color=#3CE63C, emissive intensity=1.5). Already implemented in codebase. |
| **Effort** | Low |

### 3.12 Selection Highlight Effect

| Field | Value |
|-------|-------|
| **Specs** | Emissive material override or outline shader |
| **Where used** | Selected shape glow, cursor highlight |
| **Priority** | Must-Have (emissive override exists) |
| **Procedural** | Fully procedural |
| **How to generate** | Already implemented: emissive material override with `g_fHighlightEmissiveIntensity`. Could be enhanced with an outline post-process pass (Sobel filter on entity ID buffer) but current approach works. |
| **Effort** | Low (already done) |

---

## 4. Cat Cafe Assets

### 4.1 Cat Card Background

| Field | Value |
|-------|-------|
| **Specs** | 256x128 or UI rect with rounded corners |
| **Where used** | Per-cat card in cat cafe screen |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | `Zenith_UIRect` with fill color, border, and optional glow. Different background color for 3-star cats (gold tint) vs normal (white/cream). Rounded corners achievable via SDF rect with corner radius. |
| **Effort** | Low |

### 4.2 Cat Portrait (Per Cat)

| Field | Value |
|-------|-------|
| **Specs** | 96x96 rendered to texture or inline 3D viewport |
| **Where used** | Cat cafe card left side |
| **Priority** | Nice-to-Have (text-only works) |
| **Procedural** | Partially procedural + hand-tweaked |
| **How to generate** | Option A: Render cat mesh to texture (render-to-texture pass per cat). Option B: Use 2D cat face (compute-generated, see 3.2). Option C: Colored circle with cat ear silhouette SDF overlay. For 100 cats, color variation is sufficient (5 colors x different accessories). |
| **Effort** | Medium |

### 4.3 Cafe Background

| Field | Value |
|-------|-------|
| **Specs** | Full-screen background image or gradient |
| **Where used** | Cat cafe screen background |
| **Priority** | Nice-to-Have |
| **Procedural** | Fully procedural |
| **How to generate** | Vertical gradient (warm cream #FFF5E6 to soft pink #FFE6F0) rendered via `Zenith_UIRect` with gradient fill, or a compute-generated texture with subtle pattern (paw prints, fish bones). |
| **Effort** | Low |

### 4.4 Page Indicator Dots

| Field | Value |
|-------|-------|
| **Specs** | Small circles, 8-12px, filled + hollow variants |
| **Where used** | Cat cafe pagination (13 pages) |
| **Priority** | Must-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF circles or `Zenith_UIRect` elements. Current page = filled, other pages = hollow outline. Trivial procedural rendering. |
| **Effort** | Low |

---

## 5. Branding Assets

### 5.1 Splash Screen / Logo

| Field | Value |
|-------|-------|
| **Specs** | 512x256 PNG or rendered via engine |
| **Where used** | App launch, main menu header |
| **Priority** | Must-Have |
| **Procedural** | Requires artist but AI-assistable |
| **How to generate** | "Paws & Pins" text with cat paw print + pinball integration. Use AI image generation for initial concepts, refine in vector editor. Can render in-engine using `Flux_Text` for text + SDF for paw print icon as a simpler alternative. |
| **Effort** | Medium |

### 5.2 Loading Spinner

| Field | Value |
|-------|-------|
| **Specs** | Animated icon, 64x64 or SDF rendered |
| **Where used** | Level loading, save operations |
| **Priority** | Nice-to-Have |
| **Procedural** | Fully procedural |
| **How to generate** | SDF spinning arc (partial circle that rotates). Render via `Flux_SDFs` with rotation driven by time uniform. Alternatively, cat paw print that spins. |
| **Effort** | Low |

---

## 6. Audio Assets

### 6.1 Music Tracks

| Track | Duration | Style | Priority | Generation |
|-------|----------|-------|----------|------------|
| Main Menu Theme | 60-90s loop | Calm, lofi/jazzy | Must-Have | Requires artist (commission or royalty-free library: Epidemic Sound, Artlist). AI music tools (Suno, Udio) can draft. |
| Puzzle Gameplay | 90-120s loop | Light ambient, non-distracting | Must-Have | Same as above. Key requirement: must loop seamlessly. |
| Pinball Theme | 60-90s loop | Upbeat, energetic | Must-Have | Same as above. Faster tempo, more percussive. |
| Cat Cafe Theme | 60-90s loop | Cozy, warm | Nice-to-Have | Same as above. Acoustic guitar or soft piano. |
| Victory Sting | 3-5s | Celebratory fanfare | Must-Have | Short enough to create with simple DAW or AI tools. |

**Effort per track:** Medium (commissioning) or Low (royalty-free library)

### 6.2 Sound Effects

| Sound | Priority | Generation |
|-------|----------|------------|
| Shape select (soft click) | Must-Have | Fully procedural (synth click via SFXR/BFXR) |
| Shape slide (whoosh) | Must-Have | Fully procedural (filtered noise sweep via SFXR) |
| Shape blocked (thud) | Must-Have | Fully procedural (low-pass noise burst via SFXR) |
| Cat elimination (meow + sparkle) | Must-Have | Requires artist (cat meow sample) + procedural sparkle |
| Level complete (jingle) | Must-Have | Partially procedural (ascending chime sequence) |
| Star earned (chime) | Must-Have | Fully procedural (single tone, ascending pitch per star) |
| Undo (rewind swoosh) | Nice-to-Have | Fully procedural (reverse noise sweep) |
| Hint bell | Nice-to-Have | Fully procedural (bell synth tone) |
| Button click | Must-Have | Fully procedural (UI click via SFXR) |
| Pinball launch | Must-Have | Fully procedural (spring release, rising pitch) |
| Peg hit (ping) | Must-Have | Fully procedural (pitched ping, randomized per peg) |
| Target hit (bonus) | Must-Have | Fully procedural (cash register / bright arpeggio) |
| Gate clear (fanfare) | Must-Have | Partially procedural (short brass-like sequence) |
| Shape unlock (click) | Nice-to-Have | Fully procedural (metallic click + release) |

**Tool recommendation:** SFXR/BFXR (free, open source) for procedural SFX generation. Export as WAV/OGG.

**Effort:** Low per individual SFX (minutes each with SFXR), Medium total (14 sounds)

---

## 7. Summary

### Asset Count by Category

| Category | Must-Have | Nice-to-Have | Total |
|----------|-----------|-------------|-------|
| Store Listing | 3 | 1 | 4 |
| UI Icons | 13 | 0 | 13 |
| Gameplay Visuals | 9 | 3 | 12 |
| Cat Cafe | 2 | 2 | 4 |
| Branding | 1 | 1 | 2 |
| Music | 4 | 1 | 5 |
| Sound Effects | 10 | 4 | 14 |
| **Total** | **42** | **12** | **54** |

### Procedural Feasibility Breakdown

| Feasibility | Count | Percentage |
|-------------|-------|------------|
| Fully procedural | 31 | 57% |
| Partially procedural + hand-tweaked | 11 | 20% |
| Requires artist but AI-assistable | 5 | 9% |
| Requires artist | 7 | 13% |

### Effort Estimate

| Effort Level | Count | Approximate Time |
|-------------|-------|-----------------|
| Low | 34 | Minutes to 1 hour each |
| Low-Medium | 5 | 1-2 hours each |
| Medium | 15 | 2-8 hours each |
| High | 0 | - |

### Cost Estimate (if outsourcing artist work)

| Item | Estimated Cost |
|------|---------------|
| App icon design | $50-200 |
| Feature graphic | $50-150 |
| Cat 3D model (1 model, 5 color variants) | $100-300 |
| Logo/branding | $100-300 |
| Music (4 tracks, royalty-free library) | $15-50/month subscription |
| Music (4 tracks, commissioned) | $200-800 |
| Cat meow SFX (royalty-free) | $0-20 |
| **Total (budget path)** | **$315-1,020** |
| **Total (premium path)** | **$500-1,770** |

### Key Takeaway

**77% of assets (42 out of 54) can be generated fully or partially procedurally** using existing engine capabilities (`Flux_SDFs`, `Flux_ParticleEmitterConfig`, `Zenith_MaterialAsset`, compute shaders, `Flux_Text`) and free tools (SFXR for SFX). The remaining 23% require minimal artist involvement, primarily for the app icon, logo, cat model, and music tracks. Total outsourcing cost is estimated at $300-1,800 depending on quality tier.
