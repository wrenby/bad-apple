#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include <ncurses.h>
#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <GL/freeglut.h>
#include <EGL/egl.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

bool glOk(const char* msg) {
    GLenum err = glGetError();
    if (GL_NO_ERROR == err) {
        return true;
    } else {
        fprintf(stderr, "OpenGL Error (%s): %s\n", msg, (const char*)gluErrorString(err));
        return false;
    }
}

class Renderer {
public:
    Renderer() : vert(0), frag(0), prog(0),
    display(EGL_NO_DISPLAY), ctx(EGL_NO_CONTEXT),
    framebuf(0), tex(0), vao(0) {
        vbos[0] = 0;
        vbos[1] = 0;
        getmaxyx(stdscr, height, width);
        proj = glm::ortho(-10.0f, 10.0f, -10.0f, 10.0f, 0.0f, 100.0f);
        view = glm::lookAt(glm::vec3(0.0f, 0.0f, 10.0f),
                           glm::vec3(0.0f, 0.0f, 0.0f),
                           glm::vec3(0.0f, 1.0f, 0.0f));
        pixels = new uint8_t[round_up(width)*height];
    }
    ~Renderer() {
        if (vao)
            glDeleteVertexArrays(1, &vao);
        if (vbos)
            glDeleteBuffers(2, vbos);
        if (prog)
            glDeleteProgram(prog);
        if (frag)
            glDeleteShader(frag);
        if (vert)
            glDeleteShader(vert);
        if (framebuf)
            glDeleteFramebuffers(1, &framebuf);
        if (ctx)
            eglDestroyContext(display, ctx);
        if (display)
            eglTerminate(display);
        if (pixels)
            delete[] pixels;
    }

    bool init() {
        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (EGL_NO_DISPLAY == display) {
            fprintf(stderr, "EGL could not find a default display\n");
            return false;
        }
        if (EGL_FALSE == eglInitialize(display, nullptr, nullptr)) {
            fprintf(stderr, "Failed to initialize EGL display\n");
            return false;
        }
        EGLConfig config;
        EGLint num_config;
        EGLint config_attribs[] = {
            EGL_CONFORMANT, EGL_OPENGL_BIT,
            EGL_RED_SIZE, 8,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_NONE,
        };
        if (!eglChooseConfig(display, config_attribs, &config, 1, &num_config)) {
            fprintf(stderr, "Failed to select a compatible EGL display config\n");
            return false;
        }
        if (!eglBindAPI(EGL_OPENGL_API)) {
            fprintf(stderr, "Failed to set EGL API to OpenGL\n");
            return false;
        }
        EGLint context_attribs[] = {
            EGL_CONTEXT_MAJOR_VERSION, 4,
            EGL_CONTEXT_MINOR_VERSION, 1,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
            EGL_NONE,
        };
        ctx = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attribs);
        if (EGL_NO_CONTEXT == ctx) {
            fprintf(stderr, "Failed to create OpenGL context\n");
            return false;
        }
        if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx)) {
            fprintf(stderr, "Failed to attach OpenGL context to EGL display\n");
            return false;
        }

        GLenum err = glewInit();
        if (GLEW_OK != err) {
            fprintf(stderr, "GLEW init error: %s\n", glewGetErrorString(err));
            return false;
        } else if (!GLEW_VERSION_4_1) {
            fprintf(stderr, "OpenGL 4.1 not supported\n");
            return false;
        }

        glGenFramebuffers(1, &framebuf);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, width + 1, height + 1, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
        if (!glOk("glTexImage2D")) return false;
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        if (!glOk("glFramebufferTexture2D")) return false;
        GLenum fb_check = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(fb_check != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "Frame buffer status: %d\n", fb_check);
            return false;
        }

        vert = loadShader(GL_VERTEX_SHADER, "shaders/donut.vert");
        if (vert == 0) {
            fprintf(stderr, "Failed to load vertex shader\n");
            return false;
        }
        frag = loadShader(GL_FRAGMENT_SHADER, "shaders/donut.frag");
        if (frag == 0) {
            fprintf(stderr, "Failed to load fragment shader\n");
            return false;
        }
        prog = glCreateProgram();
        glAttachShader(prog, vert);
        glAttachShader(prog, frag);
        glLinkProgram(prog);

        generateTorus(2.0f, 5.0f, 10, 10);

        return true;
    }

    void render_frame() {
        if (is_term_resized(height, width)) {
            getmaxyx(stdscr, height, width);
        }

        glBindFramebuffer(GL_FRAMEBUFFER, framebuf);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        // glUseProgram(prog);
        // glBindVertexArray(vao);
        // glDrawArrays(GL_TRIANGLE_STRIP, 0, vertices.size());
        glFinish();

        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, pixels);

        constexpr int INDEX_MAX = 9;
        // + 1 for '\0', +1 for zero-indexing
        constexpr char const index[INDEX_MAX + 2] = " .,:;izn%#";

        constexpr float FONT_ASPECT_RATIO = 0.5f; // not sure if possible to get programmatically
        int y;
        int stride = round_up(width);
        for (y = 0; y < height; y++) {
            int x;
            for (x = 0; x < width; x++) {
                fprintf(stderr, "%-3d ", pixels[y*stride + x]);
                int i = pixels[y*stride + x] / 256.0f * INDEX_MAX;
                mvaddch(y, x, index[i]);
            }
            fprintf(stderr, "\n");
        }
        fprintf(stderr, "\n");

        mvprintw(0, 0, "Q to Exit (%dx%d)", width, height);
    }
