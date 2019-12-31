#version 450

#extension GL_EXT_nonuniform_qualifier : enable
#extension GL_KHR_shader_subgroup_ballot : enable
#extension GL_KHR_shader_subgroup_arithmetic : enable

#include "voxelizationFill2_bindings.h"
#include "common.h"

layout(constant_id = DIRECTIONAL_LIGHT_COUNT_CONST_ID) const uint cDirectionalLightCount = 0;
layout(constant_id = BRICK_VOLUME_WIDTH_CONST_ID) const uint cBrickVolumeWidth = 128;
layout(constant_id = BRICK_VOLUME_HEIGHT_CONST_ID) const uint cBrickVolumeHeight = 64;
layout(constant_id = BRICK_VOLUME_DEPTH_CONST_ID) const uint cBrickVolumeDepth = 128;
layout(constant_id = VOXEL_SCALE_CONST_ID) const float cVoxelScale = 16.0;
layout(constant_id = INV_VOXEL_BRICK_SIZE_CONST_ID) const float cInvVoxelBrickSize = 0.625;
layout(constant_id = BIN_VIS_BRICK_SIZE_CONST_ID) const uint cBinVisBrickSize = 16;
layout(constant_id = COLOR_BRICK_SIZE_CONST_ID) const uint cColorBrickSize = 4;
layout(constant_id = IRRADIANCE_VOLUME_WIDTH_CONST_ID) const uint cGridWidth = 64;
layout(constant_id = IRRADIANCE_VOLUME_HEIGHT_CONST_ID) const uint cGridHeight = 32;
layout(constant_id = IRRADIANCE_VOLUME_DEPTH_CONST_ID) const uint cGridDepth = 64;
layout(constant_id = IRRADIANCE_VOLUME_CASCADES_CONST_ID) const uint cCascades = 3;
layout(constant_id = IRRADIANCE_VOLUME_BASE_SCALE_CONST_ID) const float cGridBaseScale = 2.0;
layout(constant_id = IRRADIANCE_VOLUME_PROBE_SIDE_LENGTH_CONST_ID) const uint cProbeSideLength = 8;
layout(constant_id = IRRADIANCE_VOLUME_DEPTH_PROBE_SIDE_LENGTH_CONST_ID) const uint cDepthProbeSideLength = 16;

layout(set = BRICK_PTR_IMAGE_SET, binding = BRICK_PTR_IMAGE_BINDING) uniform usampler3D uBrickPtrImage;
layout(set = BIN_VIS_IMAGE_BUFFER_SET, binding = BIN_VIS_IMAGE_BUFFER_BINDING, r32ui) uniform uimageBuffer uBinVisImageBuffer;
layout(set = COLOR_IMAGE_BUFFER_SET, binding = COLOR_IMAGE_BUFFER_BINDING, rgba16f) uniform imageBuffer uColorImageBuffer;
layout(set = IRRADIANCE_VOLUME_IMAGE_SET, binding = IRRADIANCE_VOLUME_IMAGE_BINDING) uniform sampler2D uIrradianceVolumeImage;
layout(set = IRRADIANCE_VOLUME_DEPTH_IMAGE_SET, binding = IRRADIANCE_VOLUME_DEPTH_IMAGE_BINDING) uniform sampler2D uIrradianceVolumeDepthImage;

layout(set = MATERIAL_DATA_SET, binding = MATERIAL_DATA_BINDING) readonly buffer MATERIAL_DATA 
{
    MaterialData uMaterialData[];
};

layout(set = DIRECTIONAL_LIGHT_DATA_SET, binding = DIRECTIONAL_LIGHT_DATA_BINDING) readonly buffer DIRECTIONAL_LIGHT_DATA
{
	DirectionalLightData uDirectionalLightData[];
};

layout(set = SHADOW_MATRICES_SET, binding = SHADOW_MATRICES_BINDING) readonly buffer SHADOW_MATRICES
{
	mat4 uShadowMatrices[];
};

layout(set = SHADOW_IMAGE_SET, binding = SHADOW_IMAGE_BINDING) uniform texture2DArray uShadowImage;
layout(set = SHADOW_SAMPLER_SET, binding = SHADOW_SAMPLER_BINDING) uniform sampler uShadowSampler;

