# SignPost — AB-PROP-11

`create_signpost.py` authors the SignPost in a new Blender scene using `bpy`
and NumPy. Run it in Blender's Python environment. It preserves existing scenes
and writes the isolated editable scene and GLB to
`Games/Zenithmon/Assets/Props/SignPost/`.

The prop has two blank oak arrow boards, beveled edges, four iron bolt/washer
assemblies, a post cap and a foot ferrule. The blank faces deliberately carry
no lettering, as required by the art brief. Wood texture synthesis uses a fixed
seed, irregular growth bands and longitudinal pores.

Delivery: 0.90 m wide, 2.00 m tall, 0.1515 m deep; 4,768 triangles. glTF +Y is
up, the base is at Y=0, and the bolted faces point toward +Z. Separate weathered
oak and forged iron PBR materials each embed 1024-square PNG base colour,
tangent-space normal and metallic/roughness maps. Base colour is sRGB; normal
and metallic/roughness data are linear. Roughness occupies G and metallic
occupies B, following glTF 2.0. The GLB contains one mesh with two primitives,
exercising the renderer's per-section material and index-range handling.

The tools boot imports the GLB into the standard `.zmesh`, `.zmodel`, `.zmtrl`
and `.ztxtr` bundle. The generated fallback remains in `ZM_PropGen`.
Assets stay gitignored under the project's standing source-art policy; this
script is retained so the asset can be recreated.

World placements are `DawnmereSignHome` and `DawnmereSignRoute`, in
`ZM_DawnmereDressing.h`. The existing `ZM_ImportedPropShowcase_Test` captures
both placements, checks loaded model bounds and authored scale, asserts two
complete material sections with distinct materials, and validates the captured
TGA files. After a tools boot imports the GLB, run:

```powershell
./zenith.bat test Zenithmon --filter ZM_ImportedPropShowcase_Test
```

The test command skips tool exports by default. To import a newly exported
GLB, run a normal tools boot first, or use the test harness module's
`-NoSkipToolExports` option with a Null `-DiscoveryExe`.
