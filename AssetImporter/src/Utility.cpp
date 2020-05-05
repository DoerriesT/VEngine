#include "Utility.h"
#include <Windows.h>

#undef min
#undef max
#undef OPAQUE

void Utility::fatalExit(const char *message, int exitCode)
{
	MessageBox(nullptr, message, nullptr, MB_OK | MB_ICONERROR);
	exit(exitCode);
}