#include "bindingHelper.hlsli"
#include "velocityInitialization.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

Texture2D<float2> g_DepthImage : REGISTER_SRV(DEPTH_IMAGE_BINDING, 0);

PUSH_CONSTS(PushConsts, g_PushConsts);

float4 main(PSInput input) : SV_Target0
{
	// get current and previous frame's pixel position
	float depth = g_DepthImage.Load(int3(int2(input.position.xy), 0)).x;
	float4 reprojectedPos = mul(g_PushConsts.reprojectionMatrix, float4(input.texCoord * float2(2.0, -2.0) - float2(1.0, -1.0), depth, 1.0));
	float2 previousTexCoord = (reprojectedPos.xy / reprojectedPos.w) * float2(0.5, -0.5) + 0.5;

	// calculate delta caused by camera movement
	float2 cameraVelocity = input.texCoord - previousTexCoord;
	
	return float4(cameraVelocity, 1.0, 1.0);
}