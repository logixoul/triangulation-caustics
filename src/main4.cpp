#include "precompiled.h"

// based on https://www.shadertoy.com/view/MtSSRz

#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "stefanfw.h"
#include "simplexnoise.h"
int wsx = 1280, wsy = 720;
int scale = 4;
int sx = wsx / ::scale;
int sy = wsy / ::scale;

float noiseTimeDim = 0;

bool pause;

gl::TextureRef tex;

struct Walker {
	vec2 pos;
	vec2 vel;
	vec2 acc;
	vec2 displacement;
	Walker() {
	}
	void update() {
		float nscale = 10 / (float)sx; // both for x and y so we preserve aspect ratio
		displacement.x = raw_noise_4d(pos.x * nscale, pos.y * nscale, noiseTimeDim, 0.0) * 40.0f;
		displacement.y = raw_noise_4d(pos.x * nscale, pos.y * nscale, noiseTimeDim, 1.0) * 40.0f;
	}
};

Array2D<Walker> walkers;

gl::GlslProgRef prog;
gl::VboMeshRef	vboMesh;

struct SApp : App {
	void setup()
	{
		createConsole();
		enableDenormalFlushToZero();
		disableGLReadClamp();

		setWindowSize(wsx, wsy);

		stefanfw::eventHandler.subscribeToEvents(*this);

		walkers = Array2D<Walker>(sx+1, sy+1, Walker());
		forxy(walkers) {
			walkers(p).pos = p;
		}

		string vs = Str()
			<< "#version 150"
			<< "uniform mat4 ciModelViewProjection;"
			<< "in vec4 ciPosition;"
		
			<< "void main()"
			<< "{"
			<< "	gl_Position = ciModelViewProjection * ciPosition;"
			<< "}";

		auto plane = geom::Plane().size(vec2(wsx, wsy)).subdivisions(ivec2(sx, sy))
			.axes(vec3(1,0,0), vec3(0,1,0));
		vector<gl::VboMesh::Layout> bufferLayout = {
			gl::VboMesh::Layout().usage(GL_DYNAMIC_DRAW).attrib(geom::Attrib::POSITION, 3)
		};

		vboMesh = gl::VboMesh::create(plane, bufferLayout);

	}
	void update()
	{
		stefanfw::beginFrame();
		stefanUpdate();
		stefanDraw();
		stefanfw::endFrame();
	}

	void keyDown(KeyEvent e)
	{
		if(keys['p'] || keys['2'])
		{
			pause = !pause;
		}
	}

	void stefanUpdate() {
		float exponent = cfg1::getOpt("exponent", 2,
			[&]() { return keys['e']; }, [&]() { return constrain<float>(mouseY, 0.0f, 1.0f) * 5; }
		);

		if (pause) {
			std::this_thread::sleep_for(50ms);
			return;
		}

		noiseTimeDim += .008f;
		for (auto& walker: walkers) {
			walker.update();
		}
		updateMesh();
		
		//tex = gtex(texDld);
	}
	void updateMesh() {

		auto mappedPosAttrib = vboMesh->mapAttrib3f(geom::Attrib::POSITION, false);
		for (int i = 0; i <= sx; i++) {
			for (int j = 0; j <= sy; j++) {
				//for (int i = 0; i < vboMesh->getNumVertices(); i++) {
				vec3 &pos = *mappedPosAttrib;
				//vec3 pos = vec3(walkers(i).pos, 0);
				auto walker = walkers(i, j);
				mappedPosAttrib->x = walker.pos.x + walker.displacement.x;// +sin(pos.x) * 50;
				mappedPosAttrib->y = walker.pos.y + walker.displacement.y;// +sin(pos.y) * 50;
				mappedPosAttrib->z = 0;
				++mappedPosAttrib;
			}
		}
		mappedPosAttrib.unmap();
	}

	void stefanDraw() {
		glDisable(GL_CULL_FACE);
		
		auto target = maketex(wsx, wsy, GL_RGB32F);
		::beginRTT(target);

		gl::setMatricesWindow(vec2(sx, sy), false);
		gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gl::disableDepthRead();
		gl::color(Colorf(.3*mouseY,0,0));
		gl::enableAdditiveBlending();
		{
			gl::ScopedGlslProg glslScope(gl::getStockShader(gl::ShaderDef().color()));
			gl::draw(vboMesh);
		}
		::endRTT();

		gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gl::setMatricesWindow(vec2(wsx, wsy), false);
		gl::draw(target, getWindowBounds());
	}
};

CINDER_APP(SApp, RendererGl)