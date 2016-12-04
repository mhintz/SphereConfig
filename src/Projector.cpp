#include "Projector.h"

using namespace ci;

// Default frustum near and far values
const float PROJ_NEAR_PLANE_Z = 0.3; // 30cm
const float PROJ_FAR_PLANE_Z = 18.0; // 18m

Projector getAcerP5515MinZoom() {
	auto hfov = Projector::horizontalAngleFromDims(2.0, 1.04);
	auto vertAngles = Projector::verticalAnglesFromHeights(2.0, 0.78, 0.90);
	return Projector(hfov, vertAngles[0], vertAngles[1]);
}

Projector getAcerP5515MaxZoom() {
	auto hfov = Projector::horizontalAngleFromDims(2.0, 1.25);
	auto vertAngles = Projector::verticalAnglesFromHeights(2.0, 0.94, 1.08);
	return Projector(hfov, vertAngles[0], vertAngles[1]);
}

std::array<float, 2> Projector::verticalAnglesFromHeights(float zDistance, float imgHeight, float topHeight) {
	float fullAngle = atan2(topHeight, zDistance);
	float baseAngle = atan2(topHeight - imgHeight, zDistance);
	return { fullAngle - baseAngle, baseAngle };
}

float Projector::horizontalAngleFromDims(float zDistance, float imgWidth) {
	return 2.f * atan2(imgWidth / 2.f, zDistance);
}

Projector & Projector::moveTo(vec3 pos) {
	mPosition = pos;
	mViewMatrixCached = false;
	return *this;
}
Projector & Projector::pointAt(vec3 target) {
	mTarget = target;
	mViewMatrixCached = false;
	return *this;
}
Projector & Projector::setUp(vec3 up) {
	mUp = up;
	mViewMatrixCached = false;
	return *this;
}

Projector & Projector::setHorFOV(float fov) {
	mHorFOV = fov;
	setProjectionMatrixDirty();
	return *this;
}
Projector & Projector::setVertBaseAngle(float angle) {
	mVertBaseAngle = angle;
	setProjectionMatrixDirty();
	return *this;
}
Projector & Projector::setVertFOV(float fov) {
	mVertFOV = fov;
	setProjectionMatrixDirty();
	return *this;
}

void Projector::setProjectionMatrixDirty() {
	mProjectionMatrixCached = false;
	mPolylineCached = false;
}

mat4 const & Projector::getViewMatrix() {
	if (!mViewMatrixCached) {
		calcViewMatrix();
	}

	return mViewMatrix;
}

mat4 const & Projector::getProjectionMatrix() {
	if (!mProjectionMatrixCached) {
		calcProjectionMatrix();
	}

	return mProjectionMatrix;
}

void Projector::draw() {
	gl::ScopedModelMatrix scpMat;
	gl::multModelMatrix(glm::inverse(getViewMatrix()));

	float projLensLength = 0.06f;
	float projBodyDepth = 0.16f;

	{
		gl::ScopedGlslProg scpShader(gl::getStockShader(gl::ShaderDef().color().lambert()));
		gl::ScopedColor scpColor(Color(0.85, 0.85, 0.85));

		{
			gl::ScopedModelMatrix scpMatInner;
			gl::rotate(M_PI / 2.f, vec3(1, 0, 0));
			gl::draw(geom::Cone().radius(0.06f, 0.04f).height(projLensLength));
		}

		{
			gl::ScopedModelMatrix scpMatInner;
			gl::translate(0, 0, projLensLength + projBodyDepth / 2.f);
			gl::draw(geom::Cube().size(0.2f, 0.1f, projBodyDepth));
		}
	}

	{
		if (!mPolylineCached) {
			calcPolyline();
		}

		gl::ScopedGlslProg scpShader(gl::getStockShader(gl::ShaderDef().color()));
		gl::ScopedColor scpColor(Color(1.0, 1.0, 1.0));

		mFrustumMesh.draw();
	}
}

void Projector::calcViewMatrix() {
	mViewMatrix = glm::lookAt(mPosition, mTarget, mUp);
	mViewMatrixCached = true;
}

std::array<float, 6> Projector::getFrustumDims() {
	float xval = tan(mHorFOV / 2.0) * PROJ_NEAR_PLANE_Z;
	float bottom = tan(mVertBaseAngle) * PROJ_NEAR_PLANE_Z;
	float top = tan(mVertBaseAngle + mVertFOV) * PROJ_NEAR_PLANE_Z;
	return { -xval, xval, bottom, top, PROJ_NEAR_PLANE_Z, PROJ_FAR_PLANE_Z };
}

void Projector::calcProjectionMatrix() {
	auto dims = getFrustumDims();
	mProjectionMatrix = glm::frustum(dims[0], dims[1], dims[2], dims[3], dims[4], dims[5]);
	mProjectionMatrixCached = true;
}

void Projector::calcPolyline() {
	auto dims = getFrustumDims();

	float l = dims[0], r = dims[1], b = dims[2], t = dims[3], n = dims[4], f = dims[5];
	vec3 xdir(1, 0, 0);
	vec3 ydir(0, 1, 0);
	vec3 zdir(0, 0, -1); // The projector "looks" down the negative z axis from its own origin

	vec3 ntl = l * xdir + t * ydir + n * zdir;
	vec3 ntr = r * xdir + t * ydir + n * zdir;
	vec3 nbl = l * xdir + b * ydir + n * zdir;
	vec3 nbr = r * xdir + b * ydir + n * zdir;

	// Using the similar triangles theorem, can calculate the location of the four boundary points of the far clipping rectangle
	float sideRatio = f / n;

	vec3 ftl = sideRatio * ntl;
	vec3 ftr = sideRatio * ntr;
	vec3 fbl = sideRatio * nbl;
	vec3 fbr = sideRatio * nbr;

	// Draw the positions into the frustum mesh
	mFrustumMesh.clear();
	mFrustumMesh.begin(GL_LINES);

	// Near rectangle
	mFrustumMesh.vertex(ntl);
	mFrustumMesh.vertex(ntr);

	mFrustumMesh.vertex(ntr);
	mFrustumMesh.vertex(nbr);

	mFrustumMesh.vertex(nbr);
	mFrustumMesh.vertex(nbl);

	mFrustumMesh.vertex(nbl);
	mFrustumMesh.vertex(ntl);

	// Connection lines
	mFrustumMesh.vertex(ntl);
	mFrustumMesh.vertex(ftl);

	mFrustumMesh.vertex(ntr);
	mFrustumMesh.vertex(ftr);

	mFrustumMesh.vertex(nbr);
	mFrustumMesh.vertex(fbr);

	mFrustumMesh.vertex(nbl);
	mFrustumMesh.vertex(fbl);

	// Far rectangle
	mFrustumMesh.vertex(ftl);
	mFrustumMesh.vertex(ftr);

	mFrustumMesh.vertex(ftr);
	mFrustumMesh.vertex(fbr);

	mFrustumMesh.vertex(fbr);
	mFrustumMesh.vertex(fbl);

	mFrustumMesh.vertex(fbl);
	mFrustumMesh.vertex(ftl);

	mFrustumMesh.end();

	mPolylineCached = true;
}