#include <vk_descriptors.h>

VKDescriptors::DescriptorLayoutBuilder VKDescriptors::DescriptorLayoutBuilder::AddBinding(uint32_t binding, VkDescriptorType type)
{
	VkDescriptorSetLayoutBinding newBind{};
	newBind.binding = binding;
	newBind.descriptorCount = 1;
	newBind.descriptorType = type;

	bindings.push_back(newBind);
	return *this;
}

void VKDescriptors::DescriptorLayoutBuilder::Clear()
{
	bindings.clear();
}

VkDescriptorSetLayout VKDescriptors::DescriptorLayoutBuilder::Build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext, VkDescriptorSetLayoutCreateFlags flags)
{
	for (auto& b : bindings)
	{
		//Apply our shaderflags to all of our bindings.
		b.stageFlags |= shaderStages;
	}

	VkDescriptorSetLayoutCreateInfo info{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO };
	info.pNext = pNext;

	info.pBindings = bindings.data();
	info.bindingCount = bindings.size();
	info.flags = flags;

	VkDescriptorSetLayout set;
	VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));
	return set;
}

VkDescriptorSet VKDescriptors::DescriptorAllocator::Allocate(VkDevice device, VkDescriptorSetLayout layout)
{
	VkDescriptorSet descriptorSet;

	VkDescriptorSetAllocateInfo descriptorAInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO };
	descriptorAInfo.pNext = nullptr;
	descriptorAInfo.descriptorPool = pool;
	descriptorAInfo.descriptorSetCount = 1;
	descriptorAInfo.pSetLayouts = &layout;

	VK_CHECK(vkAllocateDescriptorSets(device, &descriptorAInfo, &descriptorSet));
	return descriptorSet;
}

void VKDescriptors::DescriptorAllocator::InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;

	for (auto ratio : poolRatios)
	{
		poolSizes.push_back(
			VkDescriptorPoolSize{ .type = ratio.type, .descriptorCount = uint32_t(ratio.ratio * maxSets) }
		);
	}

	VkDescriptorPoolCreateInfo poolInfo{ .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO };
	poolInfo.flags = 0;
	poolInfo.maxSets = maxSets;
	poolInfo.pNext = nullptr;
	poolInfo.poolSizeCount = poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();
	vkCreateDescriptorPool(device, &poolInfo, nullptr, &pool);
}

void VKDescriptors::DescriptorAllocator::ClearDescriptors(VkDevice device)
{
	vkResetDescriptorPool(device, pool, 0);
}

void VKDescriptors::DescriptorAllocator::DestroyPool(VkDevice device)
{
	vkDestroyDescriptorPool(device, pool, nullptr);
}

//-------------
// VKDescriptorAllocatorGrowable

VkDescriptorPool VKDescriptors::DescriptorAllocatorGrowable::GetPool(VkDevice device)
{
	VkDescriptorPool newPool;

	if (readyPools.size() != 0)
	{
		newPool = readyPools.back();
		readyPools.pop_back();
	}
	else
	{
		//No pools available

		newPool = CreatePool(device, setsPerPool, ratios);

		setsPerPool = setsPerPool * 1.5;
		if (setsPerPool > 4092)
		{
			setsPerPool = 4092; 
		}
	}

	return newPool;
}

VkDescriptorPool VKDescriptors::DescriptorAllocatorGrowable::CreatePool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios)
{
	std::vector<VkDescriptorPoolSize> poolSizes;
	for (PoolSizeRatio ratio : poolRatios)
	{
		poolSizes.push_back(VkDescriptorPoolSize{
			.type = ratio.type,
			.descriptorCount = uint32_t(ratio.ratio * setCount)
			});
	}

	VkDescriptorPoolCreateInfo poolInfo{};
	poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
	poolInfo.flags = 0;
	poolInfo.maxSets = setCount;
	poolInfo.poolSizeCount = (uint32_t)poolSizes.size();
	poolInfo.pPoolSizes = poolSizes.data();

	VkDescriptorPool newPool;

	vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);
	return newPool;
}

void VKDescriptors::DescriptorAllocatorGrowable::InitPool(VkDevice device, uint32_t maxSets, std::span<PoolSizeRatio> poolRatios)
{
	ratios.clear();

	for (auto ratio : poolRatios)
	{
		ratios.push_back(
			ratio
		);
	}

	VkDescriptorPool newPool = CreatePool(device, maxSets, poolRatios);

	setsPerPool = maxSets * 1.5;

	readyPools.push_back(newPool);
}

void VKDescriptors::DescriptorAllocatorGrowable::ClearPools(VkDevice device)
{
	for (auto p : readyPools)
	{
		vkResetDescriptorPool(device, p, 0);
	}
	for (auto p : fullPools)
	{
		vkResetDescriptorPool(device, p, 0);
		readyPools.push_back(p);
	}
	fullPools.clear();
}

void VKDescriptors::DescriptorAllocatorGrowable::DestroyPool(VkDevice device)
{
	for (auto p : readyPools)
	{
		vkDestroyDescriptorPool(device, p, 0);
	}
	for (auto p : fullPools)
	{
		vkDestroyDescriptorPool(device, p, 0);
	}
	readyPools.clear();
	fullPools.clear();
}

VkDescriptorSet VKDescriptors::DescriptorAllocatorGrowable::Allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext)
{
	// Get or create a pool to alloc from 
	VkDescriptorPool pool = GetPool(device);

	VkDescriptorSetAllocateInfo allocInfo{};
	allocInfo.pNext = pNext;
	allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
	allocInfo.descriptorPool = pool;
	allocInfo.descriptorSetCount = 1;
	allocInfo.pSetLayouts = &layout;
	
	VkDescriptorSet ds;

	VkResult r = vkAllocateDescriptorSets(device, &allocInfo, &ds);

	//alloc failed, out of memory
	if (r == VK_ERROR_OUT_OF_POOL_MEMORY || r == VK_ERROR_FRAGMENTED_POOL)
	{
		fullPools.push_back(pool);
		pool = GetPool(device);

		allocInfo.descriptorPool = pool;

		VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, &ds));
	}

	readyPools.push_back(pool);
	return ds;
}

VKDescriptors::DescriptorWriter VKDescriptors::DescriptorWriter::WriteImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type)
{
	VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo
		{
			.sampler = sampler,
			.imageView = image,
			.imageLayout = layout
		});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pImageInfo = &info;

	writes.push_back(write);
	
	return *this;
}

VKDescriptors::DescriptorWriter VKDescriptors::DescriptorWriter::WriteBuffer(int binding, VkBuffer buff, size_t size, size_t offset, VkDescriptorType type)
{
	VkDescriptorBufferInfo& info = buffInfos.emplace_back(VkDescriptorBufferInfo
		{
			.buffer = buff,
			.offset = offset,
			.range = size
		});

	VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

	write.dstBinding = binding;
	write.dstSet = VK_NULL_HANDLE;
	write.descriptorCount = 1;
	write.descriptorType = type;
	write.pBufferInfo = &info;

	writes.push_back(write);
	
	return *this;
}

void VKDescriptors::DescriptorWriter::Clear()
{
	imageInfos.clear();
	writes.clear();
	buffInfos.clear();
}

void VKDescriptors::DescriptorWriter::UpdateSet(VkDevice device, VkDescriptorSet set)
{
	for (VkWriteDescriptorSet& write : writes)
	{
		write.dstSet = set;
	}

	vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, nullptr);
}