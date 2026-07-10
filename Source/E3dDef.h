#pragma once

#include "GLLoader.h"
#include "External/glm/glm/glm.hpp"
#include "External/glm/glm/gtc/type_ptr.hpp"
#include "External/glm/glm/gtc/matrix_transform.hpp"

#include "E3dUtils.h"

struct CameraContext
{
	CameraContext(glm::mat4 view, glm::mat4 projection) :view(view), projection(projection) {}

	glm::mat4 view;// ”Õº
	glm::mat4 projection;//Õ∂”∞

	//glm::vec3 cameraPos; //cameraPos = glm::vec3(glm::inverse(view)[3]);
	glm::vec3 GetCameraPos()
	{
		return glm::vec3(glm::inverse(view)[3]);
	}
};