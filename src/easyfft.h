#pragma once
#include "util.h"
#include "precompiled.h"

Array2D<vec2> fft(Array2D<float> in, int flags);
Array2D<float> ifft(Array2D<vec2> in, int flags);