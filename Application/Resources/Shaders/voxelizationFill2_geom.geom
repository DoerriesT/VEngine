#version 450

#include "voxelizationFill2_bindings.h"

#define L     0.7071067811865475244008443621048490392848359376884740	//sqrt(2)/2
#define L_SQR 0.5

layout (triangles) in;
layout (triangle_strip, max_vertices = 3) out;


layout(constant_id = BRICK_VOLUME_WIDTH_CONST_ID) const uint cBrickVolumeWidth = 128;
layout(constant_id = BRICK_VOLUME_HEIGHT_CONST_ID) const uint cBrickVolumeHeight = 64;
layout(constant_id = BRICK_VOLUME_DEPTH_CONST_ID) const uint cBrickVolumeDepth = 128;
layout(constant_id = VOXEL_SCALE_CONST_ID) const float cVoxelScale = 16.0;
layout(constant_id = INV_VOXEL_BRICK_SIZE_CONST_ID) const float cInvVoxelBrickSize = 0.625;
layout(constant_id = BIN_VIS_BRICK_SIZE_CONST_ID) const uint cBinVisBrickSize = 16;
layout(constant_id = COLOR_BRICK_SIZE_CONST_ID) const uint cColorBrickSize = 4;


layout(location = 0) in vec2 vTexCoord[];
layout(location = 1) in vec3 vNormal[];
layout(location = 2) flat in uint vMaterialIndex[];


// OUT voxels extents snapped to voxel grid (post swizzle)
layout(location = 0) flat out ivec3 vMinVoxIndexG;
layout(location = 1) flat out ivec3 vMaxVoxIndexG;

// OUT 2D projected edge normals
layout(location = 2) flat out vec2 vN_e0_xyG;
layout(location = 3) flat out vec2 vN_e1_xyG;
layout(location = 4) flat out vec2 vN_e2_xyG;
layout(location = 5) flat out vec2 vN_e0_yzG;
layout(location = 6) flat out vec2 vN_e1_yzG;
layout(location = 7) flat out vec2 vN_e2_yzG;
layout(location = 8) flat out vec2 vN_e0_zxG;
layout(location = 9) flat out vec2 vN_e1_zxG;
layout(location = 10) flat out vec2 vN_e2_zxG;

// OUT
layout(location = 11) flat out float vD_e0_xyG;
layout(location = 12) flat out float vD_e1_xyG;
layout(location = 13) flat out float vD_e2_xyG;
layout(location = 14) flat out float vD_e0_yzG;
layout(location = 15) flat out float vD_e1_yzG;
layout(location = 16) flat out float vD_e2_yzG;
layout(location = 17) flat out float vD_e0_zxG;
layout(location = 18) flat out float vD_e1_zxG;
layout(location = 19) flat out float vD_e2_zxG;

// OUT pre-calculated triangle intersection stuff
layout(location = 20) flat out vec3 vNProjG;
layout(location = 21) flat out float vDTriFatMinG;
layout(location = 22) flat out float vDTriFatMaxG;
layout(location = 23) flat out float vNzInvG;

layout(location = 24) flat out int vZG;

layout(location = 25) out vec2 vTexCoordG;
layout(location = 26) out vec3 vNormalG;
layout(location = 27) out vec3 vVoxelPosG;
layout(location = 28) flat out uint vMaterialIndexG;

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

void computeBaryCoords(inout vec3 b0, inout vec3 b1, inout vec3 b2, vec2 v0, vec2 v1, vec2 v2)
{
	vec3 vb;
	float invDetA = 1.0 / ((v0.x*v1.y) - (v0.x*v2.y) - (v1.x*v0.y) + (v1.x*v2.y) + (v2.x*v0.y) - (v2.x*v1.y));

	vb.x = ( (b0.x*v1.y) - (b0.x*v2.y) - (v1.x*b0.y) + (v1.x*v2.y) + (v2.x*b0.y) - (v2.x*v1.y) ) * invDetA;
	vb.y = ( (v0.x*b0.y) - (v0.x*v2.y) - (b0.x*v0.y) + (b0.x*v2.y) + (v2.x*v0.y) - (v2.x*b0.y) ) * invDetA;
	vb.z = 1 - vb.x - vb.y;
	b0 = vb;

	vb.x = ( (b1.x*v1.y) - (b1.x*v2.y) - (v1.x*b1.y) + (v1.x*v2.y) + (v2.x*b1.y) - (v2.x*v1.y) ) * invDetA;
	vb.y = ( (v0.x*b1.y) - (v0.x*v2.y) - (b1.x*v0.y) + (b1.x*v2.y) + (v2.x*v0.y) - (v2.x*b1.y) ) * invDetA;
	vb.z = 1 - vb.x - vb.y;
	b1 = vb;

	vb.x = ( (b2.x*v1.y) - (b2.x*v2.y) - (v1.x*b2.y) + (v1.x*v2.y) + (v2.x*b2.y) - (v2.x*v1.y) ) * invDetA;
	vb.y = ( (v0.x*b2.y) - (v0.x*v2.y) - (b2.x*v0.y) + (b2.x*v2.y) + (v2.x*v0.y) - (v2.x*b2.y) ) * invDetA;
	vb.z = 1 - vb.x - vb.y;
	b2 = vb;
}

