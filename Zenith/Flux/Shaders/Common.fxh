layout(std140, set = 0, binding=0) uniform FrameConstants{
	mat4 g_xViewMat;
	mat4 g_xProjMat;
	mat4 g_xViewProjMat;
	vec4 g_xCamPos_Pad;
	vec4 g_xSunDir_Pad;
	vec4 g_xSunColour_Pad;
};

struct DirectionalLight{
    vec4 m_xDirection;//4 bytes of padding
    vec4 m_xColour;
};