layout(set = TEXTURES_SET, binding = TEXTURES_BINDING) uniform texture2D uTextures[TEXTURE_ARRAY_SIZE];
layout(set = SAMPLERS_SET, binding = SAMPLERS_BINDING) uniform sampler uSamplers[SAMPLER_COUNT];

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

#define DIFFUSE_ONLY 1
#define SHADOW_FUNCTIONS 0
#include "commonLighting.h"

// IN voxels extents snapped to voxel grid (post swizzle)
layout(location = 0) flat in ivec3 vMinVoxIndex;
layout(location = 1) flat in ivec3 vMaxVoxIndex;

// IN 2D projected edge normals
layout(location = 2) flat in vec2 vN_e0_xy;
layout(location = 3) flat in vec2 vN_e1_xy;
layout(location = 4) flat in vec2 vN_e2_xy;
layout(location = 5) flat in vec2 vN_e0_yz;
layout(location = 6) flat in vec2 vN_e1_yz;
layout(location = 7) flat in vec2 vN_e2_yz;
layout(location = 8) flat in vec2 vN_e0_zx;
layout(location = 9) flat in vec2 vN_e1_zx;
layout(location = 10) flat in vec2 vN_e2_zx;

// IN
layout(location = 11) flat in float vD_e0_xy;
layout(location = 12) flat in float vD_e1_xy;
layout(location = 13) flat in float vD_e2_xy;
layout(location = 14) flat in float vD_e0_yz;
layout(location = 15) flat in float vD_e1_yz;
layout(location = 16) flat in float vD_e2_yz;
layout(location = 17) flat in float vD_e0_zx;
layout(location = 18) flat in float vD_e1_zx;
layout(location = 19) flat in float vD_e2_zx;

// IN pre-calculated triangle intersection stuff
layout(location = 20) flat in vec3 vNProj;
layout(location = 21) flat in float vDTriFatMin;
layout(location = 22) flat in float vDTriFatMax;
layout(location = 23) flat in float vNzInv;

layout(location = 24) flat in int vZ;

layout(location = 25) in vec2 vTexCoord;
layout(location = 26) in vec3 vNormal;
layout(location = 27) in vec3 vVoxelPos;
layout(location = 28) flat in uint vMaterialIndex;

float signNotZero(in float k) 
{
    return k >= 0.0 ? 1.0 : -1.0;
}

vec2 signNotZero(in vec2 v) 
{
    return vec2( signNotZero(v.x), signNotZero(v.y) );
}

