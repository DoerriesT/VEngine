#pragma once
#include <cstdint>

namespace VEngine
{
	namespace PointLightProxyMesh
	{
		constexpr float vertexData[] =
		{
			 0.0f, -1.0f, 0.0f,
			 0.723607f, -0.447220f, 0.525725f,
			 -0.276388f, -0.447220f, 0.850649f,
			 -0.894426f, -0.447216f, 0.0f,
			 -0.276388f, -0.447220f, -0.850649f,
			 0.723607f, -0.447220f, -0.525725f,
			 0.276388f, 0.447220f, 0.850649f,
			 -0.723607f, 0.447220f, 0.525725f,
			 -0.723607f, 0.447220f, -0.525725f,
			 0.276388f, 0.447220f, -0.850649f,
			 0.894426f, 0.447216f, 0.0f,
			 0.0f, 1.0f, 0.0f,
			 -0.162456f, -0.850654f, 0.499995f,
			 0.425323f, -0.850654f, 0.309011f,
			 0.262869f, -0.525738f, 0.809012f,
			 0.850648f, -0.525736f, 0.0f,
			 0.425323f, -0.850654f, -0.309011f,
			 -0.525730f, -0.850652f, 0.0f,
			 -0.688189f, -0.525736f, 0.499997f,
			 -0.162456f, -0.850654f, -0.499995f,
			 -0.688189f, -0.525736f, -0.499997f,
			 0.262869f, -0.525738f, -0.809012f,
			 0.951058f, 0.0f, 0.309013f,
			 0.951058f, 0.0f, -0.309013f,
			 0.0f, 0.0f, 1.0f,
			 0.587786f, 0.0f, 0.809017f,
			 -0.951058f, 0.0f, 0.309013f,
			 -0.587786f, 0.0f, 0.809017f,
			 -0.587786f, 0.0f, -0.809017f,
			 -0.951058f, 0.0f, -0.309013f,
			 0.587786f, 0.0f, -0.809017f,
			 0.0f, 0.0f, -1.0f,
			 0.688189f, 0.525736f, 0.499997f,
			 -0.262869f, 0.525738f, 0.809012f,
			 -0.850648f, 0.525736f, 0.0f,
			 -0.262869f, 0.525738f, -0.809012f,
			 0.688189f, 0.525736f, -0.499997f,
			 0.162456f, 0.850654f, 0.499995f,
			 0.525730f, 0.850652f, 0.0f,
			 -0.425323f, 0.850654f, 0.309011f,
			 -0.425323f, 0.850654f, -0.309011f,
			 0.162456f, 0.850654f, -0.499995f,
		};

		constexpr uint16_t indexData[] =
		{
			0, 13, 12,
			1, 13, 15,
			0, 12, 17,
			0, 17, 19,
			0, 19, 16,
			1, 15, 22,
			2, 14, 24,
			3, 18, 26,
			4, 20, 28,
			5, 21, 30,
			1, 22, 25,
			2, 24, 27,
			3, 26, 29,
			4, 28, 31,
			5, 30, 23,
			6, 32, 37,
			7, 33, 39,
			8, 34, 40,
			9, 35, 41,
			10, 36, 38,
			38, 41, 11,
			38, 36, 41,
			36, 9, 41,
			41, 40, 11,
			41, 35, 40,
			35, 8, 40,
			40, 39, 11,
			40, 34, 39,
			34, 7, 39,
			39, 37, 11,
			39, 33, 37,
			33, 6, 37,
			37, 38, 11,
			37, 32, 38,
			32, 10, 38,
			23, 36, 10,
			23, 30, 36,
			30, 9, 36,
			31, 35, 9,
			31, 28, 35,
			28, 8, 35,
			29, 34, 8,
			29, 26, 34,
			26, 7, 34,
			27, 33, 7,
			27, 24, 33,
			24, 6, 33,
			25, 32, 6,
			25, 22, 32,
			22, 10, 32,
			30, 31, 9,
			30, 21, 31,
			21, 4, 31,
			28, 29, 8,
			28, 20, 29,
			20, 3, 29,
			26, 27, 7,
			26, 18, 27,
			18, 2, 27,
			24, 25, 6,
			24, 14, 25,
			14, 1, 25,
			22, 23, 10,
			22, 15, 23,
			15, 5, 23,
			16, 21, 5,
			16, 19, 21,
			19, 4, 21,
			19, 20, 4,
			19, 17, 20,
			17, 3, 20,
			17, 18, 3,
			17, 12, 18,
			12, 2, 18,
			15, 16, 5,
			15, 13, 16,
			13, 0, 16,
			12, 14, 2,
			12, 13, 14,
			13, 1, 14,
		};

		constexpr size_t vertexDataSize = sizeof(vertexData);
		constexpr size_t indexDataSize = sizeof(indexData);
		constexpr size_t indexCount = indexDataSize / sizeof(indexData[0]);
	}

