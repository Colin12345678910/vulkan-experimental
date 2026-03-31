for /r %%i in (*.comp) do C:\VulkanSDK\1.4.309.0\Bin\glslc.exe %%i -O --target-env=vulkan1.3 -o %%i.spv
for /r %%i in (*.vert) do C:\VulkanSDK\1.4.309.0\Bin\glslc.exe %%i -O --target-env=vulkan1.3 -o %%i.spv
for /r %%i in (*.frag) do C:\VulkanSDK\1.4.309.0\Bin\glslc.exe %%i -O --target-env=vulkan1.3 -o %%i.spv
for /r %%i in (*.slang) do E:\Slang\bin\slangc %%i -target spirv -o %%i.spv
pause