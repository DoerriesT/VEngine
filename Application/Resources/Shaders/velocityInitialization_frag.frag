#version 450

#include "velocityInitialization_bindings.h"

layout(set = DEPTH_IMAGE_SET, binding = DEPTH_IMAGE_BINDING) uniform texture2D uDepthImage;
layout(set = POINT_SAMPLER_SET, binding = POINT_SAMPLER_BINDING) uniform sampler uPointSampler;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts  uPushConsts;
};

layout(location = 0) in vec2 vTexCoords;

layout(location = 0) out vec4 oResult;

void main() 
{
	// get current and previous frame's pixel position
	float depth = texelFetch(sampler2D(uDepthImage, uPointSampler), ivec2(gl_FragCoord.xy), 0).x;
	vec4 reprojectedPos = uPushConsts.reprojectionMatrix * vec4(vTexCoords * 2.0 - 1.0, depth, 1.0);
	vec2 previousTexCoords = (reprojectedPos.xy / reprojectedPos.w) * 0.5 + 0.5;

	// calculate delta caused by camera movement
	vec2 cameraVelocity = vTexCoords - previousTexCoords;
	
	oResult = vec4(cameraVelocity, 1.0, 1.0);
}

