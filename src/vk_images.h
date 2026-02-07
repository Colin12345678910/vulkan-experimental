/* VK_images
*	Colin D
*	June 2025
*	This is honestly a really important class, as it abstracts image conversion, which is a really
*	important aspect to writing vulkan applications as unless you wanna deal with VK1.0/1.1 style boilerplate
*	you need to handle this manually, but Vulkans manual image functions are quite boilerplatey.
*/
#pragma once 

namespace vkutil 
{
	//Transitions an image from one layout to another
	void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout, VkImageAspectFlagBits flags = VkImageAspectFlagBits::VK_IMAGE_ASPECT_NONE, int mip = 0, int layers = 0);
	void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
	void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent3D srcSize, VkExtent3D dstSize);
	void CopyBufferToImage(VkCommandBuffer cmd, VkBuffer source, VkImage destination, VkExtent2D dstSize, int mip = 0);
	void DepthToColor(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
	void CopyImageToBuffer(VkCommandBuffer cmd, VkImage source, VkBuffer destination, VkExtent2D srcSize, int mip = 0, int layers = 1);
	void CopyImageToBuffer(VkCommandBuffer cmd, VkImage source, VkBuffer destination, VkExtent3D srcSize, int mip = 0, int layers = 1);
	void GenerateMipmaps(VkCommandBuffer cmd, VkImage img, VkExtent2D imageSize);
};