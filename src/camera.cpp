#include "camera.h"

AutoFloatCVar CVAR_CameraSpeed("camera.speed", "Speed of the camera movement", 0.025f);

Camera::Camera()
{
	velocity = glm::vec3(0.0f, 0.0f, 0.0f);
	position = glm::vec3(-9.0f, 1.0, 0.0f);
	pitch = 0.0f;
	yaw = 190.0f;
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

glm::vec4 Camera::getForward()
{
	glm::mat4 cameraRotation = getRotation();
	return glm::vec4(cameraRotation * glm::vec4(0.0f, 0.0f, -1.0f, 0.0f));
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

void Camera::Update(double deltaTime)
{
	glm::mat4 cameraRotation = getRotation();
	position += glm::vec3(cameraRotation * glm::vec4(velocity * (CVAR_CameraSpeed.Get() * (float)deltaTime), 0.0f));

	//fmt::println("X {}, Y {}, Z {}", position.x, position.y, position.z);
}

void Camera::Demo()
{
	velocity = glm::vec3(0.0f, 0.0f, -1.0f);
}
