#pragma once

#define FLUX_MAX_QUADS_PER_FRAME 1024

class Flux_Quads
{
public:
	struct Quad
	{
		Quad() = default;
		Quad(Zenith_Maths::UVector4 xPosition_Size, Zenith_Maths::Vector4 xColour, uint32_t uTexture, Zenith_Maths::Vector2 xUVMult_UVAdd,
			float fCornerRadius = 0.0f, Zenith_Maths::Vector2 xSizePixels = {0,0}, Zenith_Maths::Vector4 xColour2 = {-1,-1,-1,-1})
			: m_xPosition_Size(xPosition_Size)
			, m_xColour(xColour)
			, m_uTexture(uTexture)
			, m_xUVMult_UVAdd(xUVMult_UVAdd)
			, m_fCornerRadius(fCornerRadius)
			, m_xSizePixels(xSizePixels)
			, m_xColour2(xColour2)
		{

		}
		Zenith_Maths::UVector4 m_xPosition_Size;
		Zenith_Maths::Vector4 m_xColour;
		uint32_t m_uTexture;
		Zenith_Maths::Vector2 m_xUVMult_UVAdd;
		float m_fCornerRadius = 0.0f;
		Zenith_Maths::Vector2 m_xSizePixels = {0, 0};
		Zenith_Maths::Vector4 m_xColour2 = {-1, -1, -1, -1};
	};

	static void Initialise();
	static void Shutdown();

	static void Reset();  // Clear state when scene resets (e.g., Play/Stop transitions)

	static void Render(void*);

	static void SubmitRenderTask();
	static void WaitForRenderTask();

	static void UploadQuad(const Quad& xQuad);

private:
	static void UploadInstanceData();

	static Quad s_axQuadsToRender[FLUX_MAX_QUADS_PER_FRAME];
	static uint32_t s_uQuadRenderIndex;
};