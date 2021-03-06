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
Array2D<float> base;

struct Walker {
	vec2 pos;
	vec2 displacement;
	Walker() {
	}
	void update() {
		float nscale = 10 / (float)sx; // both for x and y so we preserve aspect ratio
		displacement.x = raw_noise_4d(pos.x * nscale, pos.y * nscale, noiseTimeDim, 0.0) * 40.0f*mouseX;
		displacement.y = raw_noise_4d(pos.x * nscale, pos.y * nscale, noiseTimeDim, 1.0) * 40.0f*mouseX;
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
		base = Array2D<float>(sx + 1, sy + 1, 0.0f);
		forxy(walkers) {
			walkers(p).pos = p;
		}

		auto plane = geom::Plane().size(vec2(wsx, wsy)).subdivisions(ivec2(sx, sy))
			.axes(vec3(1,0,0), vec3(0,1,0));
		vector<gl::VboMesh::Layout> bufferLayout = {
			gl::VboMesh::Layout().usage(GL_DYNAMIC_DRAW).attrib(geom::Attrib::POSITION, 3),
			gl::VboMesh::Layout().usage(GL_DYNAMIC_DRAW).attrib(geom::Attrib::CUSTOM_0, 1)
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
	void updateBase() {
		forxy(base) {
			auto pos = p;
			float nscale = 10 / (float)sx; // both for x and y so we preserve aspect ratio
			base(p) = raw_noise_4d(pos.x * nscale, pos.y * nscale, noiseTimeDim, 0.0) * 40.0f*mouseX;
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
		updateBase();

		for (auto& walker: walkers) {
			//walker.update();
		}
		updateMesh();
		
		//tex = gtex(texDld);
	}
	void updateMesh() {

		auto grads = ::get_gradients(base);

		auto mappedPosAttrib = vboMesh->mapAttrib3f(geom::Attrib::POSITION, false);
		for (int i = 0; i <= sx; i++) {
			for (int j = 0; j <= sy; j++) {
				//for (int i = 0; i < vboMesh->getNumVertices(); i++) {
				vec3 &pos = *mappedPosAttrib;
				//vec3 pos = vec3(walkers(i).pos, 0);
				auto walker = walkers(i, j);
				mappedPosAttrib->x = walker.pos.x + grads(i, j).x;// +sin(pos.x) * 50;
				mappedPosAttrib->y = walker.pos.y + grads(i, j).y;// +sin(pos.y) * 50;
				mappedPosAttrib->z = 0;
				++mappedPosAttrib;
			}
		}
		mappedPosAttrib.unmap();

		auto mappedCustom0Attrib = vboMesh->mapAttrib1f(geom::Attrib::CUSTOM_0, false);
		for (int i = 0; i <= sx; i++) {
			for (int j = 0; j <= sy; j++) {
				//for (int i = 0; i < vboMesh->getNumVertices(); i++) {
				float &pos = *mappedCustom0Attrib;
				//vec3 pos = vec3(walkers(i).pos, 0);
				//auto walker = walkers(i, j);
				auto getDisp = [&](int i, int j) { return grads.wr(i, j); };
				float div =
					(getDisp(i+1, j).x - getDisp(i-1,j).x)
					+ (getDisp(i, j + 1).y - getDisp(i, j - 1).y);
				//div += 10 * mouseX;
				//cout << "div=" << div << endl;
				//div *= -1;
				*mappedCustom0Attrib = max(0.0f, -div+2)*mouseY*2;
				++mappedCustom0Attrib;
			}
		}
		mappedCustom0Attrib.unmap();
	}

	void stefanDraw() {
		glDisable(GL_CULL_FACE);
		
		auto target = maketex(wsx, wsy, GL_RGB32F);
		::beginRTT(target);

		gl::setMatricesWindow(vec2(sx, sy), false);
		gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gl::disableDepthRead();
		gl::enableAdditiveBlending();
		{
			auto vs = CI_GLSL(150,
				uniform mat4 ciModelViewProjection;

			in vec4 ciPosition;
			in vec4 inCustom0;
			out lowp vec4 Color;
			void main(void)
			{
				gl_Position = ciModelViewProjection * ciPosition;
				Color = inCustom0;
			}
			);
			auto fs = CI_GLSL(150,
				out vec4 oColor;
				in vec4 Color;
				void main(void)
				{
					oColor = Color;
				}
			);
			//gl::ScopedGlslProg glslScope(gl::getStockShader(gl::ShaderDef().color()));
			static auto prog = gl::GlslProg::create(
				gl::GlslProg::Format()
				.attrib(geom::Attrib::CUSTOM_0, "inCustom0")
				.fragment(fs)
				.vertex(vs)
				);
			gl::ScopedGlslProg glslScope(prog);
			gl::draw(vboMesh);
		}
		::endRTT();
		gl::disableBlending();
		target = shade2(target,
			"float f = fetch1();"
			//"f+=.2f;"
			//"f /= f + 1;"
			"vec3 c = pow(vec3(f), vec3(7, 2, 1));"
			//"c = max(vec3(0), min(vec3(1), c);"
			//"c = pow(c, vec3(1.0/2.2));" // gamma
			"_out.rgb=c;");

		gl::clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		gl::setMatricesWindow(vec2(wsx, wsy), false);
		//gl::color(Colorf(.3*mouseY, .3*mouseY, .3*mouseY));
		gl::draw(target, getWindowBounds());
		gl::color(Colorf(1,1,1));
	}
};

CINDER_APP(SApp, RendererGl)