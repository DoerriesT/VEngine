#pragma once
#include <vector>

namespace Utility
{
	void fatalExit(const char *message, int exitCode);
	std::vector<char> readBinaryFile(const char *filepath);
}