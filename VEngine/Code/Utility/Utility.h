#pragma once
#include <vector>

namespace VEngine
{
	namespace Utility
	{
		std::vector<char> readBinaryFile(const char *filepath);
		void fatalExit(const char *message, int exitCode);
		std::string getFileExtension(const std::string &filepath);
		uint32_t findFirstSetBit(uint32_t mask);
		uint32_t findLastSetBit(uint32_t mask);
	}
}