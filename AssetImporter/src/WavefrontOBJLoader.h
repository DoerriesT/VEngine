#pragma once
#include "IModelLoader.h"

class WavefrontOBJLoader : public IModelLoader
{
public:
	Model loadModel(const std::string &filepath, bool mergeByMaterial, bool invertTexcoordY) const override;
};