
#include "optix_renderer.h"
#include "scene.h"
#include "globalsettings.h"
#include "../utils/log.h"
#include "../utils/stats.h"

#include <fstream>
#include <sstream>

#include <imgui/imgui.h>

OptixRenderer::OptixRenderer(int width, int height)
{
    Scene::getInstance().setResolution(width, height);
}


void OptixRenderer::load(const char *sceneFile)
{
    if (!sceneFile)
        return;

    pugi::xml_document doc;
    auto result = doc.load_file(sceneFile);
    if (!result) {
        throw std::runtime_error(string_format("Couldn't load scene file"));
    }
    auto root = doc.child("root");
    if (!root)
        throw std::runtime_error(string_format("Invalid data in scene file"));

    GlobalSettings::getInstance().load(root.child("settings"));
    Scene::getInstance().load(root.child("scene"));

    LogInfo("Scene file '%s' was successfully loaded", sceneFile);
}


bool OptixRenderer::processInputs()
{
    Scene::getInstance().processInputs();
    return false;
}

bool OptixRenderer::updateParameters()
{
    ImGui::SetNextWindowSize(ImVec2(200, 200), ImGuiSetCond_FirstUseEver);

    ImGuiWindowFlags window_flags = 0;
    if (!ImGui::Begin("Settings", nullptr, window_flags)) // No bool flag to omit the close button.
    {
        // Early out if the window is collapsed, as an optimization.
        ImGui::End();
    } else {

        ImGui::PushItemWidth(-100); // right-aligned, keep 180 pixels for the labels.
        Scene::getInstance().updateParameters();
        ImGui::PopItemWidth();
        ImGui::End();
    }

    Logger::getInstance().Draw("Log");

    ImGui::SetNextWindowSize(ImVec2(400, 400), ImGuiSetCond_FirstUseEver);
    if (!ImGui::Begin("Statistics", nullptr))
        ImGui::End();
    else {
        if (!Scene::getInstance().renderingRunning()) {
            m_stats = GetStats();
            ClearStats();
        }
        for (auto &stat : m_stats)
            ImGui::Text(stat.c_str());

        ImGui::End();
    }

    return false;
}

void OptixRenderer::resize(int w, int h)
{
    Scene::getInstance().setResolution(w, h);

    LogInfo("Resized window to (%d, %d)", w, h);
}

void OptixRenderer::render()
{
    Scene::getInstance().render();
}

bool OptixRenderer::renderingRunning()
{
    return Scene::getInstance().renderingRunning();
}

optix::Buffer OptixRenderer::getFilmBuffer()
{
    return Scene::getInstance().getFilmBuffer();
}

void OptixRenderer::renderToFile(const std::string &filename)
{
    Scene::getInstance().renderToFile(filename);
}

void OptixRenderer::update()
{
    Scene::getInstance().update();
}
