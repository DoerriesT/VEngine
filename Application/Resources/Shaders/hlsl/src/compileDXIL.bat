dxc.exe -T vs_6_2 -E main depthPrepass_vs.hlsl -Fo ./../depthPrepass_vs.dxil -Zi -Fd ./../depthPrepass_vs.pdb
dxc.exe -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 depthPrepass_vs.hlsl -Fo ./../depthPrepass_ALPHA_MASK_ENABLED_vs.dxil -Zi -Fd ./../depthPrepass_ALPHA_MASK_ENABLED_vs.pdb
dxc.exe -T ps_6_2 -E main depthPrepass_ps.hlsl -Fo ./../depthPrepass_ps.dxil -Zi -Fd ./../depthPrepass_ps.pdb
dxc.exe -T vs_6_2 -E main forward_vs.hlsl -Fo ./../forward_vs.dxil -Zi -Fd ./../forward_vs.pdb
dxc.exe -T ps_6_2 -E main forward_ps.hlsl -Fo ./../forward_ps.dxil -Zi -Fd ./../forward_ps.pdb
dxc.exe -T vs_6_2 -E main shadows_vs.hlsl -Fo ./../shadows_vs.dxil -Zi -Fd ./../shadows_vs.pdb
dxc.exe -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 shadows_vs.hlsl -Fo ./../shadows_ALPHA_MASK_ENABLED_vs.dxil -Zi -Fd ./../shadows_ALPHA_MASK_ENABLED_vs.pdb
dxc.exe -T ps_6_2 -E main shadows_ps.hlsl -Fo ./../shadows_ps.dxil -Zi -Fd ./../shadows_ps.pdb
dxc.exe -T cs_6_2 -E main luminanceHistogram_cs.hlsl -Fo ./../luminanceHistogram_cs.dxil -Zi -Fd ./../luminanceHistogram_cs.pdb
dxc.exe -T cs_6_2 -E main exposure_cs.hlsl -Fo ./../exposure_cs.dxil -Zi -Fd ./../exposure_cs.pdb
dxc.exe -T cs_6_2 -E main tonemap_cs.hlsl -Fo ./../tonemap_cs.dxil -Zi -Fd ./../tonemap_cs.pdb
dxc.exe -T cs_6_2 -E main deferredShadows_cs.hlsl -Fo ./../deferredShadows_cs.dxil -Zi -Fd ./../deferredShadows_cs.pdb
dxc.exe -T cs_6_2 -E main volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_cs.dxil -Zi -Fd ./../volumetricFogScatter_cs.pdb
dxc.exe -T cs_6_2 -E main -D VBUFFER_ONLY=1 volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_VBUFFER_ONLY_cs.dxil -Zi -Fd ./../volumetricFogScatter_VBUFFER_ONLY_cs.pdb
dxc.exe -T cs_6_2 -E main -D IN_SCATTER_ONLY=1 volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_IN_SCATTER_ONLY_cs.dxil -Zi -Fd ./../volumetricFogScatter_IN_SCATTER_ONLY_cs.pdb
dxc.exe -T cs_6_2 -E main -D CHECKER_BOARD=1 volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_CHECKER_BOARD_cs.dxil -Zi -Fd ./../volumetricFogScatter_CHECKER_BOARD_cs.pdb
dxc.exe -T cs_6_2 -E main -D CHECKER_BOARD=1 -D VBUFFER_ONLY=1 volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_CHECKER_BOARD_VBUFFER_ONLY_cs.dxil -Zi -Fd ./../volumetricFogScatter_CHECKER_BOARD_VBUFFER_ONLY_cs.pdb
dxc.exe -T cs_6_2 -E main -D CHECKER_BOARD=1 -D IN_SCATTER_ONLY=1 volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_CHECKER_BOARD_IN_SCATTER_ONLY_cs.dxil -Zi -Fd ./../volumetricFogScatter_CHECKER_BOARD_IN_SCATTER_ONLY_cs.pdb
dxc.exe -T cs_6_2 -E main volumetricFogFilter_cs.hlsl -Fo ./../volumetricFogFilter_cs.dxil -Zi -Fd ./../volumetricFogFilter_cs.pdb
dxc.exe -T cs_6_2 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.dxil -Zi -Fd ./../volumetricFogIntegrate_cs.pdb
dxc.exe -T ps_6_2 -E main volumetricFogApply_ps.hlsl -Fo ./../volumetricFogApply_ps.dxil -Zi -Fd ./../volumetricFogApply_ps.pdb
dxc.exe -T vs_6_2 -E main sky_vs.hlsl -Fo ./../sky_vs.dxil -Zi -Fd ./../sky_vs.pdb
dxc.exe -T ps_6_2 -E main sky_ps.hlsl -Fo ./../sky_ps.dxil -Zi -Fd ./../sky_ps.pdb
dxc.exe -T cs_6_2 -E main temporalAA_cs.hlsl -Fo ./../temporalAA_cs.dxil -Zi -Fd ./../temporalAA_cs.pdb
dxc.exe -T cs_6_2 -E main sharpen_ffx_cas_cs.hlsl -Fo ./../sharpen_ffx_cas_cs.dxil -Zi -Fd ./../sharpen_ffx_cas_cs.pdb
dxc.exe -T cs_6_2 -E main fxaa_cs.hlsl -Fo ./../fxaa_cs.dxil -Zi -Fd ./../fxaa_cs.pdb
dxc.exe -T vs_6_2 -E main imgui_vs.hlsl -Fo ./../imgui_vs.dxil -Zi -Fd ./../imgui_vs.pdb
dxc.exe -T ps_6_2 -E main imgui_ps.hlsl -Fo ./../imgui_ps.dxil -Zi -Fd ./../imgui_ps.pdb
dxc.exe -T vs_6_2 -E main rasterTiling_vs.hlsl -Fo ./../rasterTiling_vs.dxil -Zi -Fd ./../rasterTiling_vs.pdb
dxc.exe -T ps_6_2 -E main rasterTiling_ps.hlsl -Fo ./../rasterTiling_ps.dxil -Zi -Fd ./../rasterTiling_ps.pdb
dxc.exe -T cs_6_2 -E main integrateBrdf_cs.hlsl -Fo ./../integrateBrdf_cs.dxil -Zi -Fd ./../integrateBrdf_cs.pdb
dxc.exe -T cs_6_2 -E main bloomDownsample_cs.hlsl -Fo ./../bloomDownsample_cs.dxil -Zi -Fd ./../bloomDownsample_cs.pdb
dxc.exe -T cs_6_2 -E main bloomUpsample_cs.hlsl -Fo ./../bloomUpsample_cs.dxil -Zi -Fd ./../bloomUpsample_cs.pdb
dxc.exe -T ps_6_2 -E main velocityInitialization_ps.hlsl -Fo ./../velocityInitialization_ps.dxil -Zi -Fd ./../velocityInitialization_ps.pdb
dxc.exe -T vs_6_2 -E main fullscreenTriangle_vs.hlsl -Fo ./../fullscreenTriangle_vs.dxil -Zi -Fd ./../fullscreenTriangle_vs.pdb
dxc.exe -T cs_6_2 -E main -D REDUCE=min hiZPyramid_cs.hlsl -Fo ./../hiZPyramid_MIN_cs.dxil -Zi -Fd ./../hiZPyramid_MIN_cs.pdb
dxc.exe -T cs_6_2 -E main -D REDUCE=max hiZPyramid_cs.hlsl -Fo ./../hiZPyramid_MAX_cs.dxil -Zi -Fd ./../hiZPyramid_MAX_cs.pdb
dxc.exe -T cs_6_2 -E main ssr_cs.hlsl -Fo ./../ssr_cs.dxil -Zi -Fd ./../ssr_cs.pdb
dxc.exe -T cs_6_2 -E main ssrResolve_cs.hlsl -Fo ./../ssrResolve_cs.dxil -Zi -Fd ./../ssrResolve_cs.pdb
dxc.exe -T cs_6_2 -E main ssrTemporalFilter_cs.hlsl -Fo ./../ssrTemporalFilter_cs.dxil -Zi -Fd ./../ssrTemporalFilter_cs.pdb
dxc.exe -T cs_6_2 -E main gaussianPyramid_cs.hlsl -Fo ./../gaussianPyramid_cs.dxil -Zi -Fd ./../gaussianPyramid_cs.pdb
dxc.exe -T cs_6_2 -E main gtao_cs.hlsl -Fo ./../gtao_cs.dxil -Zi -Fd ./../gtao_cs.pdb
dxc.exe -T cs_6_2 -E main gtaoSpatialFilter_cs.hlsl -Fo ./../gtaoSpatialFilter_cs.dxil -Zi -Fd ./../gtaoSpatialFilter_cs.pdb
dxc.exe -T cs_6_2 -E main gtaoTemporalFilter_cs.hlsl -Fo ./../gtaoTemporalFilter_cs.dxil -Zi -Fd ./../gtaoTemporalFilter_cs.pdb
dxc.exe -T vs_6_2 -E main probeGBuffer_vs.hlsl -Fo ./../probeGBuffer_vs.dxil -Zi -Fd ./../probeGBuffer_vs.pdb
dxc.exe -T ps_6_2 -E main probeGBuffer_ps.hlsl -Fo ./../probeGBuffer_ps.dxil -Zi -Fd ./../probeGBuffer_ps.pdb
dxc.exe -T ps_6_2 -E main -D ALPHA_MASK_ENABLED=1 probeGBuffer_ps.hlsl -Fo ./../probeGBuffer_ALPHA_MASK_ENABLED_ps.dxil -Zi -Fd ./../probeGBuffer_ALPHA_MASK_ENABLED_ps.pdb
dxc.exe -T cs_6_2 -E main lightProbeGBuffer_cs.hlsl -Fo ./../lightProbeGBuffer_cs.dxil -Zi -Fd ./../lightProbeGBuffer_cs.pdb
dxc.exe -T cs_6_2 -E main downsampleCubemap_cs.hlsl -Fo ./../downsampleCubemap_cs.dxil -Zi -Fd ./../downsampleCubemap_cs.pdb
dxc.exe -T cs_6_2 -E main probeFilter_cs.hlsl -Fo ./../probeFilter_cs.dxil -Zi -Fd ./../probeFilter_cs.pdb
dxc.exe -T cs_6_2 -E main probeCompressBCH6_cs.hlsl -Fo ./../probeCompressBCH6_cs.dxil -Zi -Fd ./../probeCompressBCH6_cs.pdb
dxc.exe -T cs_6_2 -E main probeFilterImportanceSampling_cs.hlsl -Fo ./../probeFilterImportanceSampling_cs.dxil -Zi -Fd ./../probeFilterImportanceSampling_cs.pdb
dxc.exe -T cs_6_2 -E main probeDownsample_cs.hlsl -Fo ./../probeDownsample_cs.dxil -Zi -Fd ./../probeDownsample_cs.pdb
dxc.exe -T cs_6_2 -E main atmosphereComputeTransmittance_cs.hlsl -Fo ./../atmosphereComputeTransmittance_cs.dxil -Zi -Fd ./../atmosphereComputeTransmittance_cs.pdb
dxc.exe -T cs_6_2 -E main atmosphereComputeDirectIrradiance_cs.hlsl -Fo ./../atmosphereComputeDirectIrradiance_cs.dxil -Zi -Fd ./../atmosphereComputeDirectIrradiance_cs.pdb
dxc.exe -T cs_6_2 -E main atmosphereComputeSingleScattering_cs.hlsl -Fo ./../atmosphereComputeSingleScattering_cs.dxil -Zi -Fd ./../atmosphereComputeSingleScattering_cs.pdb
dxc.exe -T cs_6_2 -E main atmosphereComputeScatteringDensity_cs.hlsl -Fo ./../atmosphereComputeScatteringDensity_cs.dxil -Zi -Fd ./../atmosphereComputeScatteringDensity_cs.pdb
dxc.exe -T cs_6_2 -E main atmosphereComputeIndirectIrradiance_cs.hlsl -Fo ./../atmosphereComputeIndirectIrradiance_cs.dxil -Zi -Fd ./../atmosphereComputeIndirectIrradiance_cs.pdb
dxc.exe -T cs_6_2 -E main atmosphereComputeMultipleScattering_cs.hlsl -Fo ./../atmosphereComputeMultipleScattering_cs.dxil -Zi -Fd ./../atmosphereComputeMultipleScattering_cs.pdb
dxc.exe -T cs_6_2 -E main fourierOpacityVolume_cs.hlsl -Fo ./../fourierOpacityVolume_cs.dxil -Zi -Fd ./../fourierOpacityVolume_cs.pdb
dxc.exe -T cs_6_2 -E main fourierOpacityBlur_cs.hlsl -Fo ./../fourierOpacityBlur_cs.dxil -Zi -Fd ./../fourierOpacityBlur_cs.pdb
dxc.exe -T cs_6_2 -E main gtao2_cs.hlsl -Fo ./../gtao2_cs.dxil -Zi -Fd ./../gtao2_cs.pdb
dxc.exe -T vs_6_2 -E main particles_vs.hlsl -Fo ./../particles_vs.dxil -Zi -Fd ./../particles_vs.pdb
dxc.exe -T ps_6_2 -E main particles_ps.hlsl -Fo ./../particles_ps.dxil -Zi -Fd ./../particles_ps.pdb
dxc.exe -T cs_6_2 -E main fourierOpacityParticle_cs.hlsl -Fo ./../fourierOpacityParticle_cs.dxil -Zi -Fd ./../fourierOpacityParticle_cs.pdb
dxc.exe -T cs_6_2 -E main swapChainCopy_cs.hlsl -Fo ./../swapChainCopy_cs.dxil -Zi -Fd ./../swapChainCopy_cs.pdb
dxc.exe -T cs_6_2 -E main volumetricRaymarch_cs.hlsl -Fo ./../volumetricRaymarch_cs.dxil -Zi -Fd ./../volumetricRaymarch_cs.pdb
dxc.exe -T cs_6_2 -E main depthDownsampleCBMinMax_cs.hlsl -Fo ./../depthDownsampleCBMinMax_cs.dxil -Zi -Fd ./../depthDownsampleCBMinMax_cs.pdb
dxc.exe -T ps_6_2 -E main applyIndirectLighting_ps.hlsl -Fo ./../applyIndirectLighting_ps.dxil -Zi -Fd ./../applyIndirectLighting_ps.pdb

