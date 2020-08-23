#include "bindingHelper.hlsli"
#include "probeCompressBCH6.hlsli"
#include "common.hlsli"

RWTexture2DArray<uint4> g_ResultImages[5] : REGISTER_UAV(RESULT_IMAGE_BINDING, 0);
Texture2DArray<float4> g_InputImages[5] : REGISTER_SRV(INPUT_IMAGE_BINDING, 0);

SamplerState g_Samplers[SAMPLER_COUNT] : REGISTER_SAMPLER(0, 1);

PUSH_CONSTS(PushConsts, g_PushConsts);

// based on https://github.com/knarkowicz/GPURealTimeBC6H/blob/master/bin/compress.hlsl

static const float HALF_MAX = 65504.0;

// Refine endpoints by insetting bounding box in log2 RGB space
void insetColorBBoxP1(float3 texels[16], inout float3 blockMin, inout float3 blockMax)
{
	float3 refinedBlockMin = blockMax;
	float3 refinedBlockMax = blockMin;

	for (uint i = 0; i < 16; ++i)
	{
		refinedBlockMin = min(refinedBlockMin, texels[i] == blockMin ? refinedBlockMin : texels[i]);
		refinedBlockMax = max(refinedBlockMax, texels[i] == blockMax ? refinedBlockMax : texels[i]);
	}

	float3 logRefinedBlockMax = log2(refinedBlockMax + 1.0);
	float3 logRefinedBlockMin = log2(refinedBlockMin + 1.0);

	float3 logBlockMax = log2(blockMax + 1.0);
	float3 logBlockMin = log2(blockMin + 1.0);
	float3 logBlockMaxExt = (logBlockMax - logBlockMin) * (1.0 / 32.0);

	logBlockMin += min(logRefinedBlockMin - logBlockMin, logBlockMaxExt);
	logBlockMax -= min(logBlockMax - logRefinedBlockMax, logBlockMaxExt);

	blockMin = exp2(logBlockMin) - 1.0;
	blockMax = exp2(logBlockMax) - 1.0;
}

uint computeIndex4(float texelPos, float endPoint0Pos, float endPoint1Pos)
{
	float r = (texelPos - endPoint0Pos) / (endPoint1Pos - endPoint0Pos);
	return (uint) clamp(r * 14.93333 + 0.03333 + 0.5, 0.0, 15.0);
}

// Least squares optimization to find best endpoints for the selected block indices
void optimizeEndpointsP1(float3 texels[16], inout float3 blockMin, inout float3 blockMax)
{
	float3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);

	float endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	float endPoint1Pos = f32tof16(dot(blockMax, blockDir));

	float3 alphaTexelSum = 0.0;
	float3 betaTexelSum = 0.0;
	float alphaBetaSum = 0.0;
	float alphaSqSum = 0.0;
	float betaSqSum = 0.0;

	for (int i = 0; i < 16; i++)
	{
		float texelPos = f32tof16(dot(texels[i], blockDir));
		uint texelIndex = computeIndex4(texelPos, endPoint0Pos, endPoint1Pos);

		float beta = saturate(texelIndex / 15.0);
		float alpha = 1.0 - beta;

		float3 texelF16 = f32tof16(texels[i].xyz);
		alphaTexelSum += alpha * texelF16;
		betaTexelSum += beta * texelF16;

		alphaBetaSum += alpha * beta;

		alphaSqSum += alpha * alpha;
		betaSqSum += beta * beta;
	}

	float det = alphaSqSum * betaSqSum - alphaBetaSum * alphaBetaSum;

	if (abs(det) > 0.00001)
	{
		float detRcp = rcp(det);
		blockMin = f16tof32(clamp(detRcp * (alphaTexelSum * betaSqSum - betaTexelSum * alphaBetaSum), 0.0, HALF_MAX));
		blockMax = f16tof32(clamp(detRcp * (betaTexelSum * alphaSqSum - alphaTexelSum * alphaBetaSum), 0.0, HALF_MAX));
	}
}

