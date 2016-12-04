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
	bool renderOverview = true;
	float sphereApexHeight = 2.5;
	vector<Projector> projectors;
	int projectorPov = 0;
};

Projector parseProjectorParams(JsonTree params);
JsonTree serializeProjector(Projector theProjector);

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

	// General stuff
	params::InterfaceGlRef mParams;
	ConfigMode mConfigMode = ConfigMode::Exterior;
	gl::VboMeshRef mGraticuleMesh;

	// Interior mode stuff
	CameraPersp mInteriorCamera;
	int mMinSidePixels;
	InteriorConfig mInteriorConfig;
	gl::FboRef mInteriorDistortionFbo;
	gl::GlslProgRef mInteriorDistortionShader;

	// Exterior mode stuff
	CameraPersp mExteriorCamera;
	CameraUi mExteriorUiCamera;
	ExteriorConfig mExteriorConfig;
};

static int const INTERIOR_DISTORTION_TEX_BIND_POINT = 0;
static string const PARAMS_FILE_LOCATION = "savedParams.json";

void SphereConfigApp::prepSettings(Settings * settings) {
	settings->setTitle("Sphere Configuration");
	settings->setFullScreen();
	settings->setHighDensityDisplayEnabled();
}

void SphereConfigApp::setup()
{
	// GL stuff for basic sane 3D rendering
	gl::enableDepth();
	gl::enableFaceCulling();
	gl::cullFace(GL_BACK);

	// Default projector config values
	mExteriorConfig.projectors.push_back(getAcerP5515MaxZoom().moveTo(vec3(5.0, 0, 0)));
	mExteriorConfig.projectors.push_back(getAcerP5515MaxZoom().moveTo(vec3(5.0, 0, 2 * M_PI * 1 / 3)));
	mExteriorConfig.projectors.push_back(getAcerP5515MaxZoom().moveTo(vec3(5.0, 0, 2 * M_PI * 2 / 3)));

	// Load parameters from saved file
	try {
		JsonTree appParams = loadParams();

		mExteriorConfig.projectors[0] = parseProjectorParams(appParams.getChild("projectors").getChild(0));
		mExteriorConfig.projectors[1] = parseProjectorParams(appParams.getChild("projectors").getChild(1));
		mExteriorConfig.projectors[2] = parseProjectorParams(appParams.getChild("projectors").getChild(2));

		mExteriorConfig.sphereApexHeight = appParams.getValueForKey<float>("sphereApexHeight");

		// Interior config stuff
		mInteriorConfig.cameraFov = appParams.getChild("fov").getValue<float>();
		mInteriorConfig.distortionPower = appParams.getChild("distortionPower").getValue<float>();
	} catch (ResourceLoadExc exc) {
		console() << "Failed to load parameters - they probably don't exist yet : " << exc.what() << std::endl;
	} catch (JsonTree::ExcChildNotFound exc) {
		console() << "Failed to load one of the JsonTree children: " << exc.what() << std::endl;
	}

	// Setup params
	mParams = params::InterfaceGl::create(getWindow(), "App parameters", toPixels(ivec2(200, getWindowHeight() - 20)));

	mParams->addParam("Configuration Mode", {
		"Internal Config",
		"External Config"
	}, [this] (int cfigMode) {
		switch (cfigMode) {
			case 0: mConfigMode = ConfigMode::Interior; break;
			case 1: mConfigMode = ConfigMode::Exterior; break;
			default: throw std::invalid_argument("invalid config mode parameter");
		}
	}, [this] () {
		// clang++ checks this statement for completeness and has a warning if not all enums are covered... nice! :)
		switch (mConfigMode) {
			case ConfigMode::Interior: return 0;
			case ConfigMode::Exterior: return 1;
		}
	});

	// ^^^^ Have to set up the params before the CameraUI, or else things get screwy

	// Setup graticule mesh
	// Move the center down by 1 so that it can be positioned from the top point
	auto baseMesh = bmesh::makeGraticule(vec3(0, -1.0, 0), 1.0);
	mGraticuleMesh = bmeshToVBOMesh(baseMesh);

	// Exterior view
	mExteriorCamera.lookAt(vec3(0, 0, 10), vec3(0), vec3(0, 1, 0));
	mExteriorCamera.setAspectRatio(getWindowAspectRatio());
	mExteriorUiCamera = CameraUi(& mExteriorCamera, getWindow());

	mParams->addParam("Render Overview", & mExteriorConfig.renderOverview);
	mParams->addParam("Projector POV", {
		"Projector 1", "Projector 2", "Projector 3"
	}, & mExteriorConfig.projectorPov);
	mParams->addParam("Sphere Apex Height", & mExteriorConfig.sphereApexHeight).min(2.0).max(5.0).precision(2).step(0.01f);

	mParams->addSeparator();

	for (int i = 0; i < mExteriorConfig.projectors.size(); i++) {
		string projectorName = "Projector " + std::to_string(i + 1);

		mParams->addParam(projectorName + " Position", std::function<void (vec3)>([this, i] (vec3 projPos) {
			mExteriorConfig.projectors[i].moveTo(projPos);
		}), std::function<vec3 ()>([this, i] () {
			return mExteriorConfig.projectors[i].getPos();
		}));

		mParams->addParam(projectorName + " Flipped", std::function<void (bool)>([this, i] (bool isFlipped) {
			mExteriorConfig.projectors[i].setUpsideDown(isFlipped);
		}), std::function<bool ()>([this, i] () {
			return mExteriorConfig.projectors[i].getUpsideDown();
		}));
	}

	mParams->addSeparator();

	// Interior view
	ivec2 displaySize = toPixels(getWindowSize());
	mMinSidePixels = min(displaySize.x, displaySize.y);

	// mInteriorCamera.setAspectRatio(getWindowAspectRatio());
	mInteriorCamera = CameraPersp(mMinSidePixels, mMinSidePixels, 35, 0.001f, 10.f);
	mInteriorCamera.lookAt(vec3(0, 0, 0), vec3(0, -1, 0), vec3(0, 0, 1));

	// Set up interior distortion rendering
	mInteriorDistortionFbo = gl::Fbo::create(mMinSidePixels, mMinSidePixels);
	mInteriorDistortionShader = gl::GlslProg::create(loadAsset("passThrough_v.glsl"), loadAsset("distortion_f.glsl"));
	mInteriorDistortionShader->uniform("uTex0", INTERIOR_DISTORTION_TEX_BIND_POINT);

	mParams->addParam("Camera FOV", & mInteriorConfig.cameraFov).min(60.f).max(180.f).precision(3).step(0.1f);
	mParams->addParam("Distortion Power", & mInteriorConfig.distortionPower).min(0.0f).max(4.0f).precision(5).step(0.001f);
}

