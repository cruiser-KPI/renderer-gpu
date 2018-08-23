
#ifndef RENDERER_GPU_CONFIG_H
#define RENDERER_GPU_CONFIG_H

#ifndef NDEBUG

#define USE_DEBUG_EXCEPTIONS
#endif

#include <iostream>
#include <string>

// For rtDevice*() function error checking. No OptiX context present at that time.
#define RT_CHECK_ERROR_NO_CONTEXT(func) \
  do { \
    RTresult code = func; \
    if (code != RT_SUCCESS) \
      std::cerr << "ERROR: Function " << #func << std::endl; \
  } while (0)


#ifndef RT_FUNCTION
#define RT_FUNCTION __forceinline__ __device__
#endif


static std::string shaderFolder =
#ifdef NDEBUG
    "./objects-Release/CudaPTX/src/shaders/";
#else
    "./objects-Debug/CudaPTX/src/shaders/";
#endif


#endif //RENDERER_GPU_CONFIG_H
