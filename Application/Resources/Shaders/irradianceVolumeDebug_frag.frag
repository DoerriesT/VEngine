#version 450

#include "irradianceVolumeDebug_bindings.h"

layout(constant_id = IRRADIANCE_VOLUME_WIDTH_CONST_ID) const uint cGridWidth = 64;
layout(constant_id = IRRADIANCE_VOLUME_HEIGHT_CONST_ID) const uint cGridHeight = 32;
layout(constant_id = IRRADIANCE_VOLUME_DEPTH_CONST_ID) const uint cGridDepth = 64;
layout(constant_id = IRRADIANCE_VOLUME_CASCADES_CONST_ID) const uint cVolumeCascades = 3;
layout(constant_id = IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID) const uint cProbeSideLength = 8;

layout(set = IRRADIANCE_VOLUME_IMAGE_SET, binding = IRRADIANCE_VOLUME_IMAGE_BINDING) uniform sampler3D uIrradianceVolumeImages[3];
layout(set = AGE_IMAGE_SET, binding = AGE_IMAGE_BINDING) uniform sampler3D uAgeImage;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) flat in vec3 vCoord;
layout(location = 1) in vec3 vNormal;

layout(location = 0) out vec4 oColor;

vec3 sampleAmbientCube(vec3 N, vec3 tc)
{
	vec3 nSquared = N * N;
	vec3 isNegative = mix(vec3(0.0), vec3(0.5), lessThan(N, vec3(0.0)));
	tc = tc.xzy;
	tc.z *= 0.5;
	vec3 tcz = tc.zzz + isNegative;
	tcz = tcz * (1.0 / cVolumeCascades) + float(uPushConsts.cascadeIndex) / cVolumeCascades;
	
	return nSquared.x * textureLod(uIrradianceVolumeImages[0], vec3(tc.xy, tcz.x), 0).rgb +
			nSquared.y * textureLod(uIrradianceVolumeImages[1], vec3(tc.xy, tcz.y), 0).rgb +
			nSquared.z * textureLod(uIrradianceVolumeImages[2], vec3(tc.xy, tcz.z), 0).rgb;
}

float signNotZero(in float k) 
{
    return k >= 0.0 ? 1.0 : -1.0;
}

vec2 signNotZero(in vec2 v) 
{
    return vec2( signNotZero(v.x), signNotZero(v.y) );
}

vec2 octEncode(in vec3 v) 
{
    float l1norm = abs(v.x) + abs(v.y) + abs(v.z);
    vec2 result = v.xy * (1.0/l1norm);
    if (v.z < 0.0) 
	{
        result = (1.0 - abs(result.yx)) * signNotZero(result.xy);
    }
    return result;
}

vec2 texCoordFromDir(vec3 dir, ivec3 probeCoord, int cascade)
{
	vec2 texelCoord = vec2((ivec2(cGridWidth, cGridDepth) * ivec2(probeCoord.y, cascade) + probeCoord.xz) * (cProbeSideLength + 2)) + 1.0;
	texelCoord += (octEncode(dir) * 0.5 + 0.5) * float(cProbeSideLength);
	const vec2 texelSize = 1.0 / vec2(ivec2(cGridWidth, cGridDepth) * ivec2(cGridHeight, cVolumeCascades) * ivec2(cProbeSideLength + 2));
	return texelCoord  * texelSize;
}

void main() 
{
	if (uPushConsts.showAge != 0)
	{
		vec3 tc = vCoord.xzy;
		tc.z = tc.z * (1.0 / cVolumeCascades) + float(uPushConsts.cascadeIndex) / cVolumeCascades;
		float age = textureLod(uAgeImage, tc, 0).r;
		oColor = vec4(age == 0.0 ? vec3(1.0, 0.0, 0.0) : age.xxx, 1.0);
	}
	else
	{
		//vec2 texcoord = texCoordFromDir(normalize(vNormal), ivec3(vCoord * ivec3(cGridWidth, cGridHeight, cGridDepth)), int(uPushConsts.cascadeIndex));
		//oColor = vec4(textureLod(uIrradianceVolumeImage, texcoord, 0).rgb, 1.0);
		oColor = vec4(sampleAmbientCube(normalize(vNormal), vCoord), 1.0);
	}
}

