
#include "texture.h"

#include "../utils/log.h"
#include "../utils/fileutil.h"

#include "image.h"


std::map<std::string, Image> imageCache;


TexturePool::~TexturePool()
{
    std::vector<std::string> names = extract_keys(m_textureMap);
    for (auto &name : names)
        unloadTexture(name);

    for (auto &kv : imageCache)
        kv.second.clear();

}

bool TexturePool::loadTexture(const pugi::xml_node &node, const std::string &name)
{
    TextureData data;
    if (m_textureMap.find(name) != m_textureMap.end()) {
        data = m_textureMap[name];
    }

    // TODO cubemaps
    // TODO different input and output data encodings
    // TODO texture scale
    std::string filename = node.child("filename").child_value();
    if (filename.empty()){
        return false;
    }
    int input_mip = readInt(node.child("mipCount"), 1);

    if (data.image_filename == filename && data.mipCount == input_mip)
        return true;

    Image image(input_mip);
    if (imageCache.find(filename) != imageCache.end()) {
        image = imageCache[filename];
    }
    else {
        if (!image.load(filename))
            return false;
    }
    data.image_filename = filename;
    data.mipCount = input_mip;

    float *pixels = image.pixelData();
    int width = image.width();
    int height = image.height();
    int mipCount = image.mipCount();

    try {
        if (!data.sampler) {
            data.sampler = m_context->createTextureSampler();
            data.sampler->setWrapMode(0, RT_WRAP_REPEAT);
            data.sampler->setWrapMode(1, RT_WRAP_REPEAT);
            data.sampler->setWrapMode(2, RT_WRAP_REPEAT);
            const RTfiltermode
                mipmapFilter = (1 < mipCount) ? RT_FILTER_LINEAR : RT_FILTER_NONE; // Trilinear or bilinear filtering.
            data.sampler->setFilteringModes(RT_FILTER_LINEAR, RT_FILTER_LINEAR, mipmapFilter);
            data.sampler->setMaxAnisotropy(1.0f);
        }

        // destroy buffer that contains old image
        if (data.buffer && data.buffer->get()) {
            data.buffer->destroy();
            data.buffer = nullptr;
        }

        if (!data.buffer) {
            data.buffer = m_context->createBuffer(RT_BUFFER_INPUT, RT_FORMAT_FLOAT4, width, height);
        }

        data.buffer->setMipLevelCount(mipCount);
        int offset = 0;
        for (int mipLevel = 0; mipLevel < mipCount; mipLevel++) {
            void *dst = data.buffer->map(mipLevel, RT_BUFFER_MAP_WRITE_DISCARD);
            memcpy(dst, pixels + offset, width * height * sizeof(float) * 4);
            data.buffer->unmap(mipLevel);

            offset += width * height * 4;
            width /= 2;
            height /= 2;
        }
        data.sampler->setBuffer(data.buffer);
    }
    catch(optix::Exception& e)
    {
        LogError(e.getErrorString().c_str());
        return false;
    }

    imageCache[filename] = image;
    m_textureMap[name] = data;

    return true;
}

bool TexturePool::unloadTexture(const std::string &name)
{
    TextureData &data = m_textureMap[name];

    LogInfo("Image '%s' was unloaded.", data.image_filename.c_str());
    imageCache[data.image_filename].clear();
    imageCache.erase(data.image_filename);
    data.destroy();
    m_textureMap.erase(name);

    return true;
}

bool TexturePool::load(const pugi::xml_node &node)
{
    std::vector<std::string> old_names = extract_keys(m_textureMap);

    std::vector<std::string> new_names;
    for (auto &texture_node : node.children("texture")) {
        std::string name = texture_node.attribute("name").value();
        name = GetUniqueName(new_names, name);
        if (loadTexture(texture_node, name))
            new_names.push_back(name);
    }

    // delete all textures from previous loadings that are missing now
    std::vector<std::string> texToDelete = difference(old_names, new_names);
    for (auto &tex : texToDelete) {
        unloadTexture(tex);
    }
}

int TexturePool::id(const pugi::xml_node &node, float &scale)
{
    scale = readFloat(node.child("scale"), 1.0f);

    std::string name = node.attribute("name").value();
    if (name.empty())
        return RT_TEXTURE_ID_NULL;

    if (m_textureMap.find(name) == m_textureMap.end()){
        LogWarning("Texture with name '%s' was not found", name.c_str());
        return RT_TEXTURE_ID_NULL;
    }

    optix::TextureSampler sampler = m_textureMap[name].sampler;
    if (sampler->get())
        return sampler->getId();
    return RT_TEXTURE_ID_NULL;
}

void TexturePool::setContext(optix::Context context)
{
    if (m_context != context)
        m_context = context;
}

TexturePool &TexturePool::getInstance(optix::Context context)
{
    static TexturePool instance;
    instance.setContext(context);
    return instance;
}


