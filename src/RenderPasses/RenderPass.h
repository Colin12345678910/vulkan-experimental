#pragma once
#include "VkTypes.h"

//Forward declaration.
class RenderPipeline;

class RenderPass
{
public:
	RenderPass() {};
	virtual void Draw(VkCommandBuffer cmd, RenderPipeline* pipeline) = 0;
	virtual void OnCreate(RenderPipeline* pipeline) = 0;
	virtual void OnDestroy() = 0;
	virtual std::string GetName() = 0;
	virtual void OnImGUI() {};
	virtual void OnFrameBufferUpdate(RenderPipeline* pipeline) {};

	virtual ~RenderPass() {};

	bool isActive = true;
};