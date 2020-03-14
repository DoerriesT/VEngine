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
PSOutput main(PSInput psIn)
{
	PSOutput psOut;
	
	psOut.color = float4(float3(0.529, 0.808, 0.922), 1.0);
	psOut.normal = 0.0;
	psOut.specularRoughness = 0.0;
	
	return psOut;
}