#pragma once
#include "RenderPipeline.h"
#include "VkEngine.h"

void RenderPipeline::Draw(VkCommandBuffer cmd)
{
	for (auto& pass : renderPasses)
	{
        if(pass->isActive)
		    pass->Draw(cmd, this);
	}
}

void RenderPipeline::ImGUI()
{
    for (auto& pass : renderPasses)
    {
        pass->OnImGUI();
    }
    for (int i = 0; i < renderPasses.size(); i++)
    {
        ImGui::Checkbox(fmt::format("RenderPass {}", i).c_str(), &renderPasses[i]->isActive);
    }
}

void RenderPipeline::Create()
{
    auto& engine = VulkanEngine::Get();

	//Define and push renderpasses here
	std::unique_ptr<RenderPass> backgroundPass = std::make_unique<BackgroundPass>();
    std::unique_ptr<RenderPass> geometryPass = std::make_unique<GeometryPass>();
    std::unique_ptr<RenderPass> shadowPass = std::make_unique<ShadowPass>();

	renderPasses.push_back(std::move(backgroundPass));
    renderPasses.push_back(std::move(shadowPass));
    renderPasses.push_back(std::move(geometryPass));

	//Create any resources needed explicitly for this pass.
	for (auto& pass : renderPasses)
	{
        pass->OnCreate(this);
	}
}

void RenderPipeline::Destroy()
{
	for (auto& pass : renderPasses)
	{
		pass->OnDestroy();
	}
}

void BackgroundPass::Draw(VkCommandBuffer cmd, RenderPipeline* renderPipeline)
{
	auto& engine = VulkanEngine::Get();

	ComputeEffect& effect = engine.backgroundEffects[0];
	effect.data.viewProj = glm::inverse(engine._sceneData.view);//glm::inverse(_camera.getRotation());
	effect.data.invProj = glm::inverse(engine._sceneData.proj);

	//effect.data.cameraPos = engine._camera.getPosition();
	effect.data.data4 = engine._sceneData.time;
	effect.data.screenDimensions = glm::ivec4(engine._drawExtent.width, engine._drawExtent.height, 0, 0);

	//Bind the gradientPipeline
	vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);

	//Bind the descriptorSet
	vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _computePipelineLayout, 0, 1, &engine._drawImageDescriptors, 0, nullptr);

	vkCmdPushConstants(cmd, _computePipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputePushConstants), &effect.data);

	//Exec w workgroups of 16
	vkCmdDispatch(cmd, std::ceil(engine._drawExtent.width / 16.0), std::ceil(engine._drawExtent.height / 16.0), 1);
}

void BackgroundPass::OnCreate(RenderPipeline* pipeline)
{
    auto& engine = VulkanEngine::Get();

    VkPipelineLayoutCreateInfo computeLayout{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = nullptr };
    //Define the descriptor used for this pipeline.
    computeLayout.setLayoutCount = 1;
    computeLayout.pSetLayouts = &engine._drawImageDescriptorLayout;

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(ComputePushConstants);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstants;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(engine._device, &computeLayout, nullptr, &_computePipelineLayout));

    //Create Shaader
    VkShaderModule skyShader;
    VkShaderModule advSkyShader;
    if (!vkutil::LoadShaderModule("../shaders/sky.comp.spv", engine._device, &skyShader))
    {
        fmt::println("Error when building a compute shader");
    }
    if (!vkutil::LoadShaderModule("../shaders/sky.slang.spv", engine._device, &advSkyShader))
    {
        fmt::println("Error when building a compute shader");
    }

    VkPipelineShaderStageCreateInfo stageInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr };
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.pNext = nullptr;
    stageInfo.module = skyShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = nullptr };
    computePipelineCreateInfo.layout = _computePipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    ComputeEffect sky;
    sky.layout = _computePipelineLayout;
    sky.name = "sky";
    sky.data = {};

    sky.data.data1 = glm::vec4(0.1, 0.2, 0.4, 0.97);

    VK_CHECK(vkCreateComputePipelines(engine._device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &sky.pipeline));

    //Change module to create one
    computePipelineCreateInfo.stage.module = advSkyShader;

    ComputeEffect advSky;
    advSky.layout = _computePipelineLayout;
    advSky.name = "advSky";
    advSky.data = {};

    //Default params
    advSky.data.data1 = glm::vec4(0.2, 0.2, 0.9, 0.97);
    advSky.data.data2 = glm::vec4(0.002, 0.2, 1, 0.5);
    advSky.data.data3 = glm::vec4(5, 0.002, 0, 0);

    VK_CHECK(vkCreateComputePipelines(engine._device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &advSky.pipeline));

    engine.backgroundEffects.emplace_back(advSky);
    engine.backgroundEffects.emplace_back(sky);

    vkDestroyShaderModule(engine._device, skyShader, nullptr);
    vkDestroyShaderModule(engine._device, advSkyShader, nullptr);
    auto device = engine._device;

    engine._mainDeletionQueue.Push([=]()
    {
        vkDestroyPipelineLayout(device, _computePipelineLayout, nullptr);
        vkDestroyPipeline(device, advSky.pipeline, nullptr);
        vkDestroyPipeline(device, sky.pipeline, nullptr);
    });
}

