#ifndef COMMON_VERT_H
#define COMMON_VERT_H

#include "packing.hlsli"

float3 getPosition(uint vertexID, StructuredBuffer<uint> positionsBuffer)
{
	uint baseOffset = vertexID * 3;
	bool lower = (baseOffset & 1) == 0;
	baseOffset /= 2;
	uint packedPositions_0 = positionsBuffer[baseOffset + 0];
	uint packedPositions_1 = positionsBuffer[baseOffset + 1];
	
	float4 pos4 = float4(unpackUnorm2x16(packedPositions_0), unpackUnorm2x16(packedPositions_1));
	
	return lower ? pos4.xyz : pos4.yzw;
}

float4 getQTangent(uint vertexID, StructuredBuffer<uint> tangentBuffer)
{
	return float4(unpackUnorm2x16(tangentBuffer[vertexID * 2 + 0]), unpackUnorm2x16(tangentBuffer[vertexID * 2 + 1])) * 2.0 - 1.0;
}

float2 getTexCoord(uint vertexID, StructuredBuffer<uint> texCoordBuffer)
{
	return unpackUnorm2x16(texCoordBuffer[vertexID]);
}

float4 quaternionMult(float4 p, float4 q)
{
	float4 result;
	result.x = mad(p.w, q.x, mad( p.x, q.w, mad( p.y, q.z, -p.z * q.y)));
	result.y = mad(p.w, q.y, mad( p.y, q.w, mad( p.z, q.x, -p.x * q.z)));
	result.z = mad(p.w, q.z, mad( p.z, q.w, mad( p.x, q.y, -p.y * q.x)));
	result.w = mad(p.w, q.w, mad(-p.x, q.x, mad(-p.y, q.y, -p.z * q.z)));
	
	return result;
}

void quaternionToNormalTangent(float4 q, out float3 normal, out float3 tangent)
{
	float qxx = q.x * q.x;
	float qyy = q.y * q.y;
	float qzz = q.z * q.z;
	float qxz = q.x * q.z;
	float qxy = q.x * q.y;
	float qyz = q.y * q.z;
	float qwx = q.w * q.x;
	float qwy = q.w * q.y;
	float qwz = q.w * q.z;
	
	tangent.x = 1.0 - 2.0 * (qyy +  qzz);
	tangent.y = 2.0 * (qxy + qwz);
	tangent.z = 2.0 * (qxz - qwy);
	
	normal.x = 2.0 * (qxz + qwy);
	normal.y = 2.0 * (qyz - qwx);
	normal.z = 1.0 - 2.0 * (qxx +  qyy);
}

#endif // COMMON_VERT_H