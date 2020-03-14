dxc.exe -spirv -T cs_6_2 -E main volumetricFogIntegrate_cs.hlsl -Fo ./../volumetricFogIntegrate_cs.spv
dxc.exe -spirv -T cs_6_2 -E main volumetricFogScatter_cs.hlsl -Fo ./../volumetricFogScatter_cs.spv

pause