#pragma once

#include <array>

#include "cinder/gl/gl.h"

#include "CoreMath.h"

class Projector {
public:
	// For helping in calculating configuration parameters. Returns the vertical field of view and the vertical "offset angle"
	// zDistance: distance of projector from wall
	// imgHeight: height of the projected image
	// topHeight: height of the top of the image on the wall
	// Returns [ vertical fov, "base angle" or "offset angle" ]
	static std::array<float, 2> verticalAnglesFromHeights(float zDistance, float imgHeight, float topHeight);
	// For helping in calculating configuration parameters. Returns the horizontal field of view (assumes that the horizontal offset angle is zero)
	static float horizontalAngleFromDims(float zDistance, float imgWidth);
	// Calculates aspect ratio from a width and a height

	Projector()
	: mHorFOV(M_PI / 4), mVertFOV(M_PI / 4), mVertBaseAngle(M_PI / 8) {}

	Projector(float horFOV, float vertFOV, float baseAngle)
	: mHorFOV(horFOV), mVertFOV(vertFOV), mVertBaseAngle(baseAngle) {}

	// View-changing functions
	Projector & moveTo(vec3 pos);
	Projector & setUp(vec3 up);

	// Projection-changing functions
	// For changing the projector's zoom and cast angle, which affect fov and base angle values
	Projector & setHorFOV(float fov);
	Projector & setVertFOV(float fov);
	Projector & setVertBaseAngle(float angle);

	// Accessors for the matrices
	mat4 const & getViewMatrix();
	mat4 const & getProjectionMatrix();

	// Accessors for properties
	float getHorFOV() { return mHorFOV; }
	float getVertFOV() { return mVertFOV; }
	float getVertBaseAngle() { return mVertBaseAngle; }
	vec3 getPos() { return mPosition; }
	vec3 getUp() { return mUp; }

	void draw();

private:
	// Cached matrix calculation
	void calcViewMatrix();
	void calcProjectionMatrix();
	void calcPolyline();
	void setProjectionMatrixDirty();

	std::array<float, 6> getFrustumDims();

	// Matrix caching system
	mat4 mViewMatrix;
	bool mViewMatrixCached = false;
	mat4 mProjectionMatrix;
	bool mProjectionMatrixCached = false;
	ci::gl::VertBatch mFrustumMesh = ci::gl::VertBatch(GL_LINES);
	bool mPolylineCached = false;

	// 3D position and orientation properties of the Projector's lens, here abstracted as a point in space
	vec3 mPosition = vec3(0, 0, 1); // Position
	vec3 mUp = vec3(0, 1, 0); // Up vector

	// Properties of the projector's beam, used to determine the shape of the projector's projection frustum
	// Horizontal FOV
	float mHorFOV;
	// Vertical FOV
	float mVertFOV;
	// The angular vertical shift of the projector.
	// Measured as the angle between the projector's base level and the bottom of the projected image.
	// I believe that for all projectors and projector setings, this is at least 0.
	float mVertBaseAngle;
};

// Returns configuration for an Acer P5515 projector at minimum "zoom" setting
Projector getAcerP5515MinZoom();

// Returns configuration for an Acer P5515 projector at maximum "zoom" setting
Projector getAcerP5515MaxZoom();
