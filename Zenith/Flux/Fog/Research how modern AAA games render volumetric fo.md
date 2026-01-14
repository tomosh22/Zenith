<img src="https://r2cdn.perplexity.ai/pplx-full-logo-primary-dark%402x.png" style="height:64px;margin-right:32px"/>

# Research how modern AAA games render volumetric fog and explain several techniques.

Now I have gathered comprehensive information and created supporting visuals. Let me generate the final report.

# Volumetric Fog Rendering in Modern AAA Games: Techniques and Implementation

**Modern AAA games employ several sophisticated techniques to render volumetric fog in real-time, each balancing visual quality with GPU performance budgets ranging from 1-5 milliseconds per frame.** The primary approaches include **voxel grid (froxel) rendering aligned to camera frustums, ray marching through 3D noise textures, light propagation volume cascades, temporal reprojection filtering, and screen-space post-processing effects**. These techniques have evolved significantly over the past decade, with engines like Unreal Engine 5, Unity, and proprietary solutions from studios like Guerrilla Games (Horizon series) pioneering optimization strategies that maintain cinematic quality while running on contemporary GPUs[^1][^2][^3].

## Voxel Grid and Froxel-Based Volumetric Rendering

The **most widely adopted technique in modern AAA engines is froxel-based volumetric rendering**, which uses low-resolution 3D textures aligned to the camera's view frustum[^1]. This approach divides the visible space into a grid of voxels (volumetric pixels), typically at resolutions like 160x90x64 or 480x270x128 at 1080p, creating a structured data container for fog density and lighting information[^2]. Rather than computing fog values per-pixel, the engine pre-calculates participating media density and lighting for each voxel in a single pass, storing this information in volumetric textures for efficient reuse[^1].

The computational pipeline for froxel rendering operates in three main stages. First, **density estimation** populates each voxel with participating media density values using either analytical formulas or sampled 3D noise textures[^1]. Second, **lighting calculation** evaluates how light from all contributing sources (directional light, point lights, skylights) interacts with the medium at each voxel position, using techniques like shadow maps or exponential shadow mapping to account for shadowing[^4]. Finally, **per-pixel application** occurs during the final render pass, where each screen pixel samples the volumetric texture and accumulates the stored lighting and density information along the viewing ray[^1].

Unreal Engine's volumetric fog implementation demonstrates this approach effectively, supporting a single directional light with cascaded shadow map shadowing, multiple point and spot lights with optional volumetric shadow casting, and precomputed lighting through volumetric lightmaps[^1]. Performance scales predictably with grid resolution: the **High** quality preset costs approximately 1 millisecond on PlayStation 4, while the **Epic** preset with 8x more voxels (higher resolution in all three dimensions) costs around 3 milliseconds on an NVIDIA GTX 970[^1]. This linear scaling allows developers to adjust quality based on hardware capabilities using engine scalability settings[^1].

