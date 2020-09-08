#include "bindingHelper.hlsli"
#include "rasterTiling.hlsli"
#include "common.hlsli"

struct PSInput
{
	float4 position : SV_Position;
#if VULKAN
	[[vk::builtin("HelperInvocation")]] bool helperInvocation : HELPER_INVOCATION;
#endif // VULKAN
};

//RWByteAddressBuffer g_PunctualLightsBitMask : REGISTER_UAV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, 0);
//RWByteAddressBuffer g_PunctualLightsShadowedBitMask : REGISTER_UAV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, 0);
//RWByteAddressBuffer g_ParticipatingMediaBitMask : REGISTER_UAV(PARTICIPATING_MEDIA_BIT_MASK_BINDING, 0);
//RWByteAddressBuffer g_ReflectionProbeBitMask : REGISTER_UAV(REFLECTION_PROBE_BIT_MASK_BINDING, 0);

RWTexture2DArray<uint> g_PunctualLightsBitMaskImage : REGISTER_UAV(PUNCTUAL_LIGHTS_BIT_MASK_BINDING, 0);
RWTexture2DArray<uint> g_PunctualLightsShadowedBitMaskImage : REGISTER_UAV(PUNCTUAL_LIGHTS_SHADOWED_BIT_MASK_BINDING, 0);
RWTexture2DArray<uint> g_ParticipatingMediaBitMaskImage : REGISTER_UAV(PARTICIPATING_MEDIA_BIT_MASK_BINDING, 0);
RWTexture2DArray<uint> g_ReflectionProbeBitMaskImage : REGISTER_UAV(REFLECTION_PROBE_BIT_MASK_BINDING, 0);

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
	
#if VULKAN
	const uint idx = WaveCompactValue(wordIndex, input.helperInvocation);
#else
	const uint idx = WaveCompactValue(wordIndex, false);
#endif // VULKAN
	
	// branch only for first occurrence of unique key within subgroup
	if (idx == 0)
	{
		//wordIndex <<= 2;
		if (g_PushConsts.targetBuffer == 0)
		{
			InterlockedOr(g_PunctualLightsBitMaskImage[uint3(tile, word)], lightBit);
			//g_PunctualLightsBitMask.InterlockedOr(wordIndex, lightBit);
		}
		else if (g_PushConsts.targetBuffer == 1)
		{
			InterlockedOr(g_PunctualLightsShadowedBitMaskImage[uint3(tile, word)], lightBit);
			//g_PunctualLightsShadowedBitMask.InterlockedOr(wordIndex, lightBit);
		}
		else if (g_PushConsts.targetBuffer == 2)
		{
			InterlockedOr(g_ParticipatingMediaBitMaskImage[uint3(tile, word)], lightBit);
			//g_ParticipatingMediaBitMask.InterlockedOr(wordIndex, lightBit);
		}
		else if (g_PushConsts.targetBuffer == 3)
		{
			InterlockedOr(g_ReflectionProbeBitMaskImage[uint3(tile, word)], lightBit);
			//g_ReflectionProbeBitMask.InterlockedOr(wordIndex, lightBit);
		}
	}
}