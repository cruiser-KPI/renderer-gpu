
#include "../utils/config.h"
#include <optix.h>
#include <optixu/optixu_math_namespace.h>
using namespace optix;

rtBuffer<float4, 2> sysOutputBuffer; // RGBA32F

rtDeclareVariable(uint2, theLaunchIndex, rtLaunchIndex, );
rtDeclareVariable(uint2, tileOffset, , );

RT_PROGRAM void exception()
{
#ifdef USE_DEBUG_EXCEPTIONS
  const unsigned int code = rtGetExceptionCode();
  const uint2 tileLaunchIndex = tileOffset + theLaunchIndex;
  if (RT_EXCEPTION_USER <= code)
  {
    rtPrintf("User exception %d at (%d, %d)\n", code - RT_EXCEPTION_USER, tileLaunchIndex.x, tileLaunchIndex.y);
  }
  else
  {
    rtPrintf("Exception code 0x%X at (%d, %d)\n", code, tileLaunchIndex.x, tileLaunchIndex.y);
  }
  rtPrintExceptionDetails();
  // RGBA32F super magenta as error color (makes sure this isn't accumulated away in a progressive renderer).

  sysOutputBuffer[tileLaunchIndex] = make_float4(1000000.0f, 0.0f, 1000000.0f, 1.0f);
#endif
}
