glslc --target-env=vulkan1.1 -O -Werror -c geometry_vert.vert -o geometry_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c geometry_frag.frag -o geometry_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DALPHA_MASK_ENABLED=1 geometry_frag.frag -o geometry_alpha_mask_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c lighting_comp.comp -o lighting_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c shadows_vert.vert -o shadows_vert.spv
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

pause