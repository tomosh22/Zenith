# Devil's Playground — Placeholder Asset Test-Driven Development Plan

**Document purpose:** Extend the TDD discipline of [TestPlan.md](TestPlan.md) to placeholder asset *creation itself*. Every placeholder asset in [AssetManifest.md](AssetManifest.md) is delivered with a test suite that proves the asset (a) exists, (b) loads, (c) has the structural properties the game requires, and (d) behaves correctly when instantiated. Every test in this plan is verifiable by Claude Code without human intervention — no screenshots, no audio playback, no human inspection.

**Author:** Asset Engineering (Claude)
**Companion docs:** [GameDesignDocument.md](GameDesignDocument.md) · [Shortfalls.md](Shortfalls.md) · [TestPlan.md](TestPlan.md) · [AssetManifest.md](AssetManifest.md)
**Last updated:** 2026-05-11

---

## 0. The Principle: The Placeholder Asset *Is* the Contract

A placeholder asset is not "art we'll replace later." It is the **executable specification** for the final asset.

The single guiding rule of this plan:

> **Every test that validates a placeholder must also pass against the final asset that replaces it.**

If `dp_char_villager_farmhand` is a Mixamo Y-Bot at S0, the test suite asserts:

- The mesh loads and has a humanoid skeleton.
- The skeleton has bones named `Hips`, `Spine`, `Head`, `LeftHand`, `RightHand`.
- The `RightHand` bone has a child socket named `Item_Grip`.
- The bounding box height is 1.5–2.0 m.
- The mesh has at most 14k tris LOD0.
- The default animation set contains clips named `idle_neutral`, `walk`, `jog`, `sprint`, `pickup_low`, `drop`.
- The `walk` clip is 0.9–1.3 s long and loops cleanly (first and last keyframe match within ε).

When the S2 final hand-authored villager arrives in month 12, **the same tests must pass.** If they don't, either the final asset breaks a gameplay contract or the placeholder tests were under-specified — both are real bugs.

This single rule converts asset production from a "trust the artist" workflow into a "code reviews the asset" workflow that Claude can run without human intervention.

### 0.1 What can be verified without a human

The verification surface for assets is wider than for gameplay because every asset has a deterministic in-memory representation Claude can inspect. The full surface:

| Asset property | Verifiable by Claude? | Method |
|---|---|---|
| Existence at expected path | ✅ | File-system check via `Zenith_FileAccess`. |
| Loadability | ✅ | `Zenith_AssetRegistry::Get<T>(path) != nullptr`. |
| Schema correctness (vertex format, bone count, texture format) | ✅ | Asset's runtime header / metadata. |
| Bounding-box / dimensions / poly count | ✅ | Geometry inspection. |
| Material slot count + channel population | ✅ | Material asset query. |
| Texture format + size | ✅ | Texture asset header. |
| Animation duration + keyframe count + loop continuity | ✅ | Animation asset query. |
| Animation event presence (footstep events on walk anims, etc.) | ✅ | Event table query. |
| Particle emitter parameters | ✅ | Config asset query. |
| Audio file duration + sample rate + channel count | ✅ | Audio file header. |
| **Visual quality / artistic intent** | ❌ | Human review (out of scope of this plan). |
| **Audio mix quality** | ❌ | Human review. |
| **Animation naturalness / "feel"** | ❌ | Human review. |
| **Final-asset visual matches concept art** | ❌ | Human review. |

The artistic / aesthetic surface is explicitly out of scope. **This plan keeps engineering moving; design reviews assess art quality separately.**

### 0.2 What this plan adds to TestPlan.md

[TestPlan.md](TestPlan.md) describes ~250 gameplay tests. This plan adds **~180 asset-validation tests** that run in the same harness. They share the same:

- Test struct (`Zenith_AutomatedTest` with Setup/Step/Verify/MaxFrames).
- Test runner (`zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1)`).
- JSON result format (Claude reads `<test>.json` to determine pass/fail).
- Naming convention (`Test_<Phase><System>_<Scenario>`).

Asset tests are gated by a new tier prefix `Test_Asset*` and run in dedicated batches: the **fast asset linter** (~10 s, runs on every save) and the **full asset validator** (~3 min, runs on every commit).

---

## 1. Test Class Hierarchy

Six classes of asset test. Each placeholder spec specifies which tests apply.

### 1.1 Manifest test — does the file exist?

Cheapest, most-often-failing. Reads [AssetManifest.md](AssetManifest.md)'s expected-paths and confirms each file is on disk.

**Pattern:**

```cpp
static bool Verify_AssetManifest_AllPathsExist()
{
    Zenith_Vector<const char*> axMissing;
    for (const ManifestEntry& xE : g_xManifest)
    {
        if (!Zenith_FileAccess::Exists(xE.m_szPath))
        {
            axMissing.PushBack(xE.m_szPath);
        }
    }
    if (axMissing.GetSize() > 0)
    {
        for (uint32_t u = 0; u < axMissing.GetSize(); ++u)
            Zenith_Log(LOG_CATEGORY_TEST, "FAIL: missing asset %s", axMissing.Get(u));
    }
    return axMissing.GetSize() == 0;
}
```

