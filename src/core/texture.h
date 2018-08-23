
#ifndef RENDERER_GPU_TEXTURE_H
#define RENDERER_GPU_TEXTURE_H

#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>

#include <pugixml.hpp>

#include <map>

struct TextureData
{
    optix::TextureSampler sampler;
    optix::Buffer buffer;

    std::string image_filename;
    int mipCount;

    TextureData() : sampler(nullptr), buffer(nullptr), image_filename(), mipCount(1) {}

    void destroy()
    {
        if (sampler->get())
            sampler->destroy();
        if (buffer->get())
            buffer->destroy();
    }
};

class TexturePool
{
public:
    TexturePool(optix::Context context);
    ~TexturePool();

    bool load(const pugi::xml_node &node);
    int id(const pugi::xml_node &node, float &scale);

    static TexturePool& getInstance(optix::Context context);

private:
    TexturePool() : m_context(nullptr) {}
    void setContext(optix::Context context);

    optix::Context m_context;

    bool loadTexture(const pugi::xml_node &node, const std::string &name);
    bool unloadTexture(const std::string &name);

    std::map<std::string, TextureData> m_textureMap;

};


#endif //RENDERER_GPU_TEXTURE_H
