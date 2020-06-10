#include "bindingHelper.hlsli"

struct PSInput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
	float opacity : OPACITY;
};

struct PSOutput
{
	float4 color : SV_Target0;
};

[earlydepthstencil]
PSOutput main(PSInput input)
{
	PSOutput output = (PSOutput)0;
	output.color = float4(1.0, 0.0, 0.0, input.opacity);
	
	return output;
}