#version 450

#extension GL_GOOGLE_include_directive : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 shadowPos;
layout (location = 4) in vec3 inPos;

layout (location = 0) out vec4 outFragColor;

vec3 offsetLookup(sampler2D map, vec4 location, vec2 offset)
{
	vec2 texmapScale = 2.0 / textureSize(shadowTex, 0);
	
	//vec3 lk = textureProj(map, vec4(location.xy + offset * texmapScale * location.w, location.z, location.w)).xyz;
	return texture(map, location.xy + offset * texmapScale).xyz;
}
float shadowCoeff()
{
	float sum = 0;
	float x, y;
	vec3 shade = shadowPos.xyz / shadowPos.w; 
	float currentDepth = shade.z;
	shade = shade * 0.5 + 0.5;
	
	float rand;
	rand = fract(sin(inPos.x) * sin(inPos.y) * sin(inPos.z) * 19523.12512356);
	if (sceneData.time.w == 0.0f)
	{
		rand = 0.0f;
		
		return offsetLookup(shadowTex, shade.xyzz, vec2(x + rand, y + rand)).x > currentDepth + 0.00005? 0.7 : 1;
	}
	
	for (y = -1.5; y <= 1.5; y += 1.0)
	{
		for (x = -1.5; x <= 1.5; x += 1.0)
		{
			sum += offsetLookup(shadowTex, shade.xyzz, vec2(x + rand, y + rand)).x > currentDepth + 0.00005? 0 : 1;
		}
	}
	float avg = smoothstep(0.0, 1.0, sum / 16 );
	
	return avg * 0.5 + 0.5;
}
void main()
{
	vec4 tex = texture(colorTex, inUV);
	if (tex.a < 0.2f)
	{
		discard;
		return;
	}
	
	float lightValue = max(dot(inNormal, sceneData.sunlightDirection.xyz), 0.1f);
	lightValue = min(lightValue, 1.0f);
	
	vec3 shadow = shadowPos.xyz / shadowPos.w;
	
	float currentDepth = shadow.z;
	
	//shadow.y = 1 - shadow.y;
	
	shadow = shadow * 0.5 + 0.5;
	
	float shadowDepth = shadowCoeff();//texture(shadowTex, shadow.xy).x;//
	
	
	vec3 color = inColor * tex.xyz;
	vec3 ambient = color * sceneData.ambientColor.xyz;

	//else
	//{
		outFragColor = vec4(color * lightValue * sceneData.sunlightColor.w + ambient, 1.0f);
	//
	//if (currentDepth +0.00015 < shadowDepth)
	//{
		outFragColor = outFragColor * shadowDepth;
	//}
}