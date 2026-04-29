#include "GeometryPass.h"
#include "VkEngine.h"

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
        .WriteImage(1, pipeline->images["vk::shadow"]->imageView, engine.GetDefaultSampler(), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
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

