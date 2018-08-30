
#include "camera.h"
#include "../utils/config.h"
#include "../utils/fileutil.h"
#include "../utils/log.h"

#include <iostream>

#include <imgui/imgui.h>

Camera::~Camera()
{
    m_renderBuffer->destroy();

    for (auto &program : m_programMap)
        program.second->destroy();
}

void Camera::setResolution(int w, int h)
{
    if ((m_width != w || m_height != h)) {
        // Never drop to zero viewport size. This avoids lots of checks for zero in other routines.
        m_width = (w) ? w : 1;
        m_height = (h) ? h : 1;
        m_aspect = float(m_width) / float(m_height);
        m_changed = true;

        try {
            m_renderBuffer->setSize(m_width, m_height); // RGBA32F buffer.
            m_context["resolution"]->setUint(optix::make_uint2(m_width, m_height));
        }
        catch (optix::Exception &e) {
            throw std::runtime_error(string_format("Error while resizing Camera %s",
                                                   e.getErrorString().c_str()));
        }
    }
}

void Camera::orbit(int x, int y)
{
    if (setDelta(x, y)) {
        m_phi -= float(m_dx) / float(m_width); // Inverted.
        // Wrap phi.
        if (m_phi < 0.0f) {
            m_phi += 1.0f;
        }
        else if (1.0f < m_phi) {
            m_phi -= 1.0f;
        }

        m_theta += float(m_dy) / float(m_height);
        // Clamp theta.
        if (m_theta < 0.0f) {
            m_theta = 0.0f;
        }
        else if (1.0f < m_theta) {
            m_theta = 1.0f;
        }
    }
}

void Camera::pan(int x, int y)
{
    if (setDelta(x, y)) {
        // m_speedRatio pixels will move one vector length.
        float u = float(m_dx) / m_speedRatio;
        float v = float(m_dy) / m_speedRatio;
        // Pan the center of interest, the rest will follow.
        m_center = m_center - u * m_cameraU + v * m_cameraV;
    }
}

void Camera::dolly(int x, int y)
{
    if (setDelta(x, y)) {
        // m_speedRatio pixels will move one vector length.
        float w = float(m_dy) / m_speedRatio;
        // Adjust the distance, the center of interest stays fixed so that the orbit is around the same center.
        m_distance -= w * length(m_cameraW); // Dragging down moves the camera forwards. "Drag-in the object".
        if (m_distance < 0.001f) // Avoid swapping sides. Scene units are meters [m].
        {
            m_distance = 0.001f;
        }
    }
}

void Camera::focus(int x, int y)
{
    if (setDelta(x, y)) {
        // m_speedRatio pixels will move one vector length.
        float w = float(m_dy) / m_speedRatio;
        // Adjust the center of interest.
        setFocusDistance(m_distance - w * length(m_cameraW));
    }
}

void Camera::setFocusDistance(float f)
{
    if (m_distance != f && 0.001f < f) // Avoid swapping sides.
    {
        m_distance = f;
        m_center = m_cameraPosition + m_distance
            * m_cameraW; // Keep the camera position fixed and calculate a new center of interest which is the focus plane.
        m_changed = true; // m_changed is only reset when asking for the frustum
    }
}

void Camera::zoom(float x)
{
    m_fov += float(x);
    if (m_fov < 1.0f) {
        m_fov = 1.0f;
    }
    else if (179.0 < m_fov) {
        m_fov = 179.0f;
    }
    m_changed = true;
}

bool Camera::setDelta(int x, int y)
{
    if (m_baseX != x || m_baseY != y) {
        m_dx = x - m_baseX;
        m_dy = y - m_baseY;

        m_baseX = x;
        m_baseY = y;

        m_changed = true; // m_changed is only reset when asking for the frustum.
        return true; // There is a delta.
    }
    return false;
}

void Camera::load(const pugi::xml_node &node)
{
    m_distance = readFloat(node.child("distance"), m_distance);
    m_phi = readFloat(node.child("phi"), m_phi);
    m_theta = readFloat(node.child("theta"), m_theta);
    m_center = readVector3(node.child("center"), m_center);
    m_fov = readFloat(node.child("fov"), m_fov);
    m_changed = true;

    update();
}

void Camera::save(pugi::xml_node &node)
{

}

void Camera::updateParameters()
{
    if (ImGui::CollapsingHeader("Camera")) {
        if (ImGui::DragFloat("Mouse Ratio", &m_speedRatio, 0.1f, 0.1f, 100.0f, "%.1f")) {
            m_changed = true;
        }
    }
}

