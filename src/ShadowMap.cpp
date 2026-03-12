#include "ShadowMap.h"
#include "vk_engine.h"

void ShadowMap::CreateShadowMap(VulkanEngine& engine, uint32_t width, uint32_t height)
{
    depthImage = engine.CreateImage(
        { width, height, 1 },
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        1,
		"ShadowMap::depthImage");

    shadowTexIMGUI = ImGui_ImplVulkan_AddTexture(engine.GetDefaultSampler(), depthImage.imageView, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

	engine._mainDeletionQueue.Push(ClearResources(engine));
}

void ShadowMap::TransferMapToR32(VulkanEngine& engine)
{
	ImGui::Image((ImTextureID)shadowTexIMGUI, ImVec2(256, 256));
}

std::function<void()> ShadowMap::ClearResources(VulkanEngine& engine)
{
    return [&]()
    {
		engine.DestroyImage(depthImage);
    };
}

void ShadowMap::Draw(VkCommandBuffer cmd, VkDescriptorSet globalDescriptor, VKDescriptors::DescriptorWriter writer, AllocatedBuffer gpuSceneDataBuf)
{
	VulkanEngine& engine = VulkanEngine::Get();

    VkExtent2D imgExtent;
    imgExtent.width = depthImage.imageExtent.width;
    imgExtent.height = depthImage.imageExtent.height;

    writer
        .WriteBuffer(0, gpuSceneDataBuf.buffer, sizeof(GPUSceneData), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        .UpdateSet(engine._device, globalDescriptor);

    //Begin a render pass to our drawImage
    VkRenderingAttachmentInfo colorAttachment = vkinit::attachment_info(nullptr, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
    VkRenderingAttachmentInfo depthAttachmentInfo = vkinit::depth_attachment_info(depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    VkRenderingInfo renderInfo = vkinit::rendering_info(imgExtent, &colorAttachment, &depthAttachmentInfo);
    renderInfo.colorAttachmentCount = 0;

    vkCmdBeginRendering(cmd, &renderInfo);

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, engine._meshPipeline);

    VkViewport viewport{};
    viewport.width = imgExtent.width;
    viewport.height = imgExtent.height;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor = {};
    scissor.extent.width = imgExtent.width;
    scissor.extent.height = imgExtent.height;

    vkCmdSetScissor(cmd, 0, 1, &scissor);

    engine.FlushDrawCtx(cmd, globalDescriptor, engine._meshPipeline, engine._meshPipelineLayout);

    vkCmdEndRendering(cmd);
}

void ShadowMap::Init()
{
    CreateShadowMap(VulkanEngine::Get(), 8192, 8192);
}


