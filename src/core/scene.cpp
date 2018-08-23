#include "scene.h"
#include "../utils/config.h"
#include "../utils/stats.h"

#include "camera.h"
#include "primitivepool.h"
#include "../utils/log.h"
#include "image.h"

#include <chrono>

REGISTER_DYNAMIC_STATISTIC(float, renderingTime, 0.0f, "Rendering time");
REGISTER_PERMANENT_STATISTIC(float, totalRenderingTime, 0.0f, "Total rendering time");
REGISTER_DYNAMIC_STATISTIC(int, tileNumber, 0, "Number of tiles");
REGISTER_PERMANENT_STATISTIC(int, sampleNumber, 0, "Sample number");

Scene::Scene()
    : m_running(false), currentTileOffset(optix::make_uint2(0, 0)), m_tileSize(128), m_nextTileSize(m_tileSize),
    m_iterationIndex(0), m_sceneChanged(false), m_maxDepth(6)
{
    try {

        m_context = optix::Context::create();

        // Select the GPUs to use with this context.
        unsigned int numberOfDevices = 0;
        RT_CHECK_ERROR_NO_CONTEXT(rtDeviceGetDeviceCount(&numberOfDevices));

        std::vector<int> devices;

        int devicesEncoding = 0; // Preserve this information, it can be stored in the system file.
        unsigned int i = 0;
        do {
            int device = devicesEncoding % 10;
            devices
                .push_back(device); // DAR FIXME Should be a std::set to prevent duplicate device IDs in m_devicesEncoding.
            devicesEncoding /= 10;
            ++i;
        }
        while (i < numberOfDevices && devicesEncoding);

        m_context->setDevices(devices.begin(), devices.end());

        // Print out the current configuration to make sure what's currently running.
        devices = m_context->getEnabledDevices();
        for (size_t i = 0; i < devices.size(); ++i) {
             LogInfo("OptiX Context is using local device %d: %s", devices[i],
                 m_context->getDeviceName(devices[i]).c_str());
        }

        m_context->setEntryPointCount(1); // 0 = render
        m_context->setRayTypeCount(2); // 0 = radiance

#ifdef USE_DEBUG_EXCEPTIONS
        // Disable this by default for performance, otherwise the stitched PTX code will have lots of exception handling inside.
        m_context->setPrintEnabled(true);
        m_context->setPrintLaunchIndex(0, 0);
        m_context->setExceptionEnabled(RT_EXCEPTION_ALL, true);
#endif

        m_context["sysSceneEpsilon"]->setFloat(500 * 1e-7f);
        m_context["sysPathLengths"]->setInt(3, m_maxDepth);

        reset();

    }
    catch (optix::Exception &e) {
        throw std::runtime_error(e.getErrorString());
    }

}

Scene::~Scene()
{
    m_context->destroy();
}

void Scene::setResolution(int width, int height)
{
    Camera::getInstance(m_context).setResolution(width, height);
}

