
#include "geometrypool.h"
#include "../utils/config.h"
#include "../utils/log.h"

#include <iostream>


#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>


struct MeshData
{
    std::vector<VertexAttributes> attributes;
    int nTriangles;
};

static std::map<std::string, MeshData> meshCache;

GeometryPool::~GeometryPool()
{
    std::vector<std::string> names = extract_keys(m_geometryMap);
    for (auto &name : names)
        unloadGeometry(name);

    for (auto &program : m_programMap)
        program.second->destroy();
}

bool loadGeometryFromFile(const std::string &filename, MeshData &meshData, bool &updated)
{
    if (meshCache.find(filename) != meshCache.end()) {
        meshData = meshCache[filename];
        return true;
    }

    Assimp::Importer importer;
    const aiScene *scene = importer.ReadFile(filename, aiProcess_Triangulate | aiProcess_GenSmoothNormals | aiProcess_CalcTangentSpace);

    if (!scene || scene->mFlags & AI_SCENE_FLAGS_INCOMPLETE || !scene->mRootNode) {
        LogError("Unable to load mesh '%s'. Error: ", filename.c_str(), importer.GetErrorString());
        return false;
    }

    meshData.nTriangles = 0;
    for (int meshNum = 0; meshNum < scene->mNumMeshes; meshNum++) {
        aiMesh *mesh = scene->mMeshes[meshNum];
        size_t nTriangles = mesh->mNumFaces;
        if (mesh->mTangents)
            LogWarning("No tangents provided. Using planar texture mapping (on z axis). ");

        std::vector<VertexAttributes> attributes;
        attributes.reserve(nTriangles * 3);

        for (int j = 0; j < nTriangles; j++) {
            const aiFace &face = mesh->mFaces[j];
            for (int k = 0; k < 3; k++) {
                VertexAttributes attrib;
                aiVector3D vertex = mesh->mVertices[face.mIndices[k]];
                attrib.vertex = optix::make_float3(vertex.x, vertex.y, vertex.z);
                aiVector3D normal = mesh->mNormals[face.mIndices[k]];
                attrib.normal = optix::make_float3(normal.x, normal.y, normal.z);
                if (mesh->mTangents) {
                    aiVector3D tangent = mesh->mTangents[face.mIndices[k]];
                    attrib.tangent = optix::make_float3(tangent.x, tangent.y, tangent.z);
                }
                else
                    attrib.tangent = optix::make_float3(vertex.x, vertex.y, 0);

                if (mesh->mTextureCoords[0]) {
                    aiVector3D texCoord = mesh->mTextureCoords[0][face.mIndices[k]];
                    attrib.texcoord = optix::make_float3(texCoord.x, texCoord.y, texCoord.z);
                }

                attributes.push_back(attrib);
            }
        }

        meshData.attributes.insert(meshData.attributes.begin(), attributes.begin(), attributes.end());
        meshData.nTriangles += nTriangles;
    }

    meshCache[filename] = meshData;

    LogInfo("Mesh '%s' was loaded. (%d triangles)", filename.c_str(), meshData.nTriangles);
    updated = true;
    return true;
}

