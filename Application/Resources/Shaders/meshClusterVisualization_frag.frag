#version 450

layout(early_fragment_tests) in;

layout(location = 0) out vec4 oColor;

float interleavedGradientNoise(vec2 v)
{
	vec3 magic = vec3(0.06711056, 0.00583715, 52.9829189);
	return fract(magic.z * dot(v, magic.xy));
}

void main() 
{
	const uint clusterIndex = gl_PrimitiveID / 256;// / 256 + 1;
	oColor = vec4(interleavedGradientNoise(vec2(clusterIndex * 3 + 0.5, clusterIndex * 2 + 0.5)),
				interleavedGradientNoise(vec2(clusterIndex * 2 + 0.5, clusterIndex * 5 + 0.5)),
				interleavedGradientNoise(vec2(clusterIndex * 5 + 0.5, clusterIndex * 3 + 0.5)),
				1.0);
}

