dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main forward_vs.hlsl -Fo ./../forward_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main forward_ps.hlsl -Fo ./../forward_ps.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main luminanceHistogram_cs.hlsl -Fo ./../luminanceHistogram_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main exposure_cs.hlsl -Fo ./../exposure_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main deferredShadows_cs.hlsl -Fo ./../deferredShadows_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogVBuffer_cs.hlsl -Fo ./../volumetricFogVBuffer_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T cs_6_2 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T vs_6_2 -E main sky_vs.hlsl -Fo ./../sky_vs.spv
dxc.exe -spirv -fspv-target-env=vulkan1.1 -T ps_6_2 -E main sky_ps.hlsl -Fo ./../sky_ps.spv

pause