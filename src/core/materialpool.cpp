
#include "materialpool.h"

#include "../utils/config.h"
#include "../utils/fileutil.h"
#include "../utils/log.h"
#include "../core/flags.h"

#include <cstring>


MaterialPool::~MaterialPool()
{
    m_bufferEvalBSDF->destroy();
    m_bufferSampleBSDF->destroy();

    if (m_material->get())
        m_material->destroy();
    m_materialBuffer->destroy();

    for (auto &program : m_programMap)
        program.second->destroy();
}

optix::Material MaterialPool::getMaterial(const pugi::xml_node &node, int &materialIndex, std::string &materialName)
{
    // TODO fix indexing of materials (relies on order of map elements)

    std::string material_name = node.attribute("name").value();
    std::vector<std::string> names = extract_keys(m_materialMap);
    auto material_pos = std::find(names.begin(), names.end(), material_name);
    if (material_pos != names.end()) {
        materialIndex = (int) (material_pos - names.begin());
        materialName = material_name;
    }
    else {
        LogWarning("Material '%s' was not found. Setting default (black diffuse)", material_name.c_str());
        material_pos = std::find(names.begin(), names.end(), "Default material");
        materialIndex = (int) (material_pos - names.begin());
        material_name = "Default material";
    }

    return m_material;
}

void MaterialPool::updateMaterialBuffer()
{
    std::vector<MaterialParameter> materials = extract_values(m_materialMap);

    try {
        m_materialBuffer->setSize(materials.size());

        MaterialParameter
            *dst = static_cast<MaterialParameter *>(m_materialBuffer->map(0, RT_BUFFER_MAP_WRITE_DISCARD));
        memcpy(dst, &materials[0], sizeof(MaterialParameter) * materials.size());
        m_materialBuffer->unmap();
    }
    catch (optix::Exception &e) {
        throw std::runtime_error(string_format("Error while updating material buffer %s",
                                               e.getErrorString().c_str()));
    }

}

void MaterialPool::load(const pugi::xml_node &node)
{
    std::vector<std::string> old_names = extract_keys(m_materialMap);

    std::vector<std::string> names;
    names.push_back("Default material");
    for (auto &material_node : node.children("material")) {
        std::string name = material_node.attribute("name").value();
        name = GetUniqueName(names, name);

        MaterialParameter matData;

        std::string material_type = material_node.attribute("type").value();
        if (m_materialIndices.count(material_type))
            matData.indexBSDF = m_materialIndices[material_type];
        else {
            LogWarning("Unknown material type encountered: %s. Setting default (diffuse)", material_type.c_str());
            matData.indexBSDF = m_materialIndices["diffuse"];
        }

        auto albedo_node = material_node.child("albedo");
        matData.albedo = readSpectrum(albedo_node.child("values"), optix::make_float3(1.0f));
        matData.textureID = TexturePool::getInstance(m_context).id(
            albedo_node.child("texture"), matData.textureScale);

        m_materialMap[name] = matData;
        names.push_back(name);
    }

    // delete all lights from previous loadings that are missing now
    std::vector<std::string> matToDelete = difference(old_names, names);
    for (auto &mat : matToDelete)
        m_materialMap.erase(mat);

    updateMaterialBuffer();
}

void MaterialPool::updateParameters(const std::string &materialName)
{
    MaterialParameter &material = m_materialMap[materialName];

    ImGui::Text("Material settings");
    if (ImGui::ColorEdit3("albedo", (float *) &material.albedo))
        m_changed = true;

    static int selectedCombo = 0;
    if (ImGui::Combo("material", &selectedCombo, &m_materialNames[0], m_materialNames.size()))
        m_changed = true;

    if (m_materialIndices[m_materialNames[selectedCombo]] != material.indexBSDF)
        m_changed = true;
    material.indexBSDF = m_materialIndices[m_materialNames[selectedCombo]];
}

