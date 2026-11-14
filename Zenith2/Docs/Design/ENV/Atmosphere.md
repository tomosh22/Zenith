# ENV design: atmosphere, time of day and clouds

Section file of `Zenith2/Docs/Design/ENV.md`. The sky-view, transmittance and multiple-scattering LUTs, the sky draw with aerial perspective and the froxel fog are LIGHT's passes (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Atmosphere ownership"`); ENV owns the atmosphere parameters document, the weather modulation of those parameters, the time-of-day controller and the celestial state that drive them, and the clouds. This file specifies those, and the environment blocks through which they reach LIGHT and ENV's own GPU side: ENV computes them into a world resource each tick and the engine environment extractor, the one writer of the render environment, copies them (the orchestrator's ruling on X125, `Zenith2/Docs/Design/RPROD/Renderer.md → "Render environment"`).

## Scope of this file

| Concern | Heading |
|---|---|
| the physical model, the document, validation | "Atmosphere parameters" |
| runtime API | "Zenith_AtmosphereParameters header" |
| haze and fog from weather | "Weather modulation of the atmosphere" |
| the clock, its advance, commands and markers | "Time of day" |
| runtime API | "Zenith_TimeOfDay header" |
| sun, moon and stars | "Solar and lunar position" |
| cloud model, passes, tiers and budget | "Clouds" |
| the environment blocks and what reaches the render world | "Render environment extraction" |

## Atmosphere parameters

### Physical model

The model is LIGHT's (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Media model"`): a spherical planet with Rayleigh and Mie layers of exponential density, an ozone tent layer, height fog and ground albedo, carried in `Flux_AtmosphereParams` and `Flux_FogMediumParams`. ENV's atmosphere document authors every value of that table, adds the celestial and time-of-day settings, and converts authoring units (degrees, kilometres, per-kilometre coefficients) to LIGHT's SI units at cook.

### Atmosphere document

`*.atmosphere.json`, an authoring document; a world uses the one its `Zenith_EnvironmentSettingsComponent` names:

```json
{
	"schemaVersion": 1,
	"id": "3f81…",
	"planet": { "groundRadiusKm": 6360.0, "atmosphereHeightKm": 100.0, "groundAlbedo": [ 0.3, 0.3, 0.3 ] },
	"rayleigh": { "scatteringPerKm": [ 0.005802, 0.013558, 0.0331 ], "scaleHeightKm": 8.0 },
	"mie": { "scatteringPerKm": 0.003996, "absorptionPerKm": 0.0044, "scaleHeightKm": 1.2, "anisotropy": 0.8 },
	"ozone": { "absorptionPerKm": [ 0.00065, 0.001881, 0.000085 ], "centreKm": 25.0, "halfWidthKm": 15.0 },
	"heightFog": { "density": 0.002, "albedo": 0.9, "falloff": 0.05, "baseHeight": 0.0, "anisotropy": 0.3, "emission": [ 0, 0, 0 ] },
	"sun": { "illuminanceLux": 120000.0, "angularDiameterDegrees": 0.53, "colour": [ 1.0, 1.0, 1.0 ] },
	"moon": { "illuminanceLux": 0.25, "angularDiameterDegrees": 0.52, "texture": "…guid" },
	"stars": { "texture": "…guid", "intensityNits": 0.002 },
	"timeOfDay": { "latitudeDegrees": 48.0, "dayOfYear": 172, "startHours": 9.0, "dayLengthRealSeconds": 1440, "northDegrees": 0.0 },
	"clouds": { "layerAltitudeM": 1800.0, "layerThicknessM": 900.0, "baseNoise": "…guid", "detailNoise": "…guid", "scale": 0.00012 }
}
```

### Validation limits

| Field | Accepted range | Failure |
|---|---|---|
| ground radius | 100 … 100,000 km | `ENV_ERROR_ATMOSPHERE_INVALID`, field index |
| atmosphere height | 1 … 1,000 km | as above |
| every coefficient | finite, ≥ 0, ≤ 1 per km | as above |
| scale heights | 0.01 … 100 km | as above |
| Mie anisotropy, fog anisotropy | −0.99 … 0.99 | as above |
| sun illuminance | 0 … 200,000 lux | as above |
| sun colour | each component finite in [0, 1], the largest above 0 | as above |
| moon illuminance | 0 … 10 lux | as above |
| latitude | −90 … 90 degrees | as above |
| day of year | 1 … 365 | as above |
| start hours | [0, 24) | as above |
| day length | 0 (a frozen clock) or 60 … 604,800 real seconds | as above |

The cook refuses an invalid document; the runtime's atmosphere loader refuses a cooked file whose values fall outside the same limits (defence against a corrupt cooked file) and the world then uses LIGHT's defaults (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Media model"`) with `ENV_WARNING_ATMOSPHERE_FALLBACK`.

## Zenith_AtmosphereParameters header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_AtmosphereParameters.h
#pragma once
#include "ZenithCore/Zenith_Result.h"
#include "ZenithCore/Zenith_Vector.h"
#include "ZenithCore/Zenith_Colour.h"
#include "ZenithFlux/Flux_Atmosphere.h"            // LIGHT's Flux_AtmosphereParams, Flux_FogMediumParams
#include "ZenithEnvironment/Zenith_EnvironmentErrors.h"

struct Zenith_WeatherParams;

/// @contract value Zenith_AtmosphereParameters
/// Ownership: Plain value; part of the loaded Zenith_AtmosphereAsset (Bake.md "Runtime asset objects"), copied into
///   the world's Zenith_EnvironmentState when the settings component names it.
/// Thread affinity: IMMUTABLE. Members non-const: a member of documents and runtime state that are filled field by
///   field.
/// Invalidation: None.
/// Units: SI: metres, per-metre coefficients, lux, radians.
/// Errors: Zenith_ValidateAtmosphere returns ENV_ERROR_ATMOSPHERE_INVALID with the field index of the table
///   "Validation limits".
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_AtmosphereParameters
{
	float m_fGroundRadius = 6360000.0f;
	float m_fAtmosphereHeight = 100000.0f;
	Zenith_Colour m_xGroundAlbedo;
	Zenith_Vector3 m_xRayleighScattering;      // per metre
	float m_fRayleighScaleHeight = 8000.0f;
	float m_fMieScattering = 3.996e-6f;
	float m_fMieAbsorption = 4.40e-6f;
	float m_fMieScaleHeight = 1200.0f;
	float m_fMieAnisotropy = 0.8f;
	Zenith_Vector3 m_xOzoneAbsorption;
	float m_fOzoneCentre = 25000.0f;
	float m_fOzoneHalfWidth = 15000.0f;
	float m_fFogDensity = 0.002f;
	float m_fFogAlbedo = 0.9f;
	float m_fFogFalloff = 0.05f;
	float m_fFogBaseHeight = 0.0f;
	float m_fFogAnisotropy = 0.3f;
	Zenith_Colour m_xFogEmission;
	float m_fSunIlluminance = 120000.0f;       // lux at the top of the atmosphere
	float m_fSunAngularRadius = 0.004625f;     // radians
	Zenith_Colour m_xSunColour;
	float m_fMoonIlluminance = 0.25f;          // full-moon lux at the top of the atmosphere
	float m_fMoonAngularRadius = 0.004538f;
	float m_fStarIntensity = 0.002f;           // nits
};

/// @contract functions Zenith_AtmosphereFunctions
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: SI.
/// Errors: Zenith_ValidateAtmosphere returns ENV_ERROR_ATMOSPHERE_INVALID; the others assume validated inputs.
/// Version: since Z2 Stage 3.
/// @endcontract
Zenith_Result<void, Zenith_EnvironmentError> Zenith_ValidateAtmosphere(const Zenith_AtmosphereParameters& xParams);
// "Weather modulation of the atmosphere": the parameters in effect for the blended weather.
Zenith_AtmosphereParameters Zenith_ApplyWeatherToAtmosphere(const Zenith_AtmosphereParameters& xBase,
	const Zenith_WeatherParams& xWeather);
// The Flux_AtmosphereParams and Flux_FogMediumParams values LIGHT consumes (field-by-field copy with unit conversion).
void Zenith_ToFluxAtmosphere(const Zenith_AtmosphereParameters& xParams, Flux_AtmosphereParams& xAtmosphereOut,
	Flux_FogMediumParams& xFogOut);
```

## Weather modulation of the atmosphere

`Zenith_ApplyWeatherToAtmosphere` returns the base parameters with: Mie scattering and absorption multiplied by the weather's `hazeScale`; fog density multiplied by `fogDensityScale`; fog albedo raised toward 0.98 by `0.5 × cloudCoverage` (overcast fog is whiter); ground albedo blended toward 0.8 by the converged snow cover. The function is evaluated every tick in phase 1 from the tick's blended weather when the environment blocks are built ("Render environment extraction"), so a weather transition fades the sky and fog with it. The blocks raise `m_uSkyRevision` on every tick whose atmosphere, fog or star values differ bitwise from the previous tick's; LIGHT recomputes its LUTs only when the revision it last used differs (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Transmittance LUT"`), and during a transition that is every tick.

## Time of day

### Clock representation

Time of day is simulation state in `Zenith_EnvironmentState`, advanced on the fixed tick (Z2-D-019):

| Field | Type | Meaning |
|---|---|---|
| `m_ulTimeOfDayUs` | `uint64` | microseconds since local midnight, in `[0, 86,400,000,000)` |
| `m_ulDay` | `uint64` | whole days elapsed since the world started (moon phase, calendar) |
| `m_ulTicksPerGameDay` | `uint64` | ticks one game day takes; 0 freezes the clock |
| `m_ulRemainder` | `uint64` | the Bresenham remainder of "Advance per tick" |

### Advance per tick

With `D = m_ulTicksPerGameDay` and `U = 86,400,000,000` microseconds per game day, every tick adds `q = U / D` (integer division) microseconds and `r = U mod D` to the remainder; when the remainder reaches `D`, one more microsecond is added and `D` is subtracted. After exactly `D` ticks the clock has advanced exactly one day, with no drift and no floating point. Crossing midnight increments `m_ulDay`. At world start `D = round(dayLengthRealSeconds × tickHz)` from the atmosphere document and the world's `sim.tickHz`; `D = 0` when the day length is 0.

### Commands

| Command (`Zenith_EnvironmentCommand` kind) | Payload | Effect at the next phase 1 |
|---|---|---|
| `ENVIRONMENT_COMMAND_SET_TIME_OF_DAY` | hours `f64` in [0, 24), day delta `i32` | sets the clock (`round(hours × 3.6e9)` µs), adds the day delta, zeroes the remainder |
| `ENVIRONMENT_COMMAND_SET_TIME_SCALE` | scale `f64` in [0, 10,000] | `D = scale > 0 ? max(1, round(dayLengthRealSeconds × tickHz / scale)) : 0`; zeroes the remainder |
| `ENVIRONMENT_COMMAND_REQUEST_WEATHER`, `_FORCE_WEATHER`, `_RELEASE_WEATHER` | state name hash `u64`, transition seconds `f32`, hold `bool` | the weather commands of `Zenith2/Docs/Design/ENV/WaterWeather.md → "Transition table"` |

Commands are module command kinds (`Zenith2/Docs/Architecture/CompositionRoot.md → "Module interfaces"`, `Zenith_CommandKindDesc`, allowed in phases 2 to 10): each playback appends the payload to the environment state's pending-command array (at most 64 per tick, `QUEUE_FULL_POLICY_REJECT` at merge, so an over-capacity command is an entity-level rejection in merge order, [Z2-N-CONTRACT-346]); `Environment.Advance` consumes them at the next tick's phase 1 in merge order. A script therefore sees a time-of-day change it requested in the next tick, identically on replay.

### Markers and events

`Environment.Advance` emits `Zenith_TimeOfDayEvent` (`TYPE_KIND_EVENT` "Zenith.Environment.TimeOfDay") when the clock crosses a marker: each whole hour (`TIME_OF_DAY_MARKER_HOUR`), sunrise and sunset (the sun's elevation crossing −0.833°, `TIME_OF_DAY_MARKER_SUNRISE`, `_SUNSET`), and civil dawn and dusk (−6°, `TIME_OF_DAY_MARKER_DAWN`, `_DUSK`). Markers are detected by comparing the elevation (or hour) before and after the tick's advance, so a time-scale jump over several markers emits each crossed marker in time order.

## Zenith_TimeOfDay header

```cpp
// Zenith2/Engine/ZenithEnvironment/Zenith_TimeOfDay.h
#pragma once
#include "ZenithCore/Zenith_Vector.h"

/// @contract value Zenith_TimeOfDaySettings
/// Ownership: Plain value from the atmosphere document.
/// Thread affinity: IMMUTABLE. Members non-const: a member of documents and runtime state that are filled field by
///   field.
/// Invalidation: None.
/// Units: Radians for latitude and north offset; seconds for the day length.
/// Errors: Validated with the atmosphere document.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_TimeOfDaySettings
{
	float m_fLatitude = 0.8378f;             // radians (48°)
	uint32 m_uDayOfYear = 172;
	float m_fNorthOffset = 0.0f;             // radians: world −Z points north when 0
	uint32 m_uDayLengthRealSeconds = 1440;
	double m_dStartHours = 9.0;
};

/// @contract value Zenith_TimeOfDay
/// Ownership: Plain value, part of Zenith_EnvironmentState; saved by its reflected properties.
/// Thread affinity: PHASE(Input) for writes; IMMUTABLE to readers within a tick.
/// Invalidation: None.
/// Units: Microseconds of game time; ticks.
/// Errors: None; commands are validated at playback.
/// Version: since Z2 Stage 3. Saved; a layout change is a save migration step.
/// @endcontract
struct Zenith_TimeOfDay
{
	uint64 m_ulTimeOfDayUs = 0;
	uint64 m_ulDay = 0;
	uint64 m_ulTicksPerGameDay = 0;
	uint64 m_ulRemainder = 0;

	double GetHours() const { return static_cast<double>(m_ulTimeOfDayUs) / 3.6e9; }
};

/// @contract value Zenith_CelestialState
/// Ownership: Plain value computed from a Zenith_TimeOfDay.
/// Thread affinity: IMMUTABLE. Members non-const: the owning runtime state reassigns its copy every tick.
/// Invalidation: None.
/// Units: Unit direction vectors toward the body in world space; radians; the moon phase in [0, 1) (0 new, 0.5 full).
/// Errors: None.
/// Version: since Z2 Stage 3.
/// @endcontract
struct Zenith_CelestialState
{
	Zenith_Vector3 m_xSunDirection;          // toward the sun
	float m_fSunElevation = 0.0f;
	Zenith_Vector3 m_xMoonDirection;
	float m_fMoonElevation = 0.0f;
	float m_fMoonPhase = 0.0f;
	float m_fMoonIlluminationFraction = 0.0f;
	float m_fStarRotation = 0.0f;            // sidereal rotation of the star field about the celestial pole
};

/// @contract functions Zenith_TimeOfDayFunctions
/// Ownership: Pure functions over values.
/// Thread affinity: ANY_THREAD.
/// Invalidation: None.
/// Units: As the types above.
/// Errors: None.
/// Version: since Z2 Stage 3. The formulas are fixed; a change alters every saved world's sky and is a save migration.
/// @endcontract
Zenith_TimeOfDay Zenith_InitialTimeOfDay(const Zenith_TimeOfDaySettings& xSettings, uint32 uTickHz);
void Zenith_AdvanceTimeOfDay(Zenith_TimeOfDay& xClock);                    // "Advance per tick"
Zenith_CelestialState Zenith_ComputeCelestialState(const Zenith_TimeOfDaySettings& xSettings,
	const Zenith_TimeOfDay& xClock, double dFractionOfTick, uint32 uTickHz);
```

## Solar and lunar position

### Solar position

`Zenith_ComputeCelestialState` evaluates, in `double`, with `N` = the day of year plus `m_ulDay`, wrapped to 1 … 365, and `h` = the local solar time in hours (the clock plus `dFractionOfTick` of a tick for the frame's interpolated time):

| Step | Formula |
|---|---|
| declination | `δ = 23.44° × sin(2π (284 + N) / 365)` (Cooper's approximation) |
| hour angle | `H = 15° × (h − 12)` |
| elevation | `sin α = sin φ sin δ + cos φ cos δ cos H` |
| azimuth (from north, clockwise) | `A = atan2(−sin H × cos δ, cos φ sin δ − sin φ cos δ cos H)` |
| world direction | `(sin A cos α, sin α, −cos A cos α)` rotated about +Y by `m_fNorthOffset` |

The result is narrowed to `float` last. Simulation-visible uses (the markers) read the elevation computed at the tick's clock (`dFractionOfTick = 0`), so they are deterministic; the renderer's sun uses the interpolated value.

### Lunar position and phase

The moon's phase is `((m_ulDay + h / 24) mod 29.530589) / 29.530589`; its direction is the sun direction rotated about the celestial axis (the world axis toward the pole at elevation `φ` in the north direction) by `2π × phase`, with the moon's declination offset ignored. The illuminated fraction is `(1 − cos(2π × phase)) / 2`, which scales `m_fMoonIlluminance`.

### Night sky

The star field (an HDR cubemap of the `stars.texture`, LIGHT samples it behind the atmosphere as the sky's emissive background at `m_fStarIntensity`) rotates about the celestial axis by `2π × (h / 23.9344696)`; LIGHT's sky draw adds it where the sky transmittance lets it through (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Star field"`), reading the cube, the rotation and the intensity from the environment blocks' star fields.

### Sun and moon illuminance

The sun and the moon occupy the render environment's two explicit light slots, `Flux_RenderEnvironment::m_xSun` and `m_xMoon` (the X125 ruling, `Zenith2/Docs/Design/RPROD/Renderer.md → "Render environment"`), which LIGHT treats as directional lights 0 and 1 whatever other directional lights the scene holds (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light and decal list build"`). ENV fills them in the environment blocks as two `Flux_RenderLightDelta` values (`Zenith2/Docs/Design/LIGHT/Lighting.md → "The delta record"`), every member set as the table below states, from the celestial state at the tick's clock. The flags meet LIGHT's slot flag contract (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`, row "slot flag contract"; LIGHT's row L39): the sun carries `FLUX_LIGHT_FLAG_CASTS_SHADOW`, `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` and `FLUX_LIGHT_FLAG_VOLUMETRIC`; the moon carries `FLUX_LIGHT_FLAG_VOLUMETRIC`, plus `FLUX_LIGHT_FLAG_CASTS_SHADOW` while its intensity exceeds 1 lux; ENV never sets the moon's contact bit (LIGHT clears it on slot 1 in any case), and every other bit of `m_uFlags` is zero. "Every other bit zero" concerns `m_uFlags` alone: the lighting-channel mask and the volumetric scale are separate members, which ENV sets non-zero, because a zero mask lights no surface and a zero scale injects nothing into the froxel volume. Which body a slot holds is its position, never a flag (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Light record encodings"`). LIGHT multiplies by the transmittance per shaded point (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Sun transmittance"`). A body below the horizon by more than its angular radius plus 0.1 rad keeps its slot, its flags and every other member with intensity 0, so a scene light never takes a slot over at night.

| Member | `m_xSun` | `m_xMoon` |
|---|---|---|
| `m_xKey` | `Flux_MakeNamedLightKey("Zenith.Environment.Sun", uWorldTag)` | `Flux_MakeNamedLightKey("Zenith.Environment.Moon", uWorldTag)` |
| `m_eOp` | `FLUX_LIGHT_DELTA_OP_UPDATE`: a slot is restated whole every tick and never removed | as the sun |
| `m_eType` | `FLUX_LIGHT_TYPE_DIRECTIONAL` | as the sun |
| `m_uLightingChannels` | 0x01, channel 0 (LIGHT's delta default); the froxel volume injects every light whose mask contains channel 0 (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Lighting channels"`) | 0x01 |
| `m_uVolumetricScale` | 128 (1.0, `unorm8`) | 128 |
| `m_uFlags` | `FLUX_LIGHT_FLAG_CASTS_SHADOW`, `FLUX_LIGHT_FLAG_CONTACT_SHADOWS` and `FLUX_LIGHT_FLAG_VOLUMETRIC` (0x7) | `FLUX_LIGHT_FLAG_VOLUMETRIC` (0x4); 0x5 (with `FLUX_LIGHT_FLAG_CASTS_SHADOW`) while `m_fIntensity` > 1 lux |
| `m_xPositionWS`, `m_xTangentWS` | zero (unused for a directional light) | zero |
| `m_fRange` | `FLT_MAX` | `FLT_MAX` |
| `m_xDirectionWS` | `−m_xSunDirection` of `Zenith_CelestialState` (the direction the light travels), unit | `−m_xMoonDirection` |
| `m_fSourceRadius` | `m_fSunAngularRadius`, radians (half the document's `angularDiameterDegrees`) | `m_fMoonAngularRadius` |
| `m_fIntensity` | `m_fSunIlluminance`, lux at the top of the atmosphere; 0 below the horizon cut | `m_fMoonIlluminance × m_fMoonIlluminationFraction`; 0 below the cut |
| `m_xTint` | `m_xSunColour` divided by its largest component (LIGHT's "max component 1.0"; the validation limits keep it above 0), alpha 1 | (1, 1, 1, 1) |
| `m_fSpotInnerHalfAngle`, `m_fSpotOuterHalfAngle`, `m_fWidth`, `m_fHeight` | 0 | 0 |
| `m_fShadowPriorityBias`, `m_uShadowMaxTileTexels`, `m_fShadowNormalOffsetTexels`, `m_fShadowDepthBias`, `m_uReserved` | LIGHT's defaults: 1, 0, 1.5, 0, 0 | as the sun |

| Case | Slots | Scene directional lights |
|---|---|---|
| settings drive the bodies (`m_bDriveSunAndMoon`, the default) | `m_bHasSun` and `m_bHasMoon` true | dropped by LIGHT's directional limit of two (its overflow rule, counted in `Flux.Lights.DirectionalDropped`), so the slots always win; ENV logs `ENV_WARNING_SCENE_SUN_IGNORED` once per world when the world also holds an entity with LIGHT's directional light component, which the extractor sees through its declared read of that component type by stable name |
| settings do not drive them | both false; ENV writes no light | the scene's directional light components, in LIGHT's directional order |
| no settings component, or the atmosphere asset failed | both false, `m_bValid` false | as the row above, with LIGHT's world-lighting defaults for the atmosphere |

The slots carry the tick's celestial state: at the default day length of 1,440 real seconds the sun moves 0.0042° per 60 Hz tick, below a texel of the first shadow cascade, so no per-frame interpolation is applied. The CPU mirror `Flux_ComputeAtmosphereTransmittance` (LIGHT's reference function) gives gameplay the ground illuminance (`Zenith_EnvironmentState::GetSunGroundIlluminance`).

## Clouds

### Cloud model

Clouds are a documented approximation, not a physical simulation. Coverage and density come from the blended weather; shape comes from two tileable 3D noise volumes (Perlin–Worley, `R8G8B8A8_UNORM`, 128³ for base shapes and 32³ for detail) that ENV cooks itself ("Cloud noise volumes"), scrolled by the wind at the cloud layer's altitude.

| Quantity | Rule |
|---|---|
| density at a point in the layer | `ρ = saturate(remap(noise(p × scale + wind × t), 1 − coverage, 1, 0, 1)) × density × heightGradient(y)`, with `remap` of "Cloud noise generator" (its `b − a` is the coverage, positive whenever a cloud pass runs) |
| height gradient | a smooth rise over the bottom 20% of the layer and fall over the top 40% |
| lighting | the sun's transmittance through the atmosphere at the layer altitude, Beer's law through the cloud with a powder term, two-lobe Henyey–Greenstein (g = 0.8 and −0.3, blended 0.7/0.3), ambient from the sky-view LUT's zenith and horizon |

### Cloud noise volumes

GEOM's `.ztex` holds 2D images alone, so a 3D volume is cooked by its owner (GEOM's row GM30), and ENV owns these:

| Element | Specification |
|---|---|
| sources | two engine-content documents, `Zenith2/Engine/ZenithEnvironment/Content/Clouds/BaseNoise.cloudnoise.json` and `DetailNoise.cloudnoise.json` (`Zenith2/Docs/Design/ENV/Bake.md → "Cloud noise document schema"`); the atmosphere document names the two cooked assets |
| asset kind | code 34 of ASSET's closed table: `ASSET_KIND_CLOUD_NOISE`, stable name `Environment.CloudNoise`, script name `CloudNoise`, runtime object `Flux_CloudNoiseResource`, owner ENV, extension `.zcloudnoise`, magic `ZCLN`, memory subsystem RENDERING, fallback NULL_OBJECT, cacheable, GPU resident; added in the diff that lands the clouds, with ASSET as reviewer, by ASSET's growth rule (`Zenith2/Docs/Design/ASSET.md → "Asset kinds"`, row EN22), taking the next free code if another area has taken 34 by then |
| cook | ENV's importer `env.cloudNoise` reads the document and writes it as the product's intermediate; ENV's step `env.cloudNoise` generates the volume below and writes the file of `Zenith2/Docs/Design/ENV/Bake.md → "Cloud noise file"`; key inputs the document bytes and `uENV_CLOUD_NOISE_ALGORITHM_VERSION` = 1 |
| generation | the generator of "Cloud noise generator" below, per texel of mip 0 of an N³ volume (N = 128 base, 32 detail), in `double`, every lattice index wrapped so the volume tiles; the hash is the pinned one (D-SEL-HASH (open, Stage 1)) |
| quantisation and chain | each channel `floor(v × 255 + 0.5)` clamped to [0, 255]; mip l + 1 of the full chain is the 2 × 2 × 2 integer mean `(s + 4) / 8` per channel of mip l |
| runtime | `Flux_CloudNoiseKindLoader` (`Zenith2/Engine/ZenithFlux/Internal/Environment/`, ENV's GPU side) creates one 3D `R8G8B8A8_UNORM` `Flux_PersistentReadOnlyTexture` with the file's chain (`Zenith2/Docs/Rendering/GpuResources.md → "Persistent resources"`) and queues its upload; `Flux_CloudNoiseResource::GetSRV()` is its descriptor; a headless world uses ASSET's validate-only loader |
| reference | `Environment.Advance` requests both volumes at `ASSET_PRIORITY_NORMAL` when an atmosphere asset activates and places their handles in `Flux_EnvironmentClouds` once they are active; a null base handle adds no cloud pass, a null detail handle samples the base volume alone, and the S24 low tier binds the base volume alone |

`Test_CloudNoise_CookDeterministicAndTileable` cooks each document twice and compares the bytes, and asserts that texels across opposite faces of every mip continue the noise (the wrap); `Test_EnvironmentReaders_RejectOverLimitCounts` covers the reader.

### Cloud noise generator

```cpp
// Zenith2/Tools/ZenithCook/Internal/Environment/Zenith_CloudNoiseGenerator.h (cook-side; called by the step env.cloudNoise)
// A draw: a value in [0, 1) for (key, seed, draw index). The production draw is
// Zenith_RandomToUnitDouble(Zenith_Hash64U64(uDraw, Zenith_Hash64U64(ulKey, ulSeed))); tests pass their own.
using Zenith_PfnCloudNoiseDraw = double (*)(uint64 ulKey, uint64 ulSeed, uint32 uDraw, void* pUserData);
double Zenith_CloudNoiseProductionDraw(uint64 ulKey, uint64 ulSeed, uint32 uDraw, void* pUserData);
// The four channels of texel (uI, uJ, uK) of mip 0, before quantisation.
void Zenith_EvaluateCloudNoiseTexel(const Zenith_CloudNoiseDocument& xDocument, uint32 uI, uint32 uJ, uint32 uK,
	Zenith_PfnCloudNoiseDraw pfnDraw, void* pUserData, double adOut[4]);
// The whole quantised chain in the layout of section 0x0702; ENV_ERROR_INVALID_DESC when axOut's size is not 4 × Σ (N >> l)³.
Zenith_Result<void, Zenith_EnvironmentError> Zenith_GenerateCloudNoise(const Zenith_CloudNoiseDocument& xDocument,
	Zenith_PfnCloudNoiseDraw pfnDraw, void* pUserData, Zenith_Span<uint8> axOut);
```

| Term | Definition (all arithmetic in `double`, integer arithmetic exact) |
|---|---|
| texel centre | `q = ((i + 0.5) / N, (j + 0.5) / N, (k + 0.5) / N)` for texel (i, j, k) of mip 0: i along x (fastest in the file), j along y, k along z |
| wrap | `w(n, f) = ((n mod f) + f) mod f` per component, so a negative neighbour index wraps |
| `pack3` | `pack3(a, b, c) = (uint64(a) << 42) OR (uint64(b) << 21) OR uint64(c)` for wrapped indices below 2²¹: x in bits 42–62, y in bits 21–41, z in bits 0–20, bit 63 zero |
| draw | `D(key, s, k) = pfnDraw(key, s, k)`, the production draw above; `seed` is the document's `seed` |
| Worley `W(q, f)` | `x = q × f`, `c = floor(x)`; for each of the 27 cells `n = c + δ`, δ ∈ {−1, 0, 1}³, the feature point `F(n) = n + (D(key, seed, 0), D(key, seed, 1), D(key, seed, 2))` with `key = pack3(w(n, f))`; `W = clamp(1 − min over n of ‖x − F(n)‖₂, 0, 1)`: the Euclidean distance in cell units, measured to the unwrapped n so it is continuous across the wrap |
| Worley sum | `Wf(q, f) = 0.625 W(q, f) + 0.25 W(q, 2f) + 0.125 W(q, 4f)` |
| gradient of lattice point L at frequency f | `γ(L) = normalize(2 × (d0, d1, d2) − 1)` with `d_k = D(pack3(w(L, f)), seed ^ 0x47726164, k)`, k = 0, 1, 2; γ = (0, 1, 0) when `‖2 × (d0, d1, d2) − 1‖₂ < 1e-12` |
| gradient noise `g(q, f)` | `x = q × f`, `i0 = floor(x)`, `u = x − i0`; each corner `L = i0 + (a, b, c)`, a, b, c ∈ {0, 1}, contributes `dot(γ(L), u − (a, b, c))`; the eight are blended trilinearly with the weight `s(u_x)` on axis x for a = 1 and `1 − s(u_x)` for a = 0 (likewise y and z), `s(t) = 6t⁵ − 15t⁴ + 10t³` |
| gradient sum | `G(q, f) = saturate(0.5 + 0.5 × (Σ for o = 0 … 3 of 0.5^o × g(q, f × 2^o)) / 1.875)`; 1.875 is the amplitude sum 1 + 0.5 + 0.25 + 0.125 |
| `remap` | `remap(x, a, b, c, d) = c + (x − a) × (d − c) / (b − a)`, unclamped; every use has `b > a` (the base volume's `b − a = 2 − Wf ≥ 1`) |
| base volume | R = `saturate(remap(G(q, 8), Wf(q, 4) − 1, 1, 0, 1))`, G = `Wf(q, 8)`, B = `Wf(q, 16)`, A = `Wf(q, 32)` |
| detail volume | R = `Wf(q, 8)`, G = `Wf(q, 16)`, B = `Wf(q, 32)`, A = 1 |

Two tests in `ZenithCook_Tests` pin this reading independently of the pinned hash, so two workers cannot pass them with different formulas:

- `Test_CloudNoise_GeneratorGoldenTexels` evaluates the two engine documents (base N = 128, detail N = 32) with the test draw `D ≡ 0.5`, which puts every feature point at its cell centre and gives every lattice point the (0, 1, 0) fallback gradient, so each value follows from the formulas alone, and asserts the mip-0 texels of the table below after quantisation (R, G, B, A). A different octave normalisation, fade, distance metric or fallback changes at least half of them.
- `Test_CloudNoise_GeneratorDrawArguments` evaluates texel (0, 0, 0) of a base document with a recording draw and asserts the argument set: the Worley draws at f = 4 use the document's seed, draw indices 0, 1 and 2, and the 27 keys `pack3(w(n, 4))` for n ∈ {−1, 0, 1}³ (among them `pack3(3, 3, 3)` = 0xC0000600003); the gradient draws at f = 8 use `seed ^ 0x47726164`, indices 0, 1 and 2, and the 8 keys `pack3(a, b, c)`, a, b, c ∈ {0, 1} (among them `pack3(1, 0, 0)` = 0x40000000000 and `pack3(0, 1, 0)` = 0x200000).

| Volume | Texel (i, j, k) → RGBA |
|---|---|
| base | (0, 0, 0) → 189, 57, 79, 124; (1, 2, 3) → 186, 122, 156, 161; (17, 63, 5) → 171, 104, 124, 161; (127, 127, 127) → 181, 57, 79, 124; (64, 0, 31) → 189, 57, 79, 124; (5, 100, 77) → 175, 155, 171, 161; (99, 13, 42) → 157, 149, 171, 161; (31, 31, 31) → 181, 57, 79, 124 |
| detail | (0, 0, 0) → 124, 158, 172, 255; (1, 2, 3) → 161, 158, 172, 255; (5, 30, 17) → 193, 158, 172, 255; (31, 31, 31) → 124, 158, 172, 255; (16, 0, 8) → 124, 158, 172, 255; (7, 19, 25) → 141, 158, 172, 255; (30, 1, 12) → 161, 158, 172, 255; (11, 22, 3) → 141, 158, 172, 255 |

### Cloud layer pass

Stage 4, every tier: `Env_AddCloudLayer` (`Zenith2/Shaders/Environment/Passes/CloudLayer.slang` → `Flux_CloudLayerParams`, compute, once per frame, shared by the views of a world) evaluates the cloud model as a thin layer by integrating 8 (S24) or 16 (desktop) steps through the layer along each direction of a sky-view parameterised texture (192 × 108, the sky-view LUT's layout), writing in-scattered radiance (R11G11B10F, pre-exposed by the main view's pre-exposure) and transmittance (R8) targets into `Flux_FrameShared::m_xCloudLayer` and `m_xCloudTransmittance`; LIGHT's sky draw composites them: `sky = cloudRadiance + cloudTransmittance × sky` behind the aerial-perspective term (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Cloud layer composite"`, whose `Flux_SkyDrawParams` fields are pending X103). The pass is not added when coverage × density is 0.

### Volumetric clouds

Stage 5, desktop high tier: `Env_AddVolumetricClouds` raymarches the layer in a quarter-resolution target with 64 primary steps and 6 light steps, a blue-noise jittered start, and temporal reprojection with a 1/16 update per pixel per frame into a persistent per-view history (a view-owned history like TAA's, keyed by the view ID, [Z2-N-REND-598]); the result is upsampled with a depth-aware filter and composited in the sky draw as above, and occludes the sun disc. The cloud layer pass remains the fallback on every other tier.

### Cloud shadows

Stage 5, desktop and S24 high: a 512 × 512 cloud-shadow map (R8), the transmittance of the cloud layer along the sun direction over a 4 km square centred on the environment anchor snapped to 64 m (computed in phase 1, "Extraction steps" step 7), written by `Env_AddCloudShadowMap` once per frame into `Flux_FrameShared::m_xCloudShadowMap`; LIGHT's directional light evaluation multiplies the sun's contribution by it (`Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Cloud shadow on the sun"`), with the square's origin, extent and outside transmittance from the environment blocks.

### Cloud quality tiers

| Tier class | Technique | Steps | Fallback |
|---|---|---|---|
| S24 low | cloud layer, base noise only | 8 | coverage-only flat colour when `environment.clouds.enabled` is false |
| S24 high | cloud layer, base plus detail noise | 8 | cloud layer low |
| desktop low | cloud layer, base plus detail noise | 16 | S24 high |
| desktop high | volumetric clouds (S5), cloud layer until Stage 5 | 64 primary, 6 light | cloud layer |

The tier is `environment.clouds.tier` (`layerLow`, `layerHigh`, `volumetric`; allowed in the quality-tier layer). The S24 fallback required of Stage 4 is the cloud layer at 8 steps, budgeted inside `Z2-B-ENV-001` Sky and atmosphere GPU together with LIGHT's sky passes (the row names sky and atmosphere; ENV owns it).

### Cloud error budget

| Quantity | Reference | Budget | Test |
|---|---|---|---|
| cloud layer radiance vs a CPU integrator of the same model at 256 steps | test-code integrator | FLIP mean within the pending row "Cloud layer approximation error" | `Test_Clouds_LayerMatchesReference` |
| volumetric clouds (Stage 5) vs the same pass at 512 steps with no temporal reuse | GPU reference run | FLIP mean within the pending row "Volumetric cloud approximation error" | `Test_Clouds_VolumetricMatchesHighStepReference` |
| temporal stability: mean absolute frame-to-frame luminance change with a static camera and frozen wind | zero | ≤ 1% of the mean luminance after 32 frames | `Test_Clouds_TemporallyStable` |
| coverage 0 | the sky without clouds | bit-identical sky pixels | `Test_Clouds_ZeroCoverageAddsNothing` |

## Render environment extraction

### Blocks ENV writes

The render environment has one writer, the engine environment extractor `Render.ExtractEnvironment` (RPROD's, phase 12, one per world), which calls `SetEnvironment` once per world per frame ([Z2-N-REND-008]); ENV supplies what it copies, and never calls `SetEnvironment` (the X125 ruling; ENV's block setters of revision 0 are withdrawn).

<!-- assumption: pending RPROD, LIGHT, REND (X125, RP25, L31) -->
| Element | Specification |
|---|---|
| the blocks | `Flux_EnvironmentBlocks` (`Zenith2/Docs/Design/ENV.md → "Render-world records"`), a ZenithFlux type ENV declares in `Flux_Environment.h`, so ZenithRuntime names it without a module edge |
| where they live | the world resource `Zenith.Environment.RenderBlocks` of that type (the declaration, its fields, owning phase and read contract are `Zenith2/Docs/Design/ENV.md → "Render-world records"`), owned by `PHASE_INPUT` and written by `Environment.Advance` alone, after the tick's state step ("Extraction steps"); it has no serialized property, so it is in no save and adds nothing to the world state hash, and it is a pure function of the simulation state, so it is identical on every worker and on replay |
| who reads them | `Render.ExtractEnvironment` declares a read of the resource; the phase boundary orders that read after the phase-1 write of the same tick ([Z2-N-ARCH-472]), so no ordering edge inside phase 12 exists or is needed, and no phase-12 node writes the resource (phases 11 and 12 are read-only for world resources, [Z2-N-ARCH-476]; extractors write only their partitions, [Z2-N-ARCH-419]). With no Environment module the resource is absent and the extractor uses LIGHT's world-lighting values and `Flux_EnvironmentBlocks`' member defaults; in a frame whose ticks did not run `Environment.Advance` (a frame with zero ticks) the resource still holds the last tick's values, which the extractor copies unchanged. This answers RPROD's row RP25 (row EN17) |
| precedence | the extractor fills LIGHT's `Flux_RenderLightingEnvironment` (`Zenith2/Docs/Design/LIGHT.md → "Flux_LightSettings header"`): its atmosphere block `Flux_AtmosphereBlock` from the resource (`m_bValid`, `m_uSkyRevision`, `m_xAtmosphere`, `m_xFog`, the local fog volumes, `m_xStars`, `m_xCloudShadow`), its world-lighting block from LIGHT's world-lighting component; `m_xSun`, `m_xMoon`, `m_bHasSun` and `m_bHasMoon` go into the slots (`Zenith2/Docs/Design/LIGHT/Lighting.md → "Sun and moon slots"`); the ENV-only members (clouds, wind, weather, tick rate) into `Flux_RenderEnvironment::m_xBlocks` |
| LIGHT's hooks | the star field, cloud-shadow mapping and local fog volumes travel in the resource with LIGHT's own types (`Flux_StarField`, `Flux_CloudShadowMapping`, `Flux_LocalFogVolume`; `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Star field"`, "Cloud shadow on the sun", "Local fog volumes"). The per-view underwater overrides do not: which views exist and where their cameras are is known only at extraction, so they are ENV's `Flux_RenderEnvironmentView` records, whose payload is LIGHT's `Flux_ViewMediumOverride` (at most 16 per world), and LIGHT copies their payloads into `Flux_LightingFrame::m_axViewMediumOverrides` on the render side (`Zenith2/Docs/Design/LIGHT.md → "Flux_Lighting header"`; at most `uFLUX_MAX_VIEW_MEDIUM_OVERRIDES` = 16 per frame across worlds, `Zenith2/Docs/Design/LIGHT/Volumetrics.md → "Underwater medium override"`; rows EN17 and EN23) |
<!-- /assumption -->

| Block member | Content | Consumer |
|---|---|---|
| atmosphere, fog | `Flux_AtmosphereParams`, `Flux_FogMediumParams` from `Zenith_ToFluxAtmosphere(Zenith_ApplyWeatherToAtmosphere(…))`, and `m_uSkyRevision` | LIGHT's sky and fog passes, through `m_xLighting` |
| sun, moon | the two light slots of "Sun and moon illuminance" | LIGHT |
| stars | `Flux_StarField`: cube texture GUID, rotation, intensity | LIGHT's sky draw |
| clouds | `Flux_EnvironmentClouds`: coverage, density, layer altitude and thickness, the accumulated noise offset, noise scale, the two cloud-noise asset handles | ENV's cloud passes |
| wind | `Env_WindParams` without its time fields | foliage, water and every vertex modifier through `Env_FrameEnvironment`; VFX's GPU particles (pending VFX) |
| local wind sources (Stage 5) | at most 16 `Env_LocalWindSource`, nearest the environment anchor, rebased | the same, through `Env_FrameEnvironment::m_xLocalWindSources` |
| weather | `Env_WeatherParams` | `Env_ApplySurfaceWeather` in materials |
| cloud shadow (Stage 5) | `Flux_CloudShadowMapping`: the square's origin, extent and outside transmittance | ENV's cloud-shadow pass and LIGHT |
| local fog volumes (Stage 5) | at most 64 `Flux_LocalFogVolume`, nearest the environment anchor | LIGHT |
| tick rate | `m_uTickHz` | ENV's GPU side, which derives each frame's wind time and water phases |

ENV's other render data are ordinary render-world records that its own extractor appends (`Flux_RenderTerrainDelta`, `Flux_RenderWaterBodyDelta`, `Flux_RenderEnvironmentView`; `Zenith2/Docs/Design/ENV.md → "Render-world records"`), GEOM's procedural records for detail foliage (`Zenith2/Docs/Design/ENV/Foliage.md → "Detail cells and lattice"`), and EDIT's preview records (`Zenith2/Docs/Design/ENV/Terrain.md → "Live preview"`); X125 leaves them unchanged.

### Extraction steps

In phase 1, `Environment.Advance`, after stepping the clock, weather and wind (`Zenith2/Docs/Design/ENV.md → "Tick-level flow of ENV work"`):

1. `m_bValid` = a settings component exists and its atmosphere asset is active; when false, every member keeps its default and steps 2 to 7 are skipped.
2. The atmosphere and fog members from the tick's blended weather; a bitwise difference from the previous tick's atmosphere, fog or star values increments `m_uSkyRevision` (starting at 1 when the world attaches, so a restore that changes the sky always changes the revision).
3. The sun and moon slots from the celestial state at the tick's clock ("Sun and moon illuminance").
4. The star rotation of "Night sky".
5. The cloud member: the coverage and density of the blended weather, the two cloud-noise handles once active, and the noise offset, which advances by the wind at the layer altitude × the tick duration and wraps modulo the noise tile period (it is presentation, kept in the resource and not saved, so a restored world starts it at 0).
6. The wind and weather members and `m_uTickHz`.
7. Stage 5: the local fog volumes and local wind sources nearest the anchor, and the cloud-shadow square (the anchor snapped to 64 m).

In phase 12, ENV's extractor `Environment.ExtractEnvironment` (`Zenith2/Docs/Architecture/CPUFrameGraph.md → "Render extraction and pipelining"`) appends its records through the writer members of `Zenith2/Docs/Design/ENV.md → "Record writer"`. It never consumes a phase-1 queue: it compares the terrain resource's live terrain set and the water resource's live body set with its own bookkeeping (the keys it has emitted with their slots, assets and wind amplitude scales; presentation state written by this extractor alone) and emits, in ascending key order, ADD for a live key it has not emitted, UPDATE for a body whose amplitude scale or asset changed, and REMOVE for an emitted key no longer live. Because it diffs states rather than draining events, a frame's records do not depend on how many ticks ran since the previous frame (`Test_EnvironmentExtract_RecordsIndependentOfTickCount`). It then writes the per-view records of `Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"`. On a full-state extraction ([Z2-N-REND-015]) it writes every live terrain (with its slot) and water body as ADD. On the render side, `Flux_EnvironmentRenderer` fills the per-frame time fields on `Flux.BuildGraph`: the wind time `(tick + α) × tickSeconds` reduced modulo 3,600 s in `double`, its previous-frame value, and every wave's phase offset of `Zenith2/Docs/Design/ENV/WaterWeather.md → "Time base"`, from `Flux_RenderWorld::m_ulSimulationTick`, `m_fInterpolationAlpha` and `m_uTickHz`.
