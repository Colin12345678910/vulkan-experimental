#pragma once
// A Very basic shadow map implementation/
#include <VkTypes.h>
#include <IRenderpass.h>
class VulkanEngine;

class ShadowMap : public IGeometryPass
{
public:
	void CreateShadowMap(VulkanEngine& engine, uint32_t width, uint32_t height);
	void TransferMapToR32(VulkanEngine& engine);

	void Draw(VkCommandBuffer cmd, VkDescriptorSet globalDescriptor) override;
	void Init() override;

	AllocatedImage depthImage;
	VkDescriptorSet shadowTexIMGUI;
private:
	std::function<void()> ClearResources(VulkanEngine& engine);
};