void SphereConfigApp::mouseDown( MouseEvent event )
{
}

void SphereConfigApp::keyDown(KeyEvent event) {
	if (event.getCode() == KeyEvent::KEY_ESCAPE) {
		quit();
	} else if (event.getCode() == KeyEvent::KEY_SPACE) {
		saveParams();
	} else if (event.getCode() == KeyEvent::KEY_p) {
		mParams->maximize(!mParams->isMaximized());
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
			gl::ScopedViewport scpViewport(0, 0, mMinSidePixels, mMinSidePixels);
			gl::clear(Color(0, 0, 0));
			gl::ScopedMatrices scpMat;
			gl::setMatrices(mInteriorCamera);
			gl::draw(mGraticuleMesh);
		}

		{
			gl::ScopedViewport scpViewport((toPixels(getWindowWidth()) - mMinSidePixels) / 2, (toPixels(getWindowHeight()) - mMinSidePixels) / 2, mMinSidePixels, mMinSidePixels);
			gl::clear(Color(0, 0, 0));

			gl::ScopedGlslProg scpShader(mInteriorDistortionShader);
			gl::ScopedTextureBind scpTex(mInteriorDistortionFbo->getColorTexture(), INTERIOR_DISTORTION_TEX_BIND_POINT);

			gl::ScopedMatrices scpMat;
			gl::setMatricesWindow(mMinSidePixels, mMinSidePixels);
			gl::drawSolidRect(Rectf(0.0, 0.0, mMinSidePixels, mMinSidePixels));
		}
	} else if (mConfigMode == ConfigMode::Exterior) {
		gl::clear(Color(0, 0, 0));

		if (mExteriorConfig.renderOverview) {
			gl::ScopedMatrices scpMat;
			gl::setMatrices(mExteriorCamera);

			{
				gl::ScopedMatrices innerScope;
				gl::translate(0, mExteriorConfig.sphereApexHeight, 0);
				gl::draw(mGraticuleMesh);
			}

			{
				// For debugging: draw a plane at ground level
				gl::ScopedColor scpColor(Color(0.2, 0.4, 0.8));
				gl::draw(geom::WirePlane().subdivisions(ivec2(10, 10)).size(vec2(10.0, 10.0)));
			}

			for (auto & proj : mExteriorConfig.projectors) {
				proj.draw();
			}
		} else {
			Projector & viewProjector = mExteriorConfig.projectors[mExteriorConfig.projectorPov];

			gl::ScopedMatrices scpMat;
			gl::setViewMatrix(viewProjector.getViewMatrix());
			gl::setProjectionMatrix(viewProjector.getProjectionMatrix());

			gl::translate(0, mExteriorConfig.sphereApexHeight, 0);
			gl::draw(mGraticuleMesh);
		}
	}

	mParams->draw();
}

