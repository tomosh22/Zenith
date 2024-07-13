layout(std140, set = 0, binding=0) uniform FrameConstants{
	mat4 g_xViewMat;
	mat4 g_xProjMat;
	mat4 g_xViewProjMat;
	mat4 g_xInvViewProjMat;
	vec4 g_xCamPos;//vec3 plus 4 bytes of padding
	ivec2 g_xScreenRes;
	vec2 g_xRecipScreenRes;
};