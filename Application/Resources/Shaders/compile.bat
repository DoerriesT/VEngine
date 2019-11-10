glslc --target-env=vulkan1.1 -O -Werror -c geometry_vert.vert -o geometry_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c geometry_frag.frag -o geometry_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DALPHA_MASK_ENABLED=1 geometry_frag.frag -o geometry_alpha_mask_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c lighting_vert.vert -o lighting_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c lighting_frag.frag -o lighting_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DSSAO_ENABLED=1 lighting_frag.frag -o lighting_SSAO_ENABLED_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c shadows_vert.vert -o shadows_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DALPHA_MASK_ENABLED=1 shadows_vert.vert -o shadows_alpha_mask_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c shadows_frag.frag -o shadows_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c rasterTiling_vert.vert -o rasterTiling_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c rasterTiling_frag.frag -o rasterTiling_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c luminanceHistogram_comp.comp -o luminanceHistogram_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c luminanceHistogramReduceAverage_comp.comp -o luminanceHistogramReduceAverage_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c tonemap_comp.comp -o tonemap_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c taaResolve_comp.comp -o taaResolve_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c velocityInitialization_frag.frag -o velocityInitialization_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c fullscreenTriangle_vert.vert -o fullscreenTriangle_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c fxaa_comp.comp -o fxaa_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c gtao_comp.comp -o gtao_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c gtaoSpatialFilter_comp.comp -o gtaoSpatialFilter_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c gtaoTemporalFilter_comp.comp -o gtaoTemporalFilter_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c imgui_vert.vert -o imgui_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c imgui_frag.frag -o imgui_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c deferredShadows_comp.comp -o deferredShadows_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c meshClusterVisualization_vert.vert -o meshClusterVisualization_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c meshClusterVisualization_frag.frag -o meshClusterVisualization_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c occlusionCullingReproject_comp.comp -o occlusionCullingReproject_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c occlusionCullingCopyToDepth_frag.frag -o occlusionCullingCopyToDepth_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c occlusionCulling_vert.vert -o occlusionCulling_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c occlusionCulling_frag.frag -o occlusionCulling_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c occlusionCullingCreateDrawArgs_comp.comp -o occlusionCullingCreateDrawArgs_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c depthPyramid_comp.comp -o depthPyramid_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c occlusionCullingHiZ_comp.comp -o occlusionCullingHiZ_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c buildIndexBuffer_comp.comp -o buildIndexBuffer_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c screenSpaceVoxelization_comp.comp -o screenSpaceVoxelization_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c clearVoxels_comp.comp -o clearVoxels_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelDebug_vert.vert -o voxelDebug_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelDebug_frag.frag -o voxelDebug_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c irradianceVolumeDebug_vert.vert -o irradianceVolumeDebug_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c irradianceVolumeDebug_frag.frag -o irradianceVolumeDebug_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelDebug2_comp.comp -o voxelDebug2_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c fillLightingQueues_comp.comp -o fillLightingQueues_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c updateQueueProbability_comp.comp -o updateQueueProbability_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c lightIrradianceVolume_comp.comp -o lightIrradianceVolume_comp.spv

pause