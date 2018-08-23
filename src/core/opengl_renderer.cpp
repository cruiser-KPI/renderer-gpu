
#include "opengl_renderer.h"

#include "../utils/config.h"
#include "../utils/log.h"
#include "../utils/stats.h"

#include <iostream>
#include <string>
#include <stdexcept>
#include <fstream>

#include <optixu/optixu_math_namespace.h>
#include <imgui/imgui.h>
#define IMGUI_DEFINE_MATH_OPERATORS 1
#include <imgui/imgui_internal.h>
#include <imgui/imgui_impl_glfw_gl2.h>


#include "optix_renderer.h"


OpenGLRenderer::OpenGLRenderer(GLFWwindow *window,
                               int width,
                               int height,
                               OptixRenderer *renderer)
    : m_window(window), m_width(width), m_height(height), m_renderer(renderer),
    m_isGuiVisible(true), m_glslVS(0), m_glslFS(0), m_glslProgram(0)
{
    // Setup ImGui binding.
    ImGui::CreateContext();
    ImGui_ImplGlfwGL2_Init(window, true);

    // This initializes the GLFW part including the font texture.
    ImGui_ImplGlfwGL2_NewFrame();
    ImGui::EndFrame();

    initOpenGL();
}

OpenGLRenderer::~OpenGLRenderer()
{
    ImGui_ImplGlfwGL2_Shutdown();
    ImGui::DestroyContext();
}


void OpenGLRenderer::guiNewFrame()
{
    ImGui_ImplGlfwGL2_NewFrame();
}

void OpenGLRenderer::guiRender()
{
    ImGui::Render();
    ImGui_ImplGlfwGL2_RenderDrawData(ImGui::GetDrawData());
}

void OpenGLRenderer::guiWindow()
{
    if (!m_isGuiVisible) // Use SPACE to toggle the display of the GUI window.
    {
        return;
    }

    m_renderer->updateParameters();
}

void OpenGLRenderer::guiEventHandler()
{
    if (ImGui::IsKeyPressed(' ', false)) // Toggle the GUI window display with SPACE key.
    {
        m_isGuiVisible = !m_isGuiVisible;
    }

    m_renderer->processInputs();
}

void OpenGLRenderer::reshape(int width, int height)
{
    if ((width != 0 && height != 0) && // Zero sized interop buffers are not allowed in OptiX.
        (m_width != width || m_height != height)) {
        m_width = width;
        m_height = height;

        glViewport(0, 0, m_width, m_height);
        m_renderer->resize(m_width, m_height);
    }
}

void OpenGLRenderer::initOpenGL()
{
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    glViewport(0, 0, m_width, m_height);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();

    // glPixelStorei(GL_UNPACK_ALIGNMENT, 4); // default, works for BGRA8, RGBA16F, and RGBA32F.

    glGenTextures(1, &m_hdrTexture);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_hdrTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, 0);

    initGLSL();
}

void OpenGLRenderer::initGLSL()
{
    static const std::string vsSource =
        "#version 330\n"
        "layout(location = 0) in vec4 attrPosition;\n"
        "layout(location = 8) in vec2 attrTexCoord0;\n"
        "out vec2 varTexCoord0;\n"
        "void main()\n"
        "{\n"
        "  gl_Position  = attrPosition;\n"
        "  varTexCoord0 = attrTexCoord0;\n"
        "}\n";

    static const std::string fsSource =
        "#version 330\n"
        "uniform sampler2D samplerHDR;\n"
        "in vec2 varTexCoord0;\n"
        "layout(location = 0, index = 0) out vec4 outColor;\n"
        "void main()\n"
        "{\n"
        "  outColor = texture(samplerHDR, varTexCoord0);\n"
        "}\n";

    GLint vsCompiled = 0;
    GLint fsCompiled = 0;

    m_glslVS = glCreateShader(GL_VERTEX_SHADER);
    if (m_glslVS) {
        GLsizei len = (GLsizei) vsSource.size();
        const GLchar *vs = vsSource.c_str();
        glShaderSource(m_glslVS, 1, &vs, &len);
        glCompileShader(m_glslVS);

        glGetShaderiv(m_glslVS, GL_COMPILE_STATUS, &vsCompiled);
    }

    m_glslFS = glCreateShader(GL_FRAGMENT_SHADER);
    if (m_glslFS) {
        GLsizei len = (GLsizei) fsSource.size();
        const GLchar *fs = fsSource.c_str();
        glShaderSource(m_glslFS, 1, &fs, &len);
        glCompileShader(m_glslFS);

        glGetShaderiv(m_glslFS, GL_COMPILE_STATUS, &fsCompiled);
    }

    m_glslProgram = glCreateProgram();
    if (m_glslProgram) {
        GLint programLinked = 0;

        if (m_glslVS && vsCompiled) {
            glAttachShader(m_glslProgram, m_glslVS);
        }
        if (m_glslFS && fsCompiled) {
            glAttachShader(m_glslProgram, m_glslFS);
        }
        glLinkProgram(m_glslProgram);

        glGetProgramiv(m_glslProgram, GL_LINK_STATUS, &programLinked);
        if (programLinked) {
            glUseProgram(m_glslProgram);

            glUniform1i(glGetUniformLocation(m_glslProgram, "samplerHDR"), 0); // texture image unit 0

            glUseProgram(0);
        }
    }
}

void OpenGLRenderer::render()
{
    m_renderer->render();
}

void OpenGLRenderer::display()
{
    // update OpenGL texture when rendering is done
    if (!m_renderer->renderingRunning()) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m_hdrTexture);
        optix::Buffer renderBuffer = m_renderer->getFilmBuffer();
        const void *data = renderBuffer->map(0, RT_BUFFER_MAP_READ);
        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     GL_RGBA32F,
                     (GLsizei) m_width,
                     (GLsizei) m_height,
                     0,
                     GL_RGBA,
                     GL_FLOAT,
                     data); // RGBA32F
        renderBuffer->unmap();
    }

    glBindTexture(GL_TEXTURE_2D, m_hdrTexture);

    glUseProgram(m_glslProgram);

    glBegin(GL_QUADS);
    glTexCoord2f(0.0f, 0.0f);
    glVertex2f(-1.0f, -1.0f);
    glTexCoord2f(1.0f, 0.0f);
    glVertex2f(1.0f, -1.0f);
    glTexCoord2f(1.0f, 1.0f);
    glVertex2f(1.0f, 1.0f);
    glTexCoord2f(0.0f, 1.0f);
    glVertex2f(-1.0f, 1.0f);
    glEnd();

    glUseProgram(0);
}

void OpenGLRenderer::update()
{
    m_renderer->update();
}