dxc.exe -T vs_6_2 -E main visibilityBuffer_vs.hlsl -Fo ./../visibilityBuffer_vs.dxil -Zi -Fd ./../visibilityBuffer_vs.pdb
dxc.exe -T vs_6_2 -E main -D ALPHA_MASK_ENABLED=1 visibilityBuffer_vs.hlsl -Fo ./../visibilityBuffer_ALPHA_MASK_ENABLED_vs.dxil -Zi -Fd ./../visibilityBuffer_ALPHA_MASK_ENABLED_vs.pdb
dxc.exe -T ps_6_2 -E main visibilityBuffer_ps.hlsl -Fo ./../visibilityBuffer_ps.dxil -Zi -Fd ./../visibilityBuffer_ps.pdb
dxc.exe -T ps_6_2 -E main -D ALPHA_MASK_ENABLED=1 visibilityBuffer_ps.hlsl -Fo ./../visibilityBuffer_ALPHA_MASK_ENABLED_ps.dxil -Zi -Fd ./../visibilityBuffer_ALPHA_MASK_ENABLED_ps.pdb
dxc.exe -T ps_6_2 -E main shadeVisibilityBuffer_ps.hlsl -Fo ./../shadeVisibilityBuffer_ps.dxil -Zi -Fd ./../shadeVisibilityBuffer_ps.pdb

