
#ifndef RENDERER_GPU_LIGHTPOOL_H
#define RENDERER_GPU_LIGHTPOOL_H

#include <map>
#include <memory>

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>

#include <pugixml.hpp>

#include "lightdata.h"

class TexturePool;


class LightPool
{
public:
    ~LightPool();

    void load(const pugi::xml_node &node);

    bool update();
    void updateParameters();
    static LightPool& getInstance(optix::Context context);

private:
    LightPool() : m_context(nullptr), m_lightsChanged(true) {}
    void setContext(optix::Context context);

    void updateLightBuffer();
    void clearEnvironmentLight();

    optix::Context m_context;

    std::map<std::string, optix::Program> m_programMap;
    optix::Buffer m_bufferLights;
    optix::Buffer m_bufferSampleLight;

    std::map<std::string, LightDefinition> m_lightMap;

    std::shared_ptr<TexturePool> m_environmentTexture;
    bool m_lightsChanged;
};


#endif //RENDERER_GPU_LIGHTPOOL_H
