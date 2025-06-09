#pragma once
#include <vk_types.h>
#include <vk_descriptors.h>
#include <vk_pipelines.h>
/* GLTFMetalicRoughness
*	Colin D.
*	June 2025.
*	This set of code handles creating materials that are compatible with the GLTF (PBR?)
*	material file format. The idea is we should be able to load a GLTF file and render
*	it correctly by """binding""" this material during the render process. WIP
*/
//Forward declaration.
class VulkanEngine;
struct GLTFMetallicRoughness
{
	MaterialPipeline opaquePipeline;
	MaterialPipeline transparentPipeline;

	VkDescriptorSetLayout materialLayout;

	struct MaterialConstants
	{
		glm::vec4 colorFactors;
		glm::vec4 metalRoughFactors;
		//Padding
		glm::vec4 extra[14];
	};

	struct MaterialResources
	{
		AllocatedImage colorImage;
		VkSampler colorSampler;
		AllocatedImage metalRoughImage;
		VkBuffer dataBuffer;
		uint32_t dataBufferOffset;
	};
	
	VKDescriptors::DescriptorWriter writer;



	void BuildPipelines(VulkanEngine* engine);
	void ClearResources(VkDevice device);
	void DeletePipeline(VkDevice device, MaterialPipeline pipeline);

	MaterialInstance WriteMaterial(VkDevice device, MaterialPass pass, const MaterialResources& resources, VKDescriptors::DescriptorAllocatorGrowable& descriptorAllocator);
};