	namespace SpotLightProxyMesh
	{
		constexpr float vertexData[] =
		{
			0.0f, 0.707106f, -0.707107f,
			0.146447f, 0.353553f, -0.92388f,
			0.0f, 0.382683f, -0.92388f,
			0.0f, 0.0f, -1.0f,
			0.270598f, 0.270598f, -0.92388f,
			0.270598f, 0.653281f, -0.707107f,
			0.5f, 0.5f, -0.707107f,
			0.353553f, 0.146447f, -0.92388f,
			0.653281f, 0.270598f, -0.707107f,
			0.382683f, 0.0f, -0.92388f,
			0.353553f, -0.146446f, -0.92388f,
			0.707107f, 0.0f, -0.707107f,
			0.270598f, -0.270598f, -0.92388f,
			0.653281f, -0.270598f, -0.707107f,
			0.5f, -0.5f, -0.707107f,
			0.146446f, -0.353553f, -0.92388f,
			0.270598f, -0.653281f, -0.707107f,
			0.0f, -0.382683f, -0.92388f,
			-0.146447f, -0.353553f, -0.92388f,
			0.0f, -0.707106f, -0.707107f,
			-0.270598f, -0.653281f, -0.707107f,
			-0.270598f, -0.270598f, -0.92388f,
			-0.5f, -0.499999f, -0.707107f,
			-0.353553f, -0.146446f, -0.92388f,
			-0.382683f, 0.0f, -0.92388f,
			-0.653281f, -0.270597f, -0.707107f,
			-0.353553f, 0.146447f, -0.92388f,
			-0.707106f, 0.0f, -0.707107f,
			-0.653281f, 0.270598f, -0.707107f,
			-0.270598f, 0.270598f, -0.92388f,
			-0.499999f, 0.5f, -0.707107f,
			-0.146446f, 0.353553f, -0.92388f,
			-0.270598f, 0.653281f, -0.707107f,
			0.270598f, -0.653281f, -0.707107f,
			0.5f, -0.5f, -0.707107f,
			0.0f, 0.0f, 0.0f,
			0.0f, -0.707106f, -0.707107f,
			0.653281f, 0.270598f, -0.707107f,
			0.5f, 0.5f, -0.707107f,
			0.653281f, -0.270598f, -0.707107f,
			0.707107f, 0.0f, -0.707107f,
			-0.270598f, -0.653281f, -0.707107f,
			-0.653281f, -0.270597f, -0.707107f,
			-0.5f, -0.499999f, -0.707107f,
			0.0f, 0.707106f, -0.707107f,
			-0.270598f, 0.653281f, -0.707107f,
			-0.499999f, 0.5f, -0.707107f,
			-0.707106f, 0.0f, -0.707107f,
			0.270598f, 0.653281f, -0.707107f,
			-0.653281f, 0.270598f, -0.707107f,
		};

		constexpr uint16_t indexData[] =
		{
			0, 1, 2,
			3, 2, 1,
			3, 1, 4,
			5, 4, 1,
			6, 7, 4,
			3, 4, 7,
			8, 9, 7,
			3, 7, 9,
			3, 9, 10,
			11, 10, 9,
			3, 10, 12,
			13, 12, 10,
			14, 15, 12,
			3, 12, 15,
			16, 17, 15,
			3, 15, 17,
			3, 17, 18,
			19, 18, 17,
			20, 21, 18,
			3, 18, 21,
			22, 23, 21,
			3, 21, 23,
			3, 23, 24,
			25, 24, 23,
			3, 24, 26,
			27, 26, 24,
			28, 29, 26,
			3, 26, 29,
			30, 31, 29,
			3, 29, 31,
			3, 31, 2,
			32, 2, 31,
			33, 34, 35,
			36, 33, 35,
			37, 38, 35,
			34, 39, 35,
			40, 37, 35,
			41, 36, 35,
			42, 43, 35,
			43, 41, 35,
			44, 45, 35,
			45, 46, 35,
			47, 42, 35,
			48, 44, 35,
			46, 49, 35,
			38, 48, 35,
			49, 47, 35,
			39, 40, 35,
			0, 5, 1,
			5, 6, 4,
			6, 8, 7,
			8, 11, 9,
			11, 13, 10,
			13, 14, 12,
			14, 16, 15,
			16, 19, 17,
			19, 20, 18,
			20, 22, 21,
			22, 25, 23,
			25, 27, 24,
			27, 28, 26,
			28, 30, 29,
			30, 32, 31,
			32, 0, 2,
		};

		constexpr size_t vertexDataSize = sizeof(vertexData);
		constexpr size_t indexDataSize = sizeof(indexData);
		constexpr size_t indexCount = indexDataSize / sizeof(indexData[0]);
	}

	namespace BoxProxyMesh
	{
		constexpr float vertexData[] =
		{
			-1.0f, -1.0f, -1.0f,
			1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f, 1.0f,
			1.0f, -1.0f, 1.0f,
			-1.0f, 1.0f, -1.0f,
			1.0f, 1.0f, -1.0f,
			-1.0f, 1.0f, 1.0f,
			1.0f, 1.0f, 1.0f,
		};

		constexpr uint16_t indexData[] =
		{
			0, 1, 3,
			0, 3, 2,
			3, 7, 6,
			3, 6, 2,
			1, 5, 7,
			1, 7, 3,
			5, 4, 6,
			5, 6, 7,
			0, 4, 5,
			0, 5, 1,
			2, 6, 4,
			2, 4, 0
		};

		constexpr size_t vertexDataSize = sizeof(vertexData);
		constexpr size_t indexDataSize = sizeof(indexData);
		constexpr size_t indexCount = indexDataSize / sizeof(indexData[0]);
	}
}