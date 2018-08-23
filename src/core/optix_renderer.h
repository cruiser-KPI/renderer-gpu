
#ifndef RENDERER_GPU_OPTIX_RENDERER_H
#define RENDERER_GPU_OPTIX_RENDERER_H

#include <vector>
#include <memory>

#include <optixu/optixpp_namespace.h>

class OptixRenderer
{
public:
    OptixRenderer(int width, int height);

    bool updateParameters();
    bool processInputs();
    void update();

    void load(const char *sceneFile);
    void resize(int w, int h);

    void render();
    void renderToFile(const std::string &filename);
    bool renderingRunning();

    optix::Buffer getFilmBuffer();

private:
    std::vector<std::string> m_stats;

};


#endif //RENDERER_GPU_OPTIX_RENDERER_H
