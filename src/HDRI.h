/* HDRI.h
*	Colin D
*	Jan 2026
* This class will handle loading and storing HDRI images for use in the engine, as well as handling converting to radiance cubemaps.
*/

class VulkanEngine;
#pragma once
#include <vk_types.h>
#include <stb_image.h>
class HDRI
{
public:
	AllocatedImage hdrImage;
	VkSampler hdrSampler;
	AllocatedImage radianceCubemap;
	VkSampler radianceSampler;

	bool LoadHDRI(const char* filepath, VulkanEngine* engine);
};
