#include "camera.h"

Camera::Camera()
{
	velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	position = glm::vec3(0.0f, 0.0f, 5.0f);
	pitch = 0.0f;
	yaw = 0.0f;
}

glm::mat4 Camera::getView()
{
	glm::mat4 cameraTranslation = glm::translate(glm::mat4(1.0f), position);
	glm::mat4 cameraRotation = getRotation();

	return glm::inverse(cameraTranslation * cameraRotation);
}

glm::mat4 Camera::getRotation()
{
	glm::quat pitchRotation = glm::angleAxis(pitch, glm::vec3{ 1.0f, 0.0f, 0.0f });
	glm::quat yawRotation = glm::angleAxis(yaw, glm::vec3{ 0.0f, -1.0f, 0.0f });

	return glm::toMat4(yawRotation) * glm::toMat4(pitchRotation);
}

void Camera::processSDLEvent(SDL_Event& e)
{
	switch (e.type)
	{
	case SDL_KEYDOWN:
		switch (e.key.keysym.sym)
		{
		case SDLK_w:
			velocity.z = -1;
			break;
		case SDLK_s:
			velocity.z = 1;
			break;
		case SDLK_a:
			velocity.x = -1;
			break;
		case SDLK_d:
			velocity.x = 1;
			break;
		}
		break;
	case SDL_KEYUP:
		switch (e.key.keysym.sym)
		{
		case SDLK_w:
		case SDLK_s:
			velocity.z = 0;
			break;
		case SDLK_a:
		case SDLK_d:
			velocity.x = 0;
			break;
		}
		break;
	case SDL_MOUSEMOTION:
		yaw += (float)e.motion.xrel / 200.0f;
		pitch -= (float)e.motion.yrel / 200.0f;
		break;
	}
}

void Camera::Update()
{
	glm::mat4 cameraRotation = getRotation();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * 0.15f, 0.0f));
}
