#pragma once
#include <IRenderable.h>
class RenderNode : public IRenderable
{
public:
	glm::mat4 localTransform;
	glm::mat4 globalTransform;

	std::vector<std::shared_ptr<RenderNode>> children;
	std::weak_ptr<RenderNode> parent;

	void RefreshTransform(const glm::mat4 parentMatrix);
	virtual void Draw(const glm::mat4 topMatrix, DrawContext& ctx);
private:
	
};

class MeshNode : public RenderNode
{
public:
	virtual void Draw(const glm::mat4 topMatrix, DrawContext& ctx) override;
	std::shared_ptr<MeshAsset> mesh;
};
