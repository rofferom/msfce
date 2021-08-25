#pragma once

#include <epoxy/gl.h>

#include <msfce/core/snes.h>

class RendererGl {
public:
    RendererGl(const msfce::core::SnesConfig& snesConfig);

    int initContext();
    int clearContext();

    void setWindowSize(int windowWidth, int windowHeight);

    uint8_t* bindBackbuffer();
    int unbindBackbuffer();

    void render();

private:
    void setViewport();

private:
    msfce::core::SnesConfig m_SnesConfig;

    int m_WindowWidth = -1;
    int m_WindowHeight = -1;
    bool m_WindowSizeChanged = false;

    GLuint m_Shader = 0;
    GLint m_ScaleMatrixUniform = -1;
    GLuint m_VAO = 0;
    GLsizei m_VAO_ElemSize = 0;
    GLuint m_PBO = 0;
    int m_TextureSize = 0;
    GLuint m_Texture = 0;
    GLubyte* m_TextureData = nullptr;
};
