#version 450

#extension GL_EXT_nonuniform_qualifier : enable

#include "geometry_bindings.h"

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform texture2D uTextures[TEXTURE_ARRAY_SIZE];
layout(set = SAMPLERS_SET, binding = SAMPLERS_BINDING) uniform sampler uSamplers[SAMPLER_COUNT];

#ifndef ALPHA_MASK_ENABLED
#define ALPHA_MASK_ENABLED 0
#endif // ALPHA_MASK_ENABLED

#if !ALPHA_MASK_ENABLED
layout(early_fragment_tests) in;
#endif // ALPHA_MASK_ENABLED

layout(location = 0) in vec2 vTexCoord;
layout(location = 1) in vec3 vNormal;
layout(location = 2) in vec3 vWorldPos;
layout(location = 3) flat in uint vMaterialIndex;

layout(location = OUT_UV) out vec4 oUV;
layout(location = OUT_DDXY_LENGTH) out vec4 oDdxyLength;
layout(location = OUT_DDXY_ROTATION_MATERIAL_ID) out uvec4 oDdxyRotationMaterialId;
layout(location = OUT_TANGENT_SPACE) out uvec4 oTangentSpace;

// based on http://www.thetenthplanet.de/archives/1180
mat3 calculateTBN( vec3 N, vec3 p, vec2 uv )
{
    // get edge vectors of the pixel triangle
    vec3 dp1 = dFdx( p );
    vec3 dp2 = dFdy( p );
    vec2 duv1 = dFdx( uv );
    vec2 duv2 = dFdy( uv );
 
    // solve the linear system
    vec3 dp2perp = cross( dp2, N );
    vec3 dp1perp = cross( N, dp1 );
    vec3 T = dp2perp * duv1.x + dp1perp * duv2.x;
    vec3 B = dp2perp * duv1.y + dp1perp * duv2.y;
 
    // construct a scale-invariant frame 
    float invmax = inversesqrt( max( dot(T,T), dot(B,B) ) );
    return mat3( T * invmax, B * invmax, N );
}

float signNotZero(in float k) 
{
    return k >= 0.0 ? 1.0 : -1.0;
}

vec2 signNotZero(in vec2 v) 
{
    return vec2(signNotZero(v.x), signNotZero(v.y));
}

vec2 encodeNormal(in vec3 v) 
{
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0/l1norm);
    if (v.z < 0.0) 
	{
        result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return result;
}

void encodeDerivatives(in vec4 derivatives, out vec2 derivativesLength, out uint encodedDerivativesRot)
{
	derivativesLength = vec2(length(derivatives.xy), length(derivatives.zw));
	vec2 derivativesRot = vec2(derivatives.x / derivativesLength.x, 
						derivatives.z / derivativesLength.y) * 0.5 + 0.5;
	uint signX = (derivatives.y < 0.0) ? 1 : 0;
	uint signY = (derivatives.w < 0.0) ? 1 : 0;
	encodedDerivativesRot = (signY << 15u) 
						| (uint(derivativesRot.y * 127.0f) << 8u)
						| (signX << 7u) 
						| (uint(derivativesRot.x * 127.0));
}

uvec4 encodeTBN(in vec3 normal, in vec3 tangent, in uint bitangentHandedness)
{
	// octahedron normal vector encoding
	uvec2 encodedNormal = uvec2((encodeNormal(normal) * 0.5 + 0.5) * 1023.0);
	
	// find largest component of tangent
	vec3 tangentAbs = abs(tangent);
	float maxComp = max(max(tangentAbs.x, tangentAbs.y), tangentAbs.z);
	vec3 refVector;
	uint compIndex;
	if (maxComp == tangentAbs.x)
	{
		refVector = vec3(1.0, 0.0, 0.0);
		compIndex = 0;
	}
	else if (maxComp == tangentAbs.y)
	{
		refVector = vec3(0.0, 1.0, 0.0);
		compIndex = 1;
	}
	else
	{
		refVector = vec3(0.0, 0.0, 1.0);
		compIndex = 2;
	}
	
	// compute cosAngle and handedness of tangent
	vec3 orthoA = normalize(cross(normal, refVector));
	vec3 orthoB = cross(normal, orthoA);
	uint cosAngle = uint((dot(tangent, orthoA) * 0.5 + 0.5) * 255.0);
	uint tangentHandedness = (dot(tangent, orthoB) > 0.0001) ? 2 : 0;
	
	return uvec4(encodedNormal, (cosAngle << 2u) | compIndex, tangentHandedness | bitangentHandedness);
}

void main() 
{
#if ALPHA_MASK_ENABLED
	{
		uint albedoTextureIndex = (uMaterialData[vMaterialIndex].albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			float alpha = texture(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).a;
			alpha *= 1.0 + textureQueryLod(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).x * ALPHA_MIP_SCALE;
			if(alpha < ALPHA_CUTOFF)
			{
				discard;
			}
		}
	}
#endif // ALPHA_MASK_ENABLED

	// fractional part of texcoord
	{
		vec2 integerPart;
		oUV = vec4(modf(vTexCoord, integerPart), 0.0, 0.0);
	}
	
	// derivatives and material id
	{
		vec2 derivativesLength;
		uint encodedDerivativesRot;
		encodeDerivatives(vec4(dFdx(vTexCoord), dFdy(vTexCoord)), derivativesLength, encodedDerivativesRot);
		
		oDdxyLength = vec4(derivativesLength, 0.0, 0.0);
		oDdxyRotationMaterialId = uvec4(encodedDerivativesRot, vMaterialIndex, 0, 0);
	}
	
	// tangent frame
	{
		vec3 normal = normalize(vNormal);
		normal = gl_FrontFacing ? normal : -normal;
		mat3 tbn = calculateTBN(normal, vWorldPos, vTexCoord);
		
		oTangentSpace = encodeTBN(tbn[2], tbn[0], (dot(cross(tbn[2], tbn[0]), tbn[1]) >= 0.0) ? 1 : 0);
	}
}

