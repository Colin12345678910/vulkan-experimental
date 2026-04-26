#pragma once
#include "VkTypes.h"
#include "ShadowMap.h"

class RenderPipeline;

class RenderPass
{
public:
	RenderPass() {};
	virtual void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) = 0;
	virtual void OnCreate(RenderPipeline* pipeline) = 0;
	virtual void OnDestroy() = 0;
	virtual void OnImGUI() {};
	virtual void OnFrameBufferUpdate(RenderPipeline* pipeline) {};

	virtual ~RenderPass() {};

	bool isActive = true;
};
class BackgroundPass : public RenderPass
{
public:
	BackgroundPass() {};
	void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) override;
	void OnCreate(RenderPipeline* pipeline) override;
	void OnDestroy() override;
	void OnImGUI() override;

	VkPipelineLayout _computePipelineLayout;
};
class ShadowPass : public RenderPass
{
public:
	ShadowPass() {};
	void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) override;
	void OnCreate(RenderPipeline* pipeline) override;
	void OnDestroy() override;
	void OnImGUI() override;

	ShadowMap shadowMap;
};
class GeometryPass : public RenderPass
{
public:
	GeometryPass() {};
	void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) override;
	void OnCreate(RenderPipeline* pipeline) override;
	void OnDestroy() override;
};
class PostPass : public RenderPass
{
public:
	PostPass() {};
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

class RenderPipeline
{
public:
	std::unordered_map<std::string, AllocatedImage*> images;

	std::vector<std::unique_ptr<RenderPass>> renderPasses;
	
	void Draw(VkCommandBuffer cmd);
	void ImGUI();
	void Create();
	void UpdateFramebuffers();
	void Destroy();
};