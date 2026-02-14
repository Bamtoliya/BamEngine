layout(std140, set = 1, binding = 0) uniform CommonData
{
	mat4 view;
	mat4 projection;
	mat4 viewProjection;
	vec3 cameraPos;
	float time;
} commondata;