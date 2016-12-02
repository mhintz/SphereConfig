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

#include "Projector.h"

using namespace ci;
using namespace ci::app;
using namespace std;

enum class ConfigMode {
	Interior,
	Exterior
};

struct InteriorConfig {
	float cameraFov = 140.f;
	float distortionPower = 1.0f;
};

struct ExteriorConfig {
	vector<Projector> projectors;
};

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

	params::InterfaceGlRef mParams;

	ConfigMode mConfigMode = ConfigMode::Exterior;

	CameraPersp mInteriorCamera;

	CameraPersp mExteriorCamera;
	CameraUi mExteriorUiCamera;

	InteriorConfig mInteriorConfig;
	ExteriorConfig mExteriorConfig;

	gl::VboMeshRef mGraticuleMesh;

	gl::FboRef mInteriorDistortionFbo;
	gl::GlslProgRef mInteriorDistortionShader;

	string mParamsLoc = "savedParams.json";
};

#define INTERIOR_DISTORTION_TEX_BIND_POINT 0

void SphereConfigApp::prepSettings(Settings * settings) {
	ivec2 displaySize = Display::getMainDisplay()->getSize();
	int windowSide = min(displaySize.x, displaySize.y);
	settings->setWindowSize(windowSide, windowSide);
	// settings->setFullScreen(); // someday...
	settings->setTitle("Sphere Configuration");
	settings->setHighDensityDisplayEnabled();
}

void SphereConfigApp::setup()
{
	// Load parameters from saved file
	try {
		JsonTree appParams = loadParams();
		mInteriorConfig.cameraFov = appParams.getChild("fov").getValue<float>();
		mInteriorConfig.distortionPower = appParams.getChild("distortionPower").getValue<float>();
	} catch (ResourceLoadExc exc) {
		console() << "Failed to load parameters - they probably don't exist yet" << std::endl;
	}

	// Setup params
	mParams = params::InterfaceGl::create(getWindow(), "App parameters", toPixels(ivec2(200, 50)));

	mParams->addParam("Configuration Mode", {
		"Internal Config",
		"External Config"
	}, [this] (int cfigMode) {
		if (cfigMode == 0) { mConfigMode = ConfigMode::Interior; }
		else if (cfigMode == 1) { mConfigMode = ConfigMode::Exterior; }
	}, [this] () {
		if (mConfigMode == ConfigMode::Interior) { return 0; }
		else if (mConfigMode == ConfigMode::Exterior) { return 1; }
	});

	mParams->addParam("Camera FOV", & mInteriorConfig.cameraFov).min(60.f).max(180.f).precision(3).step(0.1f);
	mParams->addParam("Distortion Power", & mInteriorConfig.distortionPower).min(0.0f).max(4.0f).precision(5).step(0.001f);

	// ^^^^ Have to set up the params before the CameraUI, or else things get screwy

	// Set up two possible cameras
	mInteriorCamera.lookAt(vec3(0, 1, 0), vec3(0, 0, 0), vec3(0, 0, 1));
	mInteriorCamera.setAspectRatio(getWindowAspectRatio());
	// mInteriorCamera.setAspectRatio(1); // someday...

	mExteriorCamera.lookAt(vec3(0, 0, 4), vec3(0), vec3(0, 1, 0));
	mExteriorCamera.setAspectRatio(getWindowAspectRatio());
	mExteriorUiCamera = CameraUi(& mExteriorCamera, getWindow());

	// Setup mesh
	auto baseMesh = bmesh::makeGraticule(vec3(0, 0, 0), 1.0);
	mGraticuleMesh = bmeshToVBOMesh(baseMesh);

	// Set up interior distortion rendering
	mInteriorDistortionFbo = gl::Fbo::create(toPixels(getWindowWidth()), toPixels(getWindowHeight()));
	mInteriorDistortionShader = gl::GlslProg::create(loadAsset("passThrough_v.glsl"), loadAsset("distortion_f.glsl"));
	mInteriorDistortionShader->uniform("uTex0", INTERIOR_DISTORTION_TEX_BIND_POINT);
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
	mInteriorCamera.setFov(mInteriorConfig.cameraFov);
	mInteriorDistortionShader->uniform("distortionPow", mInteriorConfig.distortionPower);
}

void SphereConfigApp::draw()
{
	if (mConfigMode == ConfigMode::Interior) {	
		{
			gl::ScopedFramebuffer scpFB(mInteriorDistortionFbo);
			gl::clear(Color(0, 0, 0));
			gl::ScopedMatrices scpMat;
			gl::setMatrices(mInteriorCamera);
			gl::draw(mGraticuleMesh);
		}

		{
			gl::clear(Color(0, 0, 0));
			gl::ScopedMatrices scpMat;
			gl::setMatricesWindow(toPixels(getWindowWidth()), toPixels(getWindowHeight()));
			gl::ScopedGlslProg scpShader(mInteriorDistortionShader);
			gl::ScopedTextureBind scpTex(mInteriorDistortionFbo->getColorTexture(), INTERIOR_DISTORTION_TEX_BIND_POINT);
			gl::drawSolidRect(toPixels(getWindowBounds()));
		}
	} else if (mConfigMode == ConfigMode::Exterior) {
		{
			gl::clear(Color(0, 0, 0));
			gl::ScopedMatrices scpMat;
			gl::setMatrices(mExteriorCamera);
			gl::draw(mGraticuleMesh);
		}
	}

	mParams->draw();
}

JsonTree SphereConfigApp::loadParams() {
	return JsonTree(loadResource(mParamsLoc));
}

void SphereConfigApp::saveParams() {
	JsonTree appParams;

	appParams.addChild(JsonTree("fov", mInteriorConfig.cameraFov));
	appParams.addChild(JsonTree("distortionPower", mInteriorConfig.distortionPower));

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
