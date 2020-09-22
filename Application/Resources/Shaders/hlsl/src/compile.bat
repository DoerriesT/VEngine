dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main depthPrepass_vs.hlsl -Fo ./../depthPrepass_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 depthPrepass_vs.hlsl -Fo ./../depthPrepass_ALPHA_MASK_ENABLED_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main depthPrepass_ps.hlsl -Fo ./../depthPrepass_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main forward_vs.hlsl -Fo ./../forward_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main forward_ps.hlsl -Fo ./../forward_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main shadows_vs.hlsl -Fo ./../shadows_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 shadows_vs.hlsl -Fo ./../shadows_ALPHA_MASK_ENABLED_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main shadows_ps.hlsl -Fo ./../shadows_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main luminanceHistogram_cs.hlsl -Fo ./../luminanceHistogram_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main exposure_cs.hlsl -Fo ./../exposure_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main tonemap_cs.hlsl -Fo ./../tonemap_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main deferredShadows_cs.hlsl -Fo ./../deferredShadows_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogMerged_cs.hlsl -Fo ./../volumetricFogMerged_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogFilter2_cs.hlsl -Fo ./../volumetricFogFilter2_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main volumetricFogApply_ps.hlsl -Fo ./../volumetricFogApply_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main sky_vs.hlsl -Fo ./../sky_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main sky_ps.hlsl -Fo ./../sky_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main temporalAA_cs.hlsl -Fo ./../temporalAA_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main sharpen_ffx_cas_cs.hlsl -Fo ./../sharpen_ffx_cas_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main fxaa_cs.hlsl -Fo ./../fxaa_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main imgui_vs.hlsl -Fo ./../imgui_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main imgui_ps.hlsl -Fo ./../imgui_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main rasterTiling_vs.hlsl -Fo ./../rasterTiling_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main rasterTiling_ps.hlsl -Fo ./../rasterTiling_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main integrateBrdf_cs.hlsl -Fo ./../integrateBrdf_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main bloomDownsample_cs.hlsl -Fo ./../bloomDownsample_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main bloomUpsample_cs.hlsl -Fo ./../bloomUpsample_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main velocityInitialization_ps.hlsl -Fo ./../velocityInitialization_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main fullscreenTriangle_vs.hlsl -Fo ./../fullscreenTriangle_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main -D REDUCE=min hiZPyramid_cs.hlsl -Fo ./../hiZPyramid_MIN_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main -D REDUCE=max hiZPyramid_cs.hlsl -Fo ./../hiZPyramid_MAX_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main ssr_cs.hlsl -Fo ./../ssr_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main ssrResolve_cs.hlsl -Fo ./../ssrResolve_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main ssrTemporalFilter_cs.hlsl -Fo ./../ssrTemporalFilter_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main gaussianPyramid_cs.hlsl -Fo ./../gaussianPyramid_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main gtao_cs.hlsl -Fo ./../gtao_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main gtaoSpatialFilter_cs.hlsl -Fo ./../gtaoSpatialFilter_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main gtaoTemporalFilter_cs.hlsl -Fo ./../gtaoTemporalFilter_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main probeGBuffer_vs.hlsl -Fo ./../probeGBuffer_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main probeGBuffer_ps.hlsl -Fo ./../probeGBuffer_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main -D ALPHA_MASK_ENABLED=1 probeGBuffer_ps.hlsl -Fo ./../probeGBuffer_ALPHA_MASK_ENABLED_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main lightProbeGBuffer_cs.hlsl -Fo ./../lightProbeGBuffer_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main downsampleCubemap_cs.hlsl -Fo ./../downsampleCubemap_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main probeFilter_cs.hlsl -Fo ./../probeFilter_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main probeCompressBCH6_cs.hlsl -Fo ./../probeCompressBCH6_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main probeFilterImportanceSampling_cs.hlsl -Fo ./../probeFilterImportanceSampling_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main probeDownsample_cs.hlsl -Fo ./../probeDownsample_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main atmosphereComputeTransmittance_cs.hlsl -Fo ./../atmosphereComputeTransmittance_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main atmosphereComputeDirectIrradiance_cs.hlsl -Fo ./../atmosphereComputeDirectIrradiance_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main atmosphereComputeSingleScattering_cs.hlsl -Fo ./../atmosphereComputeSingleScattering_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main atmosphereComputeScatteringDensity_cs.hlsl -Fo ./../atmosphereComputeScatteringDensity_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main atmosphereComputeIndirectIrradiance_cs.hlsl -Fo ./../atmosphereComputeIndirectIrradiance_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main atmosphereComputeMultipleScattering_cs.hlsl -Fo ./../atmosphereComputeMultipleScattering_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main fourierOpacityVolume_cs.hlsl -Fo ./../fourierOpacityVolume_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main fourierOpacityBlur_cs.hlsl -Fo ./../fourierOpacityBlur_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main gtao2_cs.hlsl -Fo ./../gtao2_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main particles_vs.hlsl -Fo ./../particles_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main particles_ps.hlsl -Fo ./../particles_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main fourierOpacityParticle_cs.hlsl -Fo ./../fourierOpacityParticle_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main swapChainCopy_cs.hlsl -Fo ./../swapChainCopy_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricRaymarch_cs.hlsl -Fo ./../volumetricRaymarch_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main depthDownsampleCBMinMax_cs.hlsl -Fo ./../depthDownsampleCBMinMax_cs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main applyIndirectLighting_ps.hlsl -Fo ./../applyIndirectLighting_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main visibilityBuffer_vs.hlsl -Fo ./../visibilityBuffer_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 visibilityBuffer_vs.hlsl -Fo ./../visibilityBuffer_ALPHA_MASK_ENABLED_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main visibilityBuffer_ps.hlsl -Fo ./../visibilityBuffer_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main -D ALPHA_MASK_ENABLED=1 visibilityBuffer_ps.hlsl -Fo ./../visibilityBuffer_ALPHA_MASK_ENABLED_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main shadeVisibilityBuffer_ps.hlsl -Fo ./../shadeVisibilityBuffer_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main fourierOpacityVolume_vs.hlsl -Fo ./../fourierOpacityVolume_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main fourierOpacityVolume_ps.hlsl -Fo ./../fourierOpacityVolume_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main fourierOpacityGlobal_ps.hlsl -Fo ./../fourierOpacityGlobal_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main fourierOpacityParticleDirectional_vs.hlsl -Fo ./../fourierOpacityParticleDirectional_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main fourierOpacityParticleDirectional_ps.hlsl -Fo ./../fourierOpacityParticleDirectional_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main fourierOpacityVolumeDirectional_vs.hlsl -Fo ./../fourierOpacityVolumeDirectional_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main fourierOpacityVolumeDirectional_ps.hlsl -Fo ./../fourierOpacityVolumeDirectional_ps.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main fourierOpacityDepth_vs.hlsl -Fo ./../fourierOpacityDepth_vs.spv

dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main debugDraw_vs.hlsl -Fo ./../debugDraw_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main debugDraw_ps.hlsl -Fo ./../debugDraw_ps.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -fvk-invert-y -T vs_6_2 -E main billboard_vs.hlsl -Fo ./../billboard_vs.spv
dxc.exe -D VULKAN=1 -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main billboard_ps.hlsl -Fo ./../billboard_ps.spv

pause