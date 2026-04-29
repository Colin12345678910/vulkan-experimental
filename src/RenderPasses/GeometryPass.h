#pragma once
#include "RenderPass.h"

class RenderPipeline;

class GeometryPass : public RenderPass
{
public:
	GeometryPass() {};
	std::string GetName() override { return "Geometry Pass"; }
	void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) override;
	void OnCreate(RenderPipeline* pipeline) override;
	void OnDestroy() override;
};
