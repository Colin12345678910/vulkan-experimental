#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#include "input_structures.glsl"
#include "math.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 shadowPos;
layout (location = 4) in vec3 inPos;

layout (location = 5) in mat3 inTBN;

layout (location = 0) out vec4 outFragColor;

vec3 offsetLookup(sampler2D map, vec4 location, vec2 offset)
{
	vec2 texmapScale = 2.0 / textureSize(shadowTex, 0);
	
	//vec3 lk = textureProj(map, vec4(location.xy + offset * texmapScale * location.w, location.z, location.w)).xyz;
	return texture(map, location.xy + offset * texmapScale).xyz;
}
float shadowCoeff(float bias)
{
	float sum = 0;
	float x, y = 0;
	vec3 shade = shadowPos.xyz / shadowPos.w; 
	float currentDepth = shade.z;
	shade = shade * 0.5 + 0.5;
	
	float rand;
	rand = fract(sin(inPos.x) * sin(inPos.y) * sin(inPos.z) * 19523.12512356);
	if (sceneData.time.w == 0.0f)
	{
		rand = 0.0f;
		
		return offsetLookup(shadowTex, shade.xyzz, vec2(x + rand, y + rand)).x > currentDepth + bias? 0.0 : 1;
	}
	
	for (y = -1.5; y <= 1.5; y += 1.0)
	{
		for (x = -1.5; x <= 1.5; x += 1.0)
		{
			sum += offsetLookup(shadowTex, shade.xyzz, vec2(x + rand, y + rand)).x > currentDepth + bias? 0 : 1;
		}
	}
	float avg = smoothstep(0.0, 1.0, sum / 16 );
	
	return avg;
}


void main()
{
	mat3 TBN = inTBN;
	 TBN[0] = normalize(inTBN[0]);
	 TBN[2] = normalize(inTBN[2]);
	 TBN[1] = normalize(cross(TBN[2], TBN[0]));

	//PBR pathway

	//Here we set up some values we pull from textures
	vec3 normal = normalize(texture(normalTex, inUV) * 2.0 - 1.0).xyz;
	//vec3 normal = normalize(vec4(1.0, 1.0, 1.0, 1.0) * 2.0 - 1.0).xyz;
	//normal = vec3(0.5, 0.5, 1.0);

	normal = normalize(TBN * normal);
	//normal = inNormal;

	vec4 metalicRoughness = texture(metalRoughTex, inUV);
	float metallic = metalicRoughness.b;
	float ao = metalicRoughness.r;
	float roughness = max(0.02, metalicRoughness.g);

	//Setup albedo + stub for ao.
	vec3 albedo = texture(colorTex, inUV).xyz;
	albedo = pow(albedo, vec3(2.2));

	//Light colour and settings, mostly hardcoded for now.
	vec3 lightCol = vec3(3.0);
	vec3 wi = sceneData.sunlightDirection.xyz;
	float attenuation = 1.0f;
	vec3 radiance = lightCol * attenuation;

	// Setup 
	vec3 L = wi;

	//Normal * LightDir
	float NdotL = max(dot(normal, L), 0.0);

	//This attempts to create a shadow bias based on the angle bewteen the normal + light
	float shadowBias = max(0.0005 * (1.0 - NdotL), 0.00005);

	float shadowDepth = shadowCoeff(shadowBias);
	//ViewDir
	vec3 viewDir = normalize(sceneData.cameraPos.xyz - inPos.xyz);
	//viewDir = vec3(0, 0, 1);

	vec3 halfWay = normalize(viewDir + L);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	//Sum of all lighting
	//This is just skylight for now.
	vec3 Lo = vec3(0.0);
	
	//Cook-torrance BDRF
	//float NDF = DistributionGGX(normal, halfWay, roughness);
	//float G = GeometrySmith(normal, viewDir, L, roughness);

	float NDotV = max(dot(normal, viewDir), 0.01);

	vec3 F = fresnelSchlickRoughness(NDotV, F0, roughness);

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;
	vec3 irradiance = texture(irradianceImage, normal).rgb;
	vec3 diffuse = irradiance * albedo;

	//Specular
	vec3 specular = vec3(0.0);

	vec3 R = normalize(reflect(viewDir, normal)); //ViewDir does not need inversion, as the cubemap is already inverted.

	const float MAX_REFLECTION_LOD = 9; 

	vec3 prefilteredColour = textureLod(prefilterMap, R, roughness * MAX_REFLECTION_LOD).rgb;
	vec2 envBDRF = texture(brdfLUT, vec2(max(dot(normal, viewDir), 0.0), roughness)).rg;
	specular = prefilteredColour * (F * envBDRF.x + envBDRF.y);

	// vec3 numerator = NDF * G * F;
	// float denom = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
	// specular += numerator / denom;

	vec3 ambient = (kD * diffuse + specular);

	
	Lo += shadowDepth * (kD * albedo / PI + specular) * radiance * NdotL; //Add light

	//Add skylight ambient.
	vec3 skyLight = ambient;

	vec3 col = skyLight + Lo;
	col /= (col + vec3(1.0));
	col = pow(col, vec3(1.0/2.2));
	outFragColor = vec4(col, 1.0);
	//outFragColor = vec4(normalize(inTBN[2]) * 0.5 + 0.5, 1.0);
	//outFragColor = vec4(normal, 0.0);
}