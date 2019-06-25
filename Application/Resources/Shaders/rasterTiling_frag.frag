#version 450

#extension GL_KHR_shader_subgroup_ballot : enable

#include "rasterTiling_bindings.h"

layout(set = POINT_LIGHT_MASK_SET, binding = POINT_LIGHT_MASK_BINDING) buffer POINT_LIGHT_BITMASK 
{
	uint uPointLightBitMask[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

uint subgroupCompactValue(uint checkValue)
{
	uvec4 mask; // thread unique compaction mask
	for (;;) // loop until all active threads are removed
	{
		uint firstValue = subgroupBroadcastFirst(checkValue);
		// mask is only updated for active threads
		mask = subgroupBallot(firstValue == checkValue && !gl_HelperInvocation); 
		// exclude all threads with firstValue from next iteration
		if (firstValue == checkValue)
		{
			break;
		}
	}
	
	// at this point, each thread of mask should contain a bit mask of all
	// other lanes with the same value
	uint index = subgroupBallotExclusiveBitCount(mask);
	return index;
}

void main() 
{
	uvec2 tile = (uvec2(gl_FragCoord.xy) * 2) / TILE_SIZE;
	uint tileIndex = tile.x + tile.y * uPushConsts.alignedDomainSizeX;
	
	const uint lightBit = 1 << (uPushConsts.index % 32);
	const uint word = uPushConsts.index / 32;
	//const uint wordCount = (uPerFrameData.pointLightCount + 31) / 32;
	const uint wordIndex = tileIndex * uPushConsts.wordCount + word;
	
	const uint hash = subgroupCompactValue(wordIndex);
	
	// branch only for first occurrence of unique key within subgroup
	if (hash == 0)
	{
		atomicOr(uPointLightBitMask[wordIndex], lightBit);
	}
}

