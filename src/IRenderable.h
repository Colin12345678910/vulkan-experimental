#pragma once
#include <vk_types.h>
/* IRenderable
*	Colin D
*	June 2025
*	This class is intended to be an interface that defines a draw function, 
*	allowing us to create an array of IRenderables in order to construct our scene
*	in a more modular way. Basically, extend this if you want to make an object that can
*	be pushed onto the scene stack.
*/
class IRenderable
{
public:
	virtual void Draw(const glm::mat4& topMatrix, DrawContext& ctx) {};
};