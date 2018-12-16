#include "GlobalVar.h"

namespace VEngine
{
	GlobalVar<unsigned int> g_windowWidth = 1600;
	GlobalVar<unsigned int> g_windowHeight = 900;
	GlobalConst<unsigned int> g_shadowAtlasSize = 8192;
	GlobalConst<unsigned int> g_shadowAtlasMinTileSize = 256;
	GlobalConst<unsigned int> g_shadowCascadeSize = 2048;
	GlobalConst<unsigned int> g_shadowCascadeCount = 4;
}