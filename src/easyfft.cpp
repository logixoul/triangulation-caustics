#include "precompiled.h"
#include "easyfft.h"

class PlanCache {
public:
	static fftwf_plan getPlan(ivec2 arrSize, int direction, int flags) {
		Key key = { arrSize, direction };
		if(cache.find(key) == cache.end()) {
			Array2D<vec2> in(arrSize, nofill());
			Array2D<vec2> out(arrSize, nofill());
			auto plan = fftwf_plan_dft_2d(arrSize.y, arrSize.x, (fftwf_complex*)in.data, (fftwf_complex*)out.data, direction, flags);
			cache[key] = plan;
			return plan;
		}
		return cache[key];
	}
private:
	struct Key {
		ivec2 arrSize;
		int direction;
	};
	struct KeyComparator {
		bool operator()(Key const& l, Key const& r) const {
			auto tiedL = std::tie(l.arrSize.x, l.arrSize.y, l.direction);
			auto tiedR = std::tie(r.arrSize.x, r.arrSize.y, r.direction);
			return tiedL < tiedR;
		}
	};
	static std::map<Key, fftwf_plan, KeyComparator> cache;
};

std::map<PlanCache::Key, fftwf_plan, PlanCache::KeyComparator> PlanCache::cache;

Array2D<vec2> fft(Array2D<float> in, int flags)
{
	Array2D<vec2> in_complex(in.Size());
	forxy(in)
	{
		in_complex(p) = vec2(in(p), 0.0f);
	}
	Array2D<vec2> result(in.Size());
	
	auto plan = PlanCache::getPlan(in.Size(), FFTW_FORWARD, flags);
	fftwf_execute_dft(plan, (fftwf_complex*)in_complex.data, (fftwf_complex*)result.data);
	auto mul = 1.0f / sqrt((float)result.area);
	forxy(result)
	{
		result(p) *= mul;
	}
	return result;
}

Array2D<float> ifft(Array2D<vec2> in, int flags)
{
	Array2D<vec2> result(in.Size());
	auto plan = PlanCache::getPlan(in.Size(), FFTW_BACKWARD, flags);
	fftwf_execute_dft(plan, (fftwf_complex*)in.data, (fftwf_complex*)result.data);

	Array2D<float> out_real(in.Size());
	forxy(in)
	{
		out_real(p) = result(p).x;
	}
	auto mul = 1.0f / sqrt((float)out_real.area);
	forxy(out_real)
	{
		out_real(p) *= mul;
	}
	return out_real;
}