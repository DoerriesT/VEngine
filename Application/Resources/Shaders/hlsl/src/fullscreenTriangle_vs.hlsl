
struct VSOutput
{
	float4 position : SV_Position;
	float2 texCoord : TEXCOORD;
};

VSOutput main(uint vertexID : SV_VertexID) 
{
	VSOutput output;
	
	float y = -1.0 + float((vertexID & 1) << 2);
	float x = -1.0 + float((vertexID & 2) << 1);

    output.position = float4(x, y, 0.0, 1.0);
	output.texCoord = output.position.xy * 0.5 + 0.5;
	
	return output;
}