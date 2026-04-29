#pragma once
#include "RenderPasses/RenderPass.h"
#include "VkTypes.h"
#include "RenderPasses/PostPass.h"
#include "RenderPasses/ShadowPass.h"
#include "RenderPasses/GeometryPass.h"
#include "RenderPasses/BackgroundPass.h"

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