layout(set = 0, binding = 0) uniform SceneData
{
	mat4 view;
	mat4 proj;
	mat4 viewProj;
	mat4 shadowCoord;
	vec4 ambientColor;
	vec4 sunlightDirection;
	vec4 sunlightColor;
	vec4 time;
} sceneData;
layout(set = 0, binding = 1) uniform sampler2D shadowTex;

layout (set = 1, binding = 0) uniform GLTFMaterialData
{
	vec4 colorFactors;
	vec4 metalRoughFactors;
} materialData;


layout(set = 1, binding = 1) uniform sampler2D colorTex;
layout(set = 1, binding = 2) uniform sampler2D metalRoughTex;