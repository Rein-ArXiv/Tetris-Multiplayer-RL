#pragma once
#include "fake_xaudio.h"
// Model the SDK's default macro pollution so missing NOMINMAX is observable.
#ifndef NOMINMAX
#define min(a,b) (((a)<(b))?(a):(b))
#define max(a,b) (((a)>(b))?(a):(b))
#endif
