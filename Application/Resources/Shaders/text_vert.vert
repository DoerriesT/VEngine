#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out flat vec3 vColor;
layout(location = 1) out vec2 vTexCoords;
layout(location = 2) out flat uint vTextureIndex;

layout(push_constant) uniform PushConsts 
{
	vec4 scaleBias;
	vec4 color;
	vec2 texCoordOffset;
	vec2 texCoordSize;
	uint textureIndex;
} uPushConsts;

void main() 
{
	vec2 pos;
	pos.x = float(((gl_VertexIndex + 2) / 3) & 1); 
    pos.y = float(((gl_VertexIndex + 1) / 3) & 1); 
	vTexCoords = uPushConsts.texCoordSize * pos + uPushConsts.texCoordOffset;
	pos = pos * uPushConsts.scaleBias.xy + uPushConsts.scaleBias.zw;
	pos = pos * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
	vColor = uPushConsts.color.rgb;
	vTextureIndex = uPushConsts.textureIndex;
}

