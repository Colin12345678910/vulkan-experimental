#include "HDRI.h"
#include "vk_engine.h"
#include <stb_image_write.h>

bool HDRI::LoadHDRI(const char* filepath, VulkanEngine* engine)
{
	int w, h, n;
	float* data = stbi_loadf(filepath, &w, &h, &n, 4);

	hdrImage = engine->CreateImage(data, VkExtent3D{ (uint32_t)w, (uint32_t)h, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false, "HDRI::hdrImage");
	AllocatedImage irradiance = engine->CreateCubeImage(VkExtent3D{ 512, 512, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, false, "HDRI::irradiance");

    engine->_mainDeletionQueue.Push([=]() {
        engine->DestroyImage(hdrImage);
		engine->DestroyImage(irradiance);
    });

	VkShaderModule computeShader;
	if (!vkutil::LoadShaderModule("../../shaders/convolution.comp.spv", engine->_device, &computeShader))
	{
		fmt::println("Error when building a compute shader for HDRI convolution");
	}

	VKDescriptors::DescriptorLayoutBuilder builder;
	auto descriptorLayout = builder
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) //Input HDRI
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) //Output cubemap
		.Build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkDescriptorSet descriptorSet = engine->globalDescriptiorAllocator.Allocate(engine->_device, descriptorLayout);

	VkPipelineLayoutCreateInfo computeLayoutInfo = vkinit::pipeline_layout_create_info();
	computeLayoutInfo.setLayoutCount = 1;
	computeLayoutInfo.pSetLayouts = &descriptorLayout;

	VkPipelineLayout computeLayout; 
	vkCreatePipelineLayout(engine->_device, &computeLayoutInfo, nullptr, &computeLayout);

	VkPipelineShaderStageCreateInfo stageInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr };
	stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
	stageInfo.module = computeShader;
	stageInfo.pName = "main";

	VkComputePipelineCreateInfo computePipelineCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = nullptr };
	computePipelineCreateInfo.layout = computeLayout;
	computePipelineCreateInfo.stage = stageInfo;

	VkPipeline computePipeline;
	vkCreateComputePipelines(engine->_device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &computePipeline);

	auto cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	VKDescriptors::DescriptorWriter writer;
	writer.WriteImage(0, hdrImage.imageView, engine->GetDefaultSampler(true), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		.WriteImage(1, irradiance.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.UpdateSet(engine->_device, descriptorSet);

	engine->ImmediateSubmit([=](VkCommandBuffer cmd) {
		vkutil::TransitionImage(cmd, irradiance.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdDispatch(cmd, (uint32_t)std::ceil(512 / 8.0f), (uint32_t)std::ceil(512 / 8.0f), 6);
	});
	

	engine->GetCurrentFrame()._deletionQueue.Push([=]() {
		vkDestroyPipelineLayout(engine->_device, computeLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, descriptorLayout, nullptr);
		vkDestroyPipeline(engine->_device, computePipeline, nullptr);
	});

	fmt::println("Submitting HDRI convolution compute shader...");
	vkDestroyShaderModule(engine->_device, computeShader, nullptr);
	stbi_image_free(data);
	return false;
}
