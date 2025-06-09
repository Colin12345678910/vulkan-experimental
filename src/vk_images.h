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
	void TransitionImage(VkCommandBuffer cmd, VkImage image, VkImageLayout currentLayout, VkImageLayout newLayout);
	void CopyImageToImage(VkCommandBuffer cmd, VkImage source, VkImage destination, VkExtent2D srcSize, VkExtent2D dstSize);
};