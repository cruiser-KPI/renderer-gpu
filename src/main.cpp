#include <iostream>
#include <vector>
#include <stdexcept>
#include <algorithm>
#include <future>

#include "core/opengl_renderer.h"
#include "core/optix_renderer.h"
#include <GLFW/glfw3.h>

#include "utils/log.h"

#include <imgui/imgui.h>


static void error_callback(int error, const char *description)
{
    std::cerr << "Error: " << error << ": " << description << std::endl;
}


bool cmdOptionExists(char** begin, char** end, const std::string& option)
{
    return std::find(begin, end, option) != end;
}

std::string GetLineFromCin() {
    std::string line;
    std::getline(std::cin, line);
    return line;
}

int renderGUI(OptixRenderer *sceneRenderer, int windowWidth, int windowHeight)
{
    int returnCode = 0;
    bool windowCreated = false;
    try {
        glfwSetErrorCallback(error_callback);

        if (!glfwInit())
            throw std::runtime_error("GLFW failed to initialize.");

        GLFWwindow *window = glfwCreateWindow(windowWidth, windowHeight, "OptiX test", NULL, NULL);
        if (!window)
            throw std::runtime_error("glfwCreateWindow() failed.");
        windowCreated = true;

        glfwMakeContextCurrent(window);

        if (glewInit() != GL_NO_ERROR)
            throw std::runtime_error("GLEW failed to initialize.");

        auto screenRenderer = new OpenGLRenderer(window, windowWidth, windowHeight, sceneRenderer);

        while (!glfwWindowShouldClose(window)) {
            glfwPollEvents(); // Render continuously.

            glfwGetFramebufferSize(window, &windowWidth, &windowHeight);
            screenRenderer->reshape(windowWidth, windowHeight);
            screenRenderer->guiNewFrame();
            screenRenderer->guiWindow(); // draw gui, change parameters
            screenRenderer->guiEventHandler(); // handle user input
            screenRenderer->update(); // update all objects and settings
            screenRenderer->render();  // OptiX rendering and OpenGL texture update.
            screenRenderer->display(); // OpenGL display.
            ImGui::ShowDemoWindow();
            screenRenderer->guiRender();

            glfwSwapBuffers(window);

            //glfwWaitEvents(); // Render only when an event is happening. Needs some glfwPostEmptyEvent() to prevent GUI lagging one frame behind when ending an action.
        }
        delete screenRenderer;
    }
    catch (const std::exception &ex) {
        LogError("EXCEPTION: %s", ex.what());
        returnCode = 1;
    }
    if (windowCreated)
        glfwTerminate();

    return returnCode;
}


int main(int argc, char *argv[])
{
    const char *sceneFile = argv[1];
    int windowWidth=1, windowHeight=1;
    try {
        windowWidth = std::stoi(argv[2]);
        windowHeight = std::stoi(argv[3]);
    }
    catch(std::invalid_argument& e){

    }

    OptixRenderer *sceneRenderer;
    try {
        sceneRenderer = new OptixRenderer(windowWidth, windowHeight);
    }
    catch (std::runtime_error &e){
        LogError("OptiX Context wasn't created. Error: %s", e.what());
        return 1;
    }

    try {
        sceneRenderer->load(sceneFile);
    }
    catch (std::runtime_error &e){
        LogError("Unable to load scene file. Error: %s", e.what());
        return 1;
    }

    auto future = std::async(std::launch::async, GetLineFromCin);
    while (true) {
        if (future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            auto line = future.get();

            try {
                if (line == "start") {
                    sceneRenderer->renderToFile("result.img");
                    std::cout << "finished" << std::endl;
                }
                else if (line == "resize") {
                    int x = std::stoi(GetLineFromCin());
                    int y = std::stoi(GetLineFromCin());
                    sceneRenderer->resize(x, y);
                    sceneRenderer->update();
                    windowWidth = x;
                    windowHeight = y;
                }
                else if (line == "reload") {
                    std::string filename;
                    if (sceneFile)
                        filename = std::string(sceneFile);
                    else
                        filename = GetLineFromCin();
                    sceneRenderer->load(filename.c_str());
                }
                else if (line == "gui") {
                    renderGUI(sceneRenderer, windowWidth, windowHeight);
                }
                else if (line == "stop")
                    break;
            }
            catch (std::runtime_error &e){
                LogError(e.what());
            }

            future = std::async(std::launch::async, GetLineFromCin);
        }
    }
    delete sceneRenderer;

    return 0;
}