bool loadShape(const std::string &shapeType, MeshData &meshData, bool &updated)
{
    if (meshCache.find(shapeType) != meshCache.end()) {
        meshData = meshCache[shapeType];
        return true;
    }

    std::vector<VertexAttributes> attributes;
    std::vector<unsigned int> indices;

    if (shapeType == "plane") {
        optix::float3
            corner = optix::make_float3(-1.0f, 0.0f, 1.0f); // left front corner of the plane. texcoord (0.0f, 0.0f).

        int tessU = 1, tessV = 1;

        VertexAttributes attrib;

        attrib.tangent = optix::make_float3(1.0f, 0.0f, 0.0f);
        attrib.normal = optix::make_float3(0.0f, 1.0f, 0.0f);

        for (int j = 0; j <= tessV; ++j) {
            const float v = float(j) * 2;

            for (int i = 0; i <= tessU; ++i) {
                const float u = float(i) * 2;

                attrib.vertex = corner + optix::make_float3(u, 0.0f, -v);
                attrib.texcoord = optix::make_float3(u * 0.5f, v * 0.5f, 0.0f);

                attributes.push_back(attrib);
            }
        }

        const unsigned int stride = tessU + 1;
        for (int j = 0; j < tessV; ++j) {
            for (int i = 0; i < tessU; ++i) {
                indices.push_back(j * stride + i);
                indices.push_back(j * stride + i + 1);
                indices.push_back((j + 1) * stride + i + 1);

                indices.push_back((j + 1) * stride + i + 1);
                indices.push_back((j + 1) * stride + i);
                indices.push_back(j * stride + i);
            }
        }

    }
    else if (shapeType == "sphere") {
        int tessU = 180, tessV = 90;
        float maxTheta = M_PIf;
        float radius = 1;

        attributes.reserve((tessU + 1) * tessV);
        indices.reserve(6 * tessU * (tessV - 1));

        float phi_step = 2.0f * M_PIf / (float) tessU;
        float theta_step = maxTheta / (float) (tessV - 1);

        // Latitudinal rings.
        // Starting at the south pole going upwards on the y-axis.
        for (int latitude = 0; latitude < tessV; latitude++) // theta angle
        {
            float theta = (float) latitude * theta_step;
            float sinTheta = sinf(theta);
            float cosTheta = cosf(theta);

            float texv = (float) latitude / (float) (tessV - 1); // Range [0.0f, 1.0f]

            // Generate vertices along the latitudinal rings.
            // On each latitude there are tessU + 1 vertices.
            // The last one and the first one are on identical positions, but have different texture coordinates!
            // DAR FIXME Note that each second triangle connected to the two poles has zero area!
            for (int longitude = 0; longitude <= tessU; longitude++) // phi angle
            {
                float phi = (float) longitude * phi_step;
                float sinPhi = sinf(phi);
                float cosPhi = cosf(phi);

                float texu = (float) longitude / (float) tessU; // Range [0.0f, 1.0f]

                // Unit sphere coordinates are the normals.
                optix::float3 normal = optix::make_float3(cosPhi * sinTheta,
                                                          -cosTheta,                 // -y to start at the south pole.
                                                          -sinPhi * sinTheta);
                VertexAttributes attrib;

                attrib.vertex = normal * radius;
                attrib.tangent = optix::make_float3(-sinPhi, 0.0f, -cosPhi);
                attrib.normal = normal;
                attrib.texcoord = optix::make_float3(texu, texv, 0.0f);

                attributes.push_back(attrib);
            }
        }

        // We have generated tessU + 1 vertices per latitude.
        const unsigned int columns = tessU + 1;

        // Calculate indices.
        for (int latitude = 0; latitude < tessV - 1; latitude++) {
            for (int longitude = 0; longitude < tessU; longitude++) {
                indices.push_back(latitude * columns + longitude);  // lower left
                indices.push_back(latitude * columns + longitude + 1);  // lower right
                indices.push_back((latitude + 1) * columns + longitude + 1);  // upper right

                indices.push_back((latitude + 1) * columns + longitude + 1);  // upper right
                indices.push_back((latitude + 1) * columns + longitude);  // upper left
                indices.push_back(latitude * columns + longitude);  // lower left
            }
        }

    }
    else if (shapeType == "box") {
        float left = -1.0f;
        float right = 1.0f;
        float bottom = -1.0f;
        float top = 1.0f;
        float back = -1.0f;
        float front = 1.0f;

        VertexAttributes attrib;

        // Left.
        attrib.tangent = optix::make_float3(0.0f, 0.0f, 1.0f);
        attrib.normal = optix::make_float3(-1.0f, 0.0f, 0.0f);

        attrib.vertex = optix::make_float3(left, bottom, back);
        attrib.texcoord = optix::make_float3(0.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, bottom, front);
        attrib.texcoord = optix::make_float3(1.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, top, front);
        attrib.texcoord = optix::make_float3(1.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, top, back);
        attrib.texcoord = optix::make_float3(0.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        // Right.
        attrib.tangent = optix::make_float3(0.0f, 0.0f, -1.0f);
        attrib.normal = optix::make_float3(1.0f, 0.0f, 0.0f);

        attrib.vertex = optix::make_float3(right, bottom, front);
        attrib.texcoord = optix::make_float3(0.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, bottom, back);
        attrib.texcoord = optix::make_float3(1.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, top, back);
        attrib.texcoord = optix::make_float3(1.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, top, front);
        attrib.texcoord = optix::make_float3(0.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        // Back.
        attrib.tangent = optix::make_float3(-1.0f, 0.0f, 0.0f);
        attrib.normal = optix::make_float3(0.0f, 0.0f, -1.0f);

        attrib.vertex = optix::make_float3(right, bottom, back);
        attrib.texcoord = optix::make_float3(0.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, bottom, back);
        attrib.texcoord = optix::make_float3(1.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, top, back);
        attrib.texcoord = optix::make_float3(1.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, top, back);
        attrib.texcoord = optix::make_float3(0.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        // Front.
        attrib.tangent = optix::make_float3(1.0f, 0.0f, 0.0f);
        attrib.normal = optix::make_float3(0.0f, 0.0f, 1.0f);

        attrib.vertex = optix::make_float3(left, bottom, front);
        attrib.texcoord = optix::make_float3(0.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, bottom, front);
        attrib.texcoord = optix::make_float3(1.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, top, front);
        attrib.texcoord = optix::make_float3(1.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, top, front);
        attrib.texcoord = optix::make_float3(0.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        // Bottom.
        attrib.tangent = optix::make_float3(1.0f, 0.0f, 0.0f);
        attrib.normal = optix::make_float3(0.0f, -1.0f, 0.0f);

        attrib.vertex = optix::make_float3(left, bottom, back);
        attrib.texcoord = optix::make_float3(0.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, bottom, back);
        attrib.texcoord = optix::make_float3(1.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, bottom, front);
        attrib.texcoord = optix::make_float3(1.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, bottom, front);
        attrib.texcoord = optix::make_float3(0.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        // Top.
        attrib.tangent = optix::make_float3(1.0f, 0.0f, 0.0f);
        attrib.normal = optix::make_float3(0.0f, 1.0f, 0.0f);

        attrib.vertex = optix::make_float3(left, top, front);
        attrib.texcoord = optix::make_float3(0.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, top, front);
        attrib.texcoord = optix::make_float3(1.0f, 0.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(right, top, back);
        attrib.texcoord = optix::make_float3(1.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        attrib.vertex = optix::make_float3(left, top, back);
        attrib.texcoord = optix::make_float3(0.0f, 1.0f, 0.0f);
        attributes.push_back(attrib);

        for (unsigned int i = 0; i < 6; ++i) // Six faces (== 12 triangles).
        {
            const unsigned int idx = i * 4; // Four unique attributes per box face.

            indices.push_back(idx);
            indices.push_back(idx + 1);
            indices.push_back(idx + 2);

            indices.push_back(idx + 2);
            indices.push_back(idx + 3);
            indices.push_back(idx);
        }

    }
    else if (shapeType == "torus") {
        int tessU = 180, tessV = 180;
        float innerRadius = 0.75, outerRadius = 0.25;

        attributes.reserve((tessU + 1) * (tessV + 1));
        indices.reserve(8 * tessU * tessV);

        const float u = (float) tessU;
        const float v = (float) tessV;

        float phi_step = 2.0f * M_PIf / u;
        float theta_step = 2.0f * M_PIf / v;

        // Setup vertices and normals.
        // Generate the torus exactly like the sphere with rings around the origin along the latitudes.
        for (int latitude = 0; latitude <= tessV; ++latitude) // theta angle
        {
            const float theta = (float) latitude * theta_step;
            const float sinTheta = sinf(theta);
            const float cosTheta = cosf(theta);

            const float radius = innerRadius + outerRadius * cosTheta;

            for (int longitude = 0; longitude <= tessU; ++longitude) // phi angle
            {
                const float phi = (float) longitude * phi_step;
                const float sinPhi = sinf(phi);
                const float cosPhi = cosf(phi);

                VertexAttributes attrib;

                attrib.vertex = optix::make_float3(radius * cosPhi, outerRadius * sinTheta, radius * -sinPhi);
                attrib.tangent = optix::make_float3(-sinPhi, 0.0f, -cosPhi);
                attrib.normal = optix::make_float3(cosPhi * cosTheta, sinTheta, -sinPhi * cosTheta);
                attrib.texcoord = optix::make_float3((float) longitude / u, (float) latitude / v, 0.0f);

                attributes.push_back(attrib);
            }
        }

        // We have generated tessU + 1 vertices per latitude.
        const unsigned int columns = tessU + 1;

        // Setup indices
        for (int latitude = 0; latitude < tessV; ++latitude) {
            for (int longitude = 0; longitude < tessU; ++longitude) {
                indices.push_back(latitude * columns + longitude);  // lower left
                indices.push_back(latitude * columns + longitude + 1);  // lower right
                indices.push_back((latitude + 1) * columns + longitude + 1);  // upper right

                indices.push_back((latitude + 1) * columns + longitude + 1);  // upper right
                indices.push_back((latitude + 1) * columns + longitude);  // upper left
                indices.push_back(latitude * columns + longitude);  // lower left
            }
        }

    }
    else {
        LogWarning("Unknown shape type encountered: %s", shapeType.c_str());
        return false;
    }

    meshData.attributes.reserve(indices.size());
    for (auto &index : indices)
        meshData.attributes.push_back(attributes[index]);
    meshData.nTriangles = indices.size() / 3;

    meshCache[shapeType] = meshData;

    LogInfo("Shape '%s' was loaded. (%d triangles)", shapeType.c_str(), meshData.nTriangles);
    updated = true;
    return true;
}

optix::Geometry GeometryPool::getGeometry(const pugi::xml_node &node, std::string &geometryName)
{
    std::string shape_name = node.attribute("name").value();
    if (shape_name.empty())
        return nullptr;
    else if (m_geometryMap.find(shape_name) == m_geometryMap.end()){
        LogWarning("Shape with name '%s' was not found", shape_name.c_str());
        return nullptr;
    }
    geometryName = shape_name;
    return m_geometryMap[shape_name].geometry;
}

bool GeometryPool::loadGeometry(const pugi::xml_node &node, const std::string &name)
{
    GeometryData data;

    if (m_geometryMap.find(name) != m_geometryMap.end()) {
        data = m_geometryMap[name];
    }
    try {
        if (!data.geometry) {
            data.geometry = m_context->createGeometry();
            data.geometry->setIntersectionProgram(m_programMap["intersection"]);
            data.geometry->setBoundingBoxProgram(m_programMap["boundingBox"]);

        }
        bool bufferEmpty = false;
        if (!data.buffer) {
            data.buffer = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER);
            data.buffer->setElementSize(sizeof(VertexAttributes));
            data.buffer->setSize(0);
            bufferEmpty = true;
        }

        MeshData meshData;
        bool succesfulLoad = false, meshUpdated = false;
        std::string shape_type = node.attribute("type").value();
        if (shape_type.empty()) {
            LogWarning("Can't load mesh. No shape type specified");
            data.destroy();
            return false;
        }
        else if (shape_type == "mesh") {
            std::string filename = node.child("filename").child_value();
            if (!filename.empty()) {
                succesfulLoad = loadGeometryFromFile(filename, meshData, meshUpdated);
                data.mesh_name = filename;
            }
        }
        else {
            succesfulLoad = loadShape(shape_type, meshData, meshUpdated);
            data.mesh_name = shape_type;
        }

        if (!succesfulLoad) {
            data.destroy();
            return false;
        }

        if (bufferEmpty || meshUpdated) {
            data.buffer->setSize(meshData.attributes.size());

            void *dst = data.buffer->map(0, RT_BUFFER_MAP_WRITE_DISCARD);
            memcpy(dst, meshData.attributes.data(), sizeof(VertexAttributes) * meshData.attributes.size());
            data.buffer->unmap();
            data.geometry["attributesBuffer"]->setBuffer(data.buffer);

            data.geometry->setPrimitiveCount(meshData.nTriangles);
        }
    }
    catch (optix::Exception &e) {
        LogError("Error occured when creating geometry: %s", e.getErrorString().c_str());
        data.destroy();
        return false;
    }

    m_geometryMap[name] = data;

    return true;
}

bool GeometryPool::unloadGeometry(const std::string &name)
{
    GeometryData &data = m_geometryMap[name];
    data.destroy();
    m_geometryMap.erase(name);
    return true;
}

void GeometryPool::load(const pugi::xml_node &node)
{
    std::vector<std::string> old_names = extract_keys(m_geometryMap);

    // get old meshes
    std::vector<std::string> old_mesh_names;
    for (auto &kv : m_geometryMap)
        if (!kv.second.mesh_name.empty())
            old_mesh_names.push_back(kv.second.mesh_name);

    // load geometry and initialize OptiX variables
    std::vector<std::string> new_names;
    for (auto &geometry_node : node.children("shape")) {
        std::string name = geometry_node.attribute("name").value();
        name = GetUniqueName(new_names, name);
        if (loadGeometry(geometry_node, name))
            new_names.push_back(name);
    }

    // get meshes that were loaded
    std::vector<std::string> new_mesh_names;
    for (auto &name : new_names)
        if (!m_geometryMap[name].mesh_name.empty())
            new_mesh_names.push_back(m_geometryMap[name].mesh_name);

    // delete all objects from previous loadings that are missing now
    std::vector<std::string> geomToDelete = difference(old_names, new_names);
    for (auto &geom : geomToDelete) {
        try {
            unloadGeometry(geom);
        }
        catch (optix::Exception &e) {
            LogError("Error while unloading geometry %s", geom.c_str());
        }
    }

    // delete all meshes from previous loadings that are missing now
    std::vector<std::string> meshesToDelete = difference(old_mesh_names, new_mesh_names);
    for (auto &mesh : meshesToDelete){
        LogInfo("Mesh '%s' was unloaded because it's not used.", mesh.c_str());
        meshCache.erase(mesh);
    }

}
void GeometryPool::setContext(optix::Context context)
{
    if (m_context != context){

        try {
            m_context = context;

            m_programMap["boundingBox"] =
                m_context->createProgramFromPTXFile(shaderFolder + "triangle_bbox.ptx", "triangle_bbox");
            m_programMap["intersection"] =
                m_context->createProgramFromPTXFile(shaderFolder + "triangle_intersection.ptx", "triangle_intersection");
        }
        catch (optix::Exception &e) {
            throw std::runtime_error(string_format("Error while creating GeometryPool %s",
                                                   e.getErrorString().c_str()));
        }
    }
}
GeometryPool &GeometryPool::getInstance(optix::Context context)
{
    static GeometryPool instance;
    instance.setContext(context);
    return instance;
}