//swizzle triangle vertices
void swizzleTri(inout vec3 v0, inout vec3 v1, inout vec3 v2, out vec3 n, out int domAxis)
{
	//       cross(e0, e1);
	n = cross(v1 - v0, v2 - v1);

	vec3 absN = abs(n);
	float maxAbsN = max(max(absN.x, absN.y), absN.z);

	if(absN.x >= absN.y && absN.x >= absN.z)			//X-direction dominant (YZ-plane)
	{													//Then you want to look down the X-direction
		v0.xyz = v0.yzx;
		v1.xyz = v1.yzx;
		v2.xyz = v2.yzx;
		
		n.xyz = n.yzx;

		//XYZ <-> YZX
		domAxis = 1;
	}
	else if(absN.y >= absN.x && absN.y >= absN.z)		//Y-direction dominant (ZX-plane)
	{													//Then you want to look down the Y-direction
		v0.xyz = v0.zxy;
		v1.xyz = v1.zxy;
		v2.xyz = v2.zxy;

		n.xyz = n.zxy;

		//XYZ <-> ZXY
		domAxis = 0;
	}
	else												//Z-direction dominant (XY-plane)
	{													//Then you want to look down the Z-direction (the default)
		v0.xyz = v0.xyz;
		v1.xyz = v1.xyz;
		v2.xyz = v2.xyz;

		n.xyz = n.xyz;

		//XYZ <-> XYZ
		domAxis = 2;
	}
}