The manifest itself is a data file (`Games/DevilsPlayground/Assets/manifest.json`) generated from a single source of truth — same as the schema-validated list of every expected asset. New asset added to the manifest? Fails immediately until the file lands. Asset deleted? Fails immediately. Renamed without manifest update? Fails immediately.

### 1.2 Loadability test — does it parse?

For each asset type:

```cpp
Zenith_<Type>* pxAsset = Zenith_AssetRegistry::Get<Zenith_<Type>>("game:.../path.zXX");
ZENITH_ASSERT(pxAsset != nullptr, "Asset failed to load");
```

Any loadability failure is logged with the asset path and the parsing error, so Claude can find the source.

### 1.3 Schema test — does it have the expected structure?

The bulk of asset tests. Verifies the asset's *shape*, not its *content*.

Example schemas:

- A villager mesh has `>= 4000` tris, `<= 14000` tris, exactly 1 submesh, bounding-box height in [1.5, 2.0] m.
- A villager skeleton has bones named in the canonical bone-set; the `Item_Grip` socket is a child of `RightHand` bone.
- A walk animation has duration in [0.9, 1.3] s, has bone tracks for at least all locomotion bones, and loops cleanly (start frame ≈ end frame).
- A door mesh has a child bone named `Pivot` whose local transform is at the door's hinge axis.
- A material has at least the `BaseColor` channel populated.

### 1.4 Budget test — does it fit the platform target?

Catches placeholder bloat early. An artist exporting a Mixamo character at full 4K texture resolution will balloon the build size; the budget test catches it the same day.

- Texture: `<= 1024² @ S0`, `<= 2048² @ S2`.
- Mesh tri count: `<= 14000` for villagers, `<= 24000` for Aelfric, `<= 1500` for items.
- Material count per mesh: `<= 6`.
- Animation file size: `<= 100 KB` per clip (Zenith stores quantised keyframes; runaway bone counts inflate it).

### 1.5 Instantiation test — does it work in a scene?

The asset is spawned into a runtime scene and queried. This is the strongest possible verification of "the asset is wired up correctly."

```cpp
Zenith_Entity xE = g_xEngine.Scenes().CreateEntity(pxScene, "TestEntity");
Zenith_ModelComponent& xMC = xE.AddComponent<Zenith_ModelComponent>();
xMC.LoadModel("game:.../path.zmodel");
ZENITH_ASSERT(xMC.GetModelInstance() != nullptr, "Model failed to instantiate");
ZENITH_ASSERT(xMC.GetModelInstance()->GetNumMaterials() == kExpectedMatCount, "Wrong material count");
```

### 1.6 Substitution test — does the final asset slot in cleanly?

The most important test class. **Final assets ship as a drop-in replacement** for the placeholder. If the final asset breaks any test that passed for the placeholder, the substitution is rejected at PR time.

This test class is identical in structure to schema + budget + instantiation tests. The key discipline: when authoring a placeholder test, ask *"would the final asset also satisfy this?"* If yes, the test is correct. If no, the test is over-specified to the placeholder and must be relaxed.

