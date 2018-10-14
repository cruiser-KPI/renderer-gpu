
#ifndef RENDERER_GPU_CAMERA_H
#define RENDERER_GPU_CAMERA_H

#include <optix.h>
#include <optixu/optixpp_namespace.h>
#include <optixu/optixu_math_namespace.h>

#include <pugixml.hpp>

#include <map>

enum CameraState
{
    CAMERA_STATE_NONE,
    CAMERA_STATE_ORBIT,
    CAMERA_STATE_PAN,
    CAMERA_STATE_DOLLY,
    CAMERA_STATE_FOCUS
};

class Camera
{
public:
    Camera(optix::Context context);
    ~Camera();

    void updateParameters();
    void processInputs();
    bool update();

    optix::Buffer getFilmBuffer() const { return m_renderBuffer; }
    void setResolution(int w, int h);
    optix::int2 resolution() const { return optix::make_int2(m_width, m_height); }

    void load(const pugi::xml_node &node);
    void save(pugi::xml_node &node);

public: // Just to be able to load and save them easily.
    optix::float3 m_center;   // Center of interest point, around which is orbited (and the sharp plane of a depth of field camera).
    float         m_distance; // Distance of the camera from the center of intest.
    float         m_phi;      // Range [0.0f, 1.0f] from positive x-axis 360 degrees around the latitudes.
    float         m_theta;    // Range [0.0f, 1.0f] from negative to positive y-axis.
    float         m_fov;      // In degrees. Default is 60.0f

    static Camera& getInstance(optix::Context context);

private:
    Camera();
    void setContext(optix::Context context);

    bool setDelta(int x, int y);
    void setFocusDistance(float f);
    void orbit(int x, int y);
    void pan(int x, int y);
    void dolly(int x, int y);
    void focus(int x, int y);
    void zoom(float x);


private:
    optix::Context m_context;
    optix::Buffer m_renderBuffer;
    std::map<std::string, optix::Program> m_programMap;

    int   m_width;    // Viewport width.
    int   m_height;   // Viewport height.
    float m_aspect;   // m_width / m_height
    int   m_baseX;
    int   m_baseY;
    float m_speedRatio;

    // Derived values:
    int           m_dx;
    int           m_dy;
    bool          m_changed;
    optix::float3 m_cameraPosition;
    optix::float3 m_cameraU;
    optix::float3 m_cameraV;
    optix::float3 m_cameraW;

    CameraState m_cameraState;
};

#endif //RENDERER_GPU_CAMERA_H
