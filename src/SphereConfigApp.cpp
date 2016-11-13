#include "cinder/app/App.h"
#include "cinder/app/RendererGl.h"
#include "cinder/gl/gl.h"
#include "cinder/Camera.h"
#include "cinder/CameraUi.h"
#include "cinder/params/Params.h"

#include "buildmesh.h"

using namespace ci;
using namespace ci::app;
using namespace std;

// vertAng is an angle relative to the vertical y axis (0 - π), horAng is rotation around the y axis (0 - 2π)
vec3 getPointOnSphere(float vertAng, float horAng) {
	return vec3(sin(vertAng) * cos(horAng), cos(vertAng), sin(vertAng) * sin(horAng));
}

bmesh::MeshRef makeGraticule(vec3 center, float radius) {
	bmesh::MeshRef theMesh = std::make_shared<bmesh::Mesh>(bmesh::Primitive::Lines);

	int subdivisions = 24;
	float aperturesize = glm::pi<float>() / subdivisions;
	float vertChg = (glm::pi<float>() - 2 * aperturesize) / subdivisions;
	float horizChg = glm::two_pi<float>() / subdivisions;

	for (int latitude = 0; latitude <= subdivisions; latitude++) {
		for (int longitude = 0; longitude <= subdivisions; longitude++) {
			uint vertexNumber = theMesh->getNumVertices();
			float latAngle = aperturesize + latitude * vertChg;
			vec3 posVector = normalize(getPointOnSphere(latAngle, longitude * horizChg));
			vec3 vertexPos = center + radius * posVector;

			vec3 color;
			float tval = latAngle / (subdivisions * vertChg);
			color = mix(vec3(1, 0, 1), vec3(0, 1, 0), glm::smoothstep(0.0f, 0.5f, tval));
			color = mix(color, vec3(1, 1, 0), glm::smoothstep(0.5f, 1.0f, tval));

			theMesh->addVertex(bmesh::Vertex().position(vertexPos).color(color));

			if (longitude != 0) {
				// Connect to previous
				theMesh->addIndex(vertexNumber);
				theMesh->addIndex(vertexNumber - 1);
			}

			if (latitude != 0) {
				// Connect to above
				theMesh->addIndex(vertexNumber);
				// Gotta add one here, since there are (subdivisions + 1) vertices per ring
				theMesh->addIndex(vertexNumber - (subdivisions + 1));
			}
		}
	}

	return theMesh;
}

GLenum bmeshPrimitive(bmesh::Primitive prim) {
	GLenum primitive;
	switch (prim) {
		case bmesh::Primitive::Lines:
			primitive = GL_LINES;
		break;
		case bmesh::Primitive::Triangles:
			primitive = GL_TRIANGLES;
		break;
		default:
			primitive = GL_TRIANGLES; // Default to triangles because meh
		break;
	}
	return primitive;
}

gl::VboMeshRef bmeshToVBOMesh(bmesh::MeshRef theMesh) {
	static size_t vertexSize = bmesh::Mesh::getVertexSize();
	static size_t indexSize = bmesh::Mesh::getIndexSize();
	static geom::BufferLayout bmeshBufferLayout = geom::BufferLayout({
		geom::AttribInfo(geom::Attrib::POSITION, 3, vertexSize, offsetof(bmesh::Vertex, mPosition)),
		geom::AttribInfo(geom::Attrib::NORMAL, 3, vertexSize, offsetof(bmesh::Vertex, mNormal)),
		geom::AttribInfo(geom::Attrib::COLOR, 3, vertexSize, offsetof(bmesh::Vertex, mColor)),
		geom::AttribInfo(geom::Attrib::TEX_COORD_0, 2, vertexSize, offsetof(bmesh::Vertex, mTexCoord0)),
		geom::AttribInfo(geom::Attrib::TANGENT, 3, vertexSize, offsetof(bmesh::Vertex, mTangent)),
		geom::AttribInfo(geom::Attrib::BITANGENT, 3, vertexSize, offsetof(bmesh::Vertex, mBitangent))
	});

	auto numVerts = theMesh->getNumVertices();
	auto primitive = bmeshPrimitive(theMesh->getPrimitive());
	auto vertexVBO = gl::Vbo::create(GL_ARRAY_BUFFER, numVerts * vertexSize, theMesh->vertexData(), GL_STATIC_DRAW);
	auto numIndices = theMesh->getNumIndices();
	auto indexVBO = gl::Vbo::create(GL_ELEMENT_ARRAY_BUFFER, numIndices * indexSize, theMesh->indexData());

	return gl::VboMesh::create(numVerts, primitive, { { bmeshBufferLayout, vertexVBO } }, numIndices, GL_UNSIGNED_INT, indexVBO);
}

class SphereConfigApp : public App {
  public:
	static void prepSettings(Settings * settings);

	void setup() override;
	void mouseDown( MouseEvent event ) override;
	void keyDown(KeyEvent event) override;
	void update() override;
	void draw() override;

	CameraPersp mCamera;
	CameraUi mUiCamera;

	float mCameraFov = 140.f;
	float mDistortionPower = 1.0f;

	params::InterfaceGlRef mParams;

	bmesh::MeshRef mGraticuleBaseMesh;
	gl::VboMeshRef mGraticuleMesh;

	gl::FboRef mDistortionFbo;
	gl::GlslProgRef mDistortionShader;
};

#define DISTORTION_TEX_BIND_POINT 0

void SphereConfigApp::prepSettings(Settings * settings) {
	settings->setFullScreen();
	settings->setTitle("Sphere Configuration");
	settings->setHighDensityDisplayEnabled();
}

void SphereConfigApp::setup()
{
	mCamera.lookAt(vec3(0, 1, 0), vec3(0, 0, 0), vec3(0, 0, 1));
	// mCamera.setAspectRatio(1); // someday...
	mCamera.setAspectRatio(getWindowAspectRatio());
	// mUiCamera = CameraUi(&mCamera, getWindow());

	mParams = params::InterfaceGl::create(getWindow(), "App parameters", toPixels(ivec2(200, 50)));

	mParams->addParam("Camera FOV", & mCameraFov).min(60.f).max(180.f).precision(3).step(1.0f);
	mParams->addParam("Distortion Power", & mDistortionPower).min(0.0f).max(4.0f).precision(5).step(0.001f);

	mGraticuleBaseMesh = makeGraticule(vec3(0, 0, 0), 1.0);
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

CINDER_APP( SphereConfigApp, RendererGl, & SphereConfigApp::prepSettings )