float square(float v)
{
	return v * v;
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

vec2 texCoordFromDir(vec3 dir, ivec3 probeCoord, int cascade, int probeSideLength)
{
	vec2 texelCoord = vec2((ivec2(cGridWidth, cGridDepth) * ivec2(probeCoord.y, cascade) + probeCoord.xz) * (probeSideLength + 2)) + 1.0;
	texelCoord += (octEncode(dir) * 0.5 + 0.5) * float(probeSideLength);
	const vec2 texelSize = 1.0 / vec2(ivec2(cGridWidth, cGridDepth) * ivec2(cGridHeight, cCascades) * ivec2(probeSideLength + 2));
	return texelCoord  * texelSize;
}

vec3 sampleIrradianceVolumeCascade(const vec3 worldSpacePos, const vec3 worldSpaceNormal, const int cascade)
{
	const float gridScale = cGridBaseScale / (1 << cascade);
	vec3 pointGridCoord = worldSpacePos * gridScale;
	ivec3 baseCoord = ivec3(floor(pointGridCoord));
	
	vec4 sum = vec4(0.0);
	for (int i = 0; i < 8; ++i)
	{
		ivec3 probeGridCoord = baseCoord + (ivec3(i, i >> 1, i >> 2) & ivec3(1));
		vec3 trilinear =  1.0 - abs(vec3(probeGridCoord) - pointGridCoord);
		float weight = 1.0;
		
		const vec3 trueDirToProbe = vec3(probeGridCoord) - pointGridCoord;
		const bool probeInPoint = dot(trueDirToProbe, trueDirToProbe) < 1e-6;
		
		// smooth backface test
		{
			weight *= probeInPoint ? 1.0 : square(max(0.0001, (dot(normalize(trueDirToProbe), worldSpaceNormal) + 1.0) * 0.5)) + 0.2;
		}
		
		const vec3 gridSize = vec3(cGridWidth, cGridHeight, cGridDepth);
		const ivec3 wrappedProbeGridCoord = ivec3(fract((probeGridCoord) / vec3(gridSize)) * gridSize);
		
		// moment visibility test
		if (!probeInPoint)
		{
			vec3 worldSpaceProbePos = vec3(probeGridCoord) / gridScale;
			vec3 biasedProbeToPoint = worldSpacePos - worldSpaceProbePos + worldSpaceNormal * 0.25;
			vec3 dir = normalize(biasedProbeToPoint);
			vec2 texCoord = texCoordFromDir(dir, wrappedProbeGridCoord, int(cascade), int(cDepthProbeSideLength));
			float distToProbe = length(biasedProbeToPoint) + 0.12;
			
			vec2 temp = textureLod(uIrradianceVolumeDepthImage, texCoord, 0).xy;
			float mean = temp.x;
			float variance = abs(temp.x * temp.x - temp.y);
			
			float chebyshevWeight = variance / (variance + square(max(distToProbe - mean, 0.0)));
			
			chebyshevWeight = max(chebyshevWeight * chebyshevWeight * chebyshevWeight, 0.0);
			
			weight *= (distToProbe <= mean) ? 1.0 : chebyshevWeight;
		}
		
		// avoid zero weight
		weight = max(0.000001, weight);
		
		const float crushThreshold = 0.2;
		if (weight < crushThreshold)
		{
			weight *= weight * weight * (1.0 / square(crushThreshold));
		}
		
		// trilinear
		weight *= trilinear.x * trilinear.y * trilinear.z;
		
		vec2 probeTexCoord = texCoordFromDir(worldSpaceNormal, wrappedProbeGridCoord, int(cascade), int(cProbeSideLength));
		vec3 probeIrradiance = textureLod(uIrradianceVolumeImage, probeTexCoord, 0).rgb;
		
		probeIrradiance = sqrt(probeIrradiance);
		
		sum += vec4(probeIrradiance * weight, weight);
	}
	vec3 irradiance = sum.xyz / sum.w;
	
	return irradiance * irradiance;
}

vec3 sampleIrradianceVolume(const vec3 worldSpacePos, const vec3 worldSpaceNormal)
{
	vec3 camPos = uPushConsts.invViewMatrix[3].xyz;
	const ivec3 gridSize = ivec3(cGridWidth, cGridHeight, cGridDepth);
	float currentGridScale = cGridBaseScale;
	int cascadeIndex = 0;
	float cascadeWeight = 0.0;
	
	// search for cascade index
	for (; cascadeIndex < cCascades; ++cascadeIndex)
	{
		// calculate coordinate in world space fixed coordinate system (scaled to voxel size)
		vec3 gridPos = worldSpacePos * currentGridScale;
		ivec3 coord = ivec3(floor(gridPos));
		ivec3 offset = ivec3(round(camPos * currentGridScale) - (gridSize / 2));

		// if coordinate is inside grid, we found the correct cascade
		if (all(greaterThanEqual(coord, offset)) && all(lessThan(coord, gridSize + offset - 1)))
		{
			const float falloffStart = 0.2;
			vec3 smoothOffset = camPos * currentGridScale - (gridSize / 2) - 0.5;
			vec3 relativeCoord = clamp(abs(((gridPos - smoothOffset) / vec3(gridSize)) * 2.0 - 1.0), 0.0, 1.0);
			float minDistToBorder = 1.0 - max(relativeCoord.x, max(relativeCoord.y, relativeCoord.z));
			cascadeWeight = minDistToBorder < falloffStart ? smoothstep(0.0, 1.0, minDistToBorder * (1.0 / falloffStart)) : 1.0;
			break;
		}
		currentGridScale *= 0.5;
	}
	
	if (cascadeIndex < cCascades)
	{
		return sampleIrradianceVolumeCascade(worldSpacePos, worldSpaceNormal, cascadeIndex);
	}
	else
	{
		return vec3(0.18);
	}
}

void writeVoxels(ivec3 coord, uint val, vec3 color)
{
	coord += ivec3(floor(uPushConsts.gridOffset.xyz));
	vec3 localCoord = floor(fract(coord / float(cBinVisBrickSize)) * cBinVisBrickSize);
	vec3 cubeCoord = localCoord * 0.25;
	float cubesPerBrick = cBinVisBrickSize * 0.25;
	ivec3 localCubeCoord = ivec3(fract(cubeCoord / cubesPerBrick) * cubesPerBrick);
	uint cubeIdx = localCubeCoord.x + localCubeCoord.z * 4 + localCubeCoord.y * 16;
	ivec3 bitCoord = ivec3(fract(localCoord * 0.25) * 4.0);
	uint bitIdx = bitCoord.x + bitCoord.z * 4 + bitCoord.y * 16;
	bool upper = bitIdx > 31;
	bitIdx = upper ? bitIdx - 32 : bitIdx;
	
	const vec3 gridSize = vec3(cBrickVolumeWidth, cBrickVolumeHeight, cBrickVolumeDepth);
	
	vec3 brickCoord = floor(coord * cInvVoxelBrickSize);
	brickCoord = floor(fract(brickCoord / gridSize) * gridSize);
	uint brickPtr = texelFetch(uBrickPtrImage, ivec3(brickCoord), 0).x;
	
	if (brickPtr != 0)
	{
		--brickPtr;
		
		const uint binVisBrickMemSize = (cBinVisBrickSize * cBinVisBrickSize * cBinVisBrickSize) / 32;
		const int binVisIdx = int(brickPtr * binVisBrickMemSize + cubeIdx * 2 + (upper ? 1 : 0));
		uint payload = (1u << bitIdx);
		imageAtomicOr(uBinVisImageBuffer, binVisIdx, payload);
		
		const uint colorBrickMemSize = (cColorBrickSize * cColorBrickSize * cColorBrickSize);
		const ivec3 localColorCoord = ivec3(localCoord / float(cBinVisBrickSize) * cColorBrickSize);
		const int colorIdx = int(brickPtr * colorBrickMemSize + localColorCoord.x + localColorCoord.z * cColorBrickSize + localColorCoord.y * cColorBrickSize * cColorBrickSize);
		
		vec3 prevColor = imageLoad(uColorImageBuffer, colorIdx).rgb;
		imageStore(uColorImageBuffer, colorIdx, vec4(mix(color, prevColor, 0.98), 1.0));
	}
}

void main() 
{
	if(any(greaterThan(vVoxelPos, vMaxVoxIndex)) || any(lessThan(vVoxelPos, vMinVoxIndex)))
	{
		discard;
		return;
	}
	
	
	const MaterialData materialData = uMaterialData[vMaterialIndex];
	
	vec3 albedo = vec3(0.0);
	vec3 N = normalize(vNormal);
	
	// albedo
	{
		albedo = unpackUnorm4x8(materialData.albedoOpacity).rgb;
		uint albedoTextureIndex = (materialData.albedoNormalTexture & 0xFFFF0000) >> 16;
		if (albedoTextureIndex != 0)
		{
			vec4 albedoTexSample = texture(sampler2D(uTextures[nonuniformEXT(albedoTextureIndex - 1)], uSamplers[SAMPLER_LINEAR_REPEAT]), vTexCoord).rgba;
			albedo = albedoTexSample.rgb;
		}
		albedo = accurateSRGBToLinear(albedo);
	}
	vec3 worldSpacePos = (vZ == 0) ? vVoxelPos.yzx : (vZ == 1) ? vVoxelPos.zxy : vVoxelPos.xyz;
	worldSpacePos += uPushConsts.gridOffset.xyz;
	worldSpacePos /= cVoxelScale;
	
	vec3 result = vec3(0.0);
	
	// ambient
	result += sampleIrradianceVolume(worldSpacePos, N) * albedo * (1.0 / PI);
	//result += albedo * (1.0 / PI);
	
	// directional lights
	for (uint lightIdx = 0; lightIdx < cDirectionalLightCount; ++lightIdx)
	{
		const DirectionalLightData lightData = uDirectionalLightData[lightIdx];
		const uint shadowDataOffsetCount = lightData.shadowDataOffsetCount;
		const uint offset = shadowDataOffsetCount >> 16;
		const uint end = (shadowDataOffsetCount & 0xFFFF) + offset;
		
		vec4 shadowCoord = vec4(0.0);
		uint shadowMapIndex = offset;
		// find cascade
		for (; shadowMapIndex < end; ++shadowMapIndex)
		{
			shadowCoord = uShadowMatrices[shadowMapIndex] * vec4(worldSpacePos, 1.0);
			if (any(greaterThan(abs(shadowCoord.xy), vec2(1.0))))
			{
				continue;
			}
			shadowCoord.xy = shadowCoord.xy * 0.5 + 0.5;
			break;
		}
		
		float shadow = 0.0;
		if (shadowMapIndex < end)
		{
			shadowCoord.z -= 0.0001;
			shadow = texture(sampler2DArrayShadow(uShadowImage, uShadowSampler), vec4(shadowCoord.xy, shadowMapIndex, shadowCoord.z)).x;
		}
		
		
		const vec3 L = mat3(uPushConsts.invViewMatrix) * lightData.direction.xyz;
		const float NdotL = max(dot(N, L), 0.0);
		result += (1.0 - shadow) * albedo * (1.0 / PI) * NdotL * lightData.color.rgb;
	}

	ivec3 p = ivec3(floor(vVoxelPos));	//voxel coordinate (swizzled)
	int   zMin,      zMax;			//voxel Z-range
	float zMinInt,   zMaxInt;		//voxel Z-intersection min/max
	float zMinFloor, zMaxCeil;		//voxel Z-intersection floor/ceil

	float dd_e0_xy = vD_e0_xy + dot(vN_e0_xy, p.xy);
	float dd_e1_xy = vD_e1_xy + dot(vN_e1_xy, p.xy);
	float dd_e2_xy = vD_e2_xy + dot(vN_e2_xy, p.xy);

	bool xy_overlap = (dd_e0_xy >= 0) && (dd_e1_xy >= 0) && (dd_e2_xy >= 0);

	if(xy_overlap)	//figure 17 line 15, figure 18 line 14
	{
		float dot_n_p = dot(vNProj.xy, p.xy);
		zMinInt = (-dot_n_p + vDTriFatMin) * vNzInv;
		zMaxInt = (-dot_n_p + vDTriFatMax) * vNzInv;

		zMinFloor = floor(zMinInt);
		zMaxCeil  =  ceil(zMaxInt);

		zMin = int(zMinFloor) - int(zMinFloor == zMinInt);
		zMax = int(zMaxCeil ) + int(zMaxCeil  == zMaxInt);

		zMin = max(vMinVoxIndex.z, zMin);	//clamp to bounding box max Z
		zMax = min(vMaxVoxIndex.z, zMax);	//clamp to bounding box min Z

		for(p.z = zMin; p.z < zMax; p.z++)	//figure 17/18 line 18
		{
			float dd_e0_yz = vD_e0_yz + dot(vN_e0_yz, p.yz);
			float dd_e1_yz = vD_e1_yz + dot(vN_e1_yz, p.yz);
			float dd_e2_yz = vD_e2_yz + dot(vN_e2_yz, p.yz);
	
			float dd_e0_zx = vD_e0_zx + dot(vN_e0_zx, p.zx);
			float dd_e1_zx = vD_e1_zx + dot(vN_e1_zx, p.zx);
			float dd_e2_zx = vD_e2_zx + dot(vN_e2_zx, p.zx);
	
			bool yz_overlap = (dd_e0_yz >= 0) && (dd_e1_yz >= 0) && (dd_e2_yz >= 0);
			bool zx_overlap = (dd_e0_zx >= 0) && (dd_e1_zx >= 0) && (dd_e2_zx >= 0);

			if(yz_overlap && zx_overlap)	//figure 17/18 line 19
			{
				//Calculate color and other attributes here if applicable
				//vec3 color = (material.texCount == 0) ? material.diffuse.xyz : texture2D(diffuseTex, In.texCoord).xyz;
				
				ivec3 origCoord = (vZ == 0) ? p.yzx : (vZ == 1) ? p.zxy : p.xyz;	//this actually slightly outperforms unswizzle

				writeVoxels(origCoord, 1, result);	//figure 17/18 line 20
			}
		}
		//z-loop
	}
	//xy-overlap test

	discard;
}