JsonTree SphereConfigApp::loadParams() {
	return JsonTree(loadResource(PARAMS_FILE_LOCATION));
}

vec3 parseVector(JsonTree vt) {
	return vec3(vt.getValueAtIndex<float>(0), vt.getValueAtIndex<float>(1), vt.getValueAtIndex<float>(2));
}

JsonTree serializeVector(string name, vec3 v) {
	return JsonTree::makeArray(name)
		.addChild(JsonTree("", v.x))
		.addChild(JsonTree("", v.y))
		.addChild(JsonTree("", v.z));
}

Projector parseProjectorParams(JsonTree params) {
	return Projector()
		.setHorFOV(params.getValueForKey<float>("horFOV"))
		.setVertFOV(params.getValueForKey<float>("vertFOV"))
		.setVertBaseAngle(params.getValueForKey<float>("baseAngle"))
		.moveTo(parseVector(params.getChild("position")))
		.setUpsideDown(params.getValueForKey<bool>("isUpsideDown"));
}

JsonTree serializeProjector(Projector proj) {
	return JsonTree()
		.addChild(JsonTree("horFOV", proj.getHorFOV()))
		.addChild(JsonTree("vertFOV", proj.getVertFOV()))
		.addChild(JsonTree("baseAngle", proj.getVertBaseAngle()))
		.addChild(serializeVector("position", proj.getPos()))
		.addChild(JsonTree("isUpsideDown", proj.getUpsideDown()));
}

void SphereConfigApp::saveParams() {
	JsonTree appParams;

	appParams.addChild(JsonTree("fov", mInteriorConfig.cameraFov));
	appParams.addChild(JsonTree("distortionPower", mInteriorConfig.distortionPower));
	appParams.addChild(
		JsonTree::makeArray("projectors")
			.addChild(serializeProjector(mExteriorConfig.projectors[0]))
			.addChild(serializeProjector(mExteriorConfig.projectors[1]))
			.addChild(serializeProjector(mExteriorConfig.projectors[2]))
	);
	appParams.addChild(JsonTree("sphereApexHeight", mExteriorConfig.sphereApexHeight));

	string serializedParams = appParams.serialize();
	std::ofstream writeFile;

	string appOwnFile = getResourcePath(PARAMS_FILE_LOCATION).string();
	writeFile.open(appOwnFile);
	std::cout << "writing params to: " << appOwnFile << std::endl;
	writeFile << serializedParams;
	writeFile.close();

	string repoFile = fs::canonical("../../../resources/" + PARAMS_FILE_LOCATION).string();
	writeFile.open(repoFile);
	std::cout << "writing params to: " << repoFile << std::endl;
	writeFile << serializedParams;
	writeFile.close();
}

CINDER_APP( SphereConfigApp, RendererGl, & SphereConfigApp::prepSettings )
