//GLSL 4.5
#version 450

//shader in
layout (location = 0) in vec3 inColor;
layout (location = 1) in vec2 inUV;

//output write
layout (location = 0) out vec4 outFragColor;

//Texture
layout (set = 0, binding = 0) uniform sampler2D displayTexture;

void main()
{
	outFragColor = texture(displayTexture, inUV);
}