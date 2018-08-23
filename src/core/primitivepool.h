
#ifndef RENDERER_GPU_PRIMITIVEPOOL_H
#define RENDERER_GPU_PRIMITIVEPOOL_H

#include <optixu/optixpp_namespace.h>
#include <optix_world.h>
#include <pugixml.hpp>

#include <map>


#include "geometrypool.h"
#include "materialpool.h"

struct PrimitiveData
{
    optix::Geometry geometry;
    optix::Material material;
    optix::GeometryInstance instance;
    optix::Acceleration acceleration;
    optix::GeometryGroup geometryGroup;
    optix::Matrix4x4 transformMatrix;
    optix::Transform transform;

    std::string geometryName;
    std::string materialName;

    PrimitiveData() : geometry(nullptr), material(nullptr), instance(nullptr),
        acceleration(nullptr), geometryGroup(nullptr), transformMatrix(), transform() {}

    void destroy(){
        if (instance && instance->get())
            instance->destroy();
        if (acceleration && acceleration->get())
            acceleration->destroy();
        if (geometryGroup && geometryGroup->get())
            geometryGroup->destroy();
        if (transform && transform->get())
            transform->destroy();
        geometry = nullptr;
        material = nullptr;
        instance = nullptr;
        acceleration = nullptr;
        geometryGroup = nullptr;
        transform = nullptr;
        transformMatrix = optix::Matrix4x4();
    }
};

class PrimitivePool
{
public:
    ~PrimitivePool();

    void load(const pugi::xml_node &node);
    void save(pugi::xml_node &node);

    void updateParameters();
    bool update();

    static PrimitivePool& getInstance(optix::Context context);

private:
    PrimitivePool() : m_context(nullptr) {}
    void setContext(optix::Context context);


    bool loadPrimitive(const pugi::xml_node &node, const std::string &name);
    bool unloadPrimitive(const std::string &name);

    optix::Context m_context;
    optix::Group        m_rootGroup;
    optix::Acceleration m_rootAcceleration;
    std::map<std::string, optix::Program> m_programMap;

    std::map<std::string, PrimitiveData> m_primitives;
//
//    GeometryPool m_geometryPool;
//    MaterialPool m_materialPool;
};

#endif //RENDERER_GPU_PRIMITIVEPOOL_H
