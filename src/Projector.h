#pragma once

#include <array>

#include "CoreMath.h"

const float nearPlaneZ = 0.05;
static float getFarPlaneZ(float distance) { return distance * 5.0; }

class Projector {
public:
	float getFOV();
	std::array<float, 4> getDims(); // gets the left, right, top, and bottom of the projection frustum

	void setHorShiftDeg(float shiftDeg) { horShift = glm::radians(shiftDeg); }
	void setVertShiftDeg(float shiftDeg) { vertShift = glm::radians(shiftDeg); }

	vec3 position = vec3(0, 0, 1); // Position
	vec3 target = vec3(0); // Target point
	vec3 up = vec3(0, 1, 0); // Up vector
	float focusDistance; // The distance to the image focal plane, where the "screen" would be
	float throwRatio; // projector's distance to image / width of projected image (with distance and AR, can be used to calculate fov)
	float aspectRatio; // projected image aspect ratio
	float horShift; // The angular shift (in radians) in the projector's horizontal plane
	float vertShift; // The angular shift (in radians) in the projector's vertical plane
};