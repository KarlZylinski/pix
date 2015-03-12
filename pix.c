#include "pix.h"
#include "gl3w.h"

typedef struct Color
{
    char r;
    char g;
    char b;
} Color;

HWND g_window_handle;
HDC g_device_context;
HGLRC g_rendering_context;
GLuint g_screen_texture;
Color* g_pixels;
GLuint g_geometry;
GLuint g_shader;
GLuint g_shader_texture_samler_id;
unsigned g_window_width;
unsigned g_window_height;
int g_texture_dirty;

GLuint compile_glsl(const char* shader_source, GLenum shader_type)
{
    GLuint result = glCreateShader(shader_type);
    glShaderSource(result, 1, &shader_source, NULL);
    glCompileShader(result);
    return result;
}

GLuint load_shader(const char* vertex_source, const char* fragment_source)
{
    GLuint vertex_shader = compile_glsl(vertex_source, GL_VERTEX_SHADER);
    GLuint fragment_shader = compile_glsl(fragment_source, GL_FRAGMENT_SHADER);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    return program;
}

void pix_init(const char* window_title, unsigned window_width, unsigned window_height)
{
    HINSTANCE h = GetModuleHandle(NULL);
    WNDCLASS wc = {0};    
    GLuint vao;
    int border_width = GetSystemMetrics(SM_CXFIXEDFRAME);
    int h_border_thickness = GetSystemMetrics(SM_CXSIZEFRAME) + border_width;
    int v_border_thickness = GetSystemMetrics(SM_CYSIZEFRAME) + border_width;
    int caption_thickness = GetSystemMetrics(SM_CYCAPTION) + GetSystemMetrics(SM_CXPADDEDBORDER);
    PIXELFORMATDESCRIPTOR pfd =
    {
        sizeof(PIXELFORMATDESCRIPTOR),
        1,
        PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER,
        PFD_TYPE_RGBA,            //The kind of framebuffer. RGBA or palette.
        32,                       //Colordepth of the framebuffer.
        0, 0, 0, 0, 0, 0,
        0,
        0,
        0,
        0, 0, 0, 0,
        32,                       //Number of bits for the depthbuffer
        8,                        //Number of bits for the stencilbuffer
        0,                        //Number of Aux buffers in the framebuffer.
        PFD_MAIN_PLANE,
        0,
        0, 0, 0
    };
    
    static const float fullscreen_quad_data[] = {
        -1.0f, -1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        -1.0f, 1.0f, 0.0f,
        1.0f, -1.0f, 0.0f,
        1.0f, 1.0f, 0.0f,
    };

    static const char* vertex_shader = 
        "#version 410 core\n"
        "in vec3 in_position;"
        "out vec2 texcoord;"
        "void main()"
        "{"
            "texcoord = (vec2(in_position.x, -in_position.y) + vec2(1,1)) * 0.5;"
            "gl_Position = vec4(in_position, 1);"
        "}";

    static const char* fragment_shader = 
        "#version 410 core\n"
        "in vec2 texcoord;"
        "uniform sampler2D texture_sampler;"
        "out vec4 color;"
        "void main()"
        "{"
            "color = texture(texture_sampler, texcoord);"
        "}";

    unsigned texture_size = sizeof(Color) * window_width * window_height;
    g_window_width = window_width;
    g_window_height = window_height;
    g_texture_dirty = 0;
    g_pixels = (Color*)VirtualAlloc(0, texture_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    wc.hInstance = h;
    wc.lpfnWndProc = DefWindowProc;
    wc.hbrBackground = (HBRUSH)(COLOR_BACKGROUND);
    wc.lpszClassName = window_title;
    wc.style = CS_OWNDC;
    RegisterClass(&wc);
    g_window_handle = CreateWindow(window_title, window_title, WS_OVERLAPPEDWINDOW | WS_VISIBLE, 0, 0, window_width + 2 * h_border_thickness,window_height + 2 * v_border_thickness + caption_thickness, 0, 0, h, 0);
    g_device_context = GetDC(g_window_handle);
    SetPixelFormat(g_device_context, ChoosePixelFormat(g_device_context, &pfd), &pfd);
    g_rendering_context = wglCreateContext(g_device_context);
    wglMakeCurrent(g_device_context,g_rendering_context);
    gl3wInit();
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);
    glDisable(GL_DEPTH_TEST);
    glGenTextures(1, &g_screen_texture);
    glBindTexture(GL_TEXTURE_2D, g_screen_texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, window_width, window_height, 0, GL_RGB, GL_UNSIGNED_BYTE, g_pixels);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glGenBuffers(1, &g_geometry);
    glBindBuffer(GL_ARRAY_BUFFER, g_geometry);
    glBufferData(GL_ARRAY_BUFFER, sizeof(fullscreen_quad_data), fullscreen_quad_data, GL_STATIC_DRAW);
    g_shader = load_shader(vertex_shader, fragment_shader);
    g_shader_texture_samler_id = glGetUniformLocation(g_shader, "texture_sampler");
}

void pix_deinit()
{
    DestroyWindow(g_window_handle);
}

void pix_process_events()
{
    MSG msg = {0};

    while(PeekMessage(&msg,0,0,0,PM_REMOVE))
    {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

void pix_put(unsigned x, unsigned y, char r, char g, char b)
{
    Color* pixel = g_pixels + (y * g_window_width + x);
    pixel->r = r;
    pixel->g = g;
    pixel->b = b;
    g_texture_dirty = 1;
}

void pix_clear()
{
    memset(g_pixels, 0, sizeof(Color) * g_window_width * g_window_height);
    g_texture_dirty = 1;
}

void pix_flip()
{
    glUseProgram(g_shader);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, g_screen_texture);
    
    if (g_texture_dirty == 1)
    {
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, g_window_width, g_window_height, GL_RGB, GL_UNSIGNED_BYTE, g_pixels);
        g_texture_dirty = 0;
    }

    glBindBuffer(GL_ARRAY_BUFFER, g_geometry);
    glUniform1i(g_shader_texture_samler_id, 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        0,
        (void*)0
        );
    glDrawArrays(GL_TRIANGLES, 0, 6);
    SwapBuffers(g_device_context);
}

int pix_is_window_open()
{
    return IsWindow(g_window_handle);
}