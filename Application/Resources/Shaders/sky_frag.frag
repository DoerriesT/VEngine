#version 450

layout(early_fragment_tests) in;

layout(location = 0) in vec4 vRay;

layout(location = 0) out vec4 oColor;
layout(location = 1) out vec4 oNormal;
layout(location = 2) out vec4 oSpecularRoughness;

void main() 
{
	oColor = vec4(vec3(0.529, 0.808, 0.922), 1.0);
	oNormal = vec4(0.0, 0.0, 0.0, 0.0);
	oSpecularRoughness = vec4(0.0, 0.0, 0.0, 0.0);
}
