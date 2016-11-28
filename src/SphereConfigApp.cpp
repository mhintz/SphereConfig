#include <iostream>
#include <fstream>

#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/params/Params.h"
#include "cinder/Display.h"
#include "cinder/Json.h"

#include "buildmesh.h"
#include "MeshHelpers.h"

using namespace ci;
using namespace ci::app;
using namespace std;

class SphereConfigApp : public App {
  public:
	static void prepSettings(Settings * settings);

	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void keyDown(KeyEvent event) override;
	void update() override;
	void draw() override;

	JsonTree loadParams();
	void saveParams();

	CameraPersp mCamera;
	CameraUi mUiCamera;

	float mCameraFov = 140.f;
	float mDistortionPower = 1.0f;

	params::InterfaceGlRef mParams;

	bmesh::MeshRef mGraticuleBaseMesh;
	gl::VboMeshRef mGraticuleMesh;

	gl::FboRef mDistortionFbo;
	gl::GlslProgRef mDistortionShader;

	string mParamsLoc = "savedParams.json";
};

#define DISTORTION_TEX_BIND_POINT 0

void SphereConfigApp::prepSettings(Settings * settings) {
	ivec2 displaySize = Display::getMainDisplay()->getSize();
	int windowSide = min(displaySize.x, displaySize.y);
	settings->setWindowSize(windowSide, windowSide);
//	settings->setFullScreen(); // someday...
	settings->setTitle("Sphere Configuration");
	settings->setHighDensityDisplayEnabled();
}

void SphereConfigApp::setup()
{
	try {
		JsonTree appParams = loadParams();
		mCameraFov = appParams.getChild("fov").getValue<float>();
		mDistortionPower = appParams.getChild("distortionPower").getValue<float>();
	} catch (ResourceLoadExc exc) {
		console() << "Failed to load parameters - they probably don't exist yet" << std::endl;
	}

	mCamera.lookAt(vec3(0, 1, 0), vec3(0, 0, 0), vec3(0, 0, 1));
	// mCamera.setAspectRatio(1); // someday...
	mCamera.setAspectRatio(getWindowAspectRatio());
	// mUiCamera = CameraUi(&mCamera, getWindow());

	mParams = params::InterfaceGl::create(getWindow(), "App parameters", toPixels(ivec2(200, 50)));

	mParams->addParam("Camera FOV", & mCameraFov).min(60.f).max(180.f).precision(3).step(0.1f);
	mParams->addParam("Distortion Power", & mDistortionPower).min(0.0f).max(4.0f).precision(5).step(0.001f);

	mGraticuleBaseMesh = bmesh::makeGraticule(vec3(0, 0, 0), 1.0);
	mGraticuleMesh = bmeshToVBOMesh(mGraticuleBaseMesh);

	mDistortionFbo = gl::Fbo::create(toPixels(getWindowWidth()), toPixels(getWindowHeight()));
	mDistortionShader = gl::GlslProg::create(loadAsset("passThrough_v.glsl"), loadAsset("distortion_f.glsl"));
	mDistortionShader->uniform("uTex0", DISTORTION_TEX_BIND_POINT);
}

void SphereConfigApp::mouseDown( MouseEvent event )
{
}

void SphereConfigApp::keyDown(KeyEvent event) {
	if (event.getCode() == KeyEvent::KEY_ESCAPE) {
		quit();
	} else if (event.getCode() == KeyEvent::KEY_SPACE) {
		saveParams();
	}
}

void SphereConfigApp::update()
{
	mCamera.setFov(mCameraFov);

	mDistortionShader->uniform("distortionPow", mDistortionPower);
}

void SphereConfigApp::draw()
{
	{
		gl::ScopedFramebuffer scpFB(mDistortionFbo);

		gl::clear(Color(0, 0, 0));

		gl::ScopedMatrices scpMat;

		gl::setMatrices(mCamera);

		gl::draw(mGraticuleMesh);
	}

	{
		gl::clear(Color(0, 0, 0));

		gl::ScopedMatrices scpMat;

		gl::setMatricesWindow(toPixels(getWindowWidth()), toPixels(getWindowHeight()));

		gl::ScopedGlslProg scpShader(mDistortionShader);

		gl::ScopedTextureBind scpTex(mDistortionFbo->getColorTexture(), DISTORTION_TEX_BIND_POINT);

		gl::drawSolidRect(toPixels(getWindowBounds()));
	}

	mParams->draw();
}

JsonTree SphereConfigApp::loadParams() {
	return JsonTree(loadResource(mParamsLoc));
}

void SphereConfigApp::saveParams() {
	JsonTree appParams;

	appParams.addChild(JsonTree("fov", mCameraFov));
	appParams.addChild(JsonTree("distortionPower", mDistortionPower));

	string serializedParams = appParams.serialize();
	std::ofstream writeFile;

	string appOwnFile = getResourcePath(mParamsLoc).string();
	writeFile.open(appOwnFile);
	std::cout << "writing params to: " << appOwnFile << std::endl;
	writeFile << serializedParams;
	writeFile.close();

	string repoFile = fs::canonical("../../../resources/" + mParamsLoc).string();
	writeFile.open(repoFile);
	std::cout << "writing params to: " << repoFile << std::endl;
	writeFile << serializedParams;
	writeFile.close();
}

CINDER_APP( SphereConfigApp, RendererGl, & SphereConfigApp::prepSettings )