void swap(inout float3 a, inout float3 b)
{
	float3 tmp = a;
	a = b;
	b = tmp;
}

void swap(inout float a, inout float b)
{
	float tmp = a;
	a = b;
	b = tmp;
}

float3 quantize10(float3 x)
{
	return (f32tof16(x) * 1024.0f) / (0x7bff + 1.0f);
}

void encodeP1(inout uint4 block, float3 texels[16])
{
	// compute endpoints
	float3 blockMin = texels[0];
	float3 blockMax = texels[0];
	{
		for (int i = 1; i < 16; ++i)
		{
			blockMin = min(blockMin, texels[i]);
			blockMax = max(blockMax, texels[i]);
		}
	}
	
	
	insetColorBBoxP1(texels, blockMin, blockMax);
	//optimizeEndpointsP1(texels, blockMin, blockMax);
	
	float3 blockDir = blockMax - blockMin;
	blockDir = blockDir / (blockDir.x + blockDir.y + blockDir.z);
	
	float3 endpoint0 = quantize10(blockMin);
	float3 endpoint1 = quantize10(blockMax);
	float endPoint0Pos = f32tof16(dot(blockMin, blockDir));
	float endPoint1Pos = f32tof16(dot(blockMax, blockDir));

	// check if endpoint swap is required
	float fixupTexelPos = f32tof16(dot(texels[0], blockDir));
	uint fixupIndex = computeIndex4(fixupTexelPos, endPoint0Pos, endPoint1Pos);
	if (fixupIndex > 7)
	{
		swap(endPoint0Pos, endPoint1Pos);
		swap(endpoint0, endpoint1);
	}

	// compute indices
	uint indices[16];
	{
		for (uint i = 0; i < 16; ++i)
		{
			float texelPos = f32tof16(dot(texels[i], blockDir));
			indices[i] = computeIndex4(texelPos, endPoint0Pos, endPoint1Pos);
		}
	}
	
	// encode block for mode 11
	block.x = 0x03;

	// endpoints
	block.x |= (uint) endpoint0.x << 5;
	block.x |= (uint) endpoint0.y << 15;
	block.x |= (uint) endpoint0.z << 25;
	block.y |= (uint) endpoint0.z >> 7;
	block.y |= (uint) endpoint1.x << 3;
	block.y |= (uint) endpoint1.y << 13;
	block.y |= (uint) endpoint1.z << 23;
	block.z |= (uint) endpoint1.z >> 9;

	// indices
	block.z |= indices[0] << 1;
	block.z |= indices[1] << 4;
	block.z |= indices[2] << 8;
	block.z |= indices[3] << 12;
	block.z |= indices[4] << 16;
	block.z |= indices[5] << 20;
	block.z |= indices[6] << 24;
	block.z |= indices[7] << 28;
	block.w |= indices[8] << 0;
	block.w |= indices[9] << 4;
	block.w |= indices[10] << 8;
	block.w |= indices[11] << 12;
	block.w |= indices[12] << 16;
	block.w |= indices[13] << 20;
	block.w |= indices[14] << 24;
	block.w |= indices[15] << 28;
}

