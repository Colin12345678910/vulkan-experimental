#version 450

#extension GL_GOOGLE_include_directive : require
#extension GL_EXT_buffer_reference : require
#include "input_structures.glsl"

layout (location = 0) in vec3 inNormal;
layout (location = 1) in vec3 inColor;
layout (location = 2) in vec2 inUV;
layout (location = 3) in vec4 shadowPos;
layout (location = 4) in vec3 inPos;

layout (location = 5) in mat3 inTBN;

layout (location = 0) out vec4 outFragColor;

const float PI = 3.14159265359;

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
		
		return offsetLookup(shadowTex, shade.xyzz, vec2(x + rand, y + rand)).x > currentDepth + bias? 0.7 : 1;
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
//PBR functions
//At the moment I am pulling the functions from https://learnopengl.com/PBR/Lighting
//At some point I wish to make my code more distinct obviously, but when it comes to math functions
//It's probably best to use what's available and avoid trying to fully rewrite them before I fully understand
//what they do

vec3 fresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}  

float DistributionGGX(vec3 N, vec3 H, float roughness)
{
    float a      = roughness*roughness;
    float a2     = a*a;
    float NdotH  = max(dot(N, H), 0.0);
    float NdotH2 = NdotH*NdotH;
	
    float num   = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return num / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r*r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return num / denom;
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness);
    float ggx1  = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

void main()
{
	

	//PBR pathway

	//Here we set up some values we pull from textures
	vec3 normal = normalize(texture(normalTex, inUV) * 2.0 - 1.0).xyz;
	normal = normalize(inTBN * normal);
	vec4 metalicRoughness = texture(metalRoughTex, inUV);
	float metallic = metalicRoughness.b;
	float roughness = max(0.04, metalicRoughness.g);

	//Setup albedo + stub for ao.
	vec3 albedo = texture(colorTex, inUV).xyz;
	albedo = pow(albedo, vec3(2.2));
	float ao = 1.0;

	//Light colour and settings, mostly hardcoded for now.
	vec3 lightCol = vec3(3.0);
	vec3 wi = sceneData.sunlightDirection.xyz;
	float attenuation = 1.0f;
	vec3 radiance = lightCol * attenuation;

	// Setup 
	vec3 L = wi;


	//ViewDir
	vec3 viewDir = normalize(sceneData.cameraPos.xyz - inPos.xyz);
	vec3 halfWay = normalize(viewDir + L);

	vec3 F0 = vec3(0.04);
	F0 = mix(F0, albedo, metallic);

	//Sum of all lighting
	//This is just skylight for now.
	vec3 Lo = vec3(0.0);
	
	//Cook-torrance BDRF
	float NDF = DistributionGGX(normal, halfWay, roughness);
	float G = GeometrySmith(normal, viewDir, L, roughness);
	vec3 F    = fresnelSchlick(max(dot(halfWay, viewDir), 0.0), F0); 

	vec3 numerator = NDF * G * F;
	float denom = 4.0 * max(dot(normal, viewDir), 0.0) * max(dot(normal, L), 0.0) + 0.0001;
	vec3 specular = numerator / denom;

	vec3 kS = F;
	vec3 kD = vec3(1.0) - kS;
	kD *= 1.0 - metallic;
	vec3 irradiance = texture(irradianceImage, normal).rgb;
	vec3 diffuse = irradiance * albedo;
	vec3 ambient = (kD * diffuse) * ao;

	//Normal * LightDir
	float NdotL = max(dot(normal, L), 0.0);

	//This attempts to create a shadow bias based on the angle bewteen the normal + light
	float shadowBias = max(0.0005 * (1.0 - NdotL), 0.00005);

	float shadowDepth = shadowCoeff(shadowBias);
	Lo += shadowDepth * (kD * albedo / PI + specular) * radiance * NdotL; //Add light

	//Add skylight ambient.
	vec3 skyLight = ambient;

	vec3 col = skyLight + Lo;
	col /= (col + vec3(1.0));
	col = pow(col, vec3(1.0/2.2));
	outFragColor = vec4(col, 1.0);

	


	// vec4 metalicRoughness = texture(metalRoughTex, inUV);
	// vec3 normal = normalize(texture(normalTex, inUV) * 2.0 - 1.0).xyz;//normalize(inNormal);//normalize(vec4(inNormal, 0.0) * texture(normalTex, inUV));
	// normal = normalize(inTBN * normal);
	
	// vec3 color = inColor * texture(colorTex, inUV).xyz;
	
	// float lightValue = max(dot(normal.xyz, sceneData.sunlightDirection.xyz), 0.0f);
	// vec3 diffuse = vec3(lightValue);
	
	// float specularStrength = 5.0f * metalicRoughness.b;
	
	// vec3 viewDir = normalize(sceneData.cameraPos.xyz - inPos.xyz);
	// vec3 halfwayDir = normalize(sceneData.sunlightDirection.xyz + viewDir);
	
	// float spec = pow(max(dot(normal, halfwayDir), 0.0), 16 * metalicRoughness.g);
	// vec3 specular = specularStrength * spec * vec3(1.0);
	
	// float shadowDepth = shadowCoeff();
	
	// vec3 result = color * (sceneData.ambientColor.xyz + diffuse + specular);
	// result *= shadowDepth;
	// outFragColor = vec4(result, 1.0f);
}