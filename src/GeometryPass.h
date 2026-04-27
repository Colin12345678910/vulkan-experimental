#pragma once
#include "RenderPass.h"

class RenderPipeline;

class GeometryPass : public RenderPass
{
public:
	GeometryPass() {};
	void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) override;
	void OnCreate(RenderPipeline* pipeline) override;
	void OnDestroy() override;
};