[numthreads(8, 8, 1)]
void main(uint3 threadID : SV_DispatchThreadID)
{
	if (threadID.x >= g_PushConsts.resultRes || threadID.y >= g_PushConsts.resultRes)
	{
		return;
	}
	
	float2 texCoord = threadID.xy * g_PushConsts.texelSize * 4.0 + g_PushConsts.texelSize;
	float2 block0TexCoord = texCoord;
	float2 block1TexCoord = texCoord + float2(2.0 * g_PushConsts.texelSize, 0.0);
	float2 block2TexCoord = texCoord + float2(0.0, 2.0 * g_PushConsts.texelSize);
	float2 block3TexCoord = texCoord + 2.0 * g_PushConsts.texelSize;
	
	float4 block0Red = g_InputImages[g_PushConsts.mip].GatherRed(g_Samplers[SAMPLER_POINT_CLAMP], float3(block0TexCoord, threadID.z));
	float4 block0Green = g_InputImages[g_PushConsts.mip].GatherGreen(g_Samplers[SAMPLER_POINT_CLAMP], float3(block0TexCoord, threadID.z));
	float4 block0Blue = g_InputImages[g_PushConsts.mip].GatherBlue(g_Samplers[SAMPLER_POINT_CLAMP], float3(block0TexCoord, threadID.z));

	float4 block1Red = g_InputImages[g_PushConsts.mip].GatherRed(g_Samplers[SAMPLER_POINT_CLAMP], float3(block1TexCoord, threadID.z));
	float4 block1Green = g_InputImages[g_PushConsts.mip].GatherGreen(g_Samplers[SAMPLER_POINT_CLAMP], float3(block1TexCoord, threadID.z));
	float4 block1Blue = g_InputImages[g_PushConsts.mip].GatherBlue(g_Samplers[SAMPLER_POINT_CLAMP], float3(block1TexCoord, threadID.z));
	
	float4 block2Red = g_InputImages[g_PushConsts.mip].GatherRed(g_Samplers[SAMPLER_POINT_CLAMP], float3(block2TexCoord, threadID.z));
	float4 block2Green = g_InputImages[g_PushConsts.mip].GatherGreen(g_Samplers[SAMPLER_POINT_CLAMP], float3(block2TexCoord, threadID.z));
	float4 block2Blue = g_InputImages[g_PushConsts.mip].GatherBlue(g_Samplers[SAMPLER_POINT_CLAMP], float3(block2TexCoord, threadID.z));
	
	float4 block3Red = g_InputImages[g_PushConsts.mip].GatherRed(g_Samplers[SAMPLER_POINT_CLAMP], float3(block3TexCoord, threadID.z));
	float4 block3Green = g_InputImages[g_PushConsts.mip].GatherGreen(g_Samplers[SAMPLER_POINT_CLAMP], float3(block3TexCoord, threadID.z));
	float4 block3Blue = g_InputImages[g_PushConsts.mip].GatherBlue(g_Samplers[SAMPLER_POINT_CLAMP], float3(block3TexCoord, threadID.z));

	float3 texels[16];
	texels[0] = float3(block0Red.w, block0Green.w, block0Blue.w);
	texels[1] = float3(block0Red.z, block0Green.z, block0Blue.z);
	texels[2] = float3(block1Red.w, block1Green.w, block1Blue.w);
	texels[3] = float3(block1Red.z, block1Green.z, block1Blue.z);
	texels[4] = float3(block0Red.x, block0Green.x, block0Blue.x);
	texels[5] = float3(block0Red.y, block0Green.y, block0Blue.y);
	texels[6] = float3(block1Red.x, block1Green.x, block1Blue.x);
	texels[7] = float3(block1Red.y, block1Green.y, block1Blue.y);
	texels[8] = float3(block2Red.w, block2Green.w, block2Blue.w);
	texels[9] = float3(block2Red.z, block2Green.z, block2Blue.z);
	texels[10] = float3(block3Red.w, block3Green.w, block3Blue.w);
	texels[11] = float3(block3Red.z, block3Green.z, block3Blue.z);
	texels[12] = float3(block2Red.x, block2Green.x, block2Blue.x);
	texels[13] = float3(block2Red.y, block2Green.y, block2Blue.y);
	texels[14] = float3(block3Red.x, block3Green.x, block3Blue.x);
	texels[15] = float3(block3Red.y, block3Green.y, block3Blue.y);
	
	uint4 block = 0;
	encodeP1(block, texels);
	
	g_ResultImages[g_PushConsts.mip][threadID] = block;
}