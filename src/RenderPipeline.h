#pragma once
#include "RenderPass.h"
#include "VkTypes.h"
#include "PostPass.h"
#include "ShadowPass.h"
#include "GeometryPass.h"
#include "BackgroundPass.h"

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