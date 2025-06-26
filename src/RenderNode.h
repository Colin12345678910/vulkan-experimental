#pragma once
#include <IRenderable.h>
#include <vk_loader.h>
class RenderNode : public IRenderable
{
public:
	glm::mat4 localTransform;
	glm::mat4 globalTransform;

	virtual void Draw(const glm::mat4 topMatrix, DrawContext& ctx);
private:
	//TODO write a comment here
	std::weak_ptr<RenderNode> parent;
	std::vector<std::shared_ptr<RenderNode>> children;

	void RefreshTransform(const glm::mat4 parentMatrix);
};

class MeshNode : public RenderNode
{
public:
	virtual void Draw(const glm::mat4 topMatrix, DrawContext& ctx) override;
	std::shared_ptr<MeshAsset> mesh;
};
