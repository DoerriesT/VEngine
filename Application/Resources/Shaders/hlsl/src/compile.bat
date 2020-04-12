dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main depthPrepass_vs.hlsl -Fo ./../depthPrepass_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 depthPrepass_vs.hlsl -Fo ./../depthPrepass_ALPHA_MASK_ENABLED_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main depthPrepass_ps.hlsl -Fo ./../depthPrepass_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main forward_vs.hlsl -Fo ./../forward_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main forward_ps.hlsl -Fo ./../forward_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main shadows_vs.hlsl -Fo ./../shadows_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 shadows_vs.hlsl -Fo ./../shadows_ALPHA_MASK_ENABLED_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main shadows_ps.hlsl -Fo ./../shadows_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main luminanceHistogram_cs.hlsl -Fo ./../luminanceHistogram_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main exposure_cs.hlsl -Fo ./../exposure_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main tonemap_cs.hlsl -Fo ./../tonemap_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main deferredShadows_cs.hlsl -Fo ./../deferredShadows_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogVBuffer_cs.hlsl -Fo ./../volumetricFogVBuffer_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogFilter_cs.hlsl -Fo ./../volumetricFogFilter_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogApply_cs.hlsl -Fo ./../volumetricFogApply_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main sky_vs.hlsl -Fo ./../sky_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main sky_ps.hlsl -Fo ./../sky_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main temporalAA_cs.hlsl -Fo ./../temporalAA_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main sharpen_ffx_cas_cs.hlsl -Fo ./../sharpen_ffx_cas_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main fxaa_cs.hlsl -Fo ./../fxaa_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main imgui_vs.hlsl -Fo ./../imgui_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main imgui_ps.hlsl -Fo ./../imgui_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main rasterTiling_vs.hlsl -Fo ./../rasterTiling_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main rasterTiling_ps.hlsl -Fo ./../rasterTiling_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main integrateBrdf_cs.hlsl -Fo ./../integrateBrdf_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main bloomDownsample_cs.hlsl -Fo ./../bloomDownsample_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main bloomUpsample_cs.hlsl -Fo ./../bloomUpsample_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main velocityInitialization_ps.hlsl -Fo ./../velocityInitialization_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main fullscreenTriangle_vs.hlsl -Fo ./../fullscreenTriangle_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main -D REDUCE=min hiZPyramid_cs.hlsl -Fo ./../hiZPyramid_MIN_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main -D REDUCE=max hiZPyramid_cs.hlsl -Fo ./../hiZPyramid_MAX_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main ssr_cs.hlsl -Fo ./../ssr_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main ssrResolve_cs.hlsl -Fo ./../ssrResolve_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main ssrTemporalFilter_cs.hlsl -Fo ./../ssrTemporalFilter_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main gaussianPyramid_cs.hlsl -Fo ./../gaussianPyramid_cs.spv

pause