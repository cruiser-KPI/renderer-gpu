
#ifndef RENDERER_GPU_FILEUTIL_H
#define RENDERER_GPU_FILEUTIL_H

#include <pugixml.hpp>

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>
#include <optix_world.h>

std::string readString(const pugi::xml_node &node, std::string def = std::string());
int readInt(const pugi::xml_node &node, int def = 0);
float readFloat(const pugi::xml_node &node, float def = 0.0f);
optix::float3 readVector3(const pugi::xml_node &node, optix::float3 def = optix::make_float3(0.0f));
optix::Matrix4x4 readTransform(const pugi::xml_node &node);

#endif //RENDERER_GPU_FILEUTIL_H