![Comparison of Volumetric Fog Rendering Techniques in Modern AAA Games](https://ppl-ai-code-interpreter-files.s3.amazonaws.com/web/direct-files/a9aa713459c7a1e004d6cf814c4dbc1e/21e9c0e2-9ec8-4057-8e86-0346309ca9b0/4f6f1d33.png)

Comparison of Volumetric Fog Rendering Techniques in Modern AAA Games

## Ray Marching with Procedural Noise Sampling

**Ray marching represents a fundamentally different approach** that trades off voxel grid memory efficiency for per-pixel quality and dynamic control[^5][^6]. Rather than pre-computing volumetric textures, the technique repeatedly steps forward along a ray from the camera through 3D space, sampling density from noise functions at each step position[^6]. The ray accumulates density and lighting information at each sample point until reaching terrain or reaching maximum distance[^5].

This technique gained prominent recognition through **Guerrilla Games' implementation for Horizon Zero Dawn**, which achieves photorealistic volumetric clouds at 2 milliseconds using a sophisticated noise-based density function[^7][^8]. The approach uses **layered Perlin-Worley noise** (combining Perlin noise for overall structure with Worley noise for cloud details) at multiple octaves and scales to generate cloud-like density patterns[^7]. Rather than storing pre-computed noise in textures, the shader procedurally generates noise on-the-fly during ray marching, enabling dynamic animation and artist-directed shapes through parameter modulation[^7].

The mathematical foundation involves sampling multiple noise functions at progressively finer scales, each weighted by a frequency and amplitude (a technique called **fractal Brownian motion or FBM**)[^6]. Coverage and altitude-based terms modulate the noise to create realistic cloud formations, with denser regions near cloud bases and tapering at edges[^7]. **Lighting is handled through shadow sampling**: as the ray marches through a sample point, the shader offset-checks positions toward the light source to determine if that sample is in shadow, darkening illumination accordingly[^2][^5].

A critical optimization for ray marching is **stochastic sampling with blue noise dithering**[^6][^9][^10]. Rather than using uniform sample spacing along rays, blue noise patterns add controlled randomization that reduces banding artifacts while maintaining temporal stability[^9][^11]. NVIDIA's spatiotemporal blue noise research demonstrated that blue noise masks produce better image quality with fewer samples than white noise or uniform patterns—a critical advantage for keeping frame rates stable in real-time rendering[^9][^10].

![Ray marching algorithm for volumetric fog: step-by-step density and light accumulation](https://user-gen-media-assets.s3.amazonaws.com/seedream_images/037eebfe-35c2-46df-af96-69bccc1dfa17.png)

Ray marching algorithm for volumetric fog: step-by-step density and light accumulation

## Light Propagation Volumes and Multiple Scattering

**Light Propagation Volumes (LPVs)** represent a more complex but powerful technique originally developed for global illumination approximation in CryEngine 3[^12][^4]. While originally designed for indirect lighting, modified LPV schemes can approximate multiple scattering in volumetric fog, handling scenarios where light bounces between fog particles multiple times[^13]. The technique operates by injecting light information from shadow maps into a low-resolution 3D grid, then propagating that light through the grid using GPU compute shaders to simulate light traveling through the participating medium[^4][^13].

The propagation scheme models scattering according to the radiative light transfer equation during propagation, creating initial radiance distributions based on single-scattered light identified from shadow maps[^13]. After propagation completes, the resulting light distribution can be used as both a diffuse light source during rendering and as ray-march source data for volumetric effects[^13]. Performance analysis shows this approach renders at 3-5 milliseconds with support for cascading grids at varying resolutions, though the coarse voxelization introduces visible artifacts that require careful artistic control[^4][^13].

**The advantage of LPVs lies in multiple scattering approximation**: unlike simple ray marching which only computes direct lighting, propagating light through the volume multiple times captures the effect of light bouncing within the fog itself[^13]. This creates more physically plausible results in dense fog scenarios, though computational cost increases significantly with each propagation iteration[^13].

## Temporal Reprojection and Stability Filtering

**Temporal reprojection filtering addresses the low-resolution aliasing inherent to voxel-based approaches** by blending current frame data with reprojected previous frame data[^1][^14][^15]. Since volumetric texture grids are necessarily low-resolution to maintain performance, they exhibit visible banding and stepping artifacts. Temporal reprojection applies different sub-voxel jitter patterns each frame, with the filtering smoothing results across multiple frames[^1][^15].

Unreal Engine implements this as a **heavy temporal reprojection filter** with frame-to-frame blending weights typically set to 0.9 or higher[^2][^15]. Each frame, the grid is shifted slightly in sub-voxel space before rendering, ensuring different coverage at different times[^1]. When composited over multiple frames, this temporal jitter averages out to create smoother results than a single frame would produce[^1].

However, temporal reprojection introduces **ghosting artifacts on fast-moving objects**, where previous frame data "trails" behind moving fog volumes or lights[^14]. Unreal's documentation explicitly warns that fast-changing lights like flashlights and muzzle flashes leave "lighting trails" due to temporal blending[^1]. To mitigate this, developers disable temporal contribution for dynamic lights by setting **Volumetric Scattering Intensity** to 0 for short-lived light effects[^1][^15].

## Optimization Techniques: Dithering and Stochastic Sampling

**Dithering and stochastic sampling emerge as critical optimization techniques** for reducing memory footprint and computation while maintaining perceived quality[^16][^17][^11]. Rather than using high sample counts that strain GPU budgets, engines apply controlled randomization patterns that allow convergence with fewer samples[^9][^16][^11].

Blue noise dithering specifically targets the human visual system's sensitivity to low-frequency artifacts[^9][^16]. Unlike white noise which distributes energy uniformly across all frequencies, blue noise concentrates energy at high frequencies where the eye is less sensitive[^16]. When applied to volumetric ray marching, blue noise jitter reduces visible banding by 8-10x compared to uniform sampling with equivalent cost, allowing 1/4 to 1/16th the sample count while maintaining visual quality[^6][^16][^11].

**Temporal blue noise extension** further improves convergence by ensuring that blue noise properties hold across both spatial and temporal domains[^9][^10]. This enables temporal antialiasing filters and exponential moving averages to work effectively across frames, with each frame contributing well-distributed samples rather than repeating spatial patterns temporally[^9][^10]. NVIDIA's research showed that importance-sampled spatiotemporal blue noise converges substantially faster and with more temporal stability than traditional blue noise masks or white noise sequences[^10].

## Screen-Space and Post-Process Approximations

For performance-constrained scenarios, **screen-space volumetric approximations** provide lightweight alternatives by operating entirely in 2D image space rather than 3D volume space[^18][^19]. The classic "god rays" or "light shafts" effect uses radial blur: the light source is rendered to a texture, a radial blur filter is applied around it, and the blurred result is composited using depth-based edge preservation[^18][^19].

These approaches offer exceptional performance—typically under 1 millisecond—and work particularly well for cinematic light shafts where 2D accuracy is acceptable[^18][^19]. However, they fundamentally cannot represent true volumetric fog density because they operate on 2D image space without 3D position information[^18]. Objects passing through the fog volume do not properly occlude it, and multiple overlapping light shafts cannot be accurately blended[^18].

Andrew Gotow's screen-space volumetric shadowing technique extends this concept by estimating thickness of shadows in a scene using depth-based sampling, providing atmospheric scattering approximation without full volumetric computation[^18]. This hybrid approach remains popular in games prioritizing frame rate over volumetric accuracy[^18].

## Performance Metrics and Real-World Implementation

The practical cost of volumetric fog varies significantly by technique and quality target[^1][^2][^20]. In **desktop/console environments**, full volumetric fog using froxels and temporal reprojection typically costs 1-3 milliseconds at 1080p resolution with medium quality settings, scaling to 5+ milliseconds at epic quality with higher resolution grids[^1][^2]. Ray marching approaches cost 2-4 milliseconds depending on sample count and noise complexity[^7].

**Mobile implementations** require aggressive optimization, using lower grid resolutions (e.g., 64x36x32), simpler noise functions, or fallback to fake volumetric fog using particle systems[^20]. Despite these constraints, modern mobile games achieve acceptable volumetric effects by carefully balancing voxel resolution, temporal filtering, and post-processing blur[^20].

The cost of **volumetric shadowing** on point and spot lights approximately triples per-light expense compared to unshadowed lights, as additional shadow map passes must be computed for each voxel grid[^1][^15]. Directional light shadowing reuses existing cascaded shadow maps, maintaining minimal additional cost[^1][^4].

## Conclusion

Modern AAA games leverage a sophisticated ecosystem of volumetric fog techniques, each optimized for specific performance and quality targets. **Unreal Engine's froxel-based approach dominates mainstream development** due to its balance of visual quality, ease of implementation, and predictable performance scaling[^1][^2]. **Horizon Zero Dawn's ray marching with procedural noise demonstrated** that photorealistic volumetric clouds are achievable at 2ms per frame with careful optimization[^7][^8]. **Light propagation volumes continue to be used for advanced multiple scattering approximations** in projects where performance budget allows[^4][^13].

The trend across the industry emphasizes **temporal techniques combined with stochastic sampling**: rather than computing perfect per-frame results, engines compute rough approximations and filter across time to achieve high quality with acceptable frame rates[^1][^14][^10]. As GPU hardware advances and compression techniques improve, volumetric fog quality and range continue to expand, with future techniques likely focusing on ray-reordered computing and dynamic memory compression to handle larger volumetric spaces efficiently.
<span style="display:none">[^21][^22][^23][^24][^25][^26][^27][^28][^29][^30][^31][^32][^33][^34][^35][^36][^37][^38][^39][^40][^41][^42][^43][^44][^45][^46][^47][^48][^49][^50][^51]</span>

<div align="center">⁂</div>

[^1]: https://dev.epicgames.com/documentation/en-us/unreal-engine/volumetric-fog-in-unreal-engine

[^2]: https://asher.gg/create-fast-and-usable-volumetric-cloud-fog/

[^3]: https://www.daydreamsoft.com/blog/real-time-ray-marching-for-volumetric-worlds-and-next-gen-visual-effects

[^4]: https://bartwronski.com/wp-content/uploads/2014/08/bwronski_volumetric_fog_siggraph2014.pdf

[^5]: https://www.youtube.com/watch?v=eDYyBc3cRmw

[^6]: https://blog.maximeheckel.com/posts/real-time-cloudscapes-with-volumetric-raymarching/

[^7]: https://www.studocu.com/en-us/document/stanford-university/human-physiology/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn-artr/149403031

[^8]: https://www.guerrilla-games.com/read/the-real-time-volumetric-cloudscapes-of-horizon-zero-dawn

[^9]: https://research.nvidia.com/publication/2022-07_spatiotemporal-blue-noise-masks

[^10]: https://cseweb.ucsd.edu/~ravir/stbn.pdf

[^11]: https://blog.demofox.org/2020/05/10/ray-marching-fog-with-blue-noise/

[^12]: https://advances.realtimerendering.com/s2009/Light_Propagation_Volumes.pdf

[^13]: https://www.cse.chalmers.se/~uffe/multi_scatter.pdf

[^14]: https://docs.godotengine.org/pl/4.x/tutorials/3d/volumetric_fog.html

[^15]: https://80.lv/articles/a-cheat-sheet-on-volumetric-fog-in-ue4

[^16]: https://momentsingraphics.de/BlueNoise.html

[^17]: https://discourse.threejs.org/t/efficient-volumetric-clouds/66067

[^18]: https://andrewgotow.com/2016/10/05/screenspace-volumetric-shadowing/

[^19]: https://www.reddit.com/r/GraphicsProgramming/comments/1aqj2dc/volumetric_fog_god_rays/

[^20]: https://www.youtube.com/watch?v=JyBNKhk3EP4

[^21]: https://garagefarm.net/blog/volumetric-lighting-in-rendering-techniques-and-performance

[^22]: https://www.youtube.com/watch?v=IWGujvx6sSg

[^23]: https://www.gamedeveloper.com/programming/volumetric-rendering-in-realtime

[^24]: https://www.youtube.com/watch?v=Kjg6kCW2BtY

[^25]: https://www.alanzucconi.com/2016/07/01/raymarching/

[^26]: https://www.linkedin.com/pulse/complete-guide-volumetric-lighting-martin-wan-rcudc

[^27]: https://www.reddit.com/r/Unity3D/comments/i5xdtd/my_first_foray_into_raymarching_has_been_quite/

[^28]: https://docs.godotengine.org/en/latest/tutorials/3d/volumetric_fog.html

[^29]: https://shaderbits.com/blog/creating-volumetric-ray-marcher

[^30]: https://www.scitepress.org/Papers/2011/33741/33741.pdf

[^31]: https://www.reddit.com/r/Unity3D/comments/17gf3nb/made_some_progress_with_my_analytic_volumetric/

[^32]: https://www.reddit.com/r/GraphicsProgramming/comments/gfcda5/temporal_reprojection_with_volumetric_objects/

[^33]: https://discourse.threejs.org/t/volumetric-lighting-in-webgpu/87959

[^34]: https://www.reddit.com/r/GraphicsProgramming/comments/1mpcrtr/temporal_reprojection_without_disocclusion/

[^35]: https://www.pinwheelstud.io/product/beam

[^36]: https://forums.unrealengine.com/t/how-to-enable-light-propagation-volumes-gi-wip-and-beta/464

[^37]: https://irendering.net/exploring-volumetric-fog-in-unreal-engine/

[^38]: https://blog.uhawkvr.com/rendering/rendering-volumetric-clouds-using-signed-distance-fields/

[^39]: https://gizmosandgames.com/2016/11/13/voxel-volumes/

[^40]: https://coherent-labs.com/posts/overview-of-modern-volume-rendering-techniques-for-games-part-ii/

[^41]: https://advances.realtimerendering.com/s2015/The Real-time Volumetric Cloudscapes of Horizon - Zero Dawn - ARTR.pdf

[^42]: https://dev.epicgames.com/documentation/en-us/unreal-engine/shadowing-in-unreal-engine

[^43]: https://www.reddit.com/r/VoxelGameDev/comments/xs1uws/volumetric_rendering_or_mesh_generation_for_a/

[^44]: https://www.gamedev.net/tutorials/programming/graphics/overview-of-modern-volume-rendering-techniques-for-games-part-1-r3398/

[^45]: https://www.jpgrenier.org/clouds.html

[^46]: https://www.scratchapixel.com/lessons/3d-basic-rendering/volume-rendering-for-developers/volume-rendering-voxel-grids.html

[^47]: https://www.astesj.com/publications/ASTESJ_050679.pdf

[^48]: https://www.dalirenderer.com/features/

[^49]: https://kronnect.com/guides/volumetric-fog-urp-adding-volumetric-fog-mist-to-your-scene/

[^50]: http://ijdykeman.github.io/graphics/simple_fog_shader

[^51]: https://arxiv.org/html/2504.05562v1