bool MaterialPool::update()
{
    if (m_changed) {
        updateMaterialBuffer();
        m_changed = false;
        return true;
    }
    return false;

}
void MaterialPool::setContext(optix::Context context)
{
    if (m_context != context){

        try {
            m_context = context;

            m_programMap["closest_hit"] =
                m_context->createProgramFromPTXFile(shaderFolder + "closest_hit.ptx", "closest_hit");
            m_programMap["any_hit"] =
                m_context->createProgramFromPTXFile(shaderFolder + "any_hit.ptx", "any_hit");

            optix::Material material = m_context->createMaterial();
            material->setClosestHitProgram(0, m_programMap["closest_hit"]);
            material->setAnyHitProgram(1, m_programMap["any_hit"]);
            m_material = material;

            // BSDF sampling functions
            m_bufferSampleBSDF = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_PROGRAM_ID, 3);
            int *sampleBsdf = (int *) m_bufferSampleBSDF->map(0, RT_BUFFER_MAP_WRITE_DISCARD);

            optix::Program prg = m_context->createProgramFromPTXFile(
                shaderFolder + "bsdf_sampling.ptx", "sample_bsdf_diffuse_reflection");
            m_programMap["sample_bsdf_diffuse_reflection"] = prg;
            sampleBsdf[MaterialType::DIFFUSE] = prg->getId();

            prg = m_context->createProgramFromPTXFile(
                shaderFolder + "bsdf_sampling.ptx", "sample_bsdf_specular_reflection");
            m_programMap["sample_bsdf_specular_reflection"] = prg;
            sampleBsdf[MaterialType::SPECULAR] = prg->getId();

            prg = m_context->createProgramFromPTXFile(
                shaderFolder + "bsdf_sampling.ptx", "sample_bsdf_specular_reflection_transmission");
            m_programMap["sample_bsdf_specular_reflection_transmission"] = prg;
            sampleBsdf[MaterialType::SPECULAR_TRANSIMISSION] = prg->getId();

            m_bufferSampleBSDF->unmap();
            m_context["sysSampleBSDF"]->setBuffer(m_bufferSampleBSDF);

            // BSDF evaluation functions
            m_bufferEvalBSDF = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_PROGRAM_ID, 3);
            int *evalBsdf = (int *) m_bufferEvalBSDF->map(0, RT_BUFFER_MAP_WRITE_DISCARD);

            prg = m_context->createProgramFromPTXFile(
                shaderFolder + "bsdf_sampling.ptx", "eval_bsdf_diffuse_reflection");
            m_programMap["eval_bsdf_diffuse_reflection"] = prg;
            evalBsdf[MaterialType::DIFFUSE] = prg->getId();

            prg = m_context->createProgramFromPTXFile(
                shaderFolder + "bsdf_sampling.ptx", "eval_bsdf_specular_reflection");
            m_programMap["eval_bsdf_specular_reflection"] = prg;
            evalBsdf[MaterialType::SPECULAR] = prg->getId(); // All specular evaluation functions just returns float4(0.0f).
            evalBsdf[MaterialType::SPECULAR_TRANSIMISSION] =
                prg->getId(); // Reuse the same program for all specular materials to keep the kernel small.

            m_bufferEvalBSDF->unmap();
            m_context["sysEvalBSDF"]->setBuffer(m_bufferEvalBSDF);

            // create Material buffer (contains parameters for materials)
            m_materialBuffer = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_USER);
            m_materialBuffer->setElementSize(sizeof(MaterialParameter));
            m_materialBuffer->setSize(0);
            m_context["sysMaterialParameters"]->setBuffer(m_materialBuffer);


            m_materialIndices["diffuse"] = MaterialType::DIFFUSE;
            m_materialIndices["specular"] = MaterialType::SPECULAR;
            m_materialNames.reserve(m_materialIndices.size());
            for (auto const &imap: m_materialIndices)
                m_materialNames.push_back(imap.first.c_str());

            // create default material
            MaterialParameter matData;
            matData.indexBSDF = m_materialIndices["diffuse"];
            matData.albedo = optix::make_float3(0.0f);
            matData.textureID = RT_TEXTURE_ID_NULL;
            m_materialMap["Default material"] = matData;
        }
        catch (optix::Exception &e) {
            throw std::runtime_error(string_format("Error while creating MaterialPool %s",
                                                   e.getErrorString().c_str()));
        }

    }
}

MaterialPool &MaterialPool::getInstance(optix::Context context)
{
    static MaterialPool instance;
    instance.setContext(context);
    return instance;
}
