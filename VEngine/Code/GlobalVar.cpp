#include "GlobalVar.h"

namespace VEngine
{
	GlobalVar<unsigned int> g_windowWidth = 1600;
	GlobalVar<unsigned int> g_windowHeight = 900;
	GlobalVar<bool> g_TAAEnabled = true;
	GlobalVar<float> g_TAABicubicSharpness = 0.5f;
	GlobalVar<float> g_TAATemporalContrastThreshold = 0.15f;
	GlobalVar<float> g_TAALowStrengthAlpha = 0.6f;
	GlobalVar<float> g_TAAHighStrengthAlpha = 0.05f;
	GlobalVar<float> g_TAAAntiFlickeringAlpha = 1e-10f;
	GlobalConst<unsigned int> g_shadowAtlasSize = 8192;
	GlobalConst<unsigned int> g_shadowAtlasMinTileSize = 256;
	GlobalConst<unsigned int> g_shadowCascadeSize = 2048;
	GlobalConst<unsigned int> g_shadowCascadeCount = 4;
}