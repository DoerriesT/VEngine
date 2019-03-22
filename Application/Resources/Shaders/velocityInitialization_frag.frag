#version 450

layout(location = 0) in vec2 vTexCoords;

layout(location = 0) out vec4 oResult;

layout(set = 0, binding = 0) uniform sampler2D uDepthImage;

layout(push_constant) uniform PushConsts 
{
	mat4 reprojectionMatrix;
} uPushConsts;

void main() 
{
	// get current and previous frame's pixel position
	float depth = texelFetch(uDepthImage, ivec2(gl_FragCoord.xy), 0).x;
	vec4 reprojectedPos = uPushConsts.reprojectionMatrix * vec4(vTexCoords * 2.0 - 1.0, depth, 1.0);
	vec2 previousTexCoords = (reprojectedPos.xy / reprojectedPos.w) * 0.5 + 0.5;

	// calculate delta caused by camera movement
	vec2 cameraVelocity = vTexCoords - previousTexCoords;
	
	oResult = vec4(cameraVelocity, 1.0, 1.0);
}

