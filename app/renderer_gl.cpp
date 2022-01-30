#include <string>

#include <glm/gtx/transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <msfce/core/log.h>

#include "renderer_gl.h"

#define TAG "RendererGl"

namespace {

#define SIZEOF_ARRAY(x) (sizeof(x) / sizeof((x)[0]))

// clang-format off
const char *vertexShader =
    "#version 330 core\n"
    "layout (location = 0) in vec3 aPos;"
    "layout (location = 1) in vec2 aTexCoord;"

    "uniform mat4 scaleMatrix;"
    "out vec2 TexCoord;"

    "void main()"
    "{"
    "    gl_Position = scaleMatrix * vec4(aPos, 1.0);"
    "    TexCoord = vec2(aTexCoord.x, aTexCoord.y);"
    "}";

const char *fragmentShader =
    "#version 330 core\n"
    "out vec4 FragColor;"

    "in vec3 ourColor;"
    "in vec2 TexCoord;"

    "uniform sampler2D texture1;"

    "void main()"
    "{"
    "    FragColor = texture(texture1, TexCoord);"
    "}";

// clang-format on

void checkCompileErrors(GLuint shader, const std::string& type)
{
    int success;
    char infoLog[1024];

    if (type != "PROGRAM") {
        glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
        if (!success) {
            glGetShaderInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
            LOGE(
                TAG,
                "Shader compilation error of type: '%s': '%s'",
                type.c_str(),
                infoLog);
        }
    } else {
        glGetProgramiv(shader, GL_LINK_STATUS, &success);
        if (!success) {
            glGetProgramInfoLog(shader, sizeof(infoLog), nullptr, infoLog);
            LOGE(TAG, "Shader link error of type: '%s'", infoLog);
        }
    }
}

GLuint compileShader(const char* vShaderCode, const char* fShaderCode)
{
    GLuint vertex, fragment;

    // vertex shader
    vertex = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertex, 1, &vShaderCode, NULL);
    glCompileShader(vertex);
    checkCompileErrors(vertex, "VERTEX");

    // fragment Shader
    fragment = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragment, 1, &fShaderCode, NULL);
    glCompileShader(fragment);
    checkCompileErrors(fragment, "FRAGMENT");

    // shader Program
    GLuint ID = glCreateProgram();
    glAttachShader(ID, vertex);
    glAttachShader(ID, fragment);
    glLinkProgram(ID);
    checkCompileErrors(ID, "PROGRAM");

    // delete the shaders as they're linked into our program now and no longer
    // necessary
    glDeleteShader(vertex);
    glDeleteShader(fragment);

    return ID;
}

} // anonymous namespace

RendererGl::RendererGl(const msfce::core::SnesConfig& snesConfig)
    : m_SnesConfig(snesConfig)
{
}

int RendererGl::initContext()
{
    m_Shader = compileShader(vertexShader, fragmentShader);
    m_ScaleMatrixUniform = glGetUniformLocation(m_Shader, "scaleMatrix");

    // Create VAO
    // clang-format off
    static const float vertices[] = {
         // Coords            // Texture
         1.0f,  1.0f, 0.0f,   1.0f, 0.0f,
         1.0f, -1.0f, 0.0f,   1.0f, 1.0f,
        -1.0f, -1.0f, 0.0f,   0.0f, 1.0f,
        -1.0f,  1.0f, 0.0f,   0.0f, 0.0f
    };

    static const GLuint indices[] = {
        0, 1, 3,
        1, 2, 3
    };
    // clang-format on

    GLuint VBO, EBO;
    glGenVertexArrays(1, &m_VAO);
    glGenBuffers(1, &VBO);
    glGenBuffers(1, &EBO);

    glBindVertexArray(m_VAO);

    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, EBO);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
    m_VAO_ElemSize = SIZEOF_ARRAY(indices);

    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Create texture
    glGenTextures(1, &m_Texture);
    glBindTexture(GL_TEXTURE_2D, m_Texture);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    m_TextureSize = m_SnesConfig.displayWidth * m_SnesConfig.displayHeight * 3;

    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGB,
        m_SnesConfig.displayWidth,
        m_SnesConfig.displayHeight,
        0,
        GL_RGB,
        GL_UNSIGNED_BYTE,
        nullptr);

    glBindTexture(GL_TEXTURE_2D, 0);

    // Create PBO
    glGenBuffers(1, &m_PBO);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_PBO);
    glBufferData(GL_PIXEL_UNPACK_BUFFER, m_TextureSize, 0, GL_STREAM_DRAW);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);

    return 0;
}

void RendererGl::setWindowSize(int windowWidth, int windowHeight)
{
    m_WindowWidth = windowWidth;
    m_WindowHeight = windowHeight;
    m_WindowSizeChanged = true;
}

void RendererGl::setViewport()
{
    glm::mat4 m;

    const float windowRatio = (float)m_WindowWidth / (float)m_WindowHeight;
    const float ppuRatio =
        (float)m_SnesConfig.displayWidth / (float)m_SnesConfig.displayHeight;

    if (windowRatio > ppuRatio) {
        // Window is wider than expected
        const float displayedWidth =
            (float)m_SnesConfig.displayWidth *
            ((float)m_WindowHeight / (float)m_SnesConfig.displayHeight);

        const float widthRatio = displayedWidth / (float)m_WindowWidth;

        m = glm::scale(glm::vec3(widthRatio, 1.0f, 1));
    } else {
        // Window is higher than expected
        const float displayedHeight =
            (float)m_SnesConfig.displayHeight *
            ((float)m_WindowWidth / (float)m_SnesConfig.displayWidth);

        const float heightRatio =
            (float)displayedHeight / (float)m_WindowHeight;

        m = glm::scale(glm::vec3(1.0f, heightRatio, 1));
    }

    glUseProgram(m_Shader);
    glUniformMatrix4fv(m_ScaleMatrixUniform, 1, GL_FALSE, glm::value_ptr(m));
    glUseProgram(0);

    glViewport(0, 0, m_WindowWidth, m_WindowHeight);
}

uint8_t* RendererGl::bindBackbuffer()
{
    glBindTexture(GL_TEXTURE_2D, m_Texture);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, m_PBO);

    glTexSubImage2D(
        GL_TEXTURE_2D, 0, 0, 0, 256, 224, GL_RGB, GL_UNSIGNED_BYTE, 0);

    glBufferData(GL_PIXEL_UNPACK_BUFFER, m_TextureSize, 0, GL_STREAM_DRAW);

    return static_cast<uint8_t*>(
        glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY));
}

int RendererGl::unbindBackbuffer()
{
    glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glBindTexture(GL_TEXTURE_2D, 0);

    return 0;
}

void RendererGl::render()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    if (m_WindowSizeChanged) {
        setViewport();
        m_WindowSizeChanged = false;
    }

    glBindTexture(GL_TEXTURE_2D, m_Texture);

    glUseProgram(m_Shader);
    glBindVertexArray(m_VAO);
    glDrawElements(GL_TRIANGLES, m_VAO_ElemSize, GL_UNSIGNED_INT, 0);
}