void Camera::processInputs()
{
    ImGuiIO const &io = ImGui::GetIO();
    const ImVec2 mousePosition = ImGui::GetMousePos(); // Mouse coordinate window client rect.
    const int x = int(mousePosition.x);
    const int y = int(mousePosition.y);

    switch (m_cameraState) {
    case CAMERA_STATE_NONE:
        if (!io.WantCaptureMouse) // Only allow camera interactions to begin when not interacting with the GUI.
        {
            if (ImGui::IsMouseDown(0)) // LMB down event?
            {
                m_baseX = x;
                m_baseY = y;
                m_cameraState = CAMERA_STATE_ORBIT;
            }
            else if (ImGui::IsMouseDown(1)) // RMB down event?
            {
                m_baseX = x;
                m_baseY = y;
                m_cameraState = CAMERA_STATE_DOLLY;
            }
            else if (ImGui::IsMouseDown(2)) // MMB down event?
            {
                m_baseX = x;
                m_baseY = y;
                m_cameraState = CAMERA_STATE_PAN;
            }
            else if (io.MouseWheel != 0.0f) // Mouse wheel zoom.
            {
                zoom(io.MouseWheel);
            }
        }
        break;

    case CAMERA_STATE_ORBIT:
        if (ImGui::IsMouseReleased(0)) // LMB released? End of orbit mode.
        {
            m_cameraState = CAMERA_STATE_NONE;
        }
        else {
            orbit(x, y);
        }
        break;

    case CAMERA_STATE_DOLLY:
        if (ImGui::IsMouseReleased(1)) // RMB released? End of dolly mode.
        {
            m_cameraState = CAMERA_STATE_NONE;
        }
        else {
            dolly(x, y);
        }
        break;

    case CAMERA_STATE_PAN:
        if (ImGui::IsMouseReleased(2)) // MMB released? End of pan mode.
        {
            m_cameraState = CAMERA_STATE_NONE;
        }
        else {
            pan(x, y);
        }
        break;
    }
}

bool Camera::update()
{
    if (m_changed) {
        // Recalculate the camera parameters.
        const float cosPhi = cosf(m_phi * 2.0f * M_PIf);
        const float sinPhi = sinf(m_phi * 2.0f * M_PIf);
        const float cosTheta = cosf(m_theta * M_PIf);
        const float sinTheta = sinf(m_theta * M_PIf);

        // "normal", unit vector from origin to spherical coordinates (phi, theta)
        optix::float3 normal = optix::make_float3(cosPhi * sinTheta,
                                                  -cosTheta,
                                                  -sinPhi * sinTheta);

        float tanFov = tanf((m_fov * 0.5f) * M_PIf / 180.0f); // m_fov is in the range [1.0f, 179.0f].
        m_cameraPosition = m_center + m_distance * normal;

        m_cameraU = m_aspect * optix::make_float3(-sinPhi, 0.0f, -cosPhi) * tanFov;               // "tangent"
        m_cameraV = optix::make_float3(cosTheta * cosPhi, sinTheta, cosTheta * -sinPhi) * tanFov; // "bitangent"
        m_cameraW = -normal; // "-normal" to look at the center.

        try {
            m_context["sysCameraPosition"]->setFloat(m_cameraPosition);
            m_context["sysCameraU"]->setFloat(m_cameraU);
            m_context["sysCameraV"]->setFloat(m_cameraV);
            m_context["sysCameraW"]->setFloat(m_cameraW);
        }
        catch (optix::Exception &e) {
            throw std::runtime_error(string_format("Error while updating Camera: %s",
                                                   e.getErrorString().c_str()));
        }

        m_changed = false;
        return true;
    }
    return false;
}

Camera::Camera()
    : m_context(nullptr),
      m_distance(10.0f) // Some camera defaults for the demo scene.
    , m_phi(0.75f), m_theta(0.6f), m_fov(60.0f), m_width(1), m_height(1), m_aspect(1.0f), m_baseX(0), m_baseY(0),
      m_speedRatio(10.0f), m_dx(0), m_dy(0), m_changed(true), m_cameraState(CameraState::CAMERA_STATE_NONE)
{
    m_center = optix::make_float3(0.0f, 0.0f, 0.0f);
    m_cameraPosition = optix::make_float3(0.0f, 0.0f, 1.0f);
    m_cameraU = optix::make_float3(1.0f, 0.0f, 0.0f);
    m_cameraV = optix::make_float3(0.0f, 1.0f, 0.0f);
    m_cameraW = optix::make_float3(0.0f, 0.0f, -1.0f);
}

void Camera::setContext(optix::Context context)
{
    if (m_context != context)
    {
        try {
            m_context = context;

            m_renderBuffer = m_context->createBuffer(RT_BUFFER_OUTPUT);
            m_renderBuffer->setFormat(RT_FORMAT_FLOAT4); // RGBA32F
            m_renderBuffer->setSize(m_width, m_height);
            m_context["sysOutputBuffer"]->set(m_renderBuffer);

            // Set the ray generation program and the exception program.
            m_programMap["raygeneration"] = m_context->createProgramFromPTXFile(
                shaderFolder + "raygeneration.ptx", "raygeneration");
            m_context->setRayGenerationProgram(0, m_programMap["raygeneration"]);

            m_programMap["exception"] = m_context->createProgramFromPTXFile(
                shaderFolder + "exception.ptx", "exception");
            m_context->setExceptionProgram(0, m_programMap["exception"]);
        }
        catch (optix::Exception &e) {
            throw std::runtime_error(string_format("Error while creating Camera: %s",
                                                   e.getErrorString().c_str()));
        }
    }
}

Camera &Camera::getInstance(optix::Context context)
{
    static Camera instance;
    instance.setContext(context);
    return instance;
}
