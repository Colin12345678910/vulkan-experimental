#pragma once
#include "RenderPass.h"

class VulkanEngine;

class PostPass : public RenderPass
{
public:
	PostPass() {};
	std::string GetName() override { return "Kawase Pass"; }
	void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) override;
	void OnCreate(RenderPipeline* pipeline) override;
	void OnDestroy() override;
	void OnFrameBufferUpdate(RenderPipeline* pipeline) override;

	void WriteDescriptors(RenderPipeline* pipeline, std::string src, std::string dst);

	struct pushConstant
	{
		float pixelOffset;
		float samplePosMult;
		float strength;
		float padding;
	};

	VkDescriptorSetLayout _postLayout;
	VkDescriptorSet _postDescriptorPing;
	VkDescriptorSet _postDescriptorPong;
	VkPipelineLayout _postPipelineLayout;
	VkPipeline postPipeline;
};