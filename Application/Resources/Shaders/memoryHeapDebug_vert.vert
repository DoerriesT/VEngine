#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(location = 0) out flat vec3 vColor;

layout(push_constant) uniform PushConsts 
{
	vec4 scaleBias;
	vec3 color;
} uPushConsts;

void main() 
{
	vec2 pos;
	pos.x = float(((gl_VertexIndex + 2) / 3) & 1); 
    pos.y = float(((gl_VertexIndex + 1) / 3) & 1); 
	pos = pos * uPushConsts.scaleBias.xy + uPushConsts.scaleBias.zw;
	pos = pos * 2.0 - 1.0;
    gl_Position = vec4(pos, 0.0, 1.0);
	vColor = uPushConsts.color;
}