private:
    static GLuint loadShader(GLenum stage, char const *filename) {
        std::ifstream file(filename, std::ios::in);
        if (!file) {
            fprintf(stderr, "Failed to open file: %s", filename);
            return false;
        }
        const auto size = std::filesystem::file_size(filename);
        std::string src(size, '\0');
        file.read(src.data(), size);
        const GLchar *string = src.c_str();
        const GLint length = src.length();

        GLuint out = glCreateShader(stage);
        glShaderSource(out, 1, &string, &length);
        glCompileShader(out);
        GLint compiled = GL_FALSE;
        glGetShaderiv(out, GL_COMPILE_STATUS, &compiled);
        if (GL_TRUE != compiled) {
            fprintf(stderr, "Failed to compile shader: %s\n", filename);

            GLint log_len = 0;
            glGetShaderiv(out, GL_INFO_LOG_LENGTH, &log_len);
            GLchar* log = new GLchar[log_len];
            glGetShaderInfoLog(out, log_len, NULL, log);
            fprintf(stderr, "Error Log:\n%s\n", log);
            delete[] log;

            glDeleteShader(out);
            return 0;
        }
        return out;
    }

    void generateTorus(float inner_radius, float outer_radius, int sides, int rings) {
        // https://electronut.in/torus/
        float du = 2 * M_PI / rings;
        float dv = 2 * M_PI / sides;

        for (size_t i = 0; i < rings; i++) {
            float u = i * du;
            for (size_t j = 0; j <= sides; j++) {
                float v = (j % sides) * dv;
                for (size_t k = 0; k < 2; k++)
                {
                    float uu = u + k * du;
                    // compute vertex
                    float x = (outer_radius + inner_radius * cos(v)) * cos(uu);
                    float y = (outer_radius + inner_radius * cos(v)) * sin(uu);
                    float z = inner_radius * sin(v);

                    // add vertex
                    vertices.push_back(x);
                    vertices.push_back(y);
                    vertices.push_back(z);

                    // compute normal
                    float nx = cos(v) * cos(uu);
                    float ny = cos(v) * sin(uu);
                    float nz = sin(v);

                    // add normal
                    normals.push_back(nx);
                    normals.push_back(ny);
                    normals.push_back(nz);
                }
                // incr angle
                v += dv;
            }
        }

        glGenBuffers(2, vbos);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(float) * normals.size(), normals.data(), GL_STATIC_DRAW);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
    }

    // rounds up to the nearest multiple of four
    // required because the textures are stored in multiples of four bytes
    static inline int round_up(int raw) {
        return (raw + 4 - (raw%4));
    }

    int width, height;
    uint8_t* pixels;
    glm::mat4 view, proj;
    EGLDisplay display;
    EGLContext ctx;
    GLuint framebuf, tex;
    GLuint vert, frag, prog;
    std::vector<float> vertices, normals;
    GLuint vbos[2];
    GLuint vao;
};

int main(int argc, char** argv) {
    initscr();
    // hide cursor
    curs_set(0);
    // disable typing
    noecho();
    // make getch non-blocking
    nodelay(stdscr, TRUE);

    Renderer r;
    if (r.init()) {
        bool loop = true;
        while (loop) {
            int ch = getch();
            if (ch == ERR) {
                // ERR is returned in non-blocking mode -- not actually an error to be handled
            } else if (ch == 'q') {
                loop = false;
            }

            r.render_frame();
            refresh();

            using namespace std::chrono_literals;
            std::this_thread::sleep_for(16ms);
        }
    }

    endwin();
    return 0;
}