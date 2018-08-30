
#include "lightpool.h"

#include "../utils/config.h"
#include "../utils/fileutil.h"
#include "../utils/log.h"

#include <cstring>

#include <imgui/imgui.h>

#include "texture.h"

LightPool::~LightPool()
{
    m_bufferSampleLight->destroy();
    m_bufferLights->destroy();

    for (auto &program : m_programMap)
        program.second->destroy();
}

void LightPool::load(const pugi::xml_node &node)
{
    std::vector<std::string> old_names = extract_keys(m_lightMap);

    std::vector<std::string> new_names;
    new_names.push_back("Environment light");
    clearEnvironmentLight();
    for (auto &light_node : node.children("light")) {
        std::string name = light_node.attribute("name").value();
        name = GetUniqueName(new_names, name);

        std::string light_type = light_node.attribute("type").value();
        LightDefinition light;
        if (light_type == "directional") {
            light.type = LightType::DIRECTIONAL;
            light.position = readVector3(light_node.child("position"));
            light.direction = readVector3(light_node.child("direction"), optix::make_float3(0.0f, 1.0f, 0.0f));
            light.direction = optix::normalize(light.direction);
            light.emission = readVector3(light_node.child("color").child("values"), optix::make_float3(1.0f, 1.0f, 1.0f));
            light.environmentTextureID = RT_TEXTURE_ID_NULL;
        }
        else if (light_type == "environment") {

            LightDefinition &env_light = m_lightMap["Environment light"];
            env_light.emission = readVector3(light_node.child("color").child("values"), optix::make_float3(1.0f, 1.0f, 1.0f));
            env_light.environmentTextureID = TexturePool::getInstance(m_context).id(
                light_node.child("color").child("texture"), env_light.textureScale);
            continue;
        }
        else {
            LogWarning("Unknown light type specified: %s", light_type.c_str());
            continue;
        }
        m_lightMap[name] = light;
        new_names.push_back(name);
    }

    // delete all lights from previous loadings that are missing now
    std::vector<std::string> geomToDelete = difference(old_names, new_names);
    for (auto &geom : geomToDelete)
        m_lightMap.erase(geom);

    updateLightBuffer();
}

void LightPool::updateLightBuffer()
{
    std::vector<LightDefinition> lights = extract_values(m_lightMap);

    try {
        m_bufferLights->setSize(lights.size()); // This can be zero.

        void *dst = static_cast<LightDefinition *>(m_bufferLights->map(0, RT_BUFFER_MAP_WRITE_DISCARD));
        memcpy(dst, lights.data(), sizeof(LightDefinition) * lights.size());
        m_bufferLights->unmap();

        m_context["sysNumLights"]->setInt(int(lights.size()));
    }
    catch (optix::Exception &e) {
        throw std::runtime_error(string_format("Error while updating light buffer: %s",
                                               e.getErrorString().c_str()));
    }
}

void LightPool::updateParameters()
{

    if (ImGui::CollapsingHeader("Lights")) {

        static int selectedLight = 0;

        std::vector<const char *> names;
        std::vector<std::string> lightNames = extract_keys(m_lightMap);
        for (auto &name : lightNames)
            names.push_back(name.c_str());

        ImGui::PushItemWidth(ImGui::GetWindowWidth() - 10);
        ImGui::ListBox("", &selectedLight, &names[0], names.size());
        ImGui::PopItemWidth();

        LightDefinition &light = m_lightMap[lightNames[selectedLight]];

        if (ImGui::ColorEdit3("Emission", (float *) &light.emission)) {
            m_lightsChanged = true;
        }
        if (light.type == LightType::ENVIRONMENT) {
            if (ImGui::DragFloat3("Direction", (float *) &light.direction, 0.01f, -1.0f, 1.0f)) {
                m_lightsChanged = true;
                light.direction.x = 0;
                light.direction.y = 0;
            }
        }
        else if (light.type == LightType::DIRECTIONAL) {
            if (ImGui::DragFloat3("Direction", (float *) &light.direction, 0.01f, -1.0f, 1.0f)) {
                m_lightsChanged = true;
            }
        }
        else if (light.type == LightType::POINT) {

        }

    }
}
bool LightPool::update()
{
    if (m_lightsChanged) {
        updateLightBuffer();
        m_lightsChanged = false;
        return true;
    }
    return false;
}

void LightPool::setContext(optix::Context context)
{
    if (m_context != context)
    {
        try {
            m_context = context;

            // create constant environment light
            clearEnvironmentLight();

            // create light buffer
            m_bufferLights = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER);
            m_bufferLights->setElementSize(sizeof(LightDefinition));
            updateLightBuffer();
            m_context["sysLightDefinitions"]->setBuffer(m_bufferLights);

            // create sampling program for each
            m_bufferSampleLight = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_PROGRAM_ID, 3);
            int *sampleLight = (int *) m_bufferSampleLight->map(0, RT_BUFFER_MAP_WRITE_DISCARD);

            m_programMap["light_env"] = m_context->createProgramFromPTXFile(
                shaderFolder + "light_sampling.ptx", "sample_environment_light");
            sampleLight[LightType::ENVIRONMENT] = m_programMap["light_env"]->getId();

            m_programMap["light_dir"] = m_context->createProgramFromPTXFile(
                shaderFolder + "light_sampling.ptx", "sample_directional_light");
            sampleLight[LightType::DIRECTIONAL] = m_programMap["light_dir"]->getId();

            m_bufferSampleLight->unmap();
            m_context["sysSampleLight"]->setBuffer(m_bufferSampleLight);

            // create miss program to handle environment lightning
            optix::Program miss_program = m_context->createProgramFromPTXFile(
                shaderFolder + "miss.ptx", "miss_gradient");
            m_context->setMissProgram(0, miss_program);
        }
        catch (optix::Exception &e) {
            throw std::runtime_error(string_format("Error while creating LightPool: %s",
                                                   e.getErrorString().c_str()));
        }

    }
}

LightPool &LightPool::getInstance(optix::Context context)
{
    static LightPool instance;
    instance.setContext(context);
    return instance;
}

void LightPool::clearEnvironmentLight()
{
    LightDefinition env_light;
    env_light.type = LightType::ENVIRONMENT;
    env_light.emission = optix::make_float3(1.0f);
    env_light.direction = optix::make_float3(0.0f, 0.0f, 0.0f);
    env_light.environmentTextureID = RT_TEXTURE_ID_NULL;
    m_lightMap["Environment light"] = env_light;
}

