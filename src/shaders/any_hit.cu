

#include <optix.h>

#include "../core/perraydata.h"

rtDeclareVariable(PerRayData_shadow, thePrdShadow, rtPayload, );

// The shadow ray program for all materials with no cutout opacity.
RT_PROGRAM void any_hit()
{
    thePrdShadow.visible = false;
    rtTerminateRay();
}