#version 450

#include "occlusionCulling_bindings.h"

layout(set = VISIBILITY_BUFFER_SET, binding = VISIBILITY_BUFFER_BINDING) buffer VISIBILITY_BUFFER 
{
    uint uVisibilityBuffer[];
};

layout(early_fragment_tests) in;

layout(location = 0) flat in uint vInstanceID;


void main() 
{
	uVisibilityBuffer[vInstanceID] = 1;
}