void main()
{
	vec3 v0 = gl_in[0].gl_Position.xyz;
	vec3 v1 = gl_in[1].gl_Position.xyz;
	vec3 v2 = gl_in[2].gl_Position.xyz;
	
	vec3 n;
	swizzleTri(v0, v1, v2, n, vZG);

	vec3 AABBmin = min(min(v0, v1), v2);
	vec3 AABBmax = max(max(v0, v1), v2);

	const vec3 gridSize = vec3(cBrickVolumeWidth, cBrickVolumeHeight, cBrickVolumeDepth) * cBinVisBrickSize;
	const vec3 swizzledGridSize = vZG == 0 ? gridSize.zxy : vZG == 1 ? gridSize.yzx : gridSize;
	vMinVoxIndexG = ivec3(clamp(floor(AABBmin), vec3(0.0), floor(swizzledGridSize)));
	vMaxVoxIndexG = ivec3(clamp( ceil(AABBmax), vec3(0.0), floor(swizzledGridSize)));

	//Edges for swizzled vertices;
	vec3 e0 = v1 - v0;	//figure 17/18 line 2
	vec3 e1 = v2 - v1;	//figure 17/18 line 2
	vec3 e2 = v0 - v2;	//figure 17/18 line 2

	//INward Facing edge normals XY
	vN_e0_xyG = (n.z >= 0) ? vec2(-e0.y, e0.x) : vec2(e0.y, -e0.x);	//figure 17/18 line 4
	vN_e1_xyG = (n.z >= 0) ? vec2(-e1.y, e1.x) : vec2(e1.y, -e1.x);	//figure 17/18 line 4
	vN_e2_xyG = (n.z >= 0) ? vec2(-e2.y, e2.x) : vec2(e2.y, -e2.x);	//figure 17/18 line 4

	//INward Facing edge normals YZ
	vN_e0_yzG = (n.x >= 0) ? vec2(-e0.z, e0.y) : vec2(e0.z, -e0.y);	//figure 17/18 line 5
	vN_e1_yzG = (n.x >= 0) ? vec2(-e1.z, e1.y) : vec2(e1.z, -e1.y);	//figure 17/18 line 5
	vN_e2_yzG = (n.x >= 0) ? vec2(-e2.z, e2.y) : vec2(e2.z, -e2.y);	//figure 17/18 line 5

	//INward Facing edge normals ZX
	vN_e0_zxG = (n.y >= 0) ? vec2(-e0.x, e0.z) : vec2(e0.x, -e0.z);	//figure 17/18 line 6
	vN_e1_zxG = (n.y >= 0) ? vec2(-e1.x, e1.z) : vec2(e1.x, -e1.z);	//figure 17/18 line 6
	vN_e2_zxG = (n.y >= 0) ? vec2(-e2.x, e2.z) : vec2(e2.x, -e2.z);	//figure 17/18 line 6


	vD_e0_xyG = -dot(vN_e0_xyG, v0.xy) + max(0.0f, vN_e0_xyG.x) + max(0.0f, vN_e0_xyG.y);	//figure 17 line 7
	vD_e1_xyG = -dot(vN_e1_xyG, v1.xy) + max(0.0f, vN_e1_xyG.x) + max(0.0f, vN_e1_xyG.y);	//figure 17 line 7
	vD_e2_xyG = -dot(vN_e2_xyG, v2.xy) + max(0.0f, vN_e2_xyG.x) + max(0.0f, vN_e2_xyG.y);	//figure 17 line 7

	vD_e0_yzG = -dot(vN_e0_yzG, v0.yz) + max(0.0f, vN_e0_yzG.x) + max(0.0f, vN_e0_yzG.y);	//figure 17 line 8
	vD_e1_yzG = -dot(vN_e1_yzG, v1.yz) + max(0.0f, vN_e1_yzG.x) + max(0.0f, vN_e1_yzG.y);	//figure 17 line 8
	vD_e2_yzG = -dot(vN_e2_yzG, v2.yz) + max(0.0f, vN_e2_yzG.x) + max(0.0f, vN_e2_yzG.y);	//figure 17 line 8

	vD_e0_zxG = -dot(vN_e0_zxG, v0.zx) + max(0.0f, vN_e0_zxG.x) + max(0.0f, vN_e0_zxG.y);	//figure 18 line 9
	vD_e1_zxG = -dot(vN_e1_zxG, v1.zx) + max(0.0f, vN_e1_zxG.x) + max(0.0f, vN_e1_zxG.y);	//figure 18 line 9
	vD_e2_zxG = -dot(vN_e2_zxG, v2.zx) + max(0.0f, vN_e2_zxG.x) + max(0.0f, vN_e2_zxG.y);	//figure 18 line 9

	vNProjG = (n.z < 0.0) ? -n : n;	//figure 17/18 line 10

	const float dTri = dot(vNProjG, v0);

	vDTriFatMinG = dTri - max(vNProjG.x, 0) - max(vNProjG.y, 0);	//figure 17 line 11
	vDTriFatMaxG = dTri - min(vNProjG.x, 0) - min(vNProjG.y, 0);	//figure 17 line 12

	vNzInvG = 1.0 / vNProjG.z;

	vec2 n_e0_xy_norm = -normalize(vN_e0_xyG);	//edge normals must be normalized for expansion, and made OUTward facing
	vec2 n_e1_xy_norm = -normalize(vN_e1_xyG);	//edge normals must be normalized for expansion, and made OUTward facing
	vec2 n_e2_xy_norm = -normalize(vN_e2_xyG);	//edge normals must be normalized for expansion, and made OUTward facing

	vec3 v0_prime, v1_prime, v2_prime;
	v0_prime.xy = v0.xy + L * ( e2.xy / ( dot(e2.xy, n_e0_xy_norm) ) + e0.xy / ( dot(e0.xy, n_e2_xy_norm) ) );
	v1_prime.xy = v1.xy + L * ( e0.xy / ( dot(e0.xy, n_e1_xy_norm) ) + e1.xy / ( dot(e1.xy, n_e0_xy_norm) ) );
	v2_prime.xy = v2.xy + L * ( e1.xy / ( dot(e1.xy, n_e2_xy_norm) ) + e2.xy / ( dot(e2.xy, n_e1_xy_norm) ) );

	v0_prime.z = (-dot(vNProjG.xy, v0_prime.xy) + dTri) * vNzInvG;
	v1_prime.z = (-dot(vNProjG.xy, v1_prime.xy) + dTri) * vNzInvG;
	v2_prime.z = (-dot(vNProjG.xy, v2_prime.xy) + dTri) * vNzInvG;

	vec3 b0 = v0_prime;
	vec3 b1 = v1_prime;
	vec3 b2 = v2_prime;
	computeBaryCoords(b0, b1, b2, v0.xy, v1.xy, v2.xy);
	
	vMaterialIndexG = vMaterialIndex[0];

	gl_Position = vec4(v0_prime,1);
	gl_Position.xyz *= uPushConsts.invGridResolution;
	gl_Position.xy = gl_Position.xy * 2.0 - 1.0;
	vVoxelPosG = v0_prime;
	vNormalG = b0.x * vNormal[0] + b0.y * vNormal[1] + b0.z * vNormal[2];
	vTexCoordG = b0.x * vTexCoord[0] + b0.y * vTexCoord[1] + b0.z * vTexCoord[2];

	EmitVertex();

	gl_Position = vec4(v1_prime,1);
	gl_Position.xyz *= uPushConsts.invGridResolution;
	gl_Position.xy = gl_Position.xy * 2.0 - 1.0;
	vVoxelPosG = v1_prime;
	vNormalG = b1.x * vNormal[0] + b1.y * vNormal[1] + b1.z * vNormal[2];
	vTexCoordG = b1.x * vTexCoord[0] + b1.y * vTexCoord[1] + b1.z * vTexCoord[2];

	EmitVertex();

	gl_Position = vec4(v2_prime,1);
	gl_Position.xyz *= uPushConsts.invGridResolution;
	gl_Position.xy = gl_Position.xy * 2.0 - 1.0;
	vVoxelPosG = v2_prime;
	vNormalG = b2.x * vNormal[0] + b2.y * vNormal[1] + b2.z * vNormal[2];
	vTexCoordG = b2.x * vTexCoord[0] + b2.y * vTexCoord[1] + b2.z * vTexCoord[2];

	EmitVertex();

	EndPrimitive();
}