#include "HDRI.h"
#include "vk_engine.h"
#include <stb_image_write.h>

bool HDRI::LoadHDRI(const char* filepath, VulkanEngine* engine)
{
	// TODO: Check for existing cached files
	/*
	if (std::filesystem::exists("irradiance.bin") &&
		std::filesystem::exists("prefilteredEnvMap.bin") &&
		std::filesystem::exists("brdfLUT.bin"))
	{
		irradiance = CreateImageFromDisk("irradiance.bin", engine, false, true);
		brdfLUT = CreateImageFromDisk("brdfLUT.bin", engine);
		prefilteredEnvMap = CreateImageFromDisk("prefilteredEnvMap.bin", engine, true, true);
		return true;
	};
	*/ // I'm stuck here mostly bc I never expected to need cubemaps cached to disk, and didn't implement it, definitely kicking myself now. But this gives me the excuse to actually rewrite how textures are handled so, yeah.

	int w, h, n;
	float* data = stbi_loadf(filepath, &w, &h, &n, 4);
	hdrImage = engine->CreateImage(data, VkExtent3D{ (uint32_t)w, (uint32_t)h, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, false, "HDRI::hdrImage");

	engine->_mainDeletionQueue.Push([=]() {
		engine->DestroyImage(hdrImage);
	});
	stbi_image_free(data);

	GenerateRadianceCubemap(engine, filepath);
	GeneratePrefilteredEnvMap(engine, filepath);
	GenerateBRDFLUT(engine);

	WriteBinToDisk(irradiance, "irradiance.bin");
	WriteBinToDisk(prefilteredEnvMap, "prefilteredEnvMap.bin");
	WriteBinToDisk(brdfLUT, "brdfLUT.bin");
	return true;
}

/// <summary>
/// Converts an equirectangular HDRI to a radiance cubemap using a compute shader.
/// </summary>
/// <param name="engine">Instance of VKEngine</param>
/// <param name="filepath">filepath of the HDRI</param>
/// <returns></returns>
bool HDRI::GenerateRadianceCubemap(VulkanEngine* engine, const char* filepath)
{
	const int SIZE = 512;

	//Create the cubemap image, R32 format as this is HDR data.
	irradiance = engine->CreateCubeImage(VkExtent3D{ SIZE, SIZE, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, false, "HDRI::irradiance");

	engine->_mainDeletionQueue.Push([=]() {
		engine->DestroyImage(irradiance);
	});

	VkShaderModule computeShader;
	if (!vkutil::LoadShaderModule("../../shaders/convolution.comp.spv", engine->_device, &computeShader))
	{
		fmt::println("Error when building a compute shader for HDRI convolution");
	}

	// Bind both images to the compute shader
	VKDescriptors::DescriptorLayoutBuilder builder;
	auto descriptorLayout = builder
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) //Input HDRI
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) //Output cubemap
		.Build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkDescriptorSet descriptorSet = engine->globalDescriptiorAllocator.Allocate(engine->_device, descriptorLayout);

	// Create the compute shader & pipeline
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
	
	// Begin command buffer for compute work
	auto cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	// Assign images to the descriptor set, then dispatch the compute shader
	VKDescriptors::DescriptorWriter writer;
	writer.WriteImage(0, hdrImage.imageView, engine->GetDefaultSampler(true), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
		.WriteImage(1, irradiance.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.UpdateSet(engine->_device, descriptorSet);

	engine->ImmediateSubmit([=](VkCommandBuffer cmd) {
		vkutil::TransitionImage(cmd, irradiance.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdDispatch(cmd, (uint32_t)std::ceil(SIZE / 8.0f), (uint32_t)std::ceil(SIZE / 8.0f), 6);
		vkutil::TransitionImage(cmd, irradiance.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});

	// Queue all of our resources to be deleted later.
	engine->GetCurrentFrame()._deletionQueue.Push([=]() {
		vkDestroyPipelineLayout(engine->_device, computeLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, descriptorLayout, nullptr);
		vkDestroyPipeline(engine->_device, computePipeline, nullptr);
	});

	fmt::println("Submitting HDRI convolution compute shader...");
	vkDestroyShaderModule(engine->_device, computeShader, nullptr);
	
	return true;
}

bool HDRI::GeneratePrefilteredEnvMap(VulkanEngine* engine, const char* filepath)
{
	const int SIZE = 512;

	prefilteredEnvMap = engine->CreateCubeImage(VkExtent3D{ SIZE, SIZE, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, true, "HDRI::prefilteredEnvMap");

	engine->_mainDeletionQueue.Push([=]() {
		engine->DestroyImage(prefilteredEnvMap);
	});

	VkShaderModule computeShader;
	if (!vkutil::LoadShaderModule("../../shaders/prefilter.comp.spv", engine->_device, &computeShader))
	{
		fmt::println("Error when building a compute shader for HDRI convolution");
	}

	// Bind our HDRI input and prefiltered output images to the compute shader and then allocate.
	VKDescriptors::DescriptorLayoutBuilder builder;
	auto descriptorLayout = builder
		.AddBinding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) //Input HDRI
		.AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) //Output cubemap
		.AddBinding(2, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) //Push constants
		.Build(engine->_device, VK_SHADER_STAGE_COMPUTE_BIT);

	VkDescriptorSet descriptorSet = engine->globalDescriptiorAllocator.Allocate(engine->_device, descriptorLayout);

#pragma region ComputePipelineSetup
	// Create the compute shader & pipeline ----------------------------
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
	// -----------------------------------------------------------------
#pragma endregion

	auto cmdBufInfo = vkinit::command_buffer_begin_info(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);

	engine->ImmediateSubmit([=](VkCommandBuffer cmd) {
		vkutil::TransitionImage(cmd, prefilteredEnvMap.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
	});

	struct PushData // Uniform buffer data for simplicity, this is one-time use only.
	{
		float roughness;
		int mipLevel;
	} *pushData;

	AllocatedBuffer buff = engine->CreateBuffer(sizeof(PushData), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);
	vmaMapMemory(engine->_allocator, buff.allocation, (void**)&pushData);

	pushData->roughness = 0;
	pushData->mipLevel = 0;

	// For each mip level, dispatch the compute shader with the appropriate roughness value.
	for (int i = 0; i < prefilteredEnvMap.mipMapViews.size(); i++)
	{
		fmt::println("Submitting HDRI prefilter mip level {} compute shader...", i);

		engine->ImmediateSubmit([=](VkCommandBuffer cmd) {
			int currSize = SIZE;

			pushData->roughness = (float)i / (float)(prefilteredEnvMap.mipMapViews.size() - 1);

			VKDescriptors::DescriptorWriter writer;
			writer.WriteImage(0, hdrImage.imageView, engine->GetDefaultSampler(true), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
				.WriteImage(1, prefilteredEnvMap.mipMapViews[i], VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
				.WriteBuffer(2, buff.buffer, sizeof(PushData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
				.UpdateSet(engine->_device, descriptorSet);

			vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
			vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &descriptorSet, 0, nullptr);
			vkCmdDispatch(cmd, (uint32_t)std::ceil(currSize / 8.0f), (uint32_t)std::ceil(currSize / 8.0f), 6);
			currSize /= 2;
		});
	}

	// Make a final transition to shader read optimal for sampling.
	engine->ImmediateSubmit([=](VkCommandBuffer cmd) {
		vkutil::TransitionImage(cmd, prefilteredEnvMap.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});
	
	// Queue all of our resources to be deleted later.
	engine->GetCurrentFrame()._deletionQueue.Push([=]() {
		vkDestroyPipelineLayout(engine->_device, computeLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, descriptorLayout, nullptr);
		vkDestroyPipeline(engine->_device, computePipeline, nullptr);
	});

	// Anything we can delete right away, do it.
	vmaUnmapMemory(engine->_allocator, buff.allocation);
	engine->DestroyBuffer(buff);

	fmt::println("Submitting HDRI prefilter compute shader...");
	vkDestroyShaderModule(engine->_device, computeShader, nullptr);
	return true;
}

bool HDRI::GenerateBRDFLUT(VulkanEngine* engine)
{
	const int SIZE = 512;

	brdfLUT = engine->CreateImage(VkExtent3D{ (uint32_t)SIZE, (uint32_t)SIZE, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
		VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, false, "HDRI::BRDFLUT");

	engine->_mainDeletionQueue.Push([=]() {
		engine->DestroyImage(brdfLUT);
	});

	// Create compute shader
	VkShaderModule computeShader;
	if (!vkutil::LoadShaderModule("../../shaders/BDRF.comp.spv", engine->_device, &computeShader))
	{
		fmt::println("Error when building a compute shader for HDRI convolution");
	}

	// Construct descriptor set layout & allocate
	VKDescriptors::DescriptorLayoutBuilder builder;
	auto descriptorLayout = builder
		.AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) //Output BRDF LUT
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

	// Assign output image to the descriptor set, then dispatch the compute shader
	VKDescriptors::DescriptorWriter writer;
	writer.WriteImage(0, brdfLUT.imageView, engine->GetDefaultSampler(true), VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
		.UpdateSet(engine->_device, descriptorSet);

	engine->ImmediateSubmit([=](VkCommandBuffer cmd) {
		vkutil::TransitionImage(cmd, brdfLUT.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);
		vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
		vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &descriptorSet, 0, nullptr);
		vkCmdDispatch(cmd, (uint32_t)std::ceil(SIZE / 8.0f), (uint32_t)std::ceil(SIZE / 8.0f), 6);
		vkutil::TransitionImage(cmd, brdfLUT.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});

	// Queue all of our resources to be deleted later.
	engine->GetCurrentFrame()._deletionQueue.Push([=]() {
		vkDestroyPipelineLayout(engine->_device, computeLayout, nullptr);
		vkDestroyDescriptorSetLayout(engine->_device, descriptorLayout, nullptr);
		vkDestroyPipeline(engine->_device, computePipeline, nullptr);
	});

	fmt::println("Submitting HDRI convolution compute shader...");
	vkDestroyShaderModule(engine->_device, computeShader, nullptr);
	return true;
}

bool HDRI::WriteHDRIToDisk(const char* filepath)
{
	WriteBinToDisk(irradiance, "irradiance.bin");
	WriteBinToDisk(brdfLUT, "brdfLUT.bin");
	WriteBinToDisk(prefilteredEnvMap, "prefilteredEnvMap.bin");

	return true;
}

bool HDRI::WriteBinToDisk(AllocatedImage img, const char* filepath)
{
	int offset = 0;

	auto buff = VulkanEngine::Get().CreateBuffer(img.imageExtent.width * img.imageExtent.height * 4 * sizeof(float),
		VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_AUTO_PREFER_HOST);

	VulkanEngine::Get().ImmediateSubmit([&](VkCommandBuffer cmd) {
		vkutil::TransitionImage(cmd, img.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
		vkutil::CopyImageToBuffer(cmd, img.image, buff.buffer, img.imageExtent);
		vkutil::TransitionImage(cmd, img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	});

	void* data;
	vmaMapMemory(VulkanEngine::Get()._allocator, buff.allocation, (void**)&data);

	std::ofstream file(filepath, std::ios::binary);

	int size = img.imageExtent.width * img.imageExtent.height * 4 * sizeof(float);

	file.write(reinterpret_cast<char*>(data), size);

	vmaUnmapMemory(VulkanEngine::Get()._allocator, buff.allocation);

	offset += size;

	for (int i = 1; i < img.mipMapViews.size(); i++)
	{
		auto extent = VkExtent3D{
			std::max(1u, img.imageExtent.width >> i),
			std::max(1u, img.imageExtent.height >> i),
			1
		};
		int size = extent.width * extent.height * 4 * sizeof(float);

		VulkanEngine::Get().ImmediateSubmit([&](VkCommandBuffer cmd) {
			vkutil::TransitionImage(cmd, img.image, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
			vkutil::CopyImageToBuffer(cmd, img.image, buff.buffer, extent);
			vkutil::TransitionImage(cmd, img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
		});

		void* data;

		vmaMapMemory(VulkanEngine::Get()._allocator, buff.allocation, (void**)&data);

		std::ofstream file(filepath, std::ios::binary);

		file.write(reinterpret_cast<char*>(data) + offset, offset + size);
		offset += size;

		vmaUnmapMemory(VulkanEngine::Get()._allocator, buff.allocation);
	}

	VulkanEngine::Get().DestroyBuffer(buff);
	
	return false;
}

//Unused for now.
AllocatedImage HDRI::CreateImageFromDisk(const char* filepath, VulkanEngine* engine, bool mipmapped, bool cubed)
{
	uint32_t imgSize = 512;
	uint32_t fileSize = imgSize * imgSize * 4 * sizeof(float);
	// Load the HDRI maps from disk if possible
	int size = std::filesystem::file_size(filepath);
	std::ifstream file(filepath, std::ios::binary);
	std::vector<char> buffer(size);
	file.read(buffer.data(), std::streamsize(size));

	AllocatedImage img;

	if (cubed)
	{
		img = engine->CreateCubeImage(VkExtent3D{ imgSize, imgSize, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped, filepath);
	}
	else
	{
		img = engine->CreateImage(buffer.data(), VkExtent3D{ imgSize, imgSize, 1 }, VK_FORMAT_R32G32B32A32_SFLOAT,
			VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT, mipmapped, filepath);
	}

	return img;
}
