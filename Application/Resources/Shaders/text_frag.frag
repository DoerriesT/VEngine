#version 450
#extension GL_ARB_separate_shader_objects : enable

#ifndef TEXTURE_ARRAY_SIZE
#define TEXTURE_ARRAY_SIZE (512)
#endif // TEXTURE_ARRAY_SIZE

layout(location = 0) in vec3 vColor;
layout(location = 1) in vec2 vTexCoords;
layout(location = 2) in flat uint vTextureIndex;

layout(location = 0) out vec4 oColor;

layout(set = 0, binding = 0) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

void main() 
{
	oColor = vec4(vColor, textureLod(uTextures[vTextureIndex - 1], vTexCoords, 0).x);
}

