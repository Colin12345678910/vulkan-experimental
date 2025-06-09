#include <vk_pipelines.h>

bool vkutil::LoadShaderModule(const char* filepath, VkDevice device, VkShaderModule* pShaderModule)
{
	// Open the file at the end.
	std::ifstream file(filepath, std::ios::ate | std::ios::binary);

	if (!file.is_open())
	{
		return false;
	}

	//Get filesize
	size_t fsize = file.tellg();

	//Allocate a buffer the size of the file in uint32
	std::vector<uint32_t> buf(fsize / sizeof(uint32_t));

	//Return to start, read into the buffer, close.
	file.seekg(0);

	file.read((char*)buf.data(), fsize);

	file.close();

	//Create a shader module
	VkShaderModuleCreateInfo createInfo{ .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO, .pNext = nullptr };
	//Code size is in bytes.
	createInfo.codeSize = buf.size() * sizeof(uint32_t);
	createInfo.pCode = buf.data();

	VkShaderModule module;
	if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
	{
		return false;
	}

	*pShaderModule = module;
	return true;
}

void vkutil::PipelineBuilder::Clear()
{
	//Clear all of the structs

	_inputAssembly = { .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO };
	_rasterizer = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO };
	_colorBlendAttachment = {};
	_multisampling = { .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO };
	_pipelineLayout = {};
	_depthStencil = { .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO };
	_renderInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO };
	_shaderStages.clear();
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetShaders(VkShaderModule vertexShader, VkShaderModule fragmentShader)
{
	_shaderStages.clear();

	_shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_VERTEX_BIT, vertexShader)
	);

	_shaderStages.push_back(
		vkinit::pipeline_shader_stage_create_info(VK_SHADER_STAGE_FRAGMENT_BIT, fragmentShader)
	);

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetInputTopology(VkPrimitiveTopology topology)
{
	_inputAssembly.topology = topology;
	// Disable primitiveRestart
	_inputAssembly.primitiveRestartEnable = VK_FALSE;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetPolygonMode(VkPolygonMode mode)
{
	_rasterizer.polygonMode = mode;
	_rasterizer.lineWidth = 1.0f;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetCullMode(VkCullModeFlags cullMode, VkFrontFace frontFace)
{
	_rasterizer.cullMode = cullMode;
	_rasterizer.frontFace = frontFace;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetMultisamplingNone()
{
	_multisampling.sampleShadingEnable = VK_FALSE;
	//Disable multisampling
	_multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
	_multisampling.minSampleShading = 1.0f;
	_multisampling.pSampleMask = nullptr;

	//Disable alpha
	_multisampling.alphaToCoverageEnable = VK_FALSE;
	_multisampling.alphaToOneEnable = VK_FALSE;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::DisableBlending()
{
	//Default the write mask
	_colorBlendAttachment.colorWriteMask = 
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
	//RemoveBlend
	_colorBlendAttachment.blendEnable = VK_FALSE;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetColorAttachmentFormat(VkFormat format)
{
	_colorAttachmentFormat = format;

	//Connect to renderInfo
	_renderInfo.colorAttachmentCount = 1;
	_renderInfo.pColorAttachmentFormats = &_colorAttachmentFormat;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetDepthFormat(VkFormat format)
{
	_renderInfo.depthAttachmentFormat = format;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::DisableDepthTest()
{
	_depthStencil.depthTestEnable = VK_FALSE;
	_depthStencil.depthWriteEnable = VK_FALSE;
	_depthStencil.depthCompareOp = VK_COMPARE_OP_NEVER;
	_depthStencil.depthBoundsTestEnable = VK_FALSE;
	_depthStencil.stencilTestEnable = VK_FALSE;
	_depthStencil.front = {};
	_depthStencil.back = {};
	_depthStencil.minDepthBounds = 0.0f;
	_depthStencil.maxDepthBounds = 1.0f;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::EnableDepthTest(bool depthWriteEnable, VkCompareOp op)
{
	_depthStencil.depthTestEnable = VK_TRUE;
	_depthStencil.depthWriteEnable = depthWriteEnable;
	_depthStencil.depthCompareOp = op;
	_depthStencil.depthBoundsTestEnable = VK_FALSE;
	_depthStencil.stencilTestEnable = VK_FALSE;
	_depthStencil.front = {};
	_depthStencil.back = {};
	_depthStencil.minDepthBounds = 0.0f;
	_depthStencil.maxDepthBounds = 1.0f;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::CreateGenericPipelineLayout(VkDevice device, VkPipelineLayout* layout)
{
	//Build a pipeline layout that controls the in and outs, since we aren't doing much, we can just
	//setup a basic one here
	VkPipelineLayoutCreateInfo pipelineInfo = vkinit::pipeline_layout_create_info();
	VK_CHECK(vkCreatePipelineLayout(device, &pipelineInfo, nullptr, &_pipelineLayout));

	*layout = _pipelineLayout;
	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::SetPipelineLayout(VkPipelineLayout layout)
{
	_pipelineLayout = layout;
	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::EnableBlendingAdditive()
{
	// color = srcColor * srcBlendFactor <operator> dstColor * dstColorBlendFactor
	//Default the write mask
	_colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	_colorBlendAttachment.blendEnable = VK_TRUE;
	// Additive Blending BC our source takes the alpha, but the dst is always present as 1.
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE;

	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	return *this;
}

vkutil::PipelineBuilder vkutil::PipelineBuilder::EnableBlendingAlphaBlend()
{
	//Default the write mask
	_colorBlendAttachment.colorWriteMask =
		VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;

	_colorBlendAttachment.blendEnable = VK_TRUE;
	// Additive Blending BC our source takes the alpha, but the dst is always present as 1.
	_colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
	_colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;

	_colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
	_colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
	_colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
	_colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

	return *this;
}

VkPipeline vkutil::PipelineBuilder::BuildPipeline(VkDevice device)
{
	// Make viewport state form our stored
	VkPipelineViewportStateCreateInfo viewportState = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO };
	viewportState.pNext = nullptr;
	viewportState.viewportCount = 1;
	viewportState.scissorCount = 1;

	//Setup some placeholder colorblending

	VkPipelineColorBlendStateCreateInfo colorBlending{ .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO, .pNext = nullptr };
	
	colorBlending.logicOpEnable = VK_FALSE;
	colorBlending.logicOp = VK_LOGIC_OP_COPY;
	colorBlending.attachmentCount = 1;
	colorBlending.pAttachments = &_colorBlendAttachment;

	VkPipelineVertexInputStateCreateInfo _vertexInputInfo = { .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO };

	//Building the actual pipeline
	VkGraphicsPipelineCreateInfo pipelineInfo{ .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO };

	//Conncect renderinfo to pNext;

	pipelineInfo.pNext = &_renderInfo;

	pipelineInfo.stageCount = (uint32_t)_shaderStages.size();

	pipelineInfo.pStages = _shaderStages.data();
	pipelineInfo.pInputAssemblyState = &_inputAssembly;
	pipelineInfo.pRasterizationState = &_rasterizer;
	pipelineInfo.pColorBlendState = &colorBlending;
	pipelineInfo.pMultisampleState = &_multisampling;
	pipelineInfo.layout = _pipelineLayout;
	pipelineInfo.pDepthStencilState = &_depthStencil;
	pipelineInfo.pViewportState = &viewportState;
	pipelineInfo.pVertexInputState = &_vertexInputInfo;

	//Setup DynState
	VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
	
	VkPipelineDynamicStateCreateInfo dynamicInfo{ .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO };
	dynamicInfo.pDynamicStates = &state[0];
	dynamicInfo.dynamicStateCount = 2;

	pipelineInfo.pDynamicState = &dynamicInfo;

	//Create a pipeline, and error check.
	VkPipeline newPipeline;
	if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &newPipeline)
		!= VK_SUCCESS)
	{
		fmt::println("failed to create pipeline");
		return VK_NULL_HANDLE;
	}
	else
	{
		return newPipeline;
	}
	return VkPipeline();
}