void Scene::render()
{
    m_running = true;

    int nTiles = 0;
    float time = 0.0f;
    try {
        optix::int2 resolution = Camera::getInstance(m_context).resolution();

        bool increase_Y = false;
        auto startTime = std::chrono::high_resolution_clock::now();

        bool stopRendering = false;
        /*
            if OptiX rendering is running for too long GUI starts lagging,
                because OptiX and OpenGL both use graphics card to render
            to prevent this we set maximum rendering time (<16ms)
            this is done by rendering only small tiles, which can be rendered in real time
            tile size if determined at compile time
         */
        while (time < maxRenderingTime && !stopRendering) {
            int valueX = resolution.x - currentTileOffset.x;
            int valueY = resolution.y - currentTileOffset.y;
            int tileSizeX = m_tileSize, tileSizeY = m_tileSize;
            if (valueX <= m_tileSize) {
                tileSizeX = valueX;
                increase_Y = true;
            }
            if (valueY <= m_tileSize) {
                tileSizeY = valueY;
            }
            if (valueX <= m_tileSize && valueY <= m_tileSize){
                // rendering finished
                m_running = false;
                stopRendering = true;
                //time = maxRenderingTime;
            }

            m_context["tileOffset"]->setUint(currentTileOffset);
            m_context->launch(0, tileSizeX, tileSizeY);

            if (increase_Y) {
                currentTileOffset.x = 0;
                currentTileOffset.y += m_tileSize;
                increase_Y = false;
            }
            else
                currentTileOffset.x += m_tileSize;

            auto endTime = std::chrono::high_resolution_clock::now();
            time += std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime).count();
            startTime = endTime;

            nTiles++;
        }
    }
    catch (optix::Exception &e) {
        throw std::runtime_error(e.getErrorString());
    }

    renderingTime += time;
    totalRenderingTime += time / 1000.f;
    tileNumber+= nTiles;

    if (!m_running) {
        currentTileOffset = optix::make_uint2(0, 0);

        sampleNumber = m_iterationIndex;
        m_iterationIndex++;
        m_context["sysIterationIndex"]->setInt(m_iterationIndex);
    }

}
optix::Buffer Scene::getFilmBuffer() const
{
    return Camera::getInstance(m_context).getFilmBuffer();
}

void Scene::updateParameters()
{
    if (ImGui::CollapsingHeader("System")) {
        ImGui::DragInt("Tile size", &m_nextTileSize, 1, 16, 3000);
        ImGui::DragInt("Maximum depth", &m_maxDepth, 1, 1, 20);
    }

    Camera::getInstance(m_context).updateParameters();
    PrimitivePool::getInstance(m_context).updateParameters();
    LightPool::getInstance(m_context).updateParameters();

}

void Scene::processInputs()
{
    Camera::getInstance(m_context).processInputs();
}

void Scene::load(const pugi::xml_node &node)
{
    TexturePool::getInstance(m_context).load(node.child("texture_data"));

    GeometryPool::getInstance(m_context).load(node.child("geometry_data"));
    MaterialPool::getInstance(m_context).load(node.child("material_data"));

    PrimitivePool::getInstance(m_context).load(node.child("primitive_data"));
    LightPool::getInstance(m_context).load(node.child("light_data"));
    Camera::getInstance(m_context).load(node.child("camera"));

    reset();
    m_sceneChanged = true;
}

void Scene::save(pugi::xml_node &node)
{

}
void Scene::reset()
{
    m_context["sysIterationIndex"]->setInt(0);
    m_iterationIndex = 0;
    sampleNumber = 0;

    m_sceneChanged = false;
}

void Scene::renderToFile(const std::string &filename)
{
    int oldTime = maxRenderingTime;
    maxRenderingTime = INFINITY;

    render();

    Image image;
    optix::Buffer buffer = Camera::getInstance(m_context).getFilmBuffer();
    optix::int2 resolution = Camera::getInstance(m_context).resolution();
    const void *data = buffer->map(0, RT_BUFFER_MAP_READ);
    image.load((float*)data, resolution.x, resolution.y);
    image.write(filename);
    buffer->unmap();

    image.clear();
    maxRenderingTime = oldTime;
}

void Scene::update()
{
    if (!m_running) {

        if (m_tileSize != m_nextTileSize) {
            m_tileSize = m_nextTileSize;
            m_sceneChanged = true;
        }

        optix::int2 oldDepth = m_context["sysPathLengths"]->getInt2();
        if (oldDepth.y != m_maxDepth){
            oldDepth.y = m_maxDepth;
            m_context["sysPathLengths"]->setInt(oldDepth);
            m_sceneChanged = true;
        }

        m_sceneChanged |= Camera::getInstance(m_context).update();
        m_sceneChanged |= PrimitivePool::getInstance(m_context).update();
        m_sceneChanged |= LightPool::getInstance(m_context).update();

        if (m_sceneChanged)
            reset();
    }
}

Scene &Scene::getInstance()
{
    static Scene instance;
    return instance;
}