void BackgroundPass::OnDestroy()
{
}

void BackgroundPass::OnImGUI()
{
    auto& engine = VulkanEngine::Get();

    ComputeEffect& selected = engine.backgroundEffects[engine.currentBackground];

    ImGui::Text("Selected effect: ", selected.name);

    ImGui::SliderInt("Effect Index", &engine.currentBackground, 0, engine.backgroundEffects.size() - 1);

    ImGui::InputFloat4("data1", (float*)&selected.data.data1);
    ImGui::InputFloat4("data2", (float*)&selected.data.data2);
    ImGui::InputFloat4("data3", (float*)&selected.data.data3);
    ImGui::InputFloat4("data4", (float*)&selected.data.data4);
}

void GeometryPass::Draw(VkCommandBuffer cmd, RenderPipeline* pipeline)
{
    auto& engine = VulkanEngine::Get();

    auto start = std::chrono::system_clock::now();

    //Transition to colorAtt optimal bc geometry cannot draw on General.
    vkutil::TransitionImage(cmd, engine._drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
   
    auto gpuSceneDataBuf = engine.SetupGeometry(cmd);

    VkDescriptorSet globalDescriptor;
    VKDescriptors::DescriptorWriter writer;

    //Create a descriptorSet that binds the global GPU dataBuffer
    globalDescriptor = engine.globalDescriptiorAllocator.Allocate(engine._device, engine._gpuSceneDataDescriptorLayout);

    writer
        .WriteBuffer(0, gpuSceneDataBuf.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .WriteImage(1, pipeline->images["vk::shadow"].imageView, engine.GetDefaultSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        .UpdateSet(engine._device, globalDescriptor);

    engine.DrawGeometry(cmd, globalDescriptor);

    vkutil::TransitionImage(cmd, engine._drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL);

    auto end = std::chrono::system_clock::now();

    auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(end - start);

    engine.stats.meshDrawTime = elapsed.count() / 1000.0f;
}

void GeometryPass::OnCreate(RenderPipeline* pipeline)
{
}

void GeometryPass::OnDestroy()
{
}

void ShadowPass::Draw(VkCommandBuffer cmd, RenderPipeline* pipeline)
{
    auto& engine = VulkanEngine::Get();
    /*
    * Yea, I don't know how I didn't realize that I could just sneak the
    * shadowpass rendering into the main queue, there is literally zero
    * reason for the CPU to care about the status of the shadowpass anyway.
    *
    * So enjoy this hilariously simplified shadowcasting code.
    * Extra performance for no cost is always a win
    */

    //Transition the shadowmap for the next frame.
    vkutil::TransitionImage(cmd, pipeline->images["vk::shadow"].image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    auto globalDescriptor = engine.SetupShadows(cmd);
    shadowMap.Draw(cmd, globalDescriptor);

    vkutil::TransitionImage(cmd, pipeline->images["vk::shadow"].image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void ShadowPass::OnCreate(RenderPipeline* pipeline)
{
    // Load Shadowmaps
    shadowMap.Init();
    pipeline->images["vk::shadow"] = shadowMap.depthImage;
}

void ShadowPass::OnDestroy()
{
}

void ShadowPass::OnImGUI()
{
    ImGui::Begin("Camera");
    shadowMap.TransferMapToR32(VulkanEngine::Get());
    ImGui::End();
}
