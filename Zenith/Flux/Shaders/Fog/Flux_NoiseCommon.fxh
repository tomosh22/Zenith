// Flux_NoiseCommon.fxh - Shared noise functions for volumetric rendering
// Used by ray marching and froxel-based volumetric fog techniques

#ifndef FLUX_NOISE_COMMON_FXH
#define FLUX_NOISE_COMMON_FXH

// Hash function for pseudo-random values
float Hash31(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.x + p.y) * p.z);
}

vec3 Hash33(vec3 p)
{
    p = fract(p * vec3(0.1031, 0.1030, 0.0973));
    p += dot(p, p.yxz + 33.33);
    return fract((p.xxy + p.yxx) * p.zyx);
}

// Smooth interpolation
float SmoothStep01(float t)
{
    return t * t * (3.0 - 2.0 * t);
}

vec3 SmoothStep01_3(vec3 t)
{
    return t * t * (3.0 - 2.0 * t);
}

// 3D Gradient/Value noise
float GradientNoise3D(vec3 p)
{
    vec3 i = floor(p);
    vec3 f = fract(p);

    vec3 u = SmoothStep01_3(f);

    float n000 = Hash31(i + vec3(0.0, 0.0, 0.0));
    float n100 = Hash31(i + vec3(1.0, 0.0, 0.0));
    float n010 = Hash31(i + vec3(0.0, 1.0, 0.0));
    float n110 = Hash31(i + vec3(1.0, 1.0, 0.0));
    float n001 = Hash31(i + vec3(0.0, 0.0, 1.0));
    float n101 = Hash31(i + vec3(1.0, 0.0, 1.0));
    float n011 = Hash31(i + vec3(0.0, 1.0, 1.0));
    float n111 = Hash31(i + vec3(1.0, 1.0, 1.0));

    float nx00 = mix(n000, n100, u.x);
    float nx10 = mix(n010, n110, u.x);
    float nx01 = mix(n001, n101, u.x);
    float nx11 = mix(n011, n111, u.x);

    float nxy0 = mix(nx00, nx10, u.y);
    float nxy1 = mix(nx01, nx11, u.y);

    return mix(nxy0, nxy1, u.z);
}

// Worley (cellular) noise - returns distance to nearest feature point
float WorleyNoise3D(vec3 p, float cellCount)
{
    vec3 cellPos = p * cellCount;
    vec3 cellID = floor(cellPos);
    vec3 cellFrac = fract(cellPos);

    float minDist = 1.0;

    // Check 3x3x3 neighborhood
    for (int z = -1; z <= 1; z++)
    {
        for (int y = -1; y <= 1; y++)
        {
            for (int x = -1; x <= 1; x++)
            {
                vec3 neighborCell = vec3(float(x), float(y), float(z));
                vec3 featurePoint = Hash33(cellID + neighborCell);

                vec3 diff = neighborCell + featurePoint - cellFrac;
                float dist = length(diff);

                minDist = min(minDist, dist);
            }
        }
    }

    return minDist;
}

// Fractal Brownian Motion (FBM) using gradient noise
float FBM_Perlin(vec3 p, int octaves, float persistence, float lacunarity)
{
    float total = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    for (int i = 0; i < octaves; i++)
    {
        total += amplitude * GradientNoise3D(p * frequency);
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

// FBM using Worley noise
float FBM_Worley(vec3 p, int octaves, float persistence, float lacunarity, float cellCount)
{
    float total = 0.0;
    float amplitude = 1.0;
    float frequency = 1.0;
    float maxValue = 0.0;

    for (int i = 0; i < octaves; i++)
    {
        total += amplitude * WorleyNoise3D(p, cellCount * frequency);
        maxValue += amplitude;
        amplitude *= persistence;
        frequency *= lacunarity;
    }

    return total / maxValue;
}

// Combined Perlin-Worley noise (Horizon Zero Dawn style)
// Perlin provides base shape, Worley adds cloud-like edges
float PerlinWorleyNoise(vec3 p, float perlinFreq, float worleyFreq)
{
    float perlin = FBM_Perlin(p * perlinFreq, 4, 0.5, 2.0);
    float worley = 1.0 - WorleyNoise3D(p, worleyFreq);

    // Remap perlin to be centered around 0.5
    perlin = perlin * 0.5 + 0.5;

    // Use worley to erode perlin
    return clamp(perlin - worley * 0.3, 0.0, 1.0);
}

// Utility: Remap value from one range to another
float Remap(float value, float low1, float high1, float low2, float high2)
{
    return low2 + (value - low1) * (high2 - low2) / (high1 - low1);
}

// Utility: Remap and clamp
float RemapClamped(float value, float low1, float high1, float low2, float high2)
{
    float remapped = Remap(value, low1, high1, low2, high2);
    return clamp(remapped, min(low2, high2), max(low2, high2));
}

#endif // FLUX_NOISE_COMMON_FXH
