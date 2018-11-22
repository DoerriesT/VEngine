#pragma once
#include <vector>

namespace VEngine
{
	namespace Utility
	{
		std::vector<char> readBinaryFile(const char *filepath);
		void fatalExit(const char *message, int exitCode);
	}
}