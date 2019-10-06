#version 450

#include "occlusionCulling_bindings.h"

struct BoundingBox
{
	float data[6];
};

layout(set = INSTANCE_DATA_SET, binding = INSTANCE_DATA_BINDING) readonly buffer INSTANCE_DATA 
{
    SubMeshInstanceData uInstanceData[];
};

layout(set = AABB_DATA_SET, binding = AABB_DATA_BINDING) readonly buffer AABB_DATA 
{
    BoundingBox uBoundingBoxes[];
};

layout(set = TRANSFORM_DATA_SET, binding = TRANSFORM_DATA_BINDING) readonly buffer TRANSFORM_DATA 
{
    mat4 uTransformData[];
};

layout(push_constant) uniform PUSH_CONSTS 
{
	PushConsts uPushConsts;
};

layout(location = 0) flat out uint vInstanceID;

void main() 
{
	const SubMeshInstanceData instanceData = uInstanceData[gl_InstanceIndex];
	const BoundingBox boundingBox = uBoundingBoxes[instanceData.subMeshIndex];
	const vec3 position = vec3(((gl_VertexIndex & 0x4) == 0) ? boundingBox.data[0] : boundingBox.data[3],
								((gl_VertexIndex & 0x2) == 0) ? boundingBox.data[1] : boundingBox.data[4],
								((gl_VertexIndex & 0x1) == 0) ? boundingBox.data[2] : boundingBox.data[5]);
	
	const mat4 modelMatrix = uTransformData[instanceData.transformIndex];
    gl_Position = uPushConsts.jitteredViewProjectionMatrix * modelMatrix * vec4(position, 1.0);
	
	// when camera is inside the bounding box, it is possible that the bounding box
	// is fully occluded, even when the object itself is visible. therefore, bounding
	// box vertices behind the near plane are clamped in front of the near plane to 
	// avoid culling such objects
	gl_Position = (abs(gl_Position.z) > gl_Position.w || gl_Position.z < 0.0) ? vec4(clamp(gl_Position.xy, vec2(-0.999), vec2(0.999)), 1.0 - 0.0001, 1.0) : gl_Position;
	
	vInstanceID = gl_InstanceIndex;
}

