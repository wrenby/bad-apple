#include <iostream>
#include <fstream>
#include <filesystem>
#include <string>
#include <vector>
#include <mutex>

#include <chrono>
#include <thread>

#include <ncurses.h>

#include <GL/glew.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <EGL/egl.h>

#define GLM_FORCE_RADIANS
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <vlc/vlc.h>

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
    framebuf(0), colorbuf(0), vao(0),
    videoTex(0), videoWidth(1440), videoHeight(1080),
    vlc(nullptr), mediaPlayer(nullptr), videoPixels(nullptr) {
        vbos[0] = 0;
        vbos[1] = 0;
        getmaxyx(stdscr, height, width);
        pixels = new uint8_t[round_up(width)*height];
    }
    ~Renderer() {
        if (mediaPlayer) {
            libvlc_media_player_stop(mediaPlayer);
            libvlc_media_player_release(mediaPlayer);
        }
        if (vlc)
            libvlc_release(vlc);

        if (videoTex)
            glDeleteTextures(1, &videoTex);
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
        if (colorbuf)
            glDeleteRenderbuffers(1, &colorbuf);
        if (framebuf)
            glDeleteFramebuffers(1, &framebuf);
        if (ctx)
            eglDestroyContext(display, ctx);
        if (display)
            eglTerminate(display);
        if (videoPixels)
            delete[] videoPixels;
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
            EGL_CONTEXT_MINOR_VERSION, 3,
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

        // color attachment
        glGenRenderbuffers(1, &colorbuf);
        glBindRenderbuffer(GL_RENDERBUFFER, colorbuf);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_R8, width, height);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuf);

        GLenum fb_check = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(fb_check != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "Frame buffer status: %d\n", fb_check);
            return false;
        }

        vert = loadShader(GL_VERTEX_SHADER, "resources/video.vert");
        if (vert == 0) {
            fprintf(stderr, "Failed to load vertex shader\n");
            return false;
        }
        frag = loadShader(GL_FRAGMENT_SHADER, "resources/video.frag");
        if (frag == 0) {
            fprintf(stderr, "Failed to load fragment shader\n");
            return false;
        }
        prog = glCreateProgram();
        glAttachShader(prog, vert);
        glAttachShader(prog, frag);
        glLinkProgram(prog);

        initVideo();

        return true;
    }

    void on_resize(int w, int h) {
        delete[] pixels;
        pixels = new uint8_t[round_up(width)*height];

        glBindRenderbuffer(GL_RENDERBUFFER, 0);

        if (colorbuf)
            glDeleteRenderbuffers(1, &colorbuf);
        if (framebuf)
            glDeleteFramebuffers(1, &framebuf);

        glGenFramebuffers(1, &framebuf);
        glBindFramebuffer(GL_FRAMEBUFFER, framebuf);

        // color attachment
        glGenRenderbuffers(1, &colorbuf);
        glBindRenderbuffer(GL_RENDERBUFFER, colorbuf);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_R8, w, h);
        // glOk("resize color buf");
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_RENDERBUFFER, colorbuf);

        GLenum fb_check = glCheckFramebufferStatus(GL_FRAMEBUFFER);
        if(fb_check != GL_FRAMEBUFFER_COMPLETE) {
            fprintf(stderr, "Frame buffer status: %d\n", fb_check);
        }
    }

    void render_frame(int64_t dtime) {
        if (is_term_resized(height, width)) {
            getmaxyx(stdscr, height, width);
            on_resize(width, height);
        }

        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, framebuf);
        glDrawBuffer(GL_COLOR_ATTACHMENT0);

        glViewport(0, 0, width, height);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glUseProgram(prog);
        glBindVertexArray(vao);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, videoTex);
        {
            vlcMutex.lock();
            if (newFrameReady) {
                glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, videoWidth, videoHeight, 0, GL_BGR, GL_UNSIGNED_BYTE, videoPixels);
                newFrameReady = false;
            }
            vlcMutex.unlock();
        }
        glDrawArrays(GL_TRIANGLE_FAN, 0, vertices.size());
        glFinish();

        glBindFramebuffer(GL_READ_FRAMEBUFFER, framebuf);
        glReadBuffer(GL_COLOR_ATTACHMENT0);
        glReadPixels(0, 0, width, height, GL_RED, GL_UNSIGNED_BYTE, pixels);

        constexpr int INDEX_MAX = 9;
        // + 1 for '\0', +1 for zero-indexing
        constexpr char const index[INDEX_MAX + 2] = " .,:;izn%#";

        int y;
        int stride = round_up(width);
        for (y = 0; y < height; y++) {
            int x;
            for (x = 0; x < width; x++) {
                int i = std::roundf(pixels[y*stride + x] / 256.0f * INDEX_MAX);
                mvaddch(y, x, index[i]);
            }
        }

        if (!libvlc_media_player_is_playing(mediaPlayer))
            mvprintw(0, 0, "Q to Exit");
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

    bool initVideo() {
        // draw as GL_TRIANGLE_FAN
        vertices = {
            glm::vec3( 1.0f,  1.0f, -1.0f),
            glm::vec3(-1.0f,  1.0f, -1.0f),
            glm::vec3(-1.0f, -1.0f, -1.0f),
            glm::vec3( 1.0f, -1.0f, -1.0f),
        };

        texcoords = {
            glm::vec2(1.0f, 1.0f),
            glm::vec2(0.0f, 1.0f),
            glm::vec2(0.0f, 0.0f),
            glm::vec2(1.0f, 0.0f),
        };

        glGenBuffers(2, vbos);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * vertices.size(), vertices.data(), GL_STATIC_DRAW);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
        glBufferData(GL_ARRAY_BUFFER, sizeof(glm::vec3) * texcoords.size(), texcoords.data(), GL_STATIC_DRAW);

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[0]);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, nullptr);
        glEnableVertexAttribArray(1);
        glBindBuffer(GL_ARRAY_BUFFER, vbos[1]);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

        glGenTextures(1, &videoTex);
        glBindTexture(GL_TEXTURE_2D, videoTex);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_DECAL);

        const char* argv[] = {
            "--no-xlib",
            "--no-video-title-show",
#ifndef DEBUG
            "--quiet", // no console output
#endif
        };
        int argc = sizeof(argv) / sizeof(*argv);
        vlc = libvlc_new(argc, argv);

        libvlc_media_t *m;
        m = libvlc_media_new_path(vlc, "resources/bad-apple.mp4");
        mediaPlayer = libvlc_media_player_new_from_media(m);
        libvlc_media_release(m);
        videoPixels = new unsigned char[videoWidth*videoHeight*3];
        libvlc_video_set_callbacks(mediaPlayer, videoLockCallback, videoUnlockCallback, videoDisplayCallback, this);
        libvlc_video_set_format(mediaPlayer, "RV24", videoWidth, videoHeight, videoWidth*3);
        libvlc_media_player_play(mediaPlayer);

        return true;
    }

    static void *videoLockCallback(void *object, void **planes) {
      Renderer* r = (Renderer*) object;
      r->vlcMutex.lock();
      planes[0] = (void*) r->videoPixels;
      return NULL;
    }

    static void videoUnlockCallback(void *object, void *picture, void * const *planes) {
      Renderer* r = (Renderer*) object;
      r->newFrameReady= true;
      r->vlcMutex.unlock();
    }

    static void videoDisplayCallback(void *object, void *picture) {}

    // rounds up to the nearest multiple of four
    // required because the textures are stored in multiples of four bytes
    static inline int round_up(int raw) {
        int remainder = raw % 4;
        if (remainder == 0)
            return raw;
        else
            return raw + 4 - remainder;
    }

    int width, height;
    uint8_t* pixels;
    EGLDisplay display;
    EGLContext ctx;
    GLuint framebuf, colorbuf;
    GLuint vert, frag, prog;
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> texcoords;
    GLuint vbos[2];
    GLuint vao;

    unsigned char* videoPixels;
    GLuint videoTex;
    unsigned int videoWidth, videoHeight;
    libvlc_instance_t *vlc;
    libvlc_media_player_t *mediaPlayer;
    std::mutex vlcMutex;
    bool newFrameReady;
};

int main(int argc, char** argv) {
    initscr();
    // hide cursor
    curs_set(0);
    // disable typing
    noecho();
    // make getch non-blocking
    nodelay(stdscr, TRUE);

    auto start = std::chrono::system_clock::now();

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

            auto now = std::chrono::system_clock::now();
            auto dtime = std::chrono::duration_cast<std::chrono::milliseconds>(now - start).count();
            r.render_frame(dtime);
            refresh();

            auto after = std::chrono::system_clock::now();
            auto render_time = std::chrono::duration_cast<std::chrono::milliseconds>(after - now).count();
            std::this_thread::sleep_for(std::chrono::milliseconds(16 - render_time));
        }
    }

    endwin();
    return 0;
}