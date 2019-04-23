#version 450

#include "text_bindings.h"

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform sampler2D uTextures[TEXTURE_ARRAY_SIZE];

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) in vec2 vTexCoords;

layout(location = 0) out vec4 oColor;

void main() 
{
	oColor = vec4(uPushConsts.color.rgb, textureLod(uTextures[uPushConsts.textureIndex - 1], vTexCoords, 0).x);
}