Example over-specification (don't do this):

```cpp
// BAD: only passes for the Mixamo Y-Bot
ZENITH_ASSERT(pxMesh->GetVertexCount() == 7689, "Wrong vert count");
```

Correct version:

```cpp
// GOOD: passes for any villager mesh meeting the spec
ZENITH_ASSERT(pxMesh->GetVertexCount() >= 2000 &&
              pxMesh->GetVertexCount() <= 18000, "Vert count out of range");
```

---

## 2. Tests per Asset Class

For each class of asset in [AssetManifest.md](AssetManifest.md), the test spec follows.

### 2.1 Static Meshes (props, environment, items, interactables)

#### Test_AssetMesh_LoadAndDimensions

**Proves:** every mesh in the manifest loads, has valid geometry, and matches its declared dimension bucket.

- **Setup:** none.
- **Step:** iterate `g_xMeshManifest` (table of `{path, kind, expectedDimsRange}`). For each entry, load via `Zenith_AssetRegistry::Get<Zenith_Mesh>`. Capture vertex count, tri count, bounding box.
- **Verify:** every load succeeded AND for each: tri count in `[kind.minTris, kind.maxTris]` AND `bbox.height` in `[kind.minHeight, kind.maxHeight]`.
- **Sample manifest entry:**
  ```cpp
  { "game:Meshes/dp_item_iron_a.zmodel",
    MeshKind::Item,    /*minTris=*/100, /*maxTris=*/1500,
    /*minH=*/0.05f,    /*maxH=*/0.5f }
  ```
- **Frame budget:** 60 (asset loads are synchronous and fast).

#### Test_AssetMesh_NoZeroAreaTriangles

**Proves:** no mesh has degenerate triangles. Zero-area tris waste GPU cycles and trip the lightmap baker.

- **Setup:** none.
- **Step:** for each mesh, iterate its triangle list; sum the count of triangles with area < 1e-6.
- **Verify:** every mesh has zero degenerate tris.

#### Test_AssetMesh_HasMaterialSlots

**Proves:** every mesh declares at least one material slot.

- **Step:** for each mesh, assert `pxMesh->GetNumMaterialSlots() >= 1`.

#### Test_AssetMesh_ItemHeldSocket

**Proves:** every item mesh (reagents, tools, keys, charms) has a `Held` socket defined in metadata.

- **Step:** for each item-kind mesh, query `pxMesh->FindSocket("Held")`. Assert non-null.
- **Why:** items are held by villagers via the `Item_Grip` bone's socket-attach. If the item has no `Held` socket, it spawns at world origin every time, breaking the held-item visual.

#### Test_AssetMesh_DoorHasPivot

**Proves:** every door mesh has a `Pivot` socket at its hinge axis.

- **Step:** load all door meshes. For each, assert `FindSocket("Pivot") != nullptr` AND the socket's local transform's X-position is within 0.1 m of the mesh's left/right edge (consistent with a hinge on one side).

#### Test_AssetMesh_DoubleDoorHasTwoPivots

**Proves:** every double-door mesh has both `Pivot_L` and `Pivot_R` sockets.

- **Step:** load all double-door meshes; assert both sockets exist.

#### Test_AssetMesh_ChestHasLidBone

**Proves:** every chest mesh has a bone named `Lid` for the open animation.

#### Test_AssetMesh_PentagramFlat

**Proves:** the pentagram mesh is flat (a floor decal mesh). Bounding-box Y < 0.1 m.

#### Test_AssetMesh_NoNaN

**Proves:** no vertex has NaN/Inf in position, normal, UV, or vertex colour. Catches corrupted exports.

---

### 2.2 Skeletal Meshes (characters)

#### Test_AssetSkel_VillagerCanonicalBones

**Proves:** every villager skeleton has every bone in the canonical bone-set.

- **Setup:** define `kCanonicalBones[] = { "Hips", "Spine", "Spine1", "Spine2", "Neck", "Head", "LeftShoulder", "LeftArm", "LeftForeArm", "LeftHand", "RightShoulder", "RightArm", "RightForeArm", "RightHand", "LeftUpLeg", "LeftLeg", "LeftFoot", "RightUpLeg", "RightLeg", "RightFoot" }`.
- **Step:** for each villager skeleton, for each canonical bone, query `pxSkel->FindBone(name) != INVALID_BONE`.
- **Verify:** every villager skeleton has every canonical bone.

#### Test_AssetSkel_VillagerSockets

**Proves:** every villager skeleton has the sockets the gameplay code expects.

- **Setup:** define `kRequiredSockets[] = { "Item_Grip", "HeadTop", "BackPack", "Belt_L", "Belt_R", "Foot_L", "Foot_R" }`.
- **Step:** for each villager, assert each socket exists and is a child of the expected bone (`Item_Grip` child of `RightHand`, `HeadTop` child of `Head`, etc.).

#### Test_AssetSkel_VillagerBoneCountUnderLimit

**Proves:** no villager skeleton exceeds the engine's 100-bone cap.

- **Step:** for each villager skeleton, assert `pxSkel->GetBoneCount() <= 100`.

#### Test_AssetSkel_AelfricFaceBlendshapes

**Proves:** Aelfric's mesh has the 12 phoneme + 6 emotion blendshapes required for VO lip-sync.

- **Setup:** define `kVisemes[] = { "viseme_aa", "viseme_e", "viseme_i", "viseme_o", "viseme_u", "viseme_M", "viseme_F", "viseme_TH", "viseme_DD", "viseme_kk", "viseme_RR", "viseme_sil" }` and `kEmotions[] = { "emo_calm", "emo_grim", "emo_suspicious", "emo_pursuing", "emo_apprehending", "emo_dying" }`.
- **Step:** load Aelfric mesh; query blendshape names.
- **Verify:** all 18 named blendshapes present.

#### Test_AssetSkel_HoundCanonicalBones

**Proves:** hound skeleton has expected quadruped bones (`Hips`, `Spine`, `Neck`, `Jaw`, `Tail`, four legs each with hip/knee/ankle/foot).

#### Test_AssetSkel_RigRetargetable

**Proves:** every villager skeleton is retargetable from the canonical Mixamo rig (so animations author once, share many).

- **Step:** apply a stock retargeting profile. Verify no bones return `INVALID_BONE` during the retarget.

---

### 2.3 Materials

#### Test_AssetMtrl_LoadAndChannels

**Proves:** every material loads and has the channels its kind requires.

- **Setup:** define per-kind channel expectations:
  ```cpp
  struct MtrlSpec { const char* kind; bool needsBaseColor; bool needsNormal;
                    bool needsRoughness; bool needsMetallic; bool needsEmissive; };
  // Item: base + normal + roughness; metallic optional; emissive only on glowing items
  // Character: base + normal + roughness + metallic + AO
  // Environment: base + normal + roughness
  // Forge / pentagram: base + normal + roughness + emissive
  ```
- **Step:** for each material in manifest, load and inspect channel population.
- **Verify:** all required channels populated; unused channels not corrupt.

#### Test_AssetMtrl_TextureReferencesValid

**Proves:** every material's referenced texture path resolves.

- **Step:** for each material, for each channel, assert the referenced texture loads.

#### Test_AssetMtrl_BaseColorInLinearSpace

**Proves:** every authored base-color value is in valid linear-space range [0, 1].

- **Step:** for each material, query base-color vec4. Assert each component in [0.0, 1.0].

#### Test_AssetMtrl_NoPacked

**Proves:** no material accidentally references a packed ARM/ORM texture (engine doesn't support).

- **Step:** for each material, check texture component-counts. If a "roughness" channel texture has > 1 channel, fail.

#### Test_AssetMtrl_PossessedVariantsExist

**Proves:** `DPMaterials::GetOrCreatePossessedTintFor` produces a valid possessed-tint variant for every base material in the villager set.

- **Step:** for every base material on a villager mesh, query `GetOrCreatePossessedTintFor`. Assert non-null. Assert variant's emissive is non-zero (the red glow).

---

### 2.4 Textures

#### Test_AssetTex_LoadAndFormat

**Proves:** every texture loads and is in an expected format/dimension for its purpose.

- **Setup:** per-kind specs:
  ```cpp
  { "char_base",   1024, 1024, TEXTURE_FORMAT_BC7_RGBA_UNORM }
  { "char_normal", 1024, 1024, TEXTURE_FORMAT_BC5_RG_UNORM }
  { "env_base",     512,  512, TEXTURE_FORMAT_BC7_RGBA_UNORM }
  { "ui_icon",      128,  128, TEXTURE_FORMAT_RGBA8_UNORM }
  ```
- **Step:** for each texture, assert format + dimensions match spec.

#### Test_AssetTex_NormalMapIsBlue

**Proves:** every normal-map texture has a centered blue tint (indicates correct tangent-space normal authoring; a green-tinted one is OpenGL-convention and will look wrong).

- **Step:** load each normal map. Sample the centre pixel. Assert blue channel value > 0.5 AND red, green channels in [0.3, 0.7].
- **Why claudia-verifiable:** sampling a single pixel via `Zenith_Texture::SampleCPU(uvX, uvY)` is a deterministic numeric query.

#### Test_AssetTex_MipsPresentIfRequested

**Proves:** textures that declared mip generation actually have them.

- **Step:** for each texture, assert `GetMipCount() > 1` if its manifest entry says it should.

#### Test_AssetTex_NoOversized

**Proves:** no texture exceeds its declared platform budget.

- **Step:** for each texture, `width * height * bytesPerTexel <= manifestEntry.maxBytes`.

---

### 2.5 Animations

The biggest test class. Each villager archetype has ~15 animations × 24 archetypes = ~360 anim files, plus Aelfric's 30 and hounds' 20.

#### Test_AssetAnim_LoadAndDuration

**Proves:** every animation loads and falls within its declared duration bucket.

- **Setup:** per-verb durations:
  ```cpp
  { "idle_neutral",   3.5f, 5.0f },
  { "idle_alert",     2.5f, 3.5f },
  { "walk",           1.0f, 1.4f },
  { "jog",            0.8f, 1.0f },
  { "sprint",         0.6f, 0.8f },
  { "possessed_walk", 1.0f, 1.4f },
  { "possession_channel", 0.7f, 0.9f },
  { "possession_enter", 0.3f, 0.5f },
  { "faint",          1.3f, 1.7f },
  { "collapse",       1.8f, 2.2f },
  { "pickup_low",     0.7f, 0.9f },
  { "drop",           0.4f, 0.6f },
  ```
- **Step:** for each animation file, assert duration in spec range.

#### Test_AssetAnim_LoopClipsLoopCleanly

**Proves:** every animation tagged as looping has first-keyframe pose ≈ last-keyframe pose.

- **Step:** for each loop anim (idle*, walk, jog, sprint, possessed_walk, sprint_burning), sample bone transforms at t=0 and t=duration. Compute max position delta across all bones.
- **Verify:** delta < 0.01 m AND rotation delta < 1°.

#### Test_AssetAnim_FootstepEventsPresent

**Proves:** walk/jog/sprint animations have `Footstep_L` and `Footstep_R` events at the foot-contact frames.

- **Setup:** for each pace, declare expected event count:
  - walk: 2 events (one per foot per cycle).
  - jog: 2.
  - sprint: 2 (faster cycle).
- **Step:** for each anim, query event table. Assert events exist with names matching the spec.
- **Why this matters:** the audio system listens for these events to fire footstep SFX. If they're missing, the world is silent regardless of audio mix quality.

#### Test_AssetAnim_BoneCountMatchesSkeleton

**Proves:** every animation's bone-track count matches its target skeleton's bone count.

- **Step:** load anim's source skeleton; load anim. Assert `anim.boneTrackCount == skel.boneCount`.

#### Test_AssetAnim_NoNanKeyframes

**Proves:** no keyframe contains NaN/Inf. Animation exports occasionally produce these on twist-joint outliers; they show up at runtime as bones flying to infinity.

#### Test_AssetAnim_PossessionEnterIsOneShot

**Proves:** the `possession_enter` anim is not flagged as looping.

#### Test_AssetAnim_RetargetableFromMixamoRig

**Proves:** every animation is authored on the canonical bone-set (so retargeting to other characters works).

- **Step:** load anim. Iterate every bone track. Assert each bone name is in `kCanonicalBones[]` (from §2.2).
- **Why:** an outsourced animator who renames bones breaks the entire retargeting layer; this test catches it on import.

---

### 2.6 Particle Configs

#### Test_AssetVfx_LoadAndParameters

**Proves:** every particle config loads and has sensible emitter parameters.

- **Setup:** per-config sanity ranges:
  ```cpp
  // dp_vfx_possession_enter: 1-shot, burst 60-100, lifetime 0.4-0.8 s
  // dp_vfx_torch_flame:      looping, rate 30-60/s, lifetime 0.5-1.0 s
  // dp_vfx_hearth_smoke:     looping, rate 10-20/s, lifetime 3-5 s
  ```
- **Step:** for each config, load. Assert burst/rate/lifetime in range.

#### Test_AssetVfx_TexturesValid

**Proves:** every particle config's texture reference resolves.

#### Test_AssetVfx_BudgetUnderCap

**Proves:** no particle config exceeds the 256-particle default (or declares itself a GPU-compute system if it does).

- **Step:** for each config, assert `maxParticles <= 256 OR isGpuCompute == true`.

#### Test_AssetVfx_InstantiatesInScene

**Proves:** spawning each particle config in a scene does not crash.

- **Setup:** load a test scene.
- **Step:** for each config in manifest, `Flux_Particles::SpawnEmitter(config, position)`. Wait 5 frames. Query the emitter handle is valid. Despawn. Repeat for next config.
- **Verify:** every emitter spawned, ticked, and despawned without asserts or crashes.

---

### 2.7 Decals

#### Test_AssetDecal_LoadAndDimensions

**Proves:** every decal config loads. Decal box-dimensions match its kind (frost = 1 m², footprint = 0.3 m², pentagram inscription = 5 m²).

#### Test_AssetDecal_SpawnsInScene

**Proves:** spawning each decal in a scene doesn't crash.

- **Step:** for each decal config, `Flux_Decals::SpawnDecal(config, position, normal)`. Wait 5 frames. Verify the decal is in the ring-buffer.

#### Test_AssetDecal_FrostAnchorTriggersOnBurnOut

**Proves:** villager burn-out spawns the `dp_decal_frost_anchor` decal at the death location.

- **Setup:** subscribe to decal-spawn events (a new instrumentation hook on `Flux_Decals`).
- **Step:** possess a villager; set life to 0.05; wait for `DP_OnVillagerDied`; verify decal spawned within 5 frames at the body's position.

---

### 2.8 UI Assets

#### Test_AssetUI_AllSpritesLoad

**Proves:** every UI sprite referenced by any UI element loads.

#### Test_AssetUI_IconAtlasCoverage

**Proves:** every item tag, archetype, and Aelfric awareness state has an icon.

- **Step:** iterate `DP_ItemTag` enum + archetype enum + awareness states; for each, query the icon registry.
- **Verify:** zero missing icons.

#### Test_AssetUI_FontsLoad

**Proves:** the three fonts (display, body, mono) load at every required size.

- **Setup:** declare expected sizes per font (display: 48, 64, 96; body: 12, 14, 18, 24; mono: 10, 12, 14).
- **Step:** load each font + size combination.

#### Test_AssetUI_NoTruncation

**Proves:** every UI string in every language fits its container at the chosen font + size.

(Same as TestPlan §4.3 Test_P3Loc_NoTruncation; cited here for the asset-author audience.)

#### Test_AssetUI_HUDElementsInstantiate

**Proves:** every HUD element instantiates correctly when scene loads.

- **Setup:** load the GameLevel scene.
- **Step:** for each element name from GDD §7.1 (`LifeCandle`, `ReagentSlot_0`..`ReagentSlot_4`, `BodyName`, `DemonScent`, `HeldItemGlyph`, `HeldItemName`, `AelfricAwareness`, `WhisperLine`, `SunGauge`), query the canvas. Assert non-null.

---

### 2.9 Audio (special case — engine doesn't exist yet)

The audio system is net-new engine work (per [AssetManifest.md](AssetManifest.md) §8). Until it lands, the audio test class operates on a **stub** that only records emission events without playing sound. Once the system ships, the same tests promote to real audio assertions.

#### Test_AssetAudio_AllFilesExist

**Proves:** every audio file in the manifest exists. Fastest test; runs first.

#### Test_AssetAudio_DurationsInRange

**Proves:** every audio file's duration matches its declared bucket.

- **Setup:** per-kind:
  ```cpp
  { "footstep_*",      0.15f, 0.40f }
  { "door_*",          0.30f, 1.50f }
  { "bell_single",     2.00f, 4.00f }
  { "music_track",     60.0f, 360.0f }
  { "vo_aelfric_*",    0.50f, 8.00f }
  ```
- **Step:** for each file, parse header for duration. Assert in range.

#### Test_AssetAudio_SampleRateConsistent

**Proves:** every SFX is 48 kHz; every music track is 44.1 kHz (decision recorded in the audio-system spec — these are the engine targets).

#### Test_AssetAudio_EmissionPlumbing

**Proves:** the gameplay events that should emit audio do so. (Same as [TestPlan.md](TestPlan.md) Test_P2Forge_AudibleAt30m; tracked here as an asset-coverage test.)

- **Step:** for each interactable kind, perform its interaction; capture `Zenith_AudioBus::GetEmittedSoundsForTest()`; assert at least one emission with the expected name.

#### Test_AssetAudio_NoClipping

**Proves:** no SFX file has samples at ±1.0 (which on most platforms is clipped audio).

- **Step:** for each WAV / OGG, decode samples; scan for peaks. Assert max abs sample value <= 0.98.

#### Test_AssetAudio_LoopsAreSeamless

**Proves:** every audio file tagged "looping" has matching start and end samples.

- **Step:** for each looping file, sample first and last 256 samples. Cross-correlate. Assert correlation > 0.95.

---

### 2.10 Scenes (the integration-level test)

The scene is the final destination of every asset. A scene test asserts the whole asset pipeline functions: meshes load, materials apply, animations bind, particles spawn, fog holes register.

#### Test_AssetScene_VexholmeLoadsCleanly

**Proves:** the Vexholme scene loads with zero asset errors.

- **Setup:** install an asset-error capture hook on `Zenith_AssetRegistry` (logs every failed load to a Zenith_Vector during the test).
- **Step:** `LoadSceneByIndex(1)`. Wait until scene is fully initialized (entity count stabilises).
- **Verify:** captured error count == 0.

#### Test_AssetScene_AllArchetypesInstantiable

**Proves:** every villager archetype can be spawned without errors. Iterates the archetype registry; for each, spawns a temporary villager entity, asserts its model and animator initialise correctly.

#### Test_AssetScene_NightStartArtBudget

**Proves:** the full Night 1 scene total asset usage is under platform budgets.

- **Step:** after Night 1 loads, query total loaded mesh count, texture memory, material count, particle count.
- **Verify:** within budgets (`mesh count <= 400, texture memory <= 512 MB, materials <= 80, active particles <= 1024`).

---

## 3. The Asset Linter (Build-Time Tool)

For tests Claude can run in <1 second per asset, we run them as an **asset linter** in the import pipeline. This catches obvious errors on save, not on commit.

### 3.1 Linter scope

The linter wraps a subset of asset tests as standalone CLI commands:

```bash
ZenithTools.exe lint --asset game:Meshes/dp_char_villager_farmhand.zmodel
# → JSON output:
# { "asset": "...", "tests": [{ "name": "BoneCount", "passed": true }, ...] }
```

Runs on:
- **On-save** in the editor (1 asset, <100 ms).
- **Pre-commit hook** (all changed assets, ~2 s).
- **CI pre-build** (every asset in manifest, ~30 s).

### 3.2 Linter rules

The linter implements every "purely-static" test from §2:

- Manifest existence
- Loadability
- Schema (bone names, socket presence, mesh dimensions, etc.)
- Format / dimensions (texture format, anim duration, etc.)
- Budget (poly count, texture size, audio sample rate)

It does *not* implement:

- Integration tests (require a running scene)
- Substitution tests (require comparison against a baseline)
- Loop-continuity / NaN-keyframe (require iterating animation data)

Those run in the full asset validator (§4).

### 3.3 Tooling implementation

The linter is a new `ZenithTools` sub-command. ~2 engineering weeks. Output goes to a single JSON file Claude reads.

```cpp
// ZenithTools/asset_linter.cpp
int RunAssetLinter(const char* szAssetPath, const char* szResultsPath)
{
    Zenith_Vector<LinterResult> axResults;
    // ... dispatch to per-asset-type lint functions ...
    WriteJsonResults(szResultsPath, axResults);
    return axResults.HasAnyFailure() ? 1 : 0;
}
```

---

## 4. The Full Asset Validator (CI)

Wraps the full §2 test suite into the existing `zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1)`. Two batches:

- **PR-gating batch:** Manifest + loadability + schema. ~1 minute.
- **Full asset validator (runs nightly):** All §2 tests including instantiation + substitution. ~5 minutes.

### 4.1 CI YAML sketch

```yaml
# .github/workflows/dp-assets.yml
- name: Asset lint (every changed asset)
  run: ZenithTools.exe lint --batch --paths-from changed.txt --out lint.json
- name: Asset manifest tests
  run: pwsh.exe -File zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1) --filter ^Test_Asset --headless
- name: Aggregate
  run: pwsh.exe -File Tools/aggregate_dp_results.ps1 -OutSummary asset_summary.json
```

Claude reads `lint.json` + `asset_summary.json` to determine whether the asset PR is healthy.

### 4.2 The substitution validator

The most important nightly job. For every asset class, it runs the test suite twice: once with the **S0 placeholder**, once with the **current best asset** (S1 or S2 if available). Both must pass identical tests.

If the S1 asset passes a test that the S0 placeholder failed, the placeholder test was *under-specified* and is amended. If the S1 fails a test that the S0 passed, the S1 asset is rejected. **Substitution must be backwards-compatible.**

This is the load-bearing CI job for the entire asset lifecycle.

---

## 5. TDD Workflow: Authoring a New Asset

To add a new asset to the game, follow this loop:

### 5.1 Step 1 — Define the contract

Add an entry to `Games/DevilsPlayground/Assets/manifest.json`:

```json
{
  "path": "game:Meshes/dp_item_burial_coin_a.zmodel",
  "kind": "item",
  "bbox_min": [0.0, 0.0, 0.0],
  "bbox_max": [0.05, 0.02, 0.05],
  "max_tris": 400,
  "material_count": 1,
  "sockets": ["Held"]
}
```

### 5.2 Step 2 — Write the asset test (fails)

Add `Test_AssetItem_BurialCoinSchema.cpp` (or extend the parameterised `Test_AssetMesh_LoadAndDimensions` table). Test fails because the file doesn't exist.

### 5.3 Step 3 — Author the placeholder

Open Blender. Model a tiny disc. Add a `Held` socket. Export to FBX. Run `ZenithTools.exe import` to produce `.zmodel`.

### 5.4 Step 4 — Run the linter

```bash
ZenithTools.exe lint --asset game:Meshes/dp_item_burial_coin_a.zmodel
```

Linter reports schema failures. Fix in Blender (wrong socket name? wrong axis up?). Re-export. Re-lint until clean.

### 5.5 Step 5 — Run the full test

```bash
pwsh.exe -File zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1) --filter BurialCoin --headless
```

Test passes. Commit.

### 5.6 Step 6 — Integration

The item appears in the game with the placeholder mesh. Gameplay tests pass. Done.

### 5.7 Step 7 (months later) — Substitute final asset

Final artist delivers `dp_item_burial_coin_a_v2.fbx`. Re-export. Re-run the same tests. If any fail, **the artist's submission is rejected and they iterate**. Once tests pass, replace the file in-place. Game ships with the final asset; gameplay-test coverage didn't move.

---

## 6. Per-Discipline Workflow

Each art discipline gets a slightly different workflow tuned to its tooling.

### 6.1 Character art workflow

1. Concept-art sheet (peer-reviewed; not test-gated).
2. Mesh + skeleton in DCC tool (Blender, Maya, 3ds Max).
3. Export FBX.
4. Run `ZenithTools.exe import` to produce `.zmodel`, `.zskel`.
5. Lint the result. Fail = fix in DCC.
6. Add the asset's animations one-by-one (typically idle, walk, jog, sprint, then unique anims).
7. Animation linter runs per-anim. Catches missing footstep events, missing canonical bones.
8. Integration test: spawn the character in a test scene, possess it, verify it walks.

### 6.2 Environment workflow

1. Block out in DCC tool.
2. Export FBX.
3. Lint.
4. Integration test: spawn 30 instances of the prop in a stress-test scene. Verify framerate budget proxy (operation counts, per [TestPlan.md](TestPlan.md) §4.4).

### 6.3 Material workflow

Materials are usually authored programmatically against a JSON dump from a DCC tool's material parameters (per existing `DPMaterials::Initialize` pattern). The author writes JSON, the import generates `.zmtrl`.

1. Author JSON.
2. Import generates `.zmtrl`.
3. Lint material (channels populated, textures resolve).
4. Integration test: apply to a test mesh, render to off-screen target (via `Zenith_RenderBus::GetSubmittedDrawCallsForTest` from [TestPlan.md §0.4](TestPlan.md)), assert at least one draw call was submitted.

### 6.4 VFX workflow

VFX configs are JSON-authored.

1. Author JSON for `dp_vfx_<name>.zdata`.
2. Lint particle config.
3. Integration test: spawn in test scene, tick 60 frames, assert particle count > 0 and < `maxParticles`.

### 6.5 Audio workflow (post-audio-system-engineering)

1. Source / record audio file (WAV or OGG).
2. Run `ZenithTools.exe audio_import` to validate format and produce `.zaudio` (or whatever the audio system's wrapper format becomes).
3. Lint audio (duration, sample rate, no clipping, loop seamless if tagged).
4. Integration test: trigger the gameplay event that emits this audio. Verify the emission appears in `Zenith_AudioBus::GetEmittedSoundsForTest()`.

### 6.6 UI workflow

1. Author sprite in DCC tool (Photoshop, Aseprite, etc.).
2. Export PNG.
3. Lint texture (dimensions, format).
4. Add entry to UI canvas authoring code (or `.zscen` UI section).
5. Integration test: load the scene, find the UI element by name, assert visible and at expected position.

---

## 7. Roll-Out Schedule

The asset test plan deploys in lockstep with the asset roll-out from [AssetManifest.md](AssetManifest.md) §12.

| Month | Asset stage delivered | Asset tests landing |
|---|---|---|
| 0 | S0 placeholders sourced (Mixamo + Kenney + Sketchfab CC0 — Synty rejected per AssetManifest §2.1.4) | Manifest + loadability tests for every initial placeholder. ~80 tests. |
| 1–2 | S0 in-game integration | Schema tests for char/env/item meshes. Skeleton bone-set tests. Animation duration tests. ~40 tests. |
| 3–4 | District mood paintings; S1 props begin | UI sprite tests. Particle config tests. ~25 tests. |
| 5–8 | S1 props + S1 characters | Substitution tests for every S0 → S1 swap. Material channel tests. ~30 tests. |
| 9–10 | S2 hero assets | Loop-continuity tests for hero animations. Audio emission tests as audio system lands. ~20 tests. |
| 11–13 | S2 villager sweep | Full anim event coverage (~360 anims × footstep events = many parameterised assertions). |
| 14–16 | S2 env outsource delivery | Cross-DCC export-format sanity tests (UV flip, normal-map convention, tangent space). |
| 17–18 | Final polish + LQA | Loc-string fit tests against final fonts. Edge case audio (clipping, loop seam) tests. |

**Total asset tests at ship:** ~180 individual test cases covering ~2,400 assets. Asset linter runs as part of `zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1)`, taking ~5 min nightly.

---

## 8. Anti-Patterns

The pitfalls to avoid when authoring asset tests.

1. **Asserting placeholder-specific properties.** Don't `ASSERT vertCount == 7689`. Use `ASSERT vertCount in [4000, 14000]`.
2. **Asserting "looks right."** Visual quality is a human-review concern, not a test concern. Tests assert *structure*; humans assert *quality*. If you find yourself writing `// look at the screenshot to verify`, you're in the wrong document.
3. **One-off asset tests.** Tests should be parameterised over a manifest. A new asset added to the manifest must automatically pick up the relevant tests. Avoid `Test_AssetItem_BurialCoinSchema.cpp` — prefer extending `Test_AssetItem_AllItemsSchema.cpp`.
4. **Tests that mutate global state without cleanup.** Asset tests run in batch with everything else; if your test leaks a particle emitter, downstream tests fail mysteriously.
5. **Verifying via stdout / log scrape.** Always check structured state (return values, JSON results), never the assertions log. The log is for humans; the JSON is for Claude.
6. **Skipping the substitution test.** This is the test that catches "final asset broke something the placeholder didn't." If you skip it, the day final art lands, gameplay tests will start failing one by one across the suite with no obvious cause.
7. **Audio tests before the audio system.** Audio assertion tests fail until the audio system ships in Phase 1. Either gate them on `ZENITH_AUDIO_SYSTEM_AVAILABLE` or accept the months of red. Recommend gating.
8. **Test-only blendshapes / sockets.** Don't author a special test socket that the gameplay doesn't use. Every authored bone / socket / blendshape must be consumed by either gameplay or by a documented placeholder-policy reason.

---

## 9. Claude-Driven Verification Loop

A worked example of how Claude verifies a new asset without human intervention:

1. **An artist commits** `dp_char_villager_devout.zmodel` and pushes to a feature branch.
2. **CI runs** `zenith test DevilsPlayground (Tools/ZenithCli/ZenithTestHarness.psm1) --filter Asset --headless`.
3. **The PR-gating batch** completes in ~1 minute. JSON results land in `Build/artifacts/test_results/devilsplayground/`.
4. **Claude reads** `asset_summary.json`. Sees `Test_AssetSkel_VillagerCanonicalBones` failed: `missing bone 'LeftHand'`.
5. **Claude opens** the artist's commit. Examines the `.zskel` via `ZenithTools.exe inspect`. Confirms `LeftHand` is exported as `L_Hand`.
6. **Claude knows** (from the canonical bone-set spec in §2.2) the gameplay code searches for `LeftHand`. Posts a PR comment naming the issue: *"`LeftHand` bone is exported as `L_Hand`; gameplay code looks up `LeftHand`. Either rename in DCC or update the canonical bone-set spec."*
7. **Artist fixes**, re-pushes. Claude re-runs. Tests pass. Auto-merge.

This loop is fully autonomous. No human screenshot, no human listening session, no human pixel inspection. The artist's quality is judged against the test suite's contract, and the contract is the same one the final art will be judged against.

---

## 10. Summary

This plan adds **~180 asset-validation tests** to the suite described in [TestPlan.md](TestPlan.md), bringing the total project test count to **~430 tests** at ship. Every asset in [AssetManifest.md](AssetManifest.md) — placeholder or final — passes through the same gates:

1. Exists at the expected path.
2. Loads through `Zenith_AssetRegistry`.
3. Matches the schema for its kind (bones, channels, sockets, format).
4. Stays within the platform budget.
5. Instantiates correctly in a scene.
6. Survives substitution by its final equivalent.

The asset linter (`ZenithTools.exe lint`) runs on every save and pre-commit, catching ~85% of asset bugs at the source. The full asset validator runs nightly, catching the remaining 15%.

**The artist's contract is the structural test suite.** When the test suite passes, the asset meets the structural contract (loads, has expected geometry/format/budget, instantiates in a scene without crashes). This is the **necessary** condition for shipping; it is not **sufficient**.

**Toned down 2026-05-12 per round-2 peer review.** Earlier versions of this doc claimed "zero human-in-the-loop asset verification." That was overstated. Structural tests catch structural bugs — they cannot judge:

- **Visual quality and artistic intent.** Does the Aelfric model look like a 1670s witch-finder? Does the final villager mesh read at 80m camera distance? Does the candlelight feel oppressive?
- **Gameplay readability.** Can a player distinguish the four MVP archetype silhouettes during 30 seconds of fog-of-war-shrouded chase?
- **Audio mix and emotional tone.** The bus instrumentation tells tests *whether* a sound fired; it does not tell anyone *if it sounded right*.
- **Animation naturalness.** Bone-pose tests catch malformed clips, not janky retargeting.

For all of these, **human review is mandatory**. The asset test suite is a quality *floor*, not a quality *ceiling*. Specifically: every S2 final-art substitution PR requires Tomos to look at the asset in-engine and sign off before the substitution merges. The autonomous loop runs the structural tests; the human runs the eye test.

This discipline keeps engineering moving at full speed during the production runway while preserving the human's role as the final arbiter of look and feel.

---

## Document map (where to look)

- **What assets do we need?** → [AssetManifest.md](AssetManifest.md)
- **What gameplay tests prove the game works?** → [TestPlan.md](TestPlan.md)
- **What gaps exist between prototype and ship?** → [Shortfalls.md](Shortfalls.md)
- **What is the game?** → [GameDesignDocument.md](GameDesignDocument.md)
- **How do I prove a placeholder asset is correct?** → *this document*
