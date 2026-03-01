#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 shadowPos;
layout (location = 4) out vec3 outPos;

layout (location = 5) out vec3 outT;
layout (location = 6) out vec3 outB;
layout (location = 7) out vec3 outN;

layout(buffer_reference, std430) readonly buffer VertexBuffer
{
	Vertex vertices[];
};

//push constants
layout ( push_constant ) uniform constants
{
	mat4 renderMatrix;
	VertexBuffer vertexBuffer;
} PushConstants;

void main()
{
	Vertex v = PushConstants.vertexBuffer.vertices[gl_VertexIndex];
	
	vec4 position = vec4(v.position, 1.0f);
	
	shadowPos = sceneData.shadowCoord * PushConstants.renderMatrix * position;
	
	
	gl_Position = sceneData.viewProj * PushConstants.renderMatrix * position;
	
	
	outPos = vec3(PushConstants.renderMatrix * position);
	
	outColor = v.color.xyz * materialData.colorFactors.xyz;
	outUV.x = v.uv_x;
	outUV.y = v.uv_y;

	mat4 model = PushConstants.renderMatrix;//transpose(inverse(PushConstants.renderMatrix));
	
	//Tangents
	vec3 T = normalize(vec3(model * vec4(v.tangent.xyz, 0.0)));
	vec3 N = normalize(vec3(model * vec4(v.normal, 0.0)));

	T = normalize(T - dot(T, N) * N);

	vec3 B = normalize(cross(N, T)) * v.tangent.w;
	B = normalize(B);
	T = normalize(T);
	N = normalize(N);
	
	outT = T;
	outN = N;
	outB = B;
	outNormal = N;
}