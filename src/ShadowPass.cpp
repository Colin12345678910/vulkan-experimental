#include "ShadowPass.h"
#include "VkEngine.h"

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
    vkutil::TransitionImage(cmd, pipeline->images["vk::shadow"]->image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

    auto globalDescriptor = engine.SetupShadows(cmd);
    shadowMap.Draw(cmd, globalDescriptor);

    vkutil::TransitionImage(cmd, pipeline->images["vk::shadow"]->image, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_ASPECT_DEPTH_BIT);
}

void ShadowPass::OnCreate(RenderPipeline* pipeline)
{
    // Load Shadowmaps
    shadowMap.Init();
    pipeline->images["vk::shadow"] = &shadowMap.depthImage;
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

