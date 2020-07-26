#include "precompiled.h"

// based on https://www.shadertoy.com/view/MtSSRz

#include "util.h"
#include "stuff.h"
#include "shade.h"
#include "gpgpu.h"
#include "stefanfw.h"
#include "convolve_fft.h"
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
		pos.x = randFloat(0, sx);
		pos.y = randFloat(0, sy);
	}
	void update() {
		/*if (app::getElapsedFrames() % 100 == 0) {
			acc = ci::randVec2() * ci::randFloat() * .1f;
		}
		vel += acc;
		vel *= .8f;
		pos += vel;
		pos.x = fmod(pos.x, sx - 1);
		pos.y = fmod(pos.y, sy - 1);
		if (pos.x < 0) pos.x += sx - 1;
		if (pos.y < 0) pos.y += sy - 1;*/
		//vec2 displacement;
		float nscale = 10 / (float)sx; // both for x and y so we preserve aspect ratio
		displacement.x = raw_noise_4d(pos.x * nscale, pos.y * nscale, noiseTimeDim, 0.0) * 40.0f;
		displacement.y = raw_noise_4d(pos.x * nscale, pos.y * nscale, noiseTimeDim, 1.0) * 40.0f;
	}
};

vector<Walker> walkers;

struct SApp : App {
	void setup()
	{
		createConsole();
		enableDenormalFlushToZero();
		disableGLReadClamp();

		setWindowSize(wsx, wsy);

		stefanfw::eventHandler.subscribeToEvents(*this);

		for (int i = 0; i < 1000; i++) {
			walkers.push_back(Walker());
		}
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

		Array2D<float> img(sx, sy, 0);
		for (auto& walker : walkers) {
			//img.wr(walker.pos + walker.displacement) = 1.0f;
			aaPoint(img, walker.pos + walker.displacement, .1f);
		}

		auto invKernelf = getKernelf(img.Size(),
			[&](float f) { return 1.0f / pow(f, exponent); });
			//[&](float f) { return exp(-f * exponent); });
			//[&](float f) { return 1.0f / (1 + sq(f / 4.0f)); });
		auto longTailKernelf = getKernelf(img.Size(),
			[&](float f) { return 1.0f / (1 + f * f / 16); },
			true);

		auto convolvedLong = convolveFft(img, longTailKernelf);
		auto convolvedLongTex = gtex(convolvedLong);

		Array2D<vec2> sparseInfoArr(walkers.size(), 1, vec2());
		int i = 0;
		for (auto& walker : walkers) {
			sparseInfoArr(i, 0) = walker.pos;
			i++;
		}

		auto sparseInfoTex = gtex(sparseInfoArr);
		globaldict["imgWidth"] = img.w;
		tex = shade2(convolvedLongTex, sparseInfoTex,
			"float accum = 0.0;"
			"vec2 here = gl_FragCoord.xy;"
			"for(int i = 0; i < textureSize(tex2, 0).x; i++) {"
			"	vec2 walkerPos = texelFetch(tex2, ivec2(i, 0), 0).xy;" // CORRECT?
			"	float blurWidth = texelFetch(tex, ivec2(walkerPos), 0).x;"
			"	float blurWidthBak = blurWidth;"
			"	blurWidth = blurWidth *mouse.y* 100000;"
			"	float dist = distance(here, walkerPos);"
			//"	accum += 1.0 / (1.0 + dist * dist *(blurWidth*blurWidth)) * blurWidthBak;" // todo: NORMALIZED KERNEL
			"	accum += exp(-dist*dist*(blurWidth*blurWidth)) * blurWidthBak;"
			"}"
			"_out = vec3(accum);"
			, ShadeOpts().ifmt(GL_R32F)
		);
		auto texDld = gettexdata<float>(tex, GL_RED, GL_FLOAT);
		::mm(texDld, "texDld");
		texDld = ::to01(texDld);
		tex = gtex(texDld);
	}
	void stefanDraw() {
		//static auto tex = maketex(sx, sy, GL_R16F);
		//gl::draw(tex, getWindowBounds());
		/*gl::clear();
		gl::begin(GL_POINTS);
		gl::color(Color::white());
		for (auto& walker : walkers) {
			gl::vertex(walker.pos);
		}
		gl::end();*/
		auto tex2 = shade2(tex,
			"float f = fetch1();"
			//"_out.r = f / (f + 1.0f);");
			//"for(int i = 0; i < 10; i++) f = smoothstep(0, 1, f);"
			//"float fw = fwidth(f);"
			//"f = smoothstep(0.6 - fw/2, .6 + fw/2, f);"
			"_out.r = f;");
		gl::clear();
		gl::setMatricesWindow(getWindowSize(), false);
		gl::draw(tex2, getWindowBounds());
	}
};

CINDER_APP(SApp, RendererGl)