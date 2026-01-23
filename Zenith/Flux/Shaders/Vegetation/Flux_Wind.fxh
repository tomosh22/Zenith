// Wind Animation Functions
// Provides realistic wind movement for grass and foliage

// Simple wind function with layered waves and gusts
vec2 CalculateWind(vec2 xWorldPos, float fTime, vec2 xWindDir, float fStrength)
{
	// Primary wave - large scale movement
	float fWave1 = sin(dot(xWorldPos, xWindDir) * 0.1 + fTime * 1.5);

	// Secondary wave - medium scale variation
	float fWave2 = sin(dot(xWorldPos, xWindDir * 1.3) * 0.25 + fTime * 2.3) * 0.5;

	// Tertiary wave - small scale flutter
	float fWave3 = sin(dot(xWorldPos, xWindDir * 0.7) * 0.4 + fTime * 4.1) * 0.25;

	// Gust effect - occasional stronger gusts
	float fGustPhase = dot(xWorldPos, xWindDir) * 0.02 + fTime * 0.3;
	float fGust = pow(sin(fGustPhase) * 0.5 + 0.5, 2.0);

	// Combine waves
	float fCombined = (fWave1 * 0.5 + fWave2 + fWave3) * 0.333;

	// Apply gust modulation
	fCombined = fCombined * (1.0 + fGust * 0.5);

	// Return 2D displacement
	return xWindDir * fStrength * fCombined;
}

// Advanced wind with turbulence for more natural movement
vec2 CalculateWindTurbulent(vec2 xWorldPos, float fTime, vec2 xWindDir, float fStrength, float fTurbulence)
{
	// Base wind
	vec2 xWind = CalculateWind(xWorldPos, fTime, xWindDir, fStrength);

	// Add perpendicular turbulence
	vec2 xPerpDir = vec2(-xWindDir.y, xWindDir.x);
	float fTurbWave = sin(dot(xWorldPos, xPerpDir) * 0.3 + fTime * 3.0) * fTurbulence;

	return xWind + xPerpDir * fTurbWave * 0.3;
}

// Wind for trees/larger foliage - slower, heavier movement
vec2 CalculateWindTree(vec2 xWorldPos, float fTime, vec2 xWindDir, float fStrength)
{
	// Slower primary wave
	float fWave = sin(dot(xWorldPos, xWindDir) * 0.05 + fTime * 0.8);

	// Slower gust
	float fGustPhase = dot(xWorldPos, xWindDir) * 0.01 + fTime * 0.15;
	float fGust = pow(sin(fGustPhase) * 0.5 + 0.5, 3.0);

	return xWindDir * fStrength * fWave * (0.5 + fGust * 0.5);
}

// Procedural noise for wind variation
float WindNoise(vec2 xPos, float fTime)
{
	// Simple hash-based noise
	vec2 xFloor = floor(xPos);
	vec2 xFract = fract(xPos);

	// Smooth interpolation
	xFract = xFract * xFract * (3.0 - 2.0 * xFract);

	float a = fract(sin(dot(xFloor, vec2(12.9898, 78.233))) * 43758.5453);
	float b = fract(sin(dot(xFloor + vec2(1.0, 0.0), vec2(12.9898, 78.233))) * 43758.5453);
	float c = fract(sin(dot(xFloor + vec2(0.0, 1.0), vec2(12.9898, 78.233))) * 43758.5453);
	float d = fract(sin(dot(xFloor + vec2(1.0, 1.0), vec2(12.9898, 78.233))) * 43758.5453);

	return mix(mix(a, b, xFract.x), mix(c, d, xFract.x), xFract.y);
}
