#ifndef ENCODING_H
#define ENCODING_H

float packSnorm12Float(float f) 
{
    return round(clamp(f + 1.0, 0.0, 2.0) * float(2047));
}

float3 twoNorm12sEncodedAs3Unorm8sInFloat3Format(float2 s) 
{
    float3 u;
    u.x = s.x * (1.0 / 16.0);
    float t = floor(s.y * (1.0 / 256));
    u.y = (frac(u.x) * 256) + t;
    u.z = s.y - (t * 256);
    // Instead of a floor, you could just add vec3(-0.5) to u, 
    // and the hardware will take care of the flooring for you on save to an RGB8 texture
    return floor(u) * (1.0 / 255.0);
}

float3 float2To2Snorm12sEncodedAs3Unorm8sInFloat3Format(float2 v) 
{
    float2 s = float2(packSnorm12Float(v.x), packSnorm12Float(v.y));
    return twoNorm12sEncodedAs3Unorm8sInFloat3Format(s);
}

float unpackSnorm12(float f) 
{
    return clamp((float(f) / float(2047)) - 1.0, -1.0, 1.0);
}

float2 twoNorm12sEncodedAsUint3InFloat3FormatToPackedFloat2(float3 v) 
{
    float2 s;
    // Roll the (*255s) in during the quasi bit shifting. This causes two of the three multiplications to happen at compile time
    float temp = v.y * (255.0 / 16.0);
    s.x = v.x * (255.0 * 16.0) + floor(temp);
    s.y =  frac(temp) * (16 * 256) + (v.z * 255.0);
    return s;
}

float2 twoSnorm12sEncodedAsUint3InFloat3FormatToFloat2(float3 v) 
{
    float2 s = twoNorm12sEncodedAsUint3InFloat3FormatToPackedFloat2(v);
    return float2(unpackSnorm12(s.x), unpackSnorm12(s.y));
}

float signNotZero(in float k) 
{
    return k >= 0.0 ? 1.0 : -1.0;
}

float2 signNotZero(in float2 v) 
{
    return float2( signNotZero(v.x), signNotZero(v.y) );
}

float2 encodeOctahedron(float3 v)
{
	float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    float2 result = v.xy * (1.0 / l1norm);
    if (v.z < 0.0) 
	{
        result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return result;
}

float3 encodeOctahedron24(float3 v) 
{
    return float2To2Snorm12sEncodedAs3Unorm8sInFloat3Format(encodeOctahedron(v));  
}

float3 decodeOctahedron(float2 v) 
{
    float3 result = float3(v.x, v.y, 1.0 - abs(v.x) - abs(v.y));
    if (result.z < 0) 
	{
        result.xy = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return normalize(result);
}

float3 decodeOctahedron24(float3 v) 
{
    return decodeOctahedron(twoSnorm12sEncodedAsUint3InFloat3FormatToFloat2(v));
}

#endif // ENCODING_H