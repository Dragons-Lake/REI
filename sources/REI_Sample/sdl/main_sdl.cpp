#ifdef _WIN32
#    define _CRT_SECURE_NO_WARNINGS
#endif

#include <sys/stat.h>
#if defined(_WIN32)
#include <crtdbg.h>
#else
#    include <dirent.h>
#    include <unistd.h>
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "REI/Renderer/Renderer.h"

#include "REI_Sample/sample.h"

int  sample_init(REI_RendererDesc* rendererDesc, REI_SwapchainDesc* swapchainDesc);
void sample_fini();
void sample_resize(REI_SwapchainDesc* swapchainDesc);
void sample_render(uint64_t dt, uint32_t w, uint32_t h);

void sample_input_init(SDL_Window* window);
void sample_input_resize();
void sample_input_fini();
void sample_input_on_event(SDL_Event* event);
void sample_input_update();

enum
{
    IMAGE_COUNT = 2
};

SDL_Window*       window = 0;
REI_SwapchainDesc swapchainDesc;

static uint64_t baseCntr = 0;
static uint64_t cntrFreq = 0;

static const char* dir_paths[DIRECTORY_COUNT];

const char* get_filename(const char* path);

static inline uint64_t cntr_to_ns(uint64_t t) { return t * 1000000000ull / cntrFreq; }

static void  init_directories();
static FILE* register_log_output(const char* path);
static void  log_output(void* userPtr, const char* msg);

static bool sample_sdl_handleEvents()
{
    SDL_Event evt;
    while (SDL_PollEvent(&evt))
    {
        switch (evt.type)
        {
            case SDL_QUIT: return false;
            case SDL_WINDOWEVENT:
                switch (evt.window.event)
                {
                    int width, height;
                    SDL_GL_GetDrawableSize(window, &width, &height);
                    swapchainDesc.width = width;
                    swapchainDesc.height = height;
                    case SDL_WINDOWEVENT_RESIZED:
                    {
                        sample_resize(&swapchainDesc);
                        sample_input_resize();
                        break;
                    }
                }
                break;
            case SDL_KEYDOWN:
                switch (evt.key.keysym.sym)
                {
                    case SDLK_PRINTSCREEN: sample_request_screenshot(); break;
                }
        }

        sample_on_event(&evt);
        sample_input_on_event(&evt);
    }

    return true;
}

int main(int argc, char* argv[])
{
#if defined(_DEBUG) && defined(_WIN32)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF|_CRTDBG_LEAK_CHECK_DF|_CRTDBG_DELAY_FREE_MEM_DF);
#endif
    FILE* file = register_log_output(argv[0]);

    int res = SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS);
    if (res)
    {
        REI_LOG(DEBUG, "Failed to init SDL");
        fclose(file);
        return 0;
    }

    init_directories();

    uint32_t flags = 0;
    int32_t width = 0;
    int32_t height = 0;

#if ANDROID
    // https://github.com/naivisoftware/vulkansdldemo/blob/master/src/main.cpp#L809
    flags = SDL_WINDOW_VULKAN | SDL_WINDOW_FULLSCREEN;

    SDL_Rect display_rect = {};
    SDL_GetDisplayBounds(0, &display_rect);
    width = display_rect.w;
    height = display_rect.h;
#else
    flags = SDL_WINDOW_RESIZABLE;
    width = 1280;
    height = 720;
#endif

    bool vsync = true;
    const char* gpu_name = nullptr;
    for (int i = 1; i < argc; ++i)
    {
        if (sscanf(argv[i], "-w=%d", &width) == 1) continue;
        if (sscanf(argv[i], "-h=%d", &height) == 1) continue;
        if (strncmp(argv[i], "-gpu=", sizeof("-gpu=") - 1)==0) {gpu_name = argv[i] + sizeof("-gpu=") - 1; continue;}
        if (strcmp(argv[i], "-novsync")==0) {vsync = false; continue;}
    }

    window = SDL_CreateWindow("Vulkan Sample", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, width, height, flags);
    if (!window)
    {
        REI_LOG(DEBUG, "Failed to create window");
        fclose(file);
        return 0;
    }

    SDL_GL_GetDrawableSize(window, &width, &height);

    sample_input_init(window);

    SDL_SysWMinfo info;
    SDL_VERSION(&info.version);
    SDL_GetWindowWMInfo(window, &info);

    swapchainDesc.presentQueueCount = 1;
    swapchainDesc.ppPresentQueues = &gfxQueue;
    swapchainDesc.width = width;
    swapchainDesc.height = height;
    swapchainDesc.imageCount = IMAGE_COUNT;
    swapchainDesc.sampleCount = REI_SAMPLE_COUNT_1;
    swapchainDesc.colorFormat = REI_getRecommendedSwapchainFormat(false);
    swapchainDesc.enableVsync = vsync;

#if SDL_VIDEO_DRIVER_WINDOWS
    swapchainDesc.windowHandle = { info.info.win.window };
#elif SDL_VIDEO_DRIVER_ANDROID
    swapchainDesc.windowHandle = { info.info.android.window };
#elif SDL_VIDEO_DRIVER_COCOA
    swapchainDesc.windowHandle = { info.info.cocoa.window };
#else
#    error "Unsupported platform"
#endif

    REI_RendererDesc rendererDesc{ "triangle_test", gpu_name, nullptr, REI_SHADER_TARGET_5_1 };

    int run = sample_init(&rendererDesc, &swapchainDesc);
    baseCntr = SDL_GetPerformanceCounter();
    cntrFreq = SDL_GetPerformanceFrequency();
    uint64_t t1, t0 = baseCntr;
    while (run)
    {
        run = sample_sdl_handleEvents();
        sample_input_update();
        t1 = SDL_GetPerformanceCounter();
        int w, h;
        SDL_GetWindowSize(window, &w, &h);
        sample_render(cntr_to_ns(t1 - t0), w, h);
        t0 = t1;
    }

    sample_fini();
    sample_input_fini();

    SDL_DestroyWindow(window);

    SDL_Quit();

    if (file)
    {
        fflush(file);
        fclose(file);
    }
    return 0;
}

uint64_t sample_time_ns() { return cntr_to_ns(SDL_GetPerformanceCounter() - baseCntr); }

void sample_quit() { SDL_Quit(); }

const char* sample_get_path(DirectoryEnum dir, const char* path, char* out, size_t size)
{
    strcpy(out, dir_paths[dir]);
    strcat(out, "/");
    strcat(out, path);
    return out;
}

static bool dir_exists(const char* dir)
{
#if defined(_WIN32)
    struct _stat fi;
    return dir && (_stat(dir, &fi) == 0) && ((fi.st_mode & _S_IFDIR) != 0);
#else
    struct stat fi;
    return dir && (stat(dir, &fi) == 0) && (S_ISDIR(fi.st_mode));
#endif
}

static void init_directories()
{
#ifdef ANDROID
    dir_paths[DIRECTORY_DATA] = SDL_AndroidGetInternalStoragePath();
#else
    if (dir_exists("data"))
    {
        dir_paths[DIRECTORY_DATA] = "data";
    }
    else if (dir_exists("../data"))
    {
        dir_paths[DIRECTORY_DATA] = "../data";
    }
    else if (dir_exists("../../data"))
    {
        dir_paths[DIRECTORY_DATA] = "../../data";
    }
#endif
}

static FILE* register_log_output(const char* path)
{
    char logFileName[1024];
    strcpy(logFileName, get_filename(path));
    //Minimum Length check
    if (!logFileName || !logFileName[0])
    {
        strcpy(logFileName, "Log");
    }
    strcat(logFileName, ".log");
    FILE* file = fopen(logFileName, "w");
    REI_LogAddOutput(file, log_output);
    if (file)
    {
        fprintf(file, "date       time     [thread name/id ]                   file:line    v | message\n");
        fflush(file);

        REI_LogWrite(LL_INFO, __FILE__, __LINE__, "Opened log file %s", logFileName);
    }
    else
    {
        REI_LogWrite(LL_ERROR, __FILE__, __LINE__, "Failed to create log file %s", logFileName);
    }

    return file;
}

static void log_output(void* userPtr, const char* msg)
{
#ifdef _DEBUG
    REI_DebugOutput(msg);
#endif
    REI_Print(msg);
    if (FILE* file = (FILE*)userPtr)
    {
        fprintf(file, "%s", msg);
        fflush(file);
    }
}
