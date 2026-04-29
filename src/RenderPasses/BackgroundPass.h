#pragma once
#include "RenderPass.h"

class RenderPipeline;

class BackgroundPass : public RenderPass
{
public:
	BackgroundPass() {};
	std::string GetName() override { return "Background Pass"; }
	void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) override;
	void OnCreate(RenderPipeline* pipeline) override;
	void OnDestroy() override;
	void OnImGUI() override;

	VkPipelineLayout _computePipelineLayout;
};