#include "BackgroundPass.h"
#include "VkEngine.h"

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

    ImGui::InputFloat4("data1", (float*)&selected.data.data1);
    ImGui::InputFloat4("data2", (float*)&selected.data.data2);
    ImGui::InputFloat4("data3", (float*)&selected.data.data3);
    ImGui::InputFloat4("data4", (float*)&selected.data.data4);
}