#include "bindingHelper.hlsli"
#include "rasterTiling.hlsli"
#include "common.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	[[vk::builtin("HelperInvocation")]] bool helperInvocation : HELPER_INVOCATION;
};

RWByteAddressBuffer g_PunctualLightsBitMask : REGISTER_UAV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, PUNCTUAL_LIGHTS_BIT_MASK_SET);
RWByteAddressBuffer g_PunctualLightsShadowedBitMask : REGISTER_UAV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_SET);

PUSH_CONSTS(PushConsts, g_PushConsts);

uint WaveCompactValue(uint checkValue, bool helperInvocation)
{
	uint index = 128; // index of all threads with the same checkValue
	for (;;) // loop until all active threads are removed
	{
		if (helperInvocation)
		{
			break;
		}
		uint firstValue = WaveReadLaneFirst(checkValue);

		// exclude all threads with firstValue from next iteration
		if (firstValue == checkValue)
		{
			// get index of this thread in all other threads with the same checkValue
			index = WavePrefixCountBits(firstValue == checkValue);
			break;
		}
	}
	
	return index;
}

void main(PSInput input)
{
	uint2 tile = (uint2(input.position.xy) * 2) / TILE_SIZE;
	uint tileIndex = tile.x + tile.y * g_PushConsts.alignedDomainSizeX;
	
	const uint lightBit = 1 << (g_PushConsts.index % 32);
	const uint word = g_PushConsts.index / 32;
	uint wordIndex = tileIndex * g_PushConsts.wordCount + word;
	
	const uint idx = WaveCompactValue(wordIndex, input.helperInvocation);
	
	// branch only for first occurrence of unique key within subgroup
	if (idx == 0)
	{
		wordIndex <<= 2;
		if (g_PushConsts.targetBuffer == 0)
		{
			g_PunctualLightsBitMask.InterlockedOr(wordIndex, lightBit);
		}
		else if (g_PushConsts.targetBuffer == 1)
		{
			g_PunctualLightsShadowedBitMask.InterlockedOr(wordIndex, lightBit);
		}
	}
}