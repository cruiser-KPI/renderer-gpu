
#ifndef RENDERER_GPU_SCENE_H
#define RENDERER_GPU_SCENE_H

#include <optixu/optixpp_namespace.h>

#include <pugixml.hpp>

#include <memory>

#include "lightpool.h"

class Camera;
class PrimitivePool;

class Scene
{
public:
    ~Scene();

    optix::Buffer getFilmBuffer() const;
    void setResolution(int width, int height);

    void render();
    void renderToFile(const std::string &filename);
    bool renderingRunning() const { return m_running;}

    void updateParameters();
    void processInputs();

    void load(const pugi::xml_node &node);
    void save(pugi::xml_node &node);

    void update();

    static Scene &getInstance();

private:
    Scene();

    void reset();

    optix::Context m_context;

    optix::uint2 currentTileOffset;

    bool m_running;
    int m_tileSize;
    int m_nextTileSize;
    float maxRenderingTime = 15.0f;

    int m_iterationIndex;
    bool m_sceneChanged;
    int m_maxDepth;

};


#endif //RENDERER_GPU_SCENE_H
