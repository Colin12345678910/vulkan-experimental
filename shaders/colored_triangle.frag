#version 450

//Shader input
layout (location = 0) in vec3 inColor;

//ShaderOut
layout (location = 0) out vec4 outFragColor;

void main()
{
	outFragColor = vec4(inColor, 1.0f);
}