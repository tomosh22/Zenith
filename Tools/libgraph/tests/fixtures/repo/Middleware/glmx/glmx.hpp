#pragma once

// Scenario 21: unlisted third-party. Nobody's vcxproj lists this file, and
// no Entry-table middleware node's source root matches "Middleware/glmx" --
// so it synthesizes a plain third-party node named "glmx" (vcxproj: None).
struct glmx_vec3
{
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
};
