#version 450

#include "text_bindings.h"

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) out vec2 vTexCoords;

void main() 
{
	vec2 pos;
	pos.x = float(((gl_VertexIndex + 2) / 3) & 1); 
    pos.y = float(((gl_VertexIndex + 1) / 3) & 1); 
	vTexCoords = uPushConsts.texCoordSize * pos + uPushConsts.texCoordOffset;
	pos = pos * uPushConsts.scaleBias.xy + uPushConsts.scaleBias.zw;
	pos = pos * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
}

