
#ifndef RENDERER_GPU_GEOMETRYPOOL_H
#define RENDERER_GPU_GEOMETRYPOOL_H

#include <map>

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>

#include <pugixml.hpp>

#include "vertexattributes.h"

struct GeometryData
{
    optix::Geometry geometry;
    optix::Buffer buffer;
    std::string mesh_name;

    GeometryData() : geometry(nullptr), buffer(nullptr), mesh_name() {}

    void destroy() {
        if (geometry && geometry->get())
            geometry->destroy();
        if (buffer && buffer->get())
            buffer->destroy();
        geometry = nullptr;
        buffer = nullptr;
    }
};


class GeometryPool
{
public:
    GeometryPool(optix::Context context);
    ~GeometryPool();

    optix::Geometry getGeometry(const pugi::xml_node &node, std::string &geometryName);

    bool loadGeometry(const pugi::xml_node &node, const std::string& name);
    bool unloadGeometry(const std::string &name);
    void load(const pugi::xml_node &node);

    static GeometryPool& getInstance(optix::Context context);

private:
    GeometryPool() : m_context(nullptr) {}
    void setContext(optix::Context context);


    optix::Context m_context;

    std::map<std::string, optix::Program> m_programMap;
    std::map<std::string, GeometryData> m_geometryMap;
};

#endif //RENDERER_GPU_GEOMETRYPOOL_H
