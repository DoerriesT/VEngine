#ifndef PACKING_H
#define PACKING_H

uint packUnorm2x16(float2 v)
{
	uint result = 0;
	result |= ((uint)round(saturate(v.x) * 65535.0)) << 16;
	result |= ((uint)round(saturate(v.x) * 65535.0)) << 0;
	
	return result;
}

uint packSnorm2x16(float2 v)
{
	uint result = 0;
	result |= ((uint)round(clamp(v.x, -1.0, 1.0) * 32767.0)) << 16;
	result |= ((uint)round(clamp(v.x, -1.0, 1.0) * 32767.0)) << 0;
	
	return result;
}

uint packUnorm4x8(float4 v)
{
	uint result = 0;
	result |= ((uint)round(saturate(v.x) * 255.0)) << 24;
	result |= ((uint)round(saturate(v.y) * 255.0)) << 16;
	result |= ((uint)round(saturate(v.z) * 255.0)) << 8;
	result |= ((uint)round(saturate(v.w) * 255.0)) << 0;
	
	return result;
}

uint packSnorm4x8(float4 v)
{
	uint result = 0;
	result |= ((uint)round(clamp(v.x, -1.0, 1.0) * 127.0)) << 24;
	result |= ((uint)round(clamp(v.y, -1.0, 1.0) * 127.0)) << 16;
	result |= ((uint)round(clamp(v.z, -1.0, 1.0) * 127.0)) << 8;
	result |= ((uint)round(clamp(v.w, -1.0, 1.0) * 127.0)) << 0;
	
	return result;
}

float2 unpackUnorm2x16(uint p)
{
	float2 result;
	result.x = (p >> 16) & 0xFFFF;
	result.y = (p >> 0) & 0xFFFF;
	
	return result * (1.0 / 65535.0);
}

float2 unpackSnorm2x16(uint p)
{
	int2 iresult;
	iresult.x = (int)((p >> 16) & ((1 << 15) - 1));
	iresult.y = (int)((p >> 0) & ((1 << 15) - 1));
	
	iresult.x = (p & (1 << 31)) != 0 ? -iresult.x : iresult.x;
	iresult.y = (p & (1 << 15)) != 0 ? -iresult.y : iresult.y;
	
	return clamp(iresult * (1.0 / 32767.0), -1.0, 1.0);
}

float4 unpackUnorm4x8(uint p)
{
	float4 result;
	result.x = (p >> 24) & 0xFF;
	result.y = (p >> 16) & 0xFF;
	result.z = (p >> 8) & 0xFF;
	result.w = (p >> 0) & 0xFF;
	
	return result * (1.0 / 255.0);
}

float4 unpackSnorm4x8(uint p)
{
	int4 iresult;
	iresult.x = (int)((p >> 24) & ((1 << 15) - 1));
	iresult.y = (int)((p >> 16) & ((1 << 15) - 1));
	iresult.z = (int)((p >> 8) & ((1 << 15) - 1));
	iresult.w = (int)((p >> 0) & ((1 << 15) - 1));
	
	iresult.x = (p & (1 << 31)) != 0 ? -iresult.x : iresult.x;
	iresult.y = (p & (1 << 23)) != 0 ? -iresult.y : iresult.y;
	iresult.z = (p & (1 << 15)) != 0 ? -iresult.z : iresult.z;
	iresult.z = (p & (1 << 7)) != 0 ? -iresult.w : iresult.w;
	
	return clamp(iresult * (1.0 / 127.0), -1.0, 1.0);
}

#endif // PACKING_H