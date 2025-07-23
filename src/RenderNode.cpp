#include "RenderNode.h"
void RenderNode::RefreshTransform(const glm::mat4 parentMatrix)
{
	globalTransform = parentMatrix * localTransform;
	for (auto c : children)
	{
		c->RefreshTransform(parentMatrix);
	}
}
void RenderNode::Draw(const glm::mat4 topMatrix, DrawContext& ctx)
{
	for (auto c : children)
	{
		c->Draw(topMatrix, ctx);
	}
}
void MeshNode::Draw(const glm::mat4 topMatrix, DrawContext& ctx)
{
	glm::mat4 nodeMatrix = topMatrix * globalTransform;

	for (auto& s : mesh->surfaces)
	{
		RenderObject obj;
		obj.indexCount = s.count;
		obj.firstIndex = s.startIndex;
		obj.indexBuffer = mesh->meshBuffers.indexBuffer.buffer;
		obj.material = &s.material->data;

		obj.bounds = s.bounds;

		obj.transform = nodeMatrix;
		obj.vertexBufferAddress = mesh->meshBuffers.vertexBufferAddress;

		switch (s.material->data.passType)
		{
			case MaterialPass::MainColor:
				ctx.OpaqueSurfaces.push_back(obj);
				break;
			case MaterialPass::Transparent:
				ctx.TransparentSurfaces.push_back(obj);
				break;
		}
		ctx.OpaqueSurfaces.push_back(obj);
	}

	RenderNode::Draw(topMatrix, ctx);
}