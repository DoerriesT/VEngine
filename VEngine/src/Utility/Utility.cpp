#include "Utility.h"
#include <Windows.h>
#include <fstream>
#include <cassert>
#include <cmath>
#include <glm/common.hpp>

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

bool VEngine::Utility::isPowerOfTwo(uint32_t value)
{
	return (value & (value - 1)) == 0 && value > 0;
}

uint32_t VEngine::Utility::nextPowerOfTwo(uint32_t value)
{
	assert(value > 0);
	// https://graphics.stanford.edu/~seander/bithacks.html#RoundUpPowerOf2
	value--;
	value |= value >> 1;
	value |= value >> 2;
	value |= value >> 4;
	value |= value >> 8;
	value |= value >> 16;
	value++;
	return value;
}

float VEngine::Utility::halton(size_t index, size_t base)
{
	float f = 1.0f;
	float r = 0.0f;

	while (index > 0)
	{
		f /= base;
		r += f * (index % base);
		index /=base;
	}

	return r;
}

// https://github.com/neilbartlett/color-temperature
void VEngine::Utility::colorTemperatureToColor(float kelvin, float &red, float &green, float &blue)
{
	kelvin = glm::clamp(kelvin, 1000.0f, 15000.0f);
	float temperature = kelvin * 0.01f;
	red = 0.0f;
	green = 0.0f;
	blue = 0.0f;

	if (temperature < 66.0f) 
	{
		red = 255.0f;
	}
	else 
	{
		// a + b x + c Log[x] /.
		// {a -> 351.97690566805693`,
		// b -> 0.114206453784165`,
		// c -> -40.25366309332127
		//x -> (kelvin/100) - 55}
		red = temperature - 55.0f;
		red = 351.97690566805693f + 0.114206453784165f * red - 40.25366309332127f * log(red);
		red = glm::clamp(red, 0.0f, 255.0f);
	}

	/* Calculate green */

	if (temperature < 66.0f) 
	{
		// a + b x + c Log[x] /.
		// {a -> -155.25485562709179`,
		// b -> -0.44596950469579133`,
		// c -> 104.49216199393888`,
		// x -> (kelvin/100) - 2}
		green = temperature - 2.0f;
		green = -155.25485562709179f - 0.44596950469579133f * green + 104.49216199393888f * log(green);
		green = glm::clamp(green, 0.0f, 255.0f);

	}
	else 
	{
		// a + b x + c Log[x] /.
		// {a -> 325.4494125711974`,
		// b -> 0.07943456536662342`,
		// c -> -28.0852963507957`,
		// x -> (kelvin/100) - 50}
		green = temperature - 50.0f;
		green = 325.4494125711974f + 0.07943456536662342f * green - 28.0852963507957f * log(green);
		green = glm::clamp(green, 0.0f, 255.0f);
	}

	/* Calculate blue */

	if (temperature >= 66.0f) 
	{
		blue = 255.0f;
	}
	else 
	{

		if (temperature <= 20.0f) 
		{
			blue = 0.0f;
		}
		else 
		{
			// a + b x + c Log[x] /.
			// {a -> -254.76935184120902`,
			// b -> 0.8274096064007395`,
			// c -> 115.67994401066147`,
			// x -> kelvin/100 - 10}
			blue = temperature - 10.0f;
			blue = -254.76935184120902f + 0.8274096064007395f * blue + 115.67994401066147f * log(blue);
			blue = glm::clamp(blue, 0.0f, 255.0f);
		}
	}

	red *= (1.0f / 255.0f);
	green *= (1.0f / 255.0f);
	blue *= (1.0f / 255.0f);;
}
