#include "Utility.h"
#include <Windows.h>
#include <fstream>

std::vector<char> VEngine::Utility::readBinaryFile(const char *filepath)
{
	std::ifstream file(filepath, std::ios::binary | std::ios::ate);

	if (!file.is_open())
	{
		std::string msg = "Failed to open file " + std::string(filepath) + "!";
		fatalExit(msg.c_str(), -1);
	}

	std::streamsize size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	if (file.read(buffer.data(), size))
	{
		return buffer;
	}
	else
	{
		std::string msg = "Failed to read file " + std::string(filepath) + "!";
		fatalExit(msg.c_str(), -1);
		return {};
	}
}

void VEngine::Utility::fatalExit(const char *message, int exitCode)
{
	MessageBox(nullptr, message, nullptr, MB_OK | MB_ICONERROR);
	exit(exitCode);
}

std::string VEngine::Utility::getFileExtension(const std::string &filepath)
{
	return filepath.substr(filepath.find_last_of('.') + 1);
}

uint32_t VEngine::Utility::findFirstSetBit(uint32_t mask)
{
	uint32_t r = 1;

	if (!mask)
	{
		return -1;
	}

	if (!(mask & 0xFFFF))
	{
		mask >>= 16;
		r += 16;
	}
	if (!(mask & 0xFF))
	{
		mask >>= 8;
		r += 8;
	}
	if (!(mask & 0xF))
	{
		mask >>= 4;
		r += 4;
	}
	if (!(mask & 3))
	{
		mask >>= 2;
		r += 2;
	}
	if (!(mask & 1))
	{
		mask >>= 1;
		r += 1;
	}
	return r - 1;
}

uint32_t VEngine::Utility::findLastSetBit(uint32_t mask)
{
	uint32_t r = 32;

	if (!mask)
	{
		return -1;
	}

	if (!(mask & 0xFFFF0000u))
	{
		mask <<= 16;
		r -= 16;
	}
	if (!(mask & 0xFF000000u))
	{
		mask <<= 8;
		r -= 8;
	}
	if (!(mask & 0xF0000000u))
	{
		mask <<= 4;
		r -= 4;
	}
	if (!(mask & 0xC0000000u))
	{
		mask <<= 2;
		r -= 2;
	}
	if (!(mask & 0x80000000u))
	{
		mask <<= 1;
		r -= 1;
	}
	return r - 1;
}
