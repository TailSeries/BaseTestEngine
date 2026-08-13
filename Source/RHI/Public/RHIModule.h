#pragma once

#ifdef RHI_LIBRARY
#define RHIMODULE __declspec(dllexport)
#else
#define RHIMODULE __declspec(dllimport)
#endif