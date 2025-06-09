/* VKPipelines
*	Colin D
*	June 2025
*	This class handles abstracting constructing a pipeline into something that can be
*	relatively easily handled. (A pipleine basically defines all of the fixed function parameters usable
*	by a graphics card.)
*/
#pragma once 
#include <vk_types.h>
#include <fstream>
#include <vk_initializers.h>

namespace vkutil 
{
	bool LoadShaderModule(const char* filepath, VkDevice device, VkShaderModule* pShaderModule);
	class PipelineBuilder
	{
	public:
		std::vector<VkPipelineShaderStageCreateInfo> _shaderStages;

		VkPipelineInputAssemblyStateCreateInfo _inputAssembly;
		VkPipelineRasterizationStateCreateInfo _rasterizer;
		VkPipelineColorBlendAttachmentState _colorBlendAttachment;
		VkPipelineMultisampleStateCreateInfo _multisampling;
		VkPipelineLayout _pipelineLayout;
		VkPipelineDepthStencilStateCreateInfo _depthStencil;
		VkPipelineRenderingCreateInfo _renderInfo;
		VkFormat _colorAttachmentFormat;

		PipelineBuilder() { Clear(); }

		void Clear();

		//Builder Functions
		PipelineBuilder SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader);
		PipelineBuilder SetInputTopology(VkPrimitiveTopology topology);
		PipelineBuilder SetPolygonMode(VkPolygonMode mode);
		PipelineBuilder SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace);
		PipelineBuilder SetMultisamplingNone();
		PipelineBuilder DisableBlending();
		PipelineBuilder SetColorAttachmentFormat(VkFormat format);
		PipelineBuilder SetDepthFormat(VkFormat format);
		PipelineBuilder DisableDepthTest();
		PipelineBuilder EnableDepthTest(bool depthWriteEnable, VkCompareOp op);
		PipelineBuilder CreateGenericPipelineLayout(VkDevice device, VkPipelineLayout* layout);
		PipelineBuilder SetPipelineLayout(VkPipelineLayout layout);

		PipelineBuilder EnableBlendingAdditive();
		PipelineBuilder EnableBlendingAlphaBlend();

		VkPipeline BuildPipeline(VkDevice device);
	};
};