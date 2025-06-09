#pragma once
#include <vk_types.h>
/* VkDescriptors
*	Colin D
*	June 2025
*	A Set of abstractions surrounding the descriptorSetLayout system of Vulkan.
*	(General Descriptor overview)
*	Descriptors describe the general structure of memory being sent to the GPU, so
*	a descriptor would need to exist for an image or other structured buffers to
*	be accessible on the gpu, (In addition to copying the memory to the GPU)
*/
class VKDescriptors
{
public:
	struct DescriptorLayoutBuilder
	{
		std::vector<VkDescriptorSetLayoutBinding> bindings;

		DescriptorLayoutBuilder AddBinding(uint32_t binding, VkDescriptorType type);
		void Clear();

		VkDescriptorSetLayout Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0);
	};
	struct DescriptorAllocator
	{
		struct PoolSizeRatio
		{
			VkDescriptorType type;
			float ratio;
		};

		VkDescriptorPool pool;

		VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout);
		void InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios);
		void ClearDescriptors(VkDevice device);
		void DestroyPool(VkDevice device);
	};

	struct DescriptorAllocatorGrowable
	{
	public:
		struct PoolSizeRatio
		{
			VkDescriptorType type;
			float ratio;
		};
		void InitPool(VkDevice device, uint32_t initalSets, std::span<PoolSizeRatio> poolRatios);
		void ClearPools(VkDevice device);
		void DestroyPool(VkDevice device);

		VkDescriptorSet Allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr);
	private:
		VkDescriptorPool GetPool(VkDevice device);
		VkDescriptorPool CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios);

		std::vector<PoolSizeRatio> ratios;
		std::vector<VkDescriptorPool> fullPools;
		std::vector<VkDescriptorPool> readyPools;
		uint32_t setsPerPool;
	};
	struct DescriptorWriter
	{
		std::deque<VkDescriptorImageInfo> imageInfos;
		std::deque<VkDescriptorBufferInfo> buffInfos;
		std::vector<VkWriteDescriptorSet> writes;

		DescriptorWriter WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type);
		DescriptorWriter WriteBuffer(int binding, VkBuffer buff, size_t size, size_t offset, VkDescriptorType type);

		void Clear();
		void UpdateSet(VkDevice device, VkDescriptorSet set);
	};
};
