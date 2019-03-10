glslc --target-env=vulkan1.1 -O -Werror -c geometry_vert.vert -o geometry_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c geometry_frag.frag -o geometry_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DALPHA_MASK_ENABLED=1 geometry_frag.frag -o geometry_alpha_mask_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c lighting_comp.comp -o lighting_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c forward_vert.vert -o forward_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c forward_frag.frag -o forward_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c shadows_vert.vert -o shadows_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c tiling_comp.comp -o tiling_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c tonemap_comp.comp -o tonemap_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c memoryHeapDebug_vert.vert -o memoryHeapDebug_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c memoryHeapDebug_frag.frag -o memoryHeapDebug_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c text_vert.vert -o text_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c text_frag.frag -o text_frag.spv

pause