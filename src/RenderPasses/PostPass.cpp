#include "PostPass.h"
#include "VkEngine.h"

AutoFloatCVar CVAR_kawase_samplePosMult("kawase.samplePosMult", "The multiplier of the offset used in the kawase filter", 1.0f, 10.0f);
AutoIntCVar CVAR_operations("kawase.operations", "Number of 'ping pongs' that the filter will perform", 4);

void PostPass::Draw(VkCommandBuffer cmd, RenderPipeline* renderPipeline)
{
    auto& engine = VulkanEngine::Get();

    pushConstant constants{};
    constants.pixelOffset = 0;
    constants.samplePosMult = CVAR_kawase_samplePosMult.Get();
    constants.screenSize = glm::ivec2(engine._windowExtent.width - 1, engine._windowExtent.height - 1);

    //Bind the gradientPipeline
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, postPipeline);

    //Testing if shader works in general
    for (int i = 0; i < CVAR_operations.Get() * 2; i++)
    {
        if (i % 2 == 0)
        {
            //Bind the descriptorSet
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _postPipelineLayout, 0, 1, &_postDescriptorPing, 0, nullptr);
        }
        else
        {
            //Bind the descriptorSet
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, _postPipelineLayout, 0, 1, &_postDescriptorPong, 0, nullptr);
        }

        constants.pixelOffset = i;

        vkCmdPushConstants(cmd, _postPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushConstant), &constants);

        //Exec w workgroups of 16
        vkCmdDispatch(cmd, std::ceil(engine._drawExtent.width / 16.0), std::ceil(engine._drawExtent.height / 16.0), 1);
    }
}

void PostPass::OnCreate(RenderPipeline* pipeline)
{
    auto& engine = VulkanEngine::Get();
    auto device = engine._device;
    {
        VKDescriptors::DescriptorLayoutBuilder builder;
        _postLayout = builder
            .AddBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .AddBinding(1, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .Build(engine._device, VK_SHADER_STAGE_COMPUTE_BIT);
    }
    //Alloc a descriiptorset for our draw image
    _postDescriptorPing = engine.globalDescriptiorAllocator.Allocate(engine._device, _postLayout);
    _postDescriptorPong = engine.globalDescriptiorAllocator.Allocate(engine._device, _postLayout);

    WriteDescriptors(pipeline, "vk::draw", "vk::intermediate::0");

    VkPipelineLayoutCreateInfo computeLayout{ .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO, .pNext = nullptr };
    //Define the descriptor used for this pipeline.
    computeLayout.setLayoutCount = 1;
    computeLayout.pSetLayouts = &_postLayout;

    VkPushConstantRange pushConstants{};
    pushConstants.offset = 0;
    pushConstants.size = sizeof(pushConstant);
    pushConstants.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    computeLayout.pPushConstantRanges = &pushConstants;
    computeLayout.pushConstantRangeCount = 1;

    VK_CHECK(vkCreatePipelineLayout(device, &computeLayout, nullptr, &_postPipelineLayout));

    //Create Shaader
    VkShaderModule postShader;
    if (!vkutil::LoadShaderModule("../shaders/post.slang.spv", device, &postShader))
    {
        fmt::println("Error when building a compute shader");
    }

    VkPipelineShaderStageCreateInfo stageInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO, .pNext = nullptr };
    stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
    stageInfo.pNext = nullptr;
    stageInfo.module = postShader;
    stageInfo.pName = "main";

    VkComputePipelineCreateInfo computePipelineCreateInfo{ .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO, .pNext = nullptr };
    computePipelineCreateInfo.layout = _postPipelineLayout;
    computePipelineCreateInfo.stage = stageInfo;

    VK_CHECK(vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &computePipelineCreateInfo, nullptr, &postPipeline));

    vkDestroyShaderModule(device, postShader, nullptr);

    //automatically set the filter to off by default
    isActive = false;

    engine._mainDeletionQueue.Push([=]()
    {
        vkDestroyPipelineLayout(device, _postPipelineLayout, nullptr);
        vkDestroyPipeline(device, postPipeline, nullptr);
    });
}

void PostPass::OnDestroy()
{
    auto& engine = VulkanEngine::Get();
    vkDestroyDescriptorSetLayout(engine._device, _postLayout, nullptr);
}

void PostPass::OnFrameBufferUpdate(RenderPipeline* pipeline)
{
    WriteDescriptors(pipeline, "vk::draw", "vk::intermediate::0");
}

void PostPass::WriteDescriptors(RenderPipeline* pipeline, std::string src, std::string dst)
{
    auto& engine = VulkanEngine::Get();

    {
        VKDescriptors::DescriptorWriter writer;
        writer
            .WriteImage(0, pipeline->images[src]->imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .WriteImage(1, pipeline->images[dst]->imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .UpdateSet(engine._device, _postDescriptorPing);
    }
    
    {
        VKDescriptors::DescriptorWriter writer;
        writer
            .WriteImage(0, pipeline->images[dst]->imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .WriteImage(1, pipeline->images[src]->imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE)
            .UpdateSet(engine._device, _postDescriptorPong);
    }
}