dxc.exe -T vs_6_2 -E main fourierOpacityVolume_vs.hlsl -Fo ./../fourierOpacityVolume_vs.dxil -Zi -Fd ./../fourierOpacityVolume_vs.pdb
dxc.exe -T ps_6_2 -E main fourierOpacityVolume_ps.hlsl -Fo ./../fourierOpacityVolume_ps.dxil -Zi -Fd ./../fourierOpacityVolume_ps.pdb
dxc.exe -T ps_6_2 -E main fourierOpacityGlobal_ps.hlsl -Fo ./../fourierOpacityGlobal_ps.dxil -Zi -Fd ./../fourierOpacityGlobal_ps.pdb

dxc.exe -T vs_6_2 -E main fourierOpacityParticleDirectional_vs.hlsl -Fo ./../fourierOpacityParticleDirectional_vs.dxil -Zi -Fd ./../fourierOpacityParticleDirectional_vs.pdb
dxc.exe -T ps_6_2 -E main fourierOpacityParticleDirectional_ps.hlsl -Fo ./../fourierOpacityParticleDirectional_ps.dxil -Zi -Fd ./../fourierOpacityParticleDirectional_ps.pdb
dxc.exe -T vs_6_2 -E main fourierOpacityVolumeDirectional_vs.hlsl -Fo ./../fourierOpacityVolumeDirectional_vs.dxil -Zi -Fd ./../fourierOpacityVolumeDirectional_vs.pdb
dxc.exe -T ps_6_2 -E main fourierOpacityVolumeDirectional_ps.hlsl -Fo ./../fourierOpacityVolumeDirectional_ps.dxil -Zi -Fd ./../fourierOpacityVolumeDirectional_ps.pdb

dxc.exe -T vs_6_2 -E main fourierOpacityDepth_vs.hlsl -Fo ./../fourierOpacityDepth_vs.dxil -Zi -Fd ./../fourierOpacityDepth_vs.pdb

dxc.exe -T vs_6_2 -E main debugDraw_vs.hlsl -Fo ./../debugDraw_vs.dxil -Zi -Fd ./../debugDraw_vs.pdb
dxc.exe -T ps_6_2 -E main debugDraw_ps.hlsl -Fo ./../debugDraw_ps.dxil -Zi -Fd ./../debugDraw_ps.pdb
dxc.exe -T vs_6_2 -E main billboard_vs.hlsl -Fo ./../billboard_vs.dxil -Zi -Fd ./../billboard_vs.pdb
dxc.exe -T ps_6_2 -E main billboard_ps.hlsl -Fo ./../billboard_ps.dxil -Zi -Fd ./../billboard_ps.pdb

pause