#version 450

layout(location = 0) flat in vec4 vColor;

layout(location = 0) out vec4 oColor;

void main() 
{
	oColor = vColor;
}

