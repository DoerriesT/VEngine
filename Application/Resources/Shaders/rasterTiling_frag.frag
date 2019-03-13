#version 450

#define TILE_SIZE 16
#define MAX_POINT_LIGHT_WORDS 2048

layout(location = 0) in flat uint vIndex;
layout(location = 1) in flat uint vAlignedDomainSizeX;

layout(set = 0, binding = 0) uniform PerFrameData 
{
	float time;
	float fovy;
	float nearPlane;
	float farPlane;
	mat4 viewMatrix;
	mat4 projectionMatrix;
	mat4 viewProjectionMatrix;
	mat4 invViewMatrix;
	mat4 invProjectionMatrix;
	mat4 invViewProjectionMatrix;
	mat4 prevViewMatrix;
	mat4 prevProjectionMatrix;
	mat4 prevViewProjectionMatrix;
	mat4 prevInvViewMatrix;
	mat4 prevInvProjectionMatrix;
	mat4 prevInvViewProjectionMatrix;
	vec4 cameraPosition;
	vec4 cameraDirection;
	uint frame;
} uPerFrameData;

layout(set = 1, binding = 0) buffer PointLightBitMask 
{
	uint mask[];
} uPointLightBitMask;

void main() 
{
	uvec2 tile = uvec2(gl_FragCoord.xy) / TILE_SIZE;
	uint tileIndex = tile.x + tile.y * (vAlignedDomainSizeX / TILE_SIZE + ((vAlignedDomainSizeX % TILE_SIZE == 0) ? 0 : 1));
	
	const uint lightBit = 1 << (vIndex % 32);
	const uint word = vIndex / 32;
	const uint wordIndex = tileIndex * MAX_POINT_LIGHT_WORDS + word;
	
	atomicOr(uPointLightBitMask.mask[wordIndex], lightBit);
}

