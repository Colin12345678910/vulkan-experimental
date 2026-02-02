#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require

#include "input_structures.glsl"

layout (location = 0) out vec3 outNormal;
layout (location = 1) out vec3 outColor;
layout (location = 2) out vec2 outUV;
layout (location = 3) out vec4 shadowPos;
layout (location = 4) out vec3 outPos;

layout (location = 5) out mat3 outTBN;

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
	
	//Tangents
	vec3 T = normalize(vec3(PushConstants.renderMatrix * vec4(v.tangent, 0.0)));
	vec3 N = normalize(vec3(PushConstants.renderMatrix * vec4(v.normal, 0.0)));
	vec3 B = cross(outNormal, T);
	
	outTBN = mat3(T, B, N); //Tangents, Bitangents, normal matrix.
}