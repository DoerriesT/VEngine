#include "GlobalVar.h"

namespace VEngine
{
	//GlobalVar<unsigned int> g_windowWidth = 1600;
	//GlobalVar<unsigned int> g_windowHeight = 900;
	GlobalVar<bool> g_TAAEnabled = false;
	GlobalVar<float> g_TAABicubicSharpness = 0.5f;
	GlobalVar<float> g_TAATemporalContrastThreshold = 0.15f;
	GlobalVar<float> g_TAALowStrengthAlpha = 0.6f;
	GlobalVar<float> g_TAAHighStrengthAlpha = 0.05f;
	GlobalVar<float> g_TAAAntiFlickeringAlpha = 1e-10f;
	GlobalVar<bool> g_FXAAEnabled = false;
	GlobalVar<float> g_FXAAQualitySubpix = 0.75f;
	GlobalVar<float> g_FXAAQualityEdgeThreshold = 0.166f;
	GlobalVar<float> g_FXAAQualityEdgeThresholdMin = 0.0833f;
	GlobalVar<bool> g_CASEnabled = true;
	GlobalVar<float> g_CASSharpness = 0.8f;
	GlobalVar<bool> g_bloomEnabled = true;
	GlobalVar<float> g_bloomStrength = 0.04f;
	GlobalVar<unsigned int> g_ssaoEnabled = 0;
	GlobalVar<float> g_gtaoRadius = 2.0f;
	GlobalVar<unsigned int> g_gtaoSteps = 4;
	GlobalVar<unsigned int> g_gtaoMaxRadiusPixels = 256;
	GlobalConst<unsigned int> g_shadowAtlasSize = 4096;
	GlobalVar<float> g_ExposureHistogramLogMin = -10.0f;
	GlobalVar<float> g_ExposureHistogramLogMax = 5.0f;
	GlobalVar<float> g_ExposureLowPercentage = 0.5f;
	GlobalVar<float> g_ExposureHighPercentage = 0.9f;
	GlobalVar<float> g_ExposureSpeedUp = 3.0f;
	GlobalVar<float> g_ExposureSpeedDown = 1.0f;
	GlobalVar<float> g_ExposureCompensation = 1.0f;
	GlobalVar<float> g_ExposureMin = 0.01f;
	GlobalVar<float> g_ExposureMax = 100.0f;
	GlobalVar<bool> g_ExposureFixed = false;
	GlobalVar<float> g_ExposureFixedValue = 2.0f;
	GlobalConst<bool> g_vulkanDebugCallBackEnabled = true;
}