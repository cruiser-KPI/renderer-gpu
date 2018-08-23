
#include "primitivepool.h"
#include "vertexattributes.h"
#include "../utils/config.h"
#include "../utils/fileutil.h"
#include "../utils/log.h"

#include <cstring>

#include <optixu/optixu_math_namespace.h>
#include <optix_world.h>

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>




PrimitivePool::~PrimitivePool()
{
    std::vector<std::string> names = extract_keys(m_primitives);
    for (auto &name : names)
        unloadPrimitive(name);

    for (auto &program : m_programMap)
        program.second->destroy();

    m_rootGroup->destroy();
    m_rootAcceleration->destroy();
}

bool PrimitivePool::loadPrimitive(const pugi::xml_node &node, const std::string &name)
{
    bool newPrimitive = true;

    PrimitiveData data;
    // check if primitive is already loaded
    if (m_primitives.find(name) != m_primitives.end()) {
        data = m_primitives[name];
        if (data.transform)
            newPrimitive = false;
    }

    try {
        std::string geometryName;
        optix::Geometry geometry = GeometryPool::getInstance(m_context).getGeometry(node.child("shape"), geometryName);

        std::string materialName;
        int materialIndex = 0;
        optix::Material material = MaterialPool::getInstance(m_context).getMaterial(node.child("material"), materialIndex, materialName);

        if (!geometry || !material) {
            // if not new primitive destroy all optix data
            if (!newPrimitive) {
                m_rootGroup->removeChild(data.transform);
                m_rootAcceleration->markDirty();
                data.destroy();
                m_primitives[name] = data;
                return true;
            }
            return false;
        }

        // create GeometryGroup, acceleration and transform
        if (!data.instance)
            data.instance = m_context->createGeometryInstance();

        if (!data.acceleration)
            data.acceleration = m_context->createAcceleration("Trbvh");

        // if geometry have changed
        if (geometry != data.geometry || data.geometryName != geometryName) {
            data.geometry = geometry;
            data.geometryName = geometryName;

            data.instance->setGeometry(data.geometry);
            data.acceleration->markDirty();
        }

        // if material have changed
        if (material != data.material || data.materialName != materialName) {
            data.material = material;
            data.materialName = materialName;

            data.instance->setMaterialCount(1);
            data.instance->setMaterial(0, data.material);
            data.instance["materialIndex"]->setInt(materialIndex);
        }

        if (!data.geometryGroup)
            data.geometryGroup = m_context->createGeometryGroup();
        data.geometryGroup->setAcceleration(data.acceleration);
        data.geometryGroup->setChildCount(1);
        data.geometryGroup->setChild(0, data.instance);

        if (!data.transform)
            data.transform = m_context->createTransform();
        data.transform->setChild(data.geometryGroup);

        // read transform from file
        optix::Matrix4x4 transformMatrix = readTransform(node.child("transform"));
        if (data.transformMatrix != transformMatrix) {
            data.transformMatrix = transformMatrix;

            data.transform->setMatrix(false, data.transformMatrix.getData(),
                                      data.transformMatrix.inverse().getData());

            m_rootAcceleration->markDirty();
        }

        if (newPrimitive) {
            unsigned int count = m_rootGroup->getChildCount();
            m_rootGroup->setChildCount(count + 1);
            m_rootGroup->setChild(count, data.transform);

            m_rootAcceleration->markDirty();
        }

    }
    catch (optix::Exception &e) {
        LogError("Error occured when creating primitive: %s", e.getErrorString().c_str());
        return false;
    }

    m_primitives[name] = data;
    return true;
}

bool PrimitivePool::unloadPrimitive(const std::string &name)
{
    PrimitiveData &data = m_primitives[name];
    if (data.transform) {
        m_rootGroup->removeChild(data.transform);
        m_rootAcceleration->markDirty();
    }
    data.destroy();
    m_primitives.erase(name);

    return true;
}

void PrimitivePool::load(const pugi::xml_node &node)
{
    std::vector<std::string> old_names = extract_keys(m_primitives);

    // load primitives (keep those from previous loading)
    std::vector<std::string> new_names;
    for (auto &primitive_node : node.children("primitive")) {
        std::string name = primitive_node.attribute("name").value();
        name = GetUniqueName(new_names, name);
        if (loadPrimitive(primitive_node, name))
            new_names.push_back(name);
    }

    // delete all primitives from previous loadings that are missing now
    std::vector<std::string> primsToDelete = difference(old_names, new_names);
    for (auto &prim : primsToDelete) {
        try {
            unloadPrimitive(prim);
        }
        catch (optix::Exception &e) {
            LogError("Error while unloading primitive %s", prim.c_str());
        }

    }

}

void PrimitivePool::save(pugi::xml_node &node)
{

}
void PrimitivePool::updateParameters()
{
    if (ImGui::CollapsingHeader("Primitives")) {
        static int selectedPrimitive = 0;

        std::vector<const char *> names;
        std::vector<std::string> primitive_names = extract_keys(m_primitives);
        for (auto &name : primitive_names)
            names.push_back(name.c_str());

        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 10);
        ImGui::ListBox("", &selectedPrimitive, &names[0], names.size());
        ImGui::PopItemWidth();

        MaterialPool::getInstance(m_context).updateParameters(m_primitives[primitive_names[selectedPrimitive]].materialName);
    }
}
bool PrimitivePool::update()
{
    return MaterialPool::getInstance(m_context).update();
}

PrimitivePool& PrimitivePool::getInstance(optix::Context context)
{
    static PrimitivePool instance;
    instance.setContext(context);
    return instance;
}

void PrimitivePool::setContext(optix::Context context)
{
    if (m_context != context){

        try {
            m_context = context;

            m_rootAcceleration = m_context->createAcceleration("Trbvh");

            m_rootGroup = m_context->createGroup();
            m_rootGroup->setAcceleration(m_rootAcceleration);
            m_context["sysTopObject"]->set(m_rootGroup);
        }
        catch (optix::Exception &e) {
            throw std::runtime_error(string_format("Error while creating PrimitivePool %s",
                                                   e.getErrorString().c_str()));
        }

    }
}



