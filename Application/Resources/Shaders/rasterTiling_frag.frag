#version 450

#extension GL_KHR_shader_subgroup_ballot : enable

#include "common.h"

layout(location = 0) in flat uint vIndex;
layout(location = 1) in flat uint vAlignedDomainSizeX;

uint subgroupCompactValue(uint checkValue)
{
	uvec4 mask; // thread unique compaction mask
	for (;;) // loop until all active threads are removed
	{
		uint firstValue = subgroupBroadcastFirst(checkValue);
		// mask is only updated for active threads
		mask = subgroupBallot(firstValue == checkValue); 
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
	uint tileIndex = tile.x + tile.y * vAlignedDomainSizeX;
	
	const uint lightBit = 1 << (vIndex % 32);
	const uint word = vIndex / 32;
	const uint wordIndex = tileIndex * MAX_POINT_LIGHT_WORDS + word;
	
	const uint hash = subgroupCompactValue(wordIndex);
	
	// branch only for first occurrence of unique key within subgroup
	if (hash == 0)
	{
		atomicOr(uPointLightBitMask.mask[wordIndex], lightBit);
	}
}

