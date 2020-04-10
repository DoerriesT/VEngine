struct PSInput
{
	float4 ray : TEXCOORD;
};

struct PSOutput
{
	float4 color : SV_Target0;
	float4 normal : SV_Target1;
	float4 specularRoughness : SV_Target2;
};

[earlydepthstencil]
PSOutput main(PSInput input)
{
	PSOutput output;
	
	output.color = float4(float3(0.529, 0.808, 0.922), 1.0);
	output.normal = 0.0;
	output.specularRoughness = 0.0;
	
	return output;
}