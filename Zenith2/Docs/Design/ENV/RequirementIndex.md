# ENV design: requirement index

Section file of `Zenith2/Docs/Design/ENV.md`. It maps every ENV requirement to the headings of the ENV design family that specify it; the entry's heading "Requirement index" points here. The catalogue itself is `Zenith2/Docs/Requirements/ENV.md` (Stages 1 and 3) and `Zenith2/Docs/Requirements/ENV/Stage4Plus.md` (Stages 4 to 6), one numbering.

## Scope of this file

| Group | Heading |
|---|---|
| requirement → design headings, every stage | "Requirement to design map" |

## Requirement to design map

Requirements of Stages 1 and 3 are in `Zenith2/Docs/Requirements/ENV.md`, those of Stages 4 to 6 in `Zenith2/Docs/Requirements/ENV/Stage4Plus.md`.

| Requirement | Title | Stage | Design headings |
|---|---|---|---|
| Z2-R-ENV-001 | Module library target | S3 | Zenith2/Docs/Design/ENV.md → "Target summary"; Zenith2/Docs/Design/ENV.md → "ZenithEnvironment files"; Zenith2/Docs/Design/ENV.md → "ZENITH_SUPPORTED matrix" |
| Z2-R-ENV-002 | Module entry points | S3 | Zenith2/Docs/Design/ENV.md → "Zenith_Module_Environment header" |
| Z2-R-ENV-003 | Construction steps unwind | S3 | Zenith2/Docs/Design/ENV.md → "Zenith_Module_Environment header"; Zenith2/Docs/Design/ENV.md → "Construction and shutdown" |
| Z2-R-ENV-004 | Public headers carry contracts | S3 | Zenith2/Docs/Design/ENV.md → "Rules across the ENV API"; Zenith2/Docs/Design/ENV.md → "Header map" |
| Z2-R-ENV-005 | API-only slices first | S3 | Zenith2/Docs/Design/ENV.md → "API-only slices"; Zenith2/Docs/Design/ENV.md → "Ticket slices and ordering" |
| Z2-R-ENV-006 | Error record and codes | S3 | Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentErrors header"; Zenith2/Docs/Design/ENV.md → "Error enumerations" |
| Z2-R-ENV-007 | Configuration keys are presentation only | S3 | Zenith2/Docs/Design/ENV.md → "Configuration keys" |
| Z2-R-ENV-008 | Headless configuration | S3 | Zenith2/Docs/Design/ENV.md → "Zenith_Module_Environment header"; Zenith2/Docs/Design/ENV.md → "Frame-graph placement" |
| Z2-R-ENV-009 | Components registered | S3 | Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header" |
| Z2-R-ENV-010 | World resources owned by phase 1 | S3 | Zenith2/Docs/Design/ENV.md → "Frame-graph placement"; Zenith2/Docs/Design/ENV.md → "Tick-level flow of ENV work" |
| Z2-R-ENV-011 | Terrain tile grid | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Coordinates and tile grid" |
| Z2-R-ENV-012 | Float authoring heights | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Height representation" |
| Z2-R-ENV-013 | Terrain document schema | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Terrain document schema"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header" |
| Z2-R-ENV-014 | Terrain tile document schema | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Terrain tile document schema" |
| Z2-R-ENV-015 | Load validation order | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header" |
| Z2-R-ENV-016 | Shared borders stay equal | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Height clamping and border consistency"; Zenith2/Docs/Design/ENV/Terrain.md → "Brush application order" |
| Z2-R-ENV-017 | Terrain documents migrate | S4 | Zenith2/Docs/Design/ENV.md → "Migration and re-cook"; Zenith2/Docs/Design/ENV/Bake.md → "Authoring document schemas"; Zenith2/Docs/Design/ENV/Bake.md → "Environment document types" |
| Z2-R-ENV-018 | Newer documents read-only | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header" |
| Z2-R-ENV-019 | Byte-stable saves | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header" |
| Z2-R-ENV-020 | Structural terrain edits | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"; Zenith2/Docs/Design/ENV/Terrain.md → "Layers" |
| Z2-R-ENV-021 | Brush falloff | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Brush falloff" |
| Z2-R-ENV-022 | Brush application order | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Brush application order" |
| Z2-R-ENV-023 | Brushes are worker-count invariant | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Parallel application" |
| Z2-R-ENV-024 | Height brushes | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds"; Zenith2/Docs/Design/ENV/Terrain.md → "Stroke and dab model" |
| Z2-R-ENV-025 | Thermal erosion brush | S5 | Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds" |
| Z2-R-ENV-026 | Heights clamp to limits | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Height clamping and border consistency" |
| Z2-R-ENV-027 | Invalid strokes change nothing | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Stroke and dab model" |
| Z2-R-ENV-028 | Paint brushes | S4 | Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds" |
| Z2-R-ENV-029 | Weight normalisation | S4 | Zenith2/Docs/Design/ENV/Terrain.md → "Weight normalisation" |
| Z2-R-ENV-030 | Foliage scatter and erase brushes | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Scatter brush"; Zenith2/Docs/Design/ENV/Terrain.md → "Brush kinds"; Zenith2/Docs/Design/ENV/Terrain.md → "Explicit foliage instances" |
| Z2-R-ENV-031 | Undo and redo are bit-exact | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Undo integration"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainEdit contents" |
| Z2-R-ENV-032 | Stale edits refused | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Undo integration" |
| Z2-R-ENV-033 | Edit memory proportional to area | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Delta encoding" |
| Z2-R-ENV-034 | EDIT owns undo | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Undo integration"; Zenith2/Docs/Design/ENV.md → "What ENV authors" |
| Z2-R-ENV-035 | Live preview | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Live preview"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header" |
| Z2-R-ENV-036 | Sculpt preview latency | S4 | Zenith2/Docs/Design/ENV/Terrain.md → "Live preview"; Zenith2/Docs/Design/ENV.md → "Pending row needs" |
| Z2-R-ENV-037 | Cooked top-four splat | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Cooked top-four reduction" |
| Z2-R-ENV-038 | Layer blend in the material | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Layer blend in the terrain material"; Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module" |
| Z2-R-ENV-039 | Height-based blend | S4 | Zenith2/Docs/Design/ENV/Terrain.md → "Height-based blend" |
| Z2-R-ENV-040 | Layers per pixel by tier | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Layers per pixel by quality tier" |
| Z2-R-ENV-041 | Terrain material follows the material rules | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module" |
| Z2-R-ENV-042 | Terrain splat render golden | S4 | Zenith2/Docs/Design/ENV/Tests.md → "Render and parity tests" |
| Z2-R-ENV-043 | Terrain activation through the render world | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header"; Zenith2/Docs/Design/ENV/Terrain.md → "Terrain record slots"; Zenith2/Docs/Design/ENV.md → "Render-world records"; Zenith2/Docs/Design/ENV.md → "Record writer"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Extraction steps" |
| Z2-R-ENV-044 | Terrain kind loader | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Terrain file"; Zenith2/Docs/Design/ENV.md → "ZenithFlux ENV files" |
| Z2-R-ENV-045 | Height query definition | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Triangle interpolation" |
| Z2-R-ENV-046 | Height query residency errors | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Residency and tick-boundary activation" |
| Z2-R-ENV-047 | Tile activation at tick boundaries | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Residency and tick-boundary activation"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header" |
| Z2-R-ENV-048 | Physical material and hole queries | S4 | Zenith2/Docs/Design/ENV/Terrain.md → "Physical material queries"; Zenith2/Docs/Design/ENV/Terrain.md → "Holes in queries" |
| Z2-R-ENV-049 | Terrain raycast | S4 | Zenith2/Docs/Design/ENV/Terrain.md → "Terrain raycast"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainRuntime header" |
| Z2-R-ENV-050 | Foliage type document | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Foliage type document" |
| Z2-R-ENV-051 | Placed instances are cooked records | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Instance record layout"; Zenith2/Docs/Design/ENV/Foliage.md → "Foliage classes" |
| Z2-R-ENV-052 | Stable foliage render keys | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Render instance keys"; Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity" |
| Z2-R-ENV-053 | Foliage activation into the GPU scene | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation" |
| Z2-R-ENV-054 | Foliage deactivation | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation" |
| Z2-R-ENV-055 | Foliage full-state extraction | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Full-state extraction" |
| Z2-R-ENV-056 | Foliage extraction bookkeeping is presentation | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation" |
| Z2-R-ENV-057 | Tier thinning of render-only foliage | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Tier thinning rule" |
| Z2-R-ENV-058 | Simulation-visible foliage ignores tiers | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Render-only and simulation-visible instances" |
| Z2-R-ENV-059 | Instance cap and shedding | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Instance cap and shedding" |
| Z2-R-ENV-060 | Foliage instance cap row | S1 | Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV/Foliage.md → "Instance cap and shedding" |
| Z2-R-ENV-061 | Gameplay foliage queries | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Zenith_FoliageSystem header" |
| Z2-R-ENV-062 | Failed foliage cells stay empty | S3 | Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation"; Zenith2/Docs/Design/ENV.md → "Failure table" |
| Z2-R-ENV-063 | Detail foliage generation | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Detail source"; Zenith2/Docs/Design/ENV/Foliage.md → "Detail cells and lattice"; Zenith2/Docs/Design/ENV/Foliage.md → "Blade and card geometry" |
| Z2-R-ENV-064 | Detail foliage hash is stable | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Generator hash"; Zenith2/Docs/Design/ENV/Foliage.md → "Draw-time placement" |
| Z2-R-ENV-065 | Detail foliage tiers | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Detail foliage tiers"; Zenith2/Docs/Design/ENV/Foliage.md → "Tier thinning rule" |
| Z2-R-ENV-066 | Deterministic wind state | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Global wind state"; Zenith2/Docs/Design/ENV.md → "Zenith_Wind header" |
| Z2-R-ENV-067 | One shared wind function | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Env_WindSample"; Zenith2/Docs/Design/ENV/Foliage.md → "Env_FoliageWindOffset"; Zenith2/Docs/Design/ENV/Foliage.md → "Shadow and depth pass agreement" |
| Z2-R-ENV-068 | Wind CPU mirror | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "CPU mirror and tolerance" |
| Z2-R-ENV-069 | Wind time base | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Env_WindSample" |
| Z2-R-ENV-070 | Foliage vertex modifier and velocity | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Material_Foliage"; Zenith2/Docs/Design/ENV/Foliage.md → "Previous-frame evaluation for velocity" |
| Z2-R-ENV-071 | Foliage material GPU rules | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Material_Foliage" |
| Z2-R-ENV-072 | Local wind sources | S5 | Zenith2/Docs/Design/ENV/Foliage.md → "Local wind sources" |
| Z2-R-ENV-073 | Water bodies | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Water body kinds" |
| Z2-R-ENV-074 | Water document | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Water document"; Zenith2/Docs/Design/ENV/Bake.md → "Water file" |
| Z2-R-ENV-075 | Gerstner synthesis | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Gerstner sum"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Time base" |
| Z2-R-ENV-076 | Wave set validation | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Wave set validation" |
| Z2-R-ENV-077 | Deterministic water height query | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Height query by fixed-point iteration"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Zenith_WaterSystem header" |
| Z2-R-ENV-078 | Wind-driven wave amplitude | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Water document" |
| Z2-R-ENV-079 | Water queries out of bounds | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Zenith_WaterSystem header" |
| Z2-R-ENV-080 | Water surface rendering | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Water node selection"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Material_Water" |
| Z2-R-ENV-081 | Water optical model | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Optical model"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Shoreline and depth fade" |
| Z2-R-ENV-082 | Water reflections by tier | S5 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Reflections by tier" |
| Z2-R-ENV-083 | Underwater rendering | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Submersion test"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater composite pass"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater medium" |
| Z2-R-ENV-084 | Water error budget | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Water error budget"; Zenith2/Docs/Design/ENV/Tests.md → "Error-budget tests" |
| Z2-R-ENV-085 | FFT ocean tier | S5 | Zenith2/Docs/Design/ENV/WaterWeather.md → "FFT ocean tier" |
| Z2-R-ENV-086 | Rivers | S5 | Zenith2/Docs/Design/ENV/WaterWeather.md → "River flow" |
| Z2-R-ENV-087 | Caustics | S5 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Caustics" |
| Z2-R-ENV-088 | Atmosphere document | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Atmosphere document"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Validation limits"; Zenith2/Docs/Design/ENV/Bake.md → "Atmosphere file" |
| Z2-R-ENV-089 | Atmosphere fallback | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Validation limits"; Zenith2/Docs/Design/ENV.md → "Failure table" |
| Z2-R-ENV-090 | Atmosphere parameters feed LIGHT | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Blocks ENV writes"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Extraction steps"; Zenith2/Docs/Design/ENV.md → "Render-world records" |
| Z2-R-ENV-091 | Weather modulates the atmosphere | S4 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Weather modulation of the atmosphere" |
| Z2-R-ENV-092 | Sky and atmosphere GPU row in the flagship workloads | S4 | Zenith2/Docs/Design/ENV.md → "Rows owned" |
| Z2-R-ENV-093 | Integer time-of-day clock | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Clock representation"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Advance per tick" |
| Z2-R-ENV-094 | Time-of-day commands | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Commands" |
| Z2-R-ENV-095 | Solar position | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Solar position" |
| Z2-R-ENV-096 | Moon and night sky | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Lunar position and phase"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Night sky" |
| Z2-R-ENV-097 | Sun and moon lights | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Sun and moon illuminance" |
| Z2-R-ENV-098 | Time-of-day events | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Markers and events" |
| Z2-R-ENV-099 | Clock state saved | S3 | Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentState header" |
| Z2-R-ENV-100 | Day and night review | S4 | Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence" |
| Z2-R-ENV-101 | Cloud model | S4 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud model" |
| Z2-R-ENV-102 | Cloud layer on every tier | S4 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud layer pass"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud quality tiers" |
| Z2-R-ENV-103 | Cloud tier selection and S24 fallback | S4 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud quality tiers" |
| Z2-R-ENV-104 | Cloud error budget | S4 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud error budget" |
| Z2-R-ENV-105 | Volumetric clouds | S5 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Volumetric clouds" |
| Z2-R-ENV-106 | Cloud shadows | S5 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud shadows" |
| Z2-R-ENV-107 | Weather document | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather document"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather state parameters" |
| Z2-R-ENV-108 | Weather state machine table | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "States and events"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Transition table" |
| Z2-R-ENV-109 | Deterministic weather draws | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Dwell and choice draws" |
| Z2-R-ENV-110 | Weather commands | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Transition table"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Commands" |
| Z2-R-ENV-111 | Weather blending | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Blending parameters during a transition" |
| Z2-R-ENV-112 | Weather effect prefabs | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Effect prefabs" |
| Z2-R-ENV-113 | Weather presentation parameters | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Presentation parameters"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Shared wetness function" |
| Z2-R-ENV-114 | Weather events | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather events for gameplay" |
| Z2-R-ENV-115 | Weather tiers | S5 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Weather tiers" |
| Z2-R-ENV-116 | Placement rule document | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Placement rule document"; Zenith2/Docs/Design/ENV/Procedural.md → "Surface filters"; Zenith2/Docs/Design/ENV/Procedural.md → "Placement value types" |
| Z2-R-ENV-117 | Seeded placement streams | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Seeded streams" |
| Z2-R-ENV-118 | Candidate generation | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Candidate generation" |
| Z2-R-ENV-119 | Rule filters and acceptance | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Surface filters"; Zenith2/Docs/Design/ENV/Procedural.md → "Density acceptance"; Zenith2/Docs/Design/ENV/Procedural.md → "Noise modulation" |
| Z2-R-ENV-120 | Spacing and exclusion | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Same-rule spacing"; Zenith2/Docs/Design/ENV/Procedural.md → "Cross-rule exclusion"; Zenith2/Docs/Design/ENV/Procedural.md → "Authored exclusion shapes" |
| Z2-R-ENV-121 | Placement is worker and order invariant | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Cross-cell determinism"; Zenith2/Docs/Design/ENV/Procedural.md → "Determinism argument" |
| Z2-R-ENV-122 | Placement output order and capacity | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity" |
| Z2-R-ENV-123 | One placement implementation | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Runtime and editor use" |
| Z2-R-ENV-124 | Runtime reproducible placement for WORLD | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Runtime and editor use"; Zenith2/Docs/Design/ENV/Procedural.md → "Boundary with WORLD procedural generation" |
| Z2-R-ENV-125 | Collision heightfield extraction | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Heightfield extraction"; Zenith2/Docs/Design/ENV/Bake.md → "Holes and materials in collision" |
| Z2-R-ENV-126 | Collidable foliage shapes | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Collidable foliage" |
| Z2-R-ENV-127 | Navigation geometry extraction | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Navigation geometry extraction"; Zenith2/Docs/Design/ENV/Bake.md → "Area flags" |
| Z2-R-ENV-128 | Navigation hook waits for D-SEL-NAV | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Navigation geometry extraction"; Zenith2/Docs/Design/ENV.md → "Open decisions" |
| Z2-R-ENV-129 | Regeneration after edits | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Regeneration after edits" |
| Z2-R-ENV-130 | Cell alignment | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Environment cells and STREAM cells" |
| Z2-R-ENV-131 | Cell content records | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Cell content records" |
| Z2-R-ENV-132 | Cell load and unload leave nothing behind | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Activation and cancellation" |
| Z2-R-ENV-133 | Large coordinates | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Large coordinates" |
| Z2-R-ENV-134 | Resident data under pressure | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Instance cap and shedding"; Zenith2/Docs/Design/ENV.md → "Failure table" |
| Z2-R-ENV-135 | Cooked container and sections | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Section kind registry"; Zenith2/Docs/Design/ENV.md → "ENV cooked file header"; Zenith2/Docs/Design/ENV.md → "Byte order and serialization rules" |
| Z2-R-ENV-136 | Cook determinism | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Cook determinism" |
| Z2-R-ENV-137 | Derived-data keys | S3 | Zenith2/Docs/Design/ENV.md → "Cook steps and derived-data keys"; Zenith2/Docs/Design/ENV/Bake.md → "Step graph" |
| Z2-R-ENV-138 | Readers validate untrusted bytes | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Reader validation"; Zenith2/Docs/Design/ENV/Tests.md → "Fuzz targets" |
| Z2-R-ENV-139 | Mismatched cooked versions refused | S3 | Zenith2/Docs/Design/ENV.md → "Migration and re-cook"; Zenith2/Docs/Design/ENV.md → "ENV cooked file header" |
| Z2-R-ENV-140 | Terrain geometry hand-off | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Geometry hand-off to GEOM"; Zenith2/Docs/Design/ENV/Bake.md → "Terrain file" |
| Z2-R-ENV-141 | Environment documents migrate | S4 | Zenith2/Docs/Design/ENV.md → "Migration and re-cook"; Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentDocuments header"; Zenith2/Docs/Design/ENV/Bake.md → "Document value types"; Zenith2/Docs/Design/ENV/Bake.md → "Environment document types" |
| Z2-R-ENV-142 | Environment replay determinism | S4 | Zenith2/Docs/Design/ENV/Tests.md → "Determinism tests" |
| Z2-R-ENV-143 | Environment counters | S3 | Zenith2/Docs/Design/ENV.md → "Counters" |
| Z2-R-ENV-144 | On-screen environment panel | S3 | Zenith2/Docs/Design/ENV.md → "On-screen profiler rows"; Zenith2/Docs/Design/ENV.md → "Profiler zones" |
| Z2-R-ENV-145 | Environment debug views | S4 | Zenith2/Docs/Design/ENV.md → "Debug views"; Zenith2/Docs/Design/ENV.md → "Frame-dump entries" |
| Z2-R-ENV-146 | Environment log categories | S3 | Zenith2/Docs/Design/ENV.md → "Log categories" |
| Z2-R-ENV-147 | Cook report members | S3 | Zenith2/Docs/Design/ENV.md → "Cook report members" |
| Z2-R-ENV-148 | Environment Luau bindings | S4 | Zenith2/Docs/Design/ENV.md → "Luau errors and asserts" |
| Z2-R-ENV-149 | Pending ENV rows allocated | S1 | Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV.md → "Rows owned"; Zenith2/Docs/Design/ENV.md → "Stage table" |
| Z2-R-ENV-150 | ENV rows hold in W-Outdoor at Stage 3 | S3 | Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV.md → "Profiler zones" |
| Z2-R-ENV-151 | ENV rows hold in W-Outdoor at Stage 4 | S4 | Zenith2/Docs/Design/ENV.md → "Pending row needs"; Zenith2/Docs/Design/ENV.md → "Rows measured against" |
| Z2-R-ENV-152 | W-Outdoor evidence | S3 | Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence" |
| Z2-R-ENV-153 | W-Outdoor authored in Stage 3 | S3 | Zenith2/Docs/Design/ENV.md → "Stage table" |
| Z2-R-ENV-154 | Foliage activation catch-up | S4 | Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence"; Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation" |
| Z2-R-ENV-155 | S24 sustained W-Outdoor | S6 | Zenith2/Docs/Design/ENV/Tests.md → "Workload and device evidence" |
| Z2-R-ENV-156 | GPU performance rules compliance | S4 | Zenith2/Docs/Design/ENV.md → "Shader modules and engine content"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Underwater composite pass"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud layer pass" |
| Z2-R-ENV-157 | Environment backend parity | S4 | Zenith2/Docs/Design/ENV/Tests.md → "Render and parity tests" |
| Z2-R-ENV-158 | Terrain and foliage in every path | S5 | Zenith2/Docs/Design/ENV.md → "Stage table"; Zenith2/Docs/Design/ENV.md → "Assumed types from pending design documents" |
| Z2-R-ENV-159 | Local fog volumes | S5 | Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header" |
| Z2-R-ENV-160 | Terrain LOD distance ratio at least 8 | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header"; Zenith2/Docs/Design/ENV/Bake.md → "Terrain document schema" |
| Z2-R-ENV-161 | Placement keys include the rule set | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Output ordering and capacity"; Zenith2/Docs/Design/ENV/Procedural.md → "Zenith_ProceduralPlacement header" |
| Z2-R-ENV-162 | Cross-rule-set exclusion and per-set masks | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Cross-rule exclusion"; Zenith2/Docs/Design/ENV/Procedural.md → "Surface filters"; Zenith2/Docs/Design/ENV/Procedural.md → "Placement value types" |
| Z2-R-ENV-163 | Exclusion chain bounded | S3 | Zenith2/Docs/Design/ENV/Procedural.md → "Cross-rule exclusion"; Zenith2/Docs/Design/ENV/Procedural.md → "Cross-cell determinism" |
| Z2-R-ENV-164 | Scatter brush keeps footprints | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Scatter brush" |
| Z2-R-ENV-165 | Environment blocks resource | S3 | Zenith2/Docs/Design/ENV.md → "Render-world records"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Blocks ENV writes" |
| Z2-R-ENV-166 | Sun and moon slots and scene lights | S3 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Sun and moon illuminance" |
| Z2-R-ENV-167 | Detail foliage within the GEOM limits | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Blade and card geometry"; Zenith2/Docs/Design/ENV/Foliage.md → "Detail pool sizing"; Zenith2/Docs/Design/ENV/Foliage.md → "Detail cells and lattice" |
| Z2-R-ENV-168 | Detail placement at draw time | S4 | Zenith2/Docs/Design/ENV/Foliage.md → "Draw-time placement" |
| Z2-R-ENV-169 | Water material generated per document | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Water GPU side"; Zenith2/Docs/Design/ENV/Bake.md → "Water file" |
| Z2-R-ENV-170 | Water node instances | S4 | Zenith2/Docs/Design/ENV/WaterWeather.md → "Water GPU side"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Water node selection" |
| Z2-R-ENV-171 | VT-backed terrain reference scene | S5 | Zenith2/Docs/Design/ENV/Terrain.md → "VT-backed terrain materials" |
| Z2-R-ENV-172 | Terrain materials generated per terrain | S3 | Zenith2/Docs/Design/ENV/Terrain.md → "Terrain material module"; Zenith2/Docs/Design/ENV/Bake.md → "Step graph" |
| Z2-R-ENV-173 | Terrain documents merge per tile file | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Merging terrain documents" |
| Z2-R-ENV-174 | No ENV GPU cost without the module | S3 | Zenith2/Docs/Design/ENV.md → "ZenithFlux ENV files"; Zenith2/Docs/Design/ENV.md → "Cook report members" |
| Z2-R-ENV-175 | Presentation keys follow tier changes | S3 | Zenith2/Docs/Design/ENV.md → "Configuration keys" |
| Z2-R-ENV-176 | Weather and wind state saved | S4 | Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentState header" |
| Z2-R-ENV-177 | Environment anchor | S3 | Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentComponents header"; Zenith2/Docs/Design/ENV/Foliage.md → "Activation and deactivation" |
| Z2-R-ENV-178 | Cloud noise volumes cooked by ENV | S4 | Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise volumes"; Zenith2/Docs/Design/ENV/Atmosphere.md → "Cloud noise generator"; Zenith2/Docs/Design/ENV/Bake.md → "Cloud noise file"; Zenith2/Docs/Design/ENV/Bake.md → "Cloud noise document schema" |
| Z2-R-ENV-179 | Environment document types registered tool-side | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Environment document types"; Zenith2/Docs/Design/ENV.md → "Zenith_EnvironmentDocuments header"; Zenith2/Docs/Design/ENV/Terrain.md → "Zenith_TerrainDocument header" |
| Z2-R-ENV-180 | Runtime objects of the Stage 3 kinds | S3 | Zenith2/Docs/Design/ENV/Bake.md → "Runtime asset objects"; Zenith2/Docs/Design/ENV/Bake.md → "Terrain file" |
| Z2-R-ENV-181 | Runtime objects of the Stage 4 kinds | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Runtime asset objects"; Zenith2/Docs/Design/ENV/WaterWeather.md → "Zenith_WeatherSystem header" |
| Z2-R-ENV-182 | Detail material generated per terrain | S4 | Zenith2/Docs/Design/ENV/Bake.md → "Step graph"; Zenith2/Docs/Design/ENV/Foliage.md → "Draw-time placement" |
