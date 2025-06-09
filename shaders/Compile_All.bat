for /r %%i in (*.comp) do C:\VulkanSDK\1.4.309.0\Bin\glslc.exe %%i -o %%i.spv
for /r %%i in (*.vert) do C:\VulkanSDK\1.4.309.0\Bin\glslc.exe %%i -o %%i.spv
for /r %%i in (*.frag) do C:\VulkanSDK\1.4.309.0\Bin\glslc.exe %%i -o %%i.spv
pause