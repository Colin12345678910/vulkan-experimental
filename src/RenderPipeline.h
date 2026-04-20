#pragma once
#include "VkTypes.h"

class RenderPass
{
public:
	RenderPass() {};
	virtual void Draw(VkCommandBuffer cmd) = 0;
	virtual void OnCreate() = 0;
	virtual void OnDestroy() = 0;
	virtual ~RenderPass() {};
};
class BackgroundPass : public RenderPass
{
public:
	BackgroundPass() {};
	void Draw(VkCommandBuffer cmd) override;
	void OnCreate() override;
	void OnDestroy() override;

	VkPipelineLayout _computePipelineLayout;
};
class GeometryPass : public RenderPass
{
public:
	GeometryPass() {};
	void Draw(VkCommandBuffer cmd) override;
	void OnCreate() override;
	void OnDestroy() override;
};
class RenderPipeline
{
public:
	std::unordered_map<std::string, AllocatedImage> images;

	std::vector<std::unique_ptr<RenderPass>> renderPasses;
	
	void Draw(VkCommandBuffer cmd);
	void Create();
	void Destroy();
};