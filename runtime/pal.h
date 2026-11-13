#pragma once

#if defined(_WIN32) || defined(_WIN64)
#ifndef _WIN32
#define _WIN32 1
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef HMODULE sph_lib_handle;
#define SPH_LIB_OPEN(name) LoadLibraryA(name)
#define SPH_LIB_SYM(handle, sym) GetProcAddress((HMODULE)(handle), sym)
#define SPH_LIB_CLOSE(handle) FreeLibrary((HMODULE)(handle))

#else
// POSIX (Linux, macOS, BSD)
#include <pthread.h>
#include <unistd.h>
#include <time.h>
#include <dlfcn.h>

typedef void* sph_lib_handle;
#define SPH_LIB_OPEN(name) dlopen(name, RTLD_NOW)
#define SPH_LIB_SYM(handle, sym) dlsym(handle, sym)
#define SPH_LIB_CLOSE(handle) dlclose(handle)
#endif

