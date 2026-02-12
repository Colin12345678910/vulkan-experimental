
#include <vk_types.h>
#include <SDL_events.h>
#include <glm/gtx/transform.hpp>
#include <glm/gtx/quaternion.hpp>

class Camera {
public:

	Camera();
	glm::vec3 velocity;
	glm::vec3 position;

	// vertical
	float pitch = 0.0f;
	// Horizontal
	float yaw = 0.0f;

	glm::mat4 getView();
	glm::mat4 getRotation();
	glm::vec4 getForward();
	glm::vec4 getPosition() { return glm::vec4(position, 0.0f); }

	void processSDLEvent(SDL_Event& e);
	void Update(double deltaTime);
};
