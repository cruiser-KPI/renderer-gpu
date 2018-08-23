
#ifndef RENDERER_GPU_MATERIALPOOL_H
#define RENDERER_GPU_MATERIALPOOL_H

#include <map>

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>

#include <pugixml.hpp>

#include "materialdata.h"
#include "texture.h"

enum MaterialType
{
    DIFFUSE = 0,
    SPECULAR = 1,
    SPECULAR_TRANSIMISSION = 2
};

class MaterialPool
{
public:
    ~MaterialPool();

    optix::Material getMaterial(const pugi::xml_node &node, int &materialIndex, std::string &materialName);

    void load(const pugi::xml_node &node);

    void updateParameters(const std::string &materialName);
    bool update();

    static MaterialPool& getInstance(optix::Context context);

private:
    MaterialPool() : m_context(nullptr), m_changed(true) {}
    void setContext(optix::Context context);

    void updateMaterialBuffer();

    optix::Context m_context;

    std::map<std::string, optix::Program> m_programMap;
    optix::Material m_material;
    optix::Buffer m_materialBuffer;

    optix::Buffer m_bufferSampleBSDF;
    optix::Buffer m_bufferEvalBSDF;

    std::map<std::string, unsigned int> m_materialIndices;
    std::map<std::string, MaterialParameter> m_materialMap;

    std::vector<const char *> m_materialNames;

    bool m_changed;
};


#endif //RENDERER_GPU_MATERIALPOOL_H
