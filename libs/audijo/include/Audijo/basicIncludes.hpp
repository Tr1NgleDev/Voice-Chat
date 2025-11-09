// initially called pch.hpp but i ain't using precompiled headers nuh uh

#pragma once

#include <windows.h>
#ifndef FAR
#define FAR
#endif
#ifndef NEAR
#define NEAR
#endif

#include <process.h>
#include <stdint.h>

#ifndef INITGUID
#define INITGUID
#endif

#include <audioclient.h>
#include <avrt.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>

#undef min
#undef max

#include <vector>
#include <cassert>
#include <concepts>
#include <algorithm>
#include <iostream>
#include <cmath>
#include <thread>

#define LOGL(x) std::cout << x << std::endl
#define LOG(x) std::cout << x 
