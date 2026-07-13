#pragma once

#include "Flux/Flux_Enums.h"

#include <cstddef>
#include <string>
#include <vector>

// CPU-owned copy of a terrain grass-density map. Rendering receives its own
// copy through Flux_GrassImpl; this copy remains available in headless mode and
// is intentionally independent of the global grass singleton.
class ZM_GrassDensityMap
{
public:
	static constexpr u_int uEXPECTED_WIDTH = 1024;
	static constexpr u_int uEXPECTED_HEIGHT = 1024;
	static constexpr size_t ulEXPECTED_BYTE_COUNT =
		static_cast<size_t>(uEXPECTED_WIDTH) * uEXPECTED_HEIGHT * sizeof(float);
	static constexpr float fWORLD_SIZE = 4096.0f;

	static std::string BuildCanonicalPath(const std::string& strTerrainAssetDirectory);

	bool Load(const std::string& strPath);
	void Clear();

	// Shared decoded-data validation seam. Production calls this with the fixed
	// 1024-square contract; unit tests inject compact expected dimensions so the
	// format/dimension/byte-count and sampling rules remain cheap to exercise.
	bool LoadDecoded(TextureFormat eFormat, u_int uWidth, u_int uHeight,
		const void* pBytes, size_t ulByteCount, u_int uExpectedWidth,
		u_int uExpectedHeight, float fWorldSize);

	float SampleWorld(float fWorldX, float fWorldZ) const;

	bool IsLoaded() const { return !m_afPixels.empty(); }
	u_int GetWidth() const { return m_uWidth; }
	u_int GetHeight() const { return m_uHeight; }
	float GetWorldSize() const { return m_fWorldSize; }
	const float* GetPixels() const { return IsLoaded() ? m_afPixels.data() : nullptr; }
	size_t GetPixelCount() const { return m_afPixels.size(); }

private:
	std::vector<float> m_afPixels;
	u_int m_uWidth = 0;
	u_int m_uHeight = 0;
	float m_fWorldSize = 0.0f;
};
