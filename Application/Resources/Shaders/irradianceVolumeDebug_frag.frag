#version 450

#include "irradianceVolumeDebug_bindings.h"

layout(constant_id = IRRADIANCE_VOLUME_CASCADES_CONST_ID) const uint cVolumeCascades = 3;

layout(set = IRRADIANCE_VOLUME_IMAGE_SET, binding = IRRADIANCE_VOLUME_IMAGE_BINDING) uniform sampler3D uIrradianceVolumeImages[3];

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

void main() 
{
	oColor = vec4(sampleAmbientCube(normalize(vNormal), vCoord), 1.0);
}

