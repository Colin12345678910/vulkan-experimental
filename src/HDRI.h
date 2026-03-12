/* HDRI.h
*	Colin D
*	Jan 2026
* This class will handle loading and storing HDRI images for use in the engine, as well as handling converting to radiance cubemaps.
*/

class VulkanEngine;
#pragma once
#include <VkTypes.h>
#include <stb_image.h>
class HDRI
{
public:
	VkSampler hdrSampler;
	VkSampler radianceSampler;
	AllocatedImage irradiance;
	AllocatedImage prefilteredEnvMap;
	AllocatedImage brdfLUT;
	bool LoadHDRI(const char* filepath, VulkanEngine* engine);
private:
	AllocatedImage hdrImage;
	bool GenerateRadianceCubemap(VulkanEngine* engine, const char* filepath);
	bool GeneratePrefilteredEnvMap(VulkanEngine* engine, const char* filepath);
	bool GenerateBRDFLUT(VulkanEngine* engine);
	bool WriteBinToDisk(AllocatedImage img, const char* filepath, bool isCubemap = false);
	AllocatedImage CreateImageFromDisk(const char* filepath, VulkanEngine* engine, bool mipmapped = false, bool cubed = false);
};
