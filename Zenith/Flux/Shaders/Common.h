layout(std140, set = 0, binding=0) uniform FrameConstants{
	mat4 _uViewMat;
	mat4 _uProjMat;
	mat4 _uViewProjMat;
	mat4 _uInvViewProjMat;
	vec4 _uCamPos;//vec3 plus 4 bytes of padding
	ivec2 u_xScreenRes;
	vec2 u_xRecipScreenRes;
};