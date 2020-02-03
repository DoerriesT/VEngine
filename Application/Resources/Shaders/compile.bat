glslc --target-env=vulkan1.1 -O -Werror -c geometry_vert.vert -o geometry_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c geometry_frag.frag -o geometry_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DALPHA_MASK_ENABLED=1 geometry_frag.frag -o geometry_alpha_mask_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c lighting_comp.comp -o lighting_comp.spv
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
glslc --target-env=vulkan1.1 -O -Werror -c occlusionCullingHiZ_comp.comp -o occlusionCullingHiZ_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c buildIndexBuffer_comp.comp -o buildIndexBuffer_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c clearVoxels_comp.comp -o clearVoxels_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c irradianceVolumeDebug_vert.vert -o irradianceVolumeDebug_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c irradianceVolumeDebug_frag.frag -o irradianceVolumeDebug_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c fillLightingQueues_comp.comp -o fillLightingQueues_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c updateQueueProbability_comp.comp -o updateQueueProbability_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c irradianceVolumeUpdateProbes_comp.comp -o irradianceVolumeUpdateProbes_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DOUTPUT_DEPTH=1 irradianceVolumeUpdateProbes_comp.comp -o irradianceVolumeUpdateProbes_OUTPUT_DEPTH_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c rayTraceTest_comp.comp -o rayTraceTest_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c sharpen_ffx_cas_comp.comp -o sharpen_ffx_cas_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelizationMark_comp.comp -o voxelizationMark_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelizationAllocate_comp.comp -o voxelizationAllocate_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelizationFill_vert.vert -o voxelizationFill_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelizationFill_geom.geom -o voxelizationFill_geom.spv
glslc --target-env=vulkan1.1 -O -Werror -c voxelizationFill_frag.frag -o voxelizationFill_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DLIGHTING_ENABLED=1 voxelizationFill_vert.vert -o voxelizationFill_LIGHTING_ENABLED_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DLIGHTING_ENABLED=1 voxelizationFill_geom.geom -o voxelizationFill_LIGHTING_ENABLED_geom.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DLIGHTING_ENABLED=1 voxelizationFill_frag.frag -o voxelizationFill_LIGHTING_ENABLED_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c clearBricks_comp.comp -o clearBricks_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c initBrickPool_comp.comp -o initBrickPool_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c brickDebug_comp.comp -o brickDebug_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c irradianceVolumeRayMarching2_comp.comp -o irradianceVolumeRayMarching2_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c screenSpaceVoxelization2_comp.comp -o screenSpaceVoxelization2_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c indirectDiffuse_comp.comp -o indirectDiffuse_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DSSAO_ENABLED=1 indirectDiffuse_comp.comp -o indirectDiffuse_SSAO_ENABLED_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c bloomDownsample_comp.comp -o bloomDownsample_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c bloomUpsample_comp.comp -o bloomUpsample_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c taa_comp.comp -o taa_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DREDUCE=min hiZPyramid_comp.comp -o hiZPyramid_MIN_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c -DREDUCE=max hiZPyramid_comp.comp -o hiZPyramid_MAX_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c indirectLight_comp.comp -o indirectLight_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c ssr_comp.comp -o ssr_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c ssrResolve_comp.comp -o ssrResolve_comp.spv
glslc --target-env=vulkan1.1 -O -Werror -c depthPrepass_vert.vert -o depthPrepass_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c depthPrepass_frag.frag -o depthPrepass_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c forward_vert.vert -o forward_vert.spv
glslc --target-env=vulkan1.1 -O -Werror -c forward_frag.frag -o forward_frag.spv
glslc --target-env=vulkan1.1 -O -Werror -c integrateBrdf_comp.comp -o integrateBrdf_comp.spv

pause