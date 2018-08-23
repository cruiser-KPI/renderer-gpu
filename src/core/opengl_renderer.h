
#ifndef RENDERER_GPU_RENDERER_H
#define RENDERER_GPU_RENDERER_H

#include <memory>

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include <optix.h>
#include <optixu/optixpp_namespace.h>


class OptixRenderer;

class OpenGLRenderer
{
public:
    OpenGLRenderer(GLFWwindow *window,
                       int width,
                       int height,
                       OptixRenderer *renderer);
    ~OpenGLRenderer();

    void render();
    void display();
    void update();
    void reshape(int width, int height);

    void guiNewFrame();
    void guiWindow();
    void guiEventHandler();
    void guiRender();
private:
    void initOpenGL();
    void initGLSL();

    GLFWwindow* m_window;
    bool m_isGuiVisible;

    GLuint m_hdrTexture;
    GLuint m_glslProgram;
    GLuint m_glslVS, m_glslFS;

    OptixRenderer *m_renderer;

    int m_width, m_height;

};


#endif //RENDERER_GPU_RENDERER_H
