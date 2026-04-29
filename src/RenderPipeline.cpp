#pragma once
#include "RenderPipeline.h"
#include "VkEngine.h"

/*
* RenderPipeline
* Apr. 24, 2026
* This is a whole bunch of abstractions to move most of the rendering logic outside the main engine class.
* Supporting enabling and disabling renderpasses, and easing the effort needed to define new render passes.
*/

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
        ImGui::Checkbox(fmt::format("RP: {}", renderPasses[i].get()->GetName()).c_str(), &renderPasses[i]->isActive);
    }
}

void RenderPipeline::Create()
{
    auto& engine = VulkanEngine::Get();

	//Define and push renderpasses here
	std::unique_ptr<RenderPass> backgroundPass = std::make_unique<BackgroundPass>();
    std::unique_ptr<RenderPass> geometryPass = std::make_unique<GeometryPass>();
    std::unique_ptr<RenderPass> shadowPass = std::make_unique<ShadowPass>();
    std::unique_ptr<RenderPass> postPass = std::make_unique<PostPass>();

	renderPasses.push_back(std::move(backgroundPass));
    renderPasses.push_back(std::move(shadowPass));
    renderPasses.push_back(std::move(geometryPass));
    renderPasses.push_back(std::move(postPass));

	//Create any resources needed explicitly for this pass.
	for (auto& pass : renderPasses)
	{
        pass->OnCreate(this);
	}
}

void RenderPipeline::UpdateFramebuffers()
{
    for (auto& pass : renderPasses)
	{
		pass->OnFrameBufferUpdate(this);
	}
}

void RenderPipeline::Destroy()
{
	for (auto& pass : renderPasses)
	{
		pass->OnDestroy();
	}
}