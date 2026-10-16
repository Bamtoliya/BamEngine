layout(std140, set = 1, binding = 0) uniform CommonVertData
{
	mat4 view;
	mat4 projection;
	mat4 viewProjection;
	vec3 cameraPos;
	float time;
} commonVertdata;

layout(std140, set = 3, binding = 0) uniform CommonFragData
{
	mat4 view;
	mat4 projection;
	mat4 viewProjection;
	vec3 cameraPos;
	float time;
} commonFragdata;