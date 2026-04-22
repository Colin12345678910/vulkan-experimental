#pragma once
#include <VkTypes.h>
#include <VkDescriptors.h>
/*
A stub interface for a render pass, allowing us to create different types of render passes.
We may want to add metadata to this later, but for now it is just a stub.
*/
class IRenderpass
{
public:
	virtual void Init() {};
};

class IGeometryPass : public IRenderpass
{
public:
	virtual void Draw(VkCommandBuffer cmd, VkDescriptorSet globalDescriptor) {};
};