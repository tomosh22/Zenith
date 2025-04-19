#pragma once

#define FLUX_MAX_QUADS_PER_FRAME 1024

class Flux_Quads
{
public:
	struct Quad
	{
		Quad() = default;
		Quad(Zenith_Maths::UVector4 xPosition_Size, Zenith_Maths::Vector4 xColour, uint32_t uTexture)
			: m_xPosition_Size(xPosition_Size)
			, m_xColour(xColour)
			, m_uTexture(uTexture)
		{

		}
		Zenith_Maths::UVector4 m_xPosition_Size;
		Zenith_Maths::Vector4 m_xColour;
		uint32_t m_uTexture;
	};

	static void Initialise();
	static void Render();

	static void UploadQuad(const Quad& xQuad);

private:
	static void UploadInstanceData();

	static Quad s_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME];
	static uint32_t s_uQuadRenderIndex;
};