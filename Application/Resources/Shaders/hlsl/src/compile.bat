dxc.exe -spirv -T cs_6_2 -E main volumetricFogVBuffer_cs.hlsl -Fo ./../volumetricFogVBuffer_cs.spv
dxc.exe -spirv -T cs_6_2 -E main volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_cs.spv
dxc.exe -spirv -T cs_6_2 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.spv
dxc.exe -spirv -T vs_6_2 -E main sky_vs.hlsl -Fo ./../sky_vs.spv
dxc.exe -spirv -T ps_6_2 -E main sky_ps.hlsl -Fo ./../sky_ps.spv

pause