#version 450

struct PushConsts
{
	mat4 invModelViewProjection;
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) out vec4 vRay;

void main() 
{
	float y = -1.0 + float((gl_VertexIndex & 1) << 2);
	float x = -1.0 + float((gl_VertexIndex & 2) << 1);
	// set depth to 0 because we use an inverted depth buffer
    gl_Position = vec4(x, y, 0.0, 1.0);
	vRay = uPushConsts.invModelViewProjection * vec4(gl_Position.xy, 0.0,  1.0);
}