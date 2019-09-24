glslc --target-env=vulkan1.1 -O -Werror -c geometry_vert.vert -o geometry_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c geometry_frag.frag -o geometry_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DALPHA_MASK_ENABLED=1 geometry_frag.frag -o geometry_alpha_mask_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c lighting_vert.vert -o lighting_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c lighting_frag.frag -o lighting_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DSSAO_ENABLED=1 lighting_frag.frag -o lighting_SSAO_ENABLED_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c shadows_vert.vert -o shadows_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DALPHA_MASK_ENABLED=1 shadows_vert.vert -o shadows_alpha_mask_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c shadows_frag.frag -o shadows_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c memoryHeapDebug_vert.vert -o memoryHeapDebug_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c memoryHeapDebug_frag.frag -o memoryHeapDebug_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c text_vert.vert -o text_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c text_frag.frag -o text_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c rasterTiling_vert.vert -o rasterTiling_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c rasterTiling_frag.frag -o rasterTiling_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c luminanceHistogram_comp.comp -o luminanceHistogram_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c luminanceHistogramReduceAverage_comp.comp -o luminanceHistogramReduceAverage_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c tonemap_comp.comp -o tonemap_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c luminanceHistogramDebug_vert.vert -o luminanceHistogramDebug_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c luminanceHistogramDebug_frag.frag -o luminanceHistogramDebug_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c taaResolve_comp.comp -o taaResolve_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c velocityInitialization_frag.frag -o velocityInitialization_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c fullscreenTriangle_vert.vert -o fullscreenTriangle_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c prepareIndirectBuffers_comp.comp -o prepareIndirectBuffers_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c fxaa_comp.comp -o fxaa_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c transparencyWrite_frag.frag -o transparencyWrite_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c transparencyWrite_vert.vert -o transparencyWrite_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c gtao_comp.comp -o gtao_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c gtaoSpatialFilter_comp.comp -o gtaoSpatialFilter_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c gtaoTemporalFilter_comp.comp -o gtaoTemporalFilter_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c sdsmClear_comp.comp -o sdsmClear_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c sdsmDepthReduce_comp.comp -o sdsmDepthReduce_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c sdsmSplits_comp.comp -o sdsmSplits_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c sdsmBoundsReduce_comp.comp -o sdsmBoundsReduce_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c sdsmShadowMatrix_comp.comp -o sdsmShadowMatrix_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c triangleFilter_comp.comp -o triangleFilter_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c drawCallCompaction_comp.comp -o drawCallCompaction_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c rendergraphtest_comp.comp -o rendergraphtest_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c imgui_vert.vert -o imgui_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c imgui_frag.frag -o imgui_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c deferredShadows_comp.comp -o deferredShadows_comp.spv

pause