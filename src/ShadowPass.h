#pragma once
#include "RenderPass.h"
#include "ShadowMap.h"

class RenderPipeline;

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