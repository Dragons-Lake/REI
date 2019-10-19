
#include <android/configuration.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android_native_app_glue.h>

#include <sys/stat.h>

#include <REI_Sample/sample.h>

int  sample_init(REI_RendererDesc* rendererDesc, REI_SwapchainDesc* swapchainDesc);
void sample_fini();
void sample_resize(REI_SwapchainDesc* swapchainDesc);
void sample_render(uint64_t dt, uint32_t w, uint32_t h);

enum
{
    IMAGE_COUNT = 2
};

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

static FILE* register_log_output(const char* path)
{
    char logFileName[1024];
    strcpy(logFileName, path);
    strcat(logFileName, "/REI.log");
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

struct saved_state
{
};

struct Sample
{
    struct android_app* app;
    struct saved_state  state;

    REI_SwapchainDesc swapchainDesc;
    int32_t           width;
    int32_t           height;
    bool              active;
};

static int32_t engine_handle_input(struct android_app* app, AInputEvent* event)
{
    //Sample* sample = (Sample*)app->userData;
    return 0;
}

static void engine_handle_cmd(struct android_app* app, int32_t cmd)
{
    Sample* sample = (Sample*)app->userData;
    switch (cmd)
    {
        case APP_CMD_SAVE_STATE:
            // The system has asked us to save our current state.  Do so.
            sample->app->savedState = REI_malloc(sizeof(struct saved_state));
            *((struct saved_state*)sample->app->savedState) = sample->state;
            sample->app->savedStateSize = sizeof(struct saved_state);
            break;
        case APP_CMD_INIT_WINDOW:
            if (sample->app->window != NULL)
            {
                sample->width = ANativeWindow_getWidth(sample->app->window);
                sample->height = ANativeWindow_getHeight(sample->app->window);
                sample->swapchainDesc.presentQueueCount = 1;
                sample->swapchainDesc.ppPresentQueues = &gfxQueue;
                sample->swapchainDesc.width = sample->width;
                sample->swapchainDesc.height = sample->height;
                sample->swapchainDesc.imageCount = IMAGE_COUNT;
                sample->swapchainDesc.sampleCount = REI_SAMPLE_COUNT_1;
                sample->swapchainDesc.colorFormat = REI_getRecommendedSwapchainFormat(false);
                sample->swapchainDesc.enableVsync = true;
                sample->swapchainDesc.windowHandle = { sample->app->window };
                sample->active = true;
                REI_RendererDesc rendererDesc{};
                sample_init(&rendererDesc, &sample->swapchainDesc);
                sample_render(0, sample->width, sample->height);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            sample->active = false;
            sample_fini();
            break;
        case APP_CMD_GAINED_FOCUS: break;
        case APP_CMD_LOST_FOCUS:
            sample->active = false;
            sample_render(0, sample->width, sample->height);
            break;
    }
}

static const char* dir_paths[DIRECTORY_COUNT];

void copy_assets(AAssetManager* mgr, const char* dir)
{
    AAssetDir*  assetDir = AAssetManager_openDir(mgr, dir);
    const char* filename = (const char*)NULL;
    while ((filename = AAssetDir_getNextFileName(assetDir)) != NULL)
    {
        char assetPath[256];
        snprintf(assetPath, sizeof(assetPath), "%s/%s", dir, filename);
        AAsset* asset = AAssetManager_open(mgr, assetPath, AASSET_MODE_STREAMING);
        char    buf[BUFSIZ];
        int     nb_read = 0;
        char    outputPath[256];
        FILE*   out = fopen(sample_get_path(DIRECTORY_DATA, assetPath, outputPath, sizeof(outputPath)), "w");
        while ((nb_read = AAsset_read(asset, buf, BUFSIZ)) > 0)
            fwrite(buf, nb_read, 1, out);
        fclose(out);
        AAsset_close(asset);
    }
    AAssetDir_close(assetDir);
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

void prepare_assets(AAssetManager* mgr)
{
    char dir[256];

    if (dir_exists(sample_get_path(DIRECTORY_DATA, "", dir, sizeof(dir))))
    {
        return;
    }

    mkdir(dir, 0755);
    mkdir(sample_get_path(DIRECTORY_DATA, "fonts", dir, sizeof(dir)), 0755);
    copy_assets(mgr, "fonts");
}

/**
* This is the main entry point of a native application that is using
* android_native_app_glue.  It runs in its own thread, with its own
* event loop for receiving input events and doing other things.
*/
void android_main(struct android_app* state)
{
    FILE* file = register_log_output(state->activity->externalDataPath);

    Sample sample{};

    dir_paths[DIRECTORY_DATA] = state->activity->internalDataPath;

    prepare_assets(state->activity->assetManager);

    state->userData = &sample;
    state->onAppCmd = engine_handle_cmd;
    state->onInputEvent = engine_handle_input;
    sample.app = state;

    if (state->savedState != NULL)
    {
        // We are starting with a previous saved state; restore from it.
        sample.state = *(struct saved_state*)state->savedState;
    }

    uint64_t t1, t0 = sample_time_ns();
    while (1)
    {
        int                         ident;
        int                         events;
        struct android_poll_source* source;

        // If not animating, we will block forever waiting for events.
        // If animating, we loop until all events are read, then continue
        // to draw the next frame of animation.
        while ((ident = ALooper_pollAll(sample.active ? 0 : -1, NULL, &events, (void**)&source)) >= 0)
        {
            // Process this event.
            if (source != NULL)
            {
                source->process(state, source);
            }

            if (state->destroyRequested != 0)
            {
                if (file)
                {
                    fflush(file);
                    fclose(file);
                }
                return;
            }
        }

        if (sample.active)
        {
            t1 = sample_time_ns();
            sample_render(t1 - t0, sample.width, sample.height);
            t0 = t1;
        }
    }
}

uint64_t sample_time_ns()
{
    timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    return ts.tv_sec * 1000000000ull + ts.tv_nsec;
}

void sample_quit() { /*TODO*/ }

const char* sample_get_path(DirectoryEnum dir, const char* path, char* out, size_t size)
{
    snprintf(out, size, "%s/data/%s", dir_paths[dir], path);
    return out;
}
