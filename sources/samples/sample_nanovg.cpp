#ifndef SAMPLE_TEST

#    include <string.h>
#    include <math.h>

#    include "REI_Sample/sample.h"

#    include "REI_Integration/REI_nanovg.h"

#endif

struct DemoData
{
    int fontNormal, fontBold, fontIcons, fontEmoji;
    int images[12];
};

enum GraphrenderStyle
{
    GRAPH_RENDER_FPS,
    GRAPH_RENDER_MS,
    GRAPH_RENDER_PERCENT,
};

#define GRAPH_HISTORY_COUNT 100
struct PerfGraph
{
    int   style;
    char  name[32];
    float values[GRAPH_HISTORY_COUNT];
    int   head;
};

static REI_RL_State* resourceLoader;
static REI_CmdPool*  cmdPool;
static REI_Cmd*      pCmds[FRAME_COUNT];
static REI_Texture*  colorBuffer;
static REI_Texture*  depthBuffer;

static REI_Buffer*    readbackBuffer;
static REI_QueryPool* timeQueryPool;

static DemoData    data;
static NVGcontext* vg = NULL;
static PerfGraph   fps, cpuGraph, gpuGraph;
static double      gpuTimestampScaler;
static void*       timestampBufferMem;

static int mx = 0, my = 0;
static int blowup = 0;
static int premult = 0;

int  loadDemoData(NVGcontext* vg, DemoData* data);
void freeDemoData(NVGcontext* vg, DemoData* data);
void renderDemo(NVGcontext* vg, float mx, float my, float width, float height, float t, int blowup, DemoData* data);

void  initGraph(PerfGraph* fps, int style, const char* name);
void  updateGraph(PerfGraph* fps, float frameTime);
void  renderGraph(NVGcontext* vg, float x, float y, PerfGraph* fps);
float getGraphAverage(PerfGraph* fps);

int sample_on_init()
{
    REI_RL_addResourceLoader(renderer, nullptr, &resourceLoader);

    REI_addCmdPool(renderer, gfxQueue, false, &cmdPool);
    for (size_t i = 0; i < FRAME_COUNT; ++i)
        REI_addCmd(renderer, cmdPool, false, &pCmds[i]);

    REI_QueryPoolDesc queryPoolDesc{ REI_QUERY_TYPE_TIMESTAMP, 2 * FRAME_COUNT };
    REI_addQueryPool(renderer, &queryPoolDesc, &timeQueryPool);

    REI_BufferDesc bufDesc = {};
    bufDesc.memoryUsage = REI_RESOURCE_MEMORY_USAGE_GPU_TO_CPU;
    bufDesc.flags = REI_BUFFER_CREATION_FLAG_OWN_MEMORY_BIT | REI_BUFFER_CREATION_FLAG_PERSISTENT_MAP_BIT;
    bufDesc.size = queryPoolDesc.queryCount * sizeof(uint64_t);
    bufDesc.startState = REI_RESOURCE_STATE_COPY_DEST;
    REI_addBuffer(renderer, &bufDesc, &readbackBuffer);
    REI_mapBuffer(renderer, readbackBuffer, &timestampBufferMem);

    REI_QueueProperties gfxQueueProps;
    REI_getQueueProperties(gfxQueue, &gfxQueueProps);

    gpuTimestampScaler = 1.0 / gfxQueueProps.gpuTimestampFreq;

    initGraph(&fps, GRAPH_RENDER_FPS, "Frame Time");
    initGraph(&cpuGraph, GRAPH_RENDER_MS, "CPU Time");
    initGraph(&gpuGraph, GRAPH_RENDER_MS, "GPU Time");

    return 1;
}

void sample_on_fini()
{
    REI_RL_waitBatchCompleted(resourceLoader);

    REI_removeBuffer(renderer, readbackBuffer);
    REI_removeQueryPool(renderer, timeQueryPool);

    for (size_t i = 0; i < FRAME_COUNT; ++i)
        REI_removeCmd(renderer, cmdPool, pCmds[i]);
    REI_removeCmdPool(renderer, cmdPool);

    REI_RL_removeResourceLoader(resourceLoader);
}

void sample_on_swapchain_init(const REI_SwapchainDesc* swapchainDesc)
{
    REI_TextureDesc colorRTDesc{};
    colorRTDesc.format = swapchainDesc->colorFormat;
    colorRTDesc.width = swapchainDesc->width;
    colorRTDesc.height = swapchainDesc->height;
    colorRTDesc.sampleCount = REI_SAMPLE_COUNT_4;
    colorRTDesc.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET;
    REI_addTexture(renderer, &colorRTDesc, &colorBuffer);

    // Add depth buffer
    REI_TextureDesc depthRTDesc{};
    depthRTDesc.clearValue.ds.depth = 1.0f;
    depthRTDesc.clearValue.ds.stencil = 0;
    depthRTDesc.format = REI_FMT_D32_SFLOAT_S8_UINT;
    depthRTDesc.width = colorRTDesc.width;
    depthRTDesc.height = colorRTDesc.height;
    depthRTDesc.sampleCount = colorRTDesc.sampleCount;
    depthRTDesc.descriptors = REI_DESCRIPTOR_TYPE_RENDER_TARGET;
    REI_addTexture(renderer, &depthRTDesc, &depthBuffer);

    REI_NanoVG_Desc fsInfo = {
        colorRTDesc.format, depthRTDesc.format, colorRTDesc.sampleCount, FRAME_COUNT, 1024 * 256, 4096, 256
    };

    vg = REI_NanoVG_Init(renderer, gfxQueue, resourceLoader, &fsInfo);
    loadDemoData(vg, &data);
}

void sample_on_swapchain_fini()
{
    freeDemoData(vg, &data);
    REI_NanoVG_Shutdown(vg);
    REI_removeTexture(renderer, colorBuffer);
    REI_removeTexture(renderer, depthBuffer);
}

void sample_on_event(SDL_Event* evt)
{
    if (evt->type == SDL_KEYDOWN)
    {
        switch (evt->key.keysym.sym)
        {
            case SDLK_ESCAPE: sample_quit(); break;
            case SDLK_SPACE: blowup = !blowup; break;
            case SDLK_p: premult = !premult; break;
            default: break;
        }
    }
    else if (evt->type == SDL_MOUSEMOTION)
    {
        mx = evt->motion.x;
        my = evt->motion.y;
    }
}

void sample_on_frame(const FrameData* frameData)
{
    uint64_t t0 = sample_time_ns();
    uint32_t timestampIndex = frameData->setIndex * 2;

    REI_Texture* renderTarget = frameData->backBuffer;
    REI_Cmd*     cmd = pCmds[frameData->setIndex];

    REI_beginCmd(cmd);

    REI_cmdResetQueryPool(cmd, timeQueryPool, timestampIndex, 2);

    REI_cmdEndQuery(cmd, timeQueryPool, timestampIndex);

    REI_TextureBarrier barriers[] = {
        { colorBuffer, REI_RESOURCE_STATE_UNDEFINED, REI_RESOURCE_STATE_RENDER_TARGET },
        { depthBuffer, REI_RESOURCE_STATE_UNDEFINED, REI_RESOURCE_STATE_DEPTH_WRITE },
    };
    REI_cmdResourceBarrier(cmd, 0, nullptr, 2, barriers);

    REI_LoadActionsDesc loadActions = { 0 };
    loadActions.loadActionsColor[0] = REI_LOAD_ACTION_CLEAR;
    if (!premult)
    {
        loadActions.clearColorValues[0].rt.r = 0.3f;
        loadActions.clearColorValues[0].rt.g = 0.3f;
        loadActions.clearColorValues[0].rt.b = 0.32f;
        loadActions.clearColorValues[0].rt.a = 1.0f;
    }
    else
    {
        loadActions.clearColorValues[0].rt.r = 0.0f;
        loadActions.clearColorValues[0].rt.g = 0.0f;
        loadActions.clearColorValues[0].rt.b = 0.0f;
        loadActions.clearColorValues[0].rt.a = 0.0f;
    }
    loadActions.loadActionDepth = REI_LOAD_ACTION_CLEAR;
    loadActions.loadActionStencil = REI_LOAD_ACTION_CLEAR;
    loadActions.clearDepth.ds.depth = 1.0f;
    loadActions.clearDepth.ds.stencil = 0;
    REI_cmdBindRenderTargets(cmd, 1, &colorBuffer, depthBuffer, &loadActions, NULL, NULL, 0, 0);
    REI_cmdSetViewport(cmd, 0.0f, 0.0f, (float)frameData->bbWidth, (float)frameData->bbHeight, 0.0f, 1.0f);
    REI_cmdSetScissor(cmd, 0, 0, frameData->bbWidth, frameData->bbHeight);

    REI_NanoVG_SetupRender(vg, cmd, frameData->setIndex);

    nvgBeginFrame(vg, frameData->winWidth, frameData->winHeight, frameData->bbWidth / (float)frameData->winWidth);

    renderDemo(
        vg, (float)mx, (float)my, (float)frameData->winWidth, (float)frameData->winHeight, t0 * 1e-9f, blowup, &data);

    renderGraph(vg, 5, 5, &fps);
    renderGraph(vg, 5 + 200 + 5, 5, &cpuGraph);
    renderGraph(vg, 5 + 200 + 5 + 200 + 5, 5, &gpuGraph);

    nvgEndFrame(vg);

    REI_cmdBindRenderTargets(cmd, 0, NULL, NULL, NULL, NULL, NULL, 0, 0);

    barriers[0].pTexture = colorBuffer;
    barriers[0].startState = REI_RESOURCE_STATE_RENDER_TARGET;
    barriers[0].endState = REI_RESOURCE_STATE_RESOLVE_SOURCE;
    barriers[1].pTexture = renderTarget;
    barriers[1].startState = REI_RESOURCE_STATE_UNDEFINED;
    barriers[1].endState = REI_RESOURCE_STATE_RESOLVE_DEST;
    REI_cmdResourceBarrier(cmd, 0, 0, 2, barriers);

    REI_ResolveDesc resolveDesc{};
    REI_cmdResolveTexture(cmd, renderTarget, colorBuffer, &resolveDesc);

    sample_cmdPrepareBackbuffer(cmd, renderTarget, REI_RESOURCE_STATE_RESOLVE_DEST);

    REI_cmdEndQuery(cmd, timeQueryPool, timestampIndex + 1);

    REI_cmdResolveQuery(cmd, readbackBuffer, timestampIndex * sizeof(uint64_t), timeQueryPool, timestampIndex, 2);

    REI_endCmd(cmd);

    sample_submit(cmd);

    uint64_t* timestampData = (uint64_t*)timestampBufferMem;

    updateGraph(&fps, frameData->dt * 1e-9f);
    updateGraph(&cpuGraph, (sample_time_ns() - t0) * 1e-9f);
    updateGraph(
        &gpuGraph, (float)((timestampData[timestampIndex + 1] - timestampData[timestampIndex]) * gpuTimestampScaler));
}

#ifdef _MSC_VER
#    define snprintf _snprintf
#endif

#define ICON_SEARCH 0x1F50D
#define ICON_CIRCLED_CROSS 0x2716
#define ICON_CHEVRON_RIGHT 0xE75E
#define ICON_CHECK 0x2713
#define ICON_LOGIN 0xE740
#define ICON_TRASH 0xE729

//static float minf(float a, float b) { return a < b ? a : b; }
static float maxf(float a, float b) { return a > b ? a : b; }
//static float absf(float a) { return a >= 0.0f ? a : -a; }
static float clampf(float a, float mn, float mx) { return a < mn ? mn : (a > mx ? mx : a); }

// Returns 1 if col.rgba is 0.0f,0.0f,0.0f,0.0f, 0 otherwise
int isBlack(NVGcolor col)
{
    if (col.r == 0.0f && col.g == 0.0f && col.b == 0.0f && col.a == 0.0f)
    {
        return 1;
    }
    return 0;
}

static char* cpToUTF8(int cp, char* str)
{
    int n = 0;
    if (cp < 0x80)
        n = 1;
    else if (cp < 0x800)
        n = 2;
    else if (cp < 0x10000)
        n = 3;
    else if (cp < 0x200000)
        n = 4;
    else if (cp < 0x4000000)
        n = 5;
    else if (cp <= 0x7fffffff)
        n = 6;
    str[n] = '\0';
    switch (n)
    {
        case 6:
            str[5] = 0x80 | (cp & 0x3f);
            cp = cp >> 6;
            cp |= 0x4000000;
        case 5:
            str[4] = 0x80 | (cp & 0x3f);
            cp = cp >> 6;
            cp |= 0x200000;
        case 4:
            str[3] = 0x80 | (cp & 0x3f);
            cp = cp >> 6;
            cp |= 0x10000;
        case 3:
            str[2] = 0x80 | (cp & 0x3f);
            cp = cp >> 6;
            cp |= 0x800;
        case 2:
            str[1] = 0x80 | (cp & 0x3f);
            cp = cp >> 6;
            cp |= 0xc0;
        case 1: str[0] = cp;
    }
    return str;
}

void drawWindow(NVGcontext* vg, const char* title, float x, float y, float w, float h)
{
    float    cornerRadius = 3.0f;
    NVGpaint shadowPaint;
    NVGpaint headerPaint;

    nvgSave(vg);
    //	nvgClearState(vg);

    // Window
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, cornerRadius);
    nvgFillColor(vg, nvgRGBA(28, 30, 34, 192));
    //	nvgFillColor(vg, nvgRGBA(0,0,0,128));
    nvgFill(vg);

    // Drop shadow
    shadowPaint = nvgBoxGradient(vg, x, y + 2, w, h, cornerRadius * 2, 10, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, x - 10, y - 10, w + 20, h + 30);
    nvgRoundedRect(vg, x, y, w, h, cornerRadius);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, shadowPaint);
    nvgFill(vg);

    // Header
    headerPaint = nvgLinearGradient(vg, x, y, x, y + 15, nvgRGBA(255, 255, 255, 8), nvgRGBA(0, 0, 0, 16));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 1, y + 1, w - 2, 30, cornerRadius - 1);
    nvgFillPaint(vg, headerPaint);
    nvgFill(vg);
    nvgBeginPath(vg);
    nvgMoveTo(vg, x + 0.5f, y + 0.5f + 30);
    nvgLineTo(vg, x + 0.5f + w - 1, y + 0.5f + 30);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 32));
    nvgStroke(vg);

    nvgFontSize(vg, 18.0f);
    nvgFontFace(vg, "sans-bold");
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);

    nvgFontBlur(vg, 2);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 128));
    nvgText(vg, x + w / 2, y + 16 + 1, title, NULL);

    nvgFontBlur(vg, 0);
    nvgFillColor(vg, nvgRGBA(220, 220, 220, 160));
    nvgText(vg, x + w / 2, y + 16, title, NULL);

    nvgRestore(vg);
}

void drawSearchBox(NVGcontext* vg, const char* text, float x, float y, float w, float h)
{
    NVGpaint bg;
    char     icon[8];
    float    cornerRadius = h / 2 - 1;

    // Edit
    bg = nvgBoxGradient(vg, x, y + 1.5f, w, h, h / 2, 5, nvgRGBA(0, 0, 0, 16), nvgRGBA(0, 0, 0, 92));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, cornerRadius);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    /*	nvgBeginPath(vg);
	nvgRoundedRect(vg, x+0.5f,y+0.5f, w-1,h-1, cornerRadius-0.5f);
	nvgStrokeColor(vg, nvgRGBA(0,0,0,48));
	nvgStroke(vg);*/

    nvgFontSize(vg, h * 1.3f);
    nvgFontFace(vg, "icons");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 64));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + h * 0.55f, y + h * 0.55f, cpToUTF8(ICON_SEARCH, icon), NULL);

    nvgFontSize(vg, 20.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 32));

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + h * 1.05f, y + h * 0.5f, text, NULL);

    nvgFontSize(vg, h * 1.3f);
    nvgFontFace(vg, "icons");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 32));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w - h * 0.55f, y + h * 0.55f, cpToUTF8(ICON_CIRCLED_CROSS, icon), NULL);
}

void drawDropDown(NVGcontext* vg, const char* text, float x, float y, float w, float h)
{
    NVGpaint bg;
    char     icon[8];
    float    cornerRadius = 4.0f;

    bg = nvgLinearGradient(vg, x, y, x, y + h, nvgRGBA(255, 255, 255, 16), nvgRGBA(0, 0, 0, 16));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 1, y + 1, w - 2, h - 2, cornerRadius - 1);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1, h - 1, cornerRadius - 0.5f);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 48));
    nvgStroke(vg);

    nvgFontSize(vg, 20.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 160));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + h * 0.3f, y + h * 0.5f, text, NULL);

    nvgFontSize(vg, h * 1.3f);
    nvgFontFace(vg, "icons");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 64));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w - h * 0.5f, y + h * 0.5f, cpToUTF8(ICON_CHEVRON_RIGHT, icon), NULL);
}

void drawLabel(NVGcontext* vg, const char* text, float x, float y, float w, float h)
{
    NVG_NOTUSED(w);

    nvgFontSize(vg, 18.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 128));

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x, y + h * 0.5f, text, NULL);
}

void drawEditBoxBase(NVGcontext* vg, float x, float y, float w, float h)
{
    NVGpaint bg;
    // Edit
    bg = nvgBoxGradient(
        vg, x + 1, y + 1 + 1.5f, w - 2, h - 2, 3, 4, nvgRGBA(255, 255, 255, 32), nvgRGBA(32, 32, 32, 32));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 1, y + 1, w - 2, h - 2, 4 - 1);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1, h - 1, 4 - 0.5f);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 48));
    nvgStroke(vg);
}

void drawEditBox(NVGcontext* vg, const char* text, float x, float y, float w, float h)
{
    drawEditBoxBase(vg, x, y, w, h);

    nvgFontSize(vg, 20.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 64));
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + h * 0.3f, y + h * 0.5f, text, NULL);
}

void drawEditBoxNum(NVGcontext* vg, const char* text, const char* units, float x, float y, float w, float h)
{
    float uw;

    drawEditBoxBase(vg, x, y, w, h);

    uw = nvgTextBounds(vg, 0, 0, units, NULL, NULL);

    nvgFontSize(vg, 18.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 64));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w - h * 0.3f, y + h * 0.5f, units, NULL);

    nvgFontSize(vg, 20.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 128));
    nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + w - uw - h * 0.5f, y + h * 0.5f, text, NULL);
}

void drawCheckBox(NVGcontext* vg, const char* text, float x, float y, float w, float h)
{
    NVGpaint bg;
    char     icon[8];
    NVG_NOTUSED(w);

    nvgFontSize(vg, 18.0f);
    nvgFontFace(vg, "sans");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 160));

    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + 28, y + h * 0.5f, text, NULL);

    bg = nvgBoxGradient(
        vg, x + 1, y + (int)(h * 0.5f) - 9 + 1, 18, 18, 3, 3, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 92));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 1, y + (int)(h * 0.5f) - 9, 18, 18, 3);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgFontSize(vg, 40);
    nvgFontFace(vg, "icons");
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 128));
    nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
    nvgText(vg, x + 9 + 2, y + h * 0.5f, cpToUTF8(ICON_CHECK, icon), NULL);
}

void drawButton(NVGcontext* vg, int preicon, const char* text, float x, float y, float w, float h, NVGcolor col)
{
    NVGpaint bg;
    char     icon[8];
    float    cornerRadius = 4.0f;
    float    tw = 0, iw = 0;

    bg = nvgLinearGradient(
        vg, x, y, x, y + h, nvgRGBA(255, 255, 255, isBlack(col) ? 16 : 32), nvgRGBA(0, 0, 0, isBlack(col) ? 16 : 32));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 1, y + 1, w - 2, h - 2, cornerRadius - 1);
    if (!isBlack(col))
    {
        nvgFillColor(vg, col);
        nvgFill(vg);
    }
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + 0.5f, y + 0.5f, w - 1, h - 1, cornerRadius - 0.5f);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 48));
    nvgStroke(vg);

    nvgFontSize(vg, 20.0f);
    nvgFontFace(vg, "sans-bold");
    tw = nvgTextBounds(vg, 0, 0, text, NULL, NULL);
    if (preicon != 0)
    {
        nvgFontSize(vg, h * 1.3f);
        nvgFontFace(vg, "icons");
        iw = nvgTextBounds(vg, 0, 0, cpToUTF8(preicon, icon), NULL, NULL);
        iw += h * 0.15f;
    }

    if (preicon != 0)
    {
        nvgFontSize(vg, h * 1.3f);
        nvgFontFace(vg, "icons");
        nvgFillColor(vg, nvgRGBA(255, 255, 255, 96));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, x + w * 0.5f - tw * 0.5f - iw * 0.75f, y + h * 0.5f, cpToUTF8(preicon, icon), NULL);
    }

    nvgFontSize(vg, 20.0f);
    nvgFontFace(vg, "sans-bold");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 160));
    nvgText(vg, x + w * 0.5f - tw * 0.5f + iw * 0.25f, y + h * 0.5f - 1, text, NULL);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 160));
    nvgText(vg, x + w * 0.5f - tw * 0.5f + iw * 0.25f, y + h * 0.5f, text, NULL);
}

void drawSlider(NVGcontext* vg, float pos, float x, float y, float w, float h)
{
    NVGpaint bg, knob;
    float    cy = y + roundf(h * 0.5f);
    float    kr = roundf(h * 0.25f);

    nvgSave(vg);
    //	nvgClearState(vg);

    // Slot
    bg = nvgBoxGradient(vg, x, cy - 2 + 1, w, 4, 2, 2, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 128));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, cy - 2, w, 4, 2);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    // Knob Shadow
    bg = nvgRadialGradient(vg, x + (int)(pos * w), cy + 1, kr - 3, kr + 3, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, x + (int)(pos * w) - kr - 5, cy - kr - 5, kr * 2 + 5 + 5, kr * 2 + 5 + 5 + 3);
    nvgCircle(vg, x + (int)(pos * w), cy, kr);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    // Knob
    knob = nvgLinearGradient(vg, x, cy - kr, x, cy + kr, nvgRGBA(255, 255, 255, 16), nvgRGBA(0, 0, 0, 16));
    nvgBeginPath(vg);
    nvgCircle(vg, x + (int)(pos * w), cy, kr - 1);
    nvgFillColor(vg, nvgRGBA(40, 43, 48, 255));
    nvgFill(vg);
    nvgFillPaint(vg, knob);
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgCircle(vg, x + (int)(pos * w), cy, kr - 0.5f);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 92));
    nvgStroke(vg);

    nvgRestore(vg);
}

void drawEyes(NVGcontext* vg, float x, float y, float w, float h, float mx, float my, float t)
{
    NVGpaint gloss, bg;
    float    ex = w * 0.23f;
    float    ey = h * 0.5f;
    float    lx = x + ex;
    float    ly = y + ey;
    float    rx = x + w - ex;
    float    ry = y + ey;
    float    dx, dy, d;
    float    br = (ex < ey ? ex : ey) * 0.5f;
    float    blink = 1.0f - powf(sinf(t * 0.5f), 200.0f) * 0.8f;

    bg = nvgLinearGradient(vg, x, y + h * 0.5f, x + w * 0.1f, y + h, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 16));
    nvgBeginPath(vg);
    nvgEllipse(vg, lx + 3.0f, ly + 16.0f, ex, ey);
    nvgEllipse(vg, rx + 3.0f, ry + 16.0f, ex, ey);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    bg = nvgLinearGradient(
        vg, x, y + h * 0.25f, x + w * 0.1f, y + h, nvgRGBA(220, 220, 220, 255), nvgRGBA(128, 128, 128, 255));
    nvgBeginPath(vg);
    nvgEllipse(vg, lx, ly, ex, ey);
    nvgEllipse(vg, rx, ry, ex, ey);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    dx = (mx - rx) / (ex * 10);
    dy = (my - ry) / (ey * 10);
    d = sqrtf(dx * dx + dy * dy);
    if (d > 1.0f)
    {
        dx /= d;
        dy /= d;
    }
    dx *= ex * 0.4f;
    dy *= ey * 0.5f;
    nvgBeginPath(vg);
    nvgEllipse(vg, lx + dx, ly + dy + ey * 0.25f * (1 - blink), br, br * blink);
    nvgFillColor(vg, nvgRGBA(32, 32, 32, 255));
    nvgFill(vg);

    dx = (mx - rx) / (ex * 10);
    dy = (my - ry) / (ey * 10);
    d = sqrtf(dx * dx + dy * dy);
    if (d > 1.0f)
    {
        dx /= d;
        dy /= d;
    }
    dx *= ex * 0.4f;
    dy *= ey * 0.5f;
    nvgBeginPath(vg);
    nvgEllipse(vg, rx + dx, ry + dy + ey * 0.25f * (1 - blink), br, br * blink);
    nvgFillColor(vg, nvgRGBA(32, 32, 32, 255));
    nvgFill(vg);

    gloss = nvgRadialGradient(
        vg, lx - ex * 0.25f, ly - ey * 0.5f, ex * 0.1f, ex * 0.75f, nvgRGBA(255, 255, 255, 128),
        nvgRGBA(255, 255, 255, 0));
    nvgBeginPath(vg);
    nvgEllipse(vg, lx, ly, ex, ey);
    nvgFillPaint(vg, gloss);
    nvgFill(vg);

    gloss = nvgRadialGradient(
        vg, rx - ex * 0.25f, ry - ey * 0.5f, ex * 0.1f, ex * 0.75f, nvgRGBA(255, 255, 255, 128),
        nvgRGBA(255, 255, 255, 0));
    nvgBeginPath(vg);
    nvgEllipse(vg, rx, ry, ex, ey);
    nvgFillPaint(vg, gloss);
    nvgFill(vg);
}

void drawGraph(NVGcontext* vg, float x, float y, float w, float h, float t)
{
    NVGpaint bg;
    float    samples[6];
    float    sx[6], sy[6];
    float    dx = w / 5.0f;
    int      i;

    samples[0] = (1 + sinf(t * 1.2345f + cosf(t * 0.33457f) * 0.44f)) * 0.5f;
    samples[1] = (1 + sinf(t * 0.68363f + cosf(t * 1.3f) * 1.55f)) * 0.5f;
    samples[2] = (1 + sinf(t * 1.1642f + cosf(t * 0.33457f) * 1.24f)) * 0.5f;
    samples[3] = (1 + sinf(t * 0.56345f + cosf(t * 1.63f) * 0.14f)) * 0.5f;
    samples[4] = (1 + sinf(t * 1.6245f + cosf(t * 0.254f) * 0.3f)) * 0.5f;
    samples[5] = (1 + sinf(t * 0.345f + cosf(t * 0.03f) * 0.6f)) * 0.5f;

    for (i = 0; i < 6; i++)
    {
        sx[i] = x + i * dx;
        sy[i] = y + h * samples[i] * 0.8f;
    }

    // Graph background
    bg = nvgLinearGradient(vg, x, y, x, y + h, nvgRGBA(0, 160, 192, 0), nvgRGBA(0, 160, 192, 64));
    nvgBeginPath(vg);
    nvgMoveTo(vg, sx[0], sy[0]);
    for (i = 1; i < 6; i++)
        nvgBezierTo(vg, sx[i - 1] + dx * 0.5f, sy[i - 1], sx[i] - dx * 0.5f, sy[i], sx[i], sy[i]);
    nvgLineTo(vg, x + w, y + h);
    nvgLineTo(vg, x, y + h);
    nvgFillPaint(vg, bg);
    nvgFill(vg);

    // Graph line
    nvgBeginPath(vg);
    nvgMoveTo(vg, sx[0], sy[0] + 2);
    for (i = 1; i < 6; i++)
        nvgBezierTo(vg, sx[i - 1] + dx * 0.5f, sy[i - 1] + 2, sx[i] - dx * 0.5f, sy[i] + 2, sx[i], sy[i] + 2);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 32));
    nvgStrokeWidth(vg, 3.0f);
    nvgStroke(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, sx[0], sy[0]);
    for (i = 1; i < 6; i++)
        nvgBezierTo(vg, sx[i - 1] + dx * 0.5f, sy[i - 1], sx[i] - dx * 0.5f, sy[i], sx[i], sy[i]);
    nvgStrokeColor(vg, nvgRGBA(0, 160, 192, 255));
    nvgStrokeWidth(vg, 3.0f);
    nvgStroke(vg);

    // Graph sample pos
    for (i = 0; i < 6; i++)
    {
        bg = nvgRadialGradient(vg, sx[i], sy[i] + 2, 3.0f, 8.0f, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(vg);
        nvgRect(vg, sx[i] - 10, sy[i] - 10 + 2, 20, 20);
        nvgFillPaint(vg, bg);
        nvgFill(vg);
    }

    nvgBeginPath(vg);
    for (i = 0; i < 6; i++)
        nvgCircle(vg, sx[i], sy[i], 4.0f);
    nvgFillColor(vg, nvgRGBA(0, 160, 192, 255));
    nvgFill(vg);
    nvgBeginPath(vg);
    for (i = 0; i < 6; i++)
        nvgCircle(vg, sx[i], sy[i], 2.0f);
    nvgFillColor(vg, nvgRGBA(220, 220, 220, 255));
    nvgFill(vg);

    nvgStrokeWidth(vg, 1.0f);
}

void drawSpinner(NVGcontext* vg, float cx, float cy, float r, float t)
{
    float    a0 = 0.0f + t * 6;
    float    a1 = NVG_PI + t * 6;
    float    r0 = r;
    float    r1 = r * 0.75f;
    float    ax, ay, bx, by;
    NVGpaint paint;

    nvgSave(vg);

    nvgBeginPath(vg);
    nvgArc(vg, cx, cy, r0, a0, a1, NVG_CW);
    nvgArc(vg, cx, cy, r1, a1, a0, NVG_CCW);
    nvgClosePath(vg);
    ax = cx + cosf(a0) * (r0 + r1) * 0.5f;
    ay = cy + sinf(a0) * (r0 + r1) * 0.5f;
    bx = cx + cosf(a1) * (r0 + r1) * 0.5f;
    by = cy + sinf(a1) * (r0 + r1) * 0.5f;
    paint = nvgLinearGradient(vg, ax, ay, bx, by, nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 128));
    nvgFillPaint(vg, paint);
    nvgFill(vg);

    nvgRestore(vg);
}

void drawThumbnails(NVGcontext* vg, float x, float y, float w, float h, const int* images, int nimages, float t)
{
    float    cornerRadius = 3.0f;
    NVGpaint shadowPaint, imgPaint, fadePaint;
    float    ix, iy, iw, ih;
    float    thumb = 60.0f;
    float    arry = 30.5f;
    int      imgw, imgh;
    float    stackh = (nimages / 2) * (thumb + 10) + 10;
    int      i;
    float    u = (1 + cosf(t * 0.5f)) * 0.5f;
    float    u2 = (1 - cosf(t * 0.2f)) * 0.5f;
    float    scrollh, dv;

    nvgSave(vg);
    //	nvgClearState(vg);

    // Drop shadow
    shadowPaint = nvgBoxGradient(vg, x, y + 4, w, h, cornerRadius * 2, 20, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, x - 10, y - 10, w + 20, h + 30);
    nvgRoundedRect(vg, x, y, w, h, cornerRadius);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, shadowPaint);
    nvgFill(vg);

    // Window
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x, y, w, h, cornerRadius);
    nvgMoveTo(vg, x - 10, y + arry);
    nvgLineTo(vg, x + 1, y + arry - 11);
    nvgLineTo(vg, x + 1, y + arry + 11);
    nvgFillColor(vg, nvgRGBA(200, 200, 200, 255));
    nvgFill(vg);

    nvgSave(vg);
    nvgScissor(vg, x, y, w, h);
    nvgTranslate(vg, 0, -(stackh - h) * u);

    dv = 1.0f / (float)(nimages - 1);

    for (i = 0; i < nimages; i++)
    {
        float tx, ty, v, a;
        tx = x + 10;
        ty = y + 10;
        tx += (i % 2) * (thumb + 10);
        ty += (i / 2) * (thumb + 10);
        nvgImageSize(vg, images[i], &imgw, &imgh);
        if (imgw < imgh)
        {
            iw = thumb;
            ih = iw * (float)imgh / (float)imgw;
            ix = 0;
            iy = -(ih - thumb) * 0.5f;
        }
        else
        {
            ih = thumb;
            iw = ih * (float)imgw / (float)imgh;
            ix = -(iw - thumb) * 0.5f;
            iy = 0;
        }

        v = i * dv;
        a = clampf((u2 - v) / dv, 0, 1);

        if (a < 1.0f)
            drawSpinner(vg, tx + thumb / 2, ty + thumb / 2, thumb * 0.25f, t);

        imgPaint = nvgImagePattern(vg, tx + ix, ty + iy, iw, ih, 0.0f / 180.0f * NVG_PI, images[i], a);
        nvgBeginPath(vg);
        nvgRoundedRect(vg, tx, ty, thumb, thumb, 5);
        nvgFillPaint(vg, imgPaint);
        nvgFill(vg);

        shadowPaint =
            nvgBoxGradient(vg, tx - 1, ty, thumb + 2, thumb + 2, 5, 3, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
        nvgBeginPath(vg);
        nvgRect(vg, tx - 5, ty - 5, thumb + 10, thumb + 10);
        nvgRoundedRect(vg, tx, ty, thumb, thumb, 6);
        nvgPathWinding(vg, NVG_HOLE);
        nvgFillPaint(vg, shadowPaint);
        nvgFill(vg);

        nvgBeginPath(vg);
        nvgRoundedRect(vg, tx + 0.5f, ty + 0.5f, thumb - 1, thumb - 1, 4 - 0.5f);
        nvgStrokeWidth(vg, 1.0f);
        nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 192));
        nvgStroke(vg);
    }
    nvgRestore(vg);

    // Hide fades
    fadePaint = nvgLinearGradient(vg, x, y, x, y + 6, nvgRGBA(200, 200, 200, 255), nvgRGBA(200, 200, 200, 0));
    nvgBeginPath(vg);
    nvgRect(vg, x + 4, y, w - 8, 6);
    nvgFillPaint(vg, fadePaint);
    nvgFill(vg);

    fadePaint = nvgLinearGradient(vg, x, y + h, x, y + h - 6, nvgRGBA(200, 200, 200, 255), nvgRGBA(200, 200, 200, 0));
    nvgBeginPath(vg);
    nvgRect(vg, x + 4, y + h - 6, w - 8, 6);
    nvgFillPaint(vg, fadePaint);
    nvgFill(vg);

    // Scroll bar
    shadowPaint =
        nvgBoxGradient(vg, x + w - 12 + 1, y + 4 + 1, 8, h - 8, 3, 4, nvgRGBA(0, 0, 0, 32), nvgRGBA(0, 0, 0, 92));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + w - 12, y + 4, 8, h - 8, 3);
    nvgFillPaint(vg, shadowPaint);
    //	nvgFillColor(vg, nvgRGBA(255,0,0,128));
    nvgFill(vg);

    scrollh = (h / stackh) * (h - 8);
    shadowPaint = nvgBoxGradient(
        vg, x + w - 12 - 1, y + 4 + (h - 8 - scrollh) * u - 1, 8, scrollh, 3, 4, nvgRGBA(220, 220, 220, 255),
        nvgRGBA(128, 128, 128, 255));
    nvgBeginPath(vg);
    nvgRoundedRect(vg, x + w - 12 + 1, y + 4 + 1 + (h - 8 - scrollh) * u, 8 - 2, scrollh - 2, 2);
    nvgFillPaint(vg, shadowPaint);
    //	nvgFillColor(vg, nvgRGBA(0,0,0,128));
    nvgFill(vg);

    nvgRestore(vg);
}

void drawColorwheel(NVGcontext* vg, float x, float y, float w, float h, float t)
{
    int      i;
    float    r0, r1, ax, ay, bx, by, cx, cy, aeps, r;
    float    hue = sinf(t * 0.12f);
    NVGpaint paint;

    nvgSave(vg);

    /*	nvgBeginPath(vg);
	nvgRect(vg, x,y,w,h);
	nvgFillColor(vg, nvgRGBA(255,0,0,128));
	nvgFill(vg);*/

    cx = x + w * 0.5f;
    cy = y + h * 0.5f;
    r1 = (w < h ? w : h) * 0.5f - 5.0f;
    r0 = r1 - 20.0f;
    aeps = 0.5f / r1;    // half a pixel arc length in radians (2pi cancels out).

    for (i = 0; i < 6; i++)
    {
        float a0 = (float)i / 6.0f * NVG_PI * 2.0f - aeps;
        float a1 = (float)(i + 1.0f) / 6.0f * NVG_PI * 2.0f + aeps;
        nvgBeginPath(vg);
        nvgArc(vg, cx, cy, r0, a0, a1, NVG_CW);
        nvgArc(vg, cx, cy, r1, a1, a0, NVG_CCW);
        nvgClosePath(vg);
        ax = cx + cosf(a0) * (r0 + r1) * 0.5f;
        ay = cy + sinf(a0) * (r0 + r1) * 0.5f;
        bx = cx + cosf(a1) * (r0 + r1) * 0.5f;
        by = cy + sinf(a1) * (r0 + r1) * 0.5f;
        paint = nvgLinearGradient(
            vg, ax, ay, bx, by, nvgHSLA(a0 / (NVG_PI * 2), 1.0f, 0.55f, 255),
            nvgHSLA(a1 / (NVG_PI * 2), 1.0f, 0.55f, 255));
        nvgFillPaint(vg, paint);
        nvgFill(vg);
    }

    nvgBeginPath(vg);
    nvgCircle(vg, cx, cy, r0 - 0.5f);
    nvgCircle(vg, cx, cy, r1 + 0.5f);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 64));
    nvgStrokeWidth(vg, 1.0f);
    nvgStroke(vg);

    // Selector
    nvgSave(vg);
    nvgTranslate(vg, cx, cy);
    nvgRotate(vg, hue * NVG_PI * 2);

    // Marker on
    nvgStrokeWidth(vg, 2.0f);
    nvgBeginPath(vg);
    nvgRect(vg, r0 - 1, -3, r1 - r0 + 2, 6);
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 192));
    nvgStroke(vg);

    paint = nvgBoxGradient(vg, r0 - 3, -5, r1 - r0 + 6, 10, 2, 4, nvgRGBA(0, 0, 0, 128), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, r0 - 2 - 10, -4 - 10, r1 - r0 + 4 + 20, 8 + 20);
    nvgRect(vg, r0 - 2, -4, r1 - r0 + 4, 8);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, paint);
    nvgFill(vg);

    // Center triangle
    r = r0 - 6;
    ax = cosf(120.0f / 180.0f * NVG_PI) * r;
    ay = sinf(120.0f / 180.0f * NVG_PI) * r;
    bx = cosf(-120.0f / 180.0f * NVG_PI) * r;
    by = sinf(-120.0f / 180.0f * NVG_PI) * r;
    nvgBeginPath(vg);
    nvgMoveTo(vg, r, 0);
    nvgLineTo(vg, ax, ay);
    nvgLineTo(vg, bx, by);
    nvgClosePath(vg);
    paint = nvgLinearGradient(vg, r, 0, ax, ay, nvgHSLA(hue, 1.0f, 0.5f, 255), nvgRGBA(255, 255, 255, 255));
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    paint = nvgLinearGradient(vg, (r + ax) * 0.5f, (0 + ay) * 0.5f, bx, by, nvgRGBA(0, 0, 0, 0), nvgRGBA(0, 0, 0, 255));
    nvgFillPaint(vg, paint);
    nvgFill(vg);
    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 64));
    nvgStroke(vg);

    // Select circle on triangle
    ax = cosf(120.0f / 180.0f * NVG_PI) * r * 0.3f;
    ay = sinf(120.0f / 180.0f * NVG_PI) * r * 0.4f;
    nvgStrokeWidth(vg, 2.0f);
    nvgBeginPath(vg);
    nvgCircle(vg, ax, ay, 5);
    nvgStrokeColor(vg, nvgRGBA(255, 255, 255, 192));
    nvgStroke(vg);

    paint = nvgRadialGradient(vg, ax, ay, 7, 9, nvgRGBA(0, 0, 0, 64), nvgRGBA(0, 0, 0, 0));
    nvgBeginPath(vg);
    nvgRect(vg, ax - 20, ay - 20, 40, 40);
    nvgCircle(vg, ax, ay, 7);
    nvgPathWinding(vg, NVG_HOLE);
    nvgFillPaint(vg, paint);
    nvgFill(vg);

    nvgRestore(vg);

    nvgRestore(vg);
}

void drawLines(NVGcontext* vg, float x, float y, float w, float h, float t)
{
    int   i, j;
    float pad = 5.0f, s = w / 9.0f - pad * 2;
    float pts[4 * 2], fx, fy;
    int   joins[3] = { NVG_MITER, NVG_ROUND, NVG_BEVEL };
    int   caps[3] = { NVG_BUTT, NVG_ROUND, NVG_SQUARE };
    NVG_NOTUSED(h);

    nvgSave(vg);
    pts[0] = -s * 0.25f + cosf(t * 0.3f) * s * 0.5f;
    pts[1] = sinf(t * 0.3f) * s * 0.5f;
    pts[2] = -s * 0.25f;
    pts[3] = 0;
    pts[4] = s * 0.25f;
    pts[5] = 0;
    pts[6] = s * 0.25f + cosf(-t * 0.3f) * s * 0.5f;
    pts[7] = sinf(-t * 0.3f) * s * 0.5f;

    for (i = 0; i < 3; i++)
    {
        for (j = 0; j < 3; j++)
        {
            fx = x + s * 0.5f + (i * 3 + j) / 9.0f * w + pad;
            fy = y - s * 0.5f + pad;

            nvgLineCap(vg, caps[i]);
            nvgLineJoin(vg, joins[j]);

            nvgStrokeWidth(vg, s * 0.3f);
            nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 160));
            nvgBeginPath(vg);
            nvgMoveTo(vg, fx + pts[0], fy + pts[1]);
            nvgLineTo(vg, fx + pts[2], fy + pts[3]);
            nvgLineTo(vg, fx + pts[4], fy + pts[5]);
            nvgLineTo(vg, fx + pts[6], fy + pts[7]);
            nvgStroke(vg);

            nvgLineCap(vg, NVG_BUTT);
            nvgLineJoin(vg, NVG_BEVEL);

            nvgStrokeWidth(vg, 1.0f);
            nvgStrokeColor(vg, nvgRGBA(0, 192, 255, 255));
            nvgBeginPath(vg);
            nvgMoveTo(vg, fx + pts[0], fy + pts[1]);
            nvgLineTo(vg, fx + pts[2], fy + pts[3]);
            nvgLineTo(vg, fx + pts[4], fy + pts[5]);
            nvgLineTo(vg, fx + pts[6], fy + pts[7]);
            nvgStroke(vg);
        }
    }

    nvgRestore(vg);
}

int loadDemoData(NVGcontext* vg, DemoData* data)
{
    if (vg == NULL)
        return -1;

    char file[128];
    char path[256];
    for (int i = 0; i < 12; i++)
    {
        snprintf(file, 128, "images/image%d.jpg", i + 1);
        data->images[i] = nvgCreateImage(vg, sample_get_path(DIRECTORY_DATA, file, path, sizeof(path)), 0);
        if (data->images[i] == 0)
        {
            printf("Could not load %s.\n", file);
            return -1;
        }
    }

    data->fontIcons =
        nvgCreateFont(vg, "icons", sample_get_path(DIRECTORY_DATA, "fonts/entypo.ttf", path, sizeof(path)));
    if (data->fontIcons == -1)
    {
        printf("Could not add font icons.\n");
        return -1;
    }
    data->fontNormal = nvgCreateFont(
        vg, "sans", sample_get_path(DIRECTORY_DATA, "fonts/Roboto-Regular.ttf", path, sizeof(path)));
    if (data->fontNormal == -1)
    {
        printf("Could not add font italic.\n");
        return -1;
    }
    data->fontBold = nvgCreateFont(
        vg, "sans-bold", sample_get_path(DIRECTORY_DATA, "fonts/Roboto-Bold.ttf", path, sizeof(path)));
    if (data->fontBold == -1)
    {
        printf("Could not add font bold.\n");
        return -1;
    }
    data->fontEmoji = nvgCreateFont(
        vg, "emoji", sample_get_path(DIRECTORY_DATA, "fonts/NotoEmoji-Regular.ttf", path, sizeof(path)));
    if (data->fontEmoji == -1)
    {
        printf("Could not add font emoji.\n");
        return -1;
    }
    nvgAddFallbackFontId(vg, data->fontNormal, data->fontEmoji);
    nvgAddFallbackFontId(vg, data->fontBold, data->fontEmoji);

    return 0;
}

void freeDemoData(NVGcontext* vg, DemoData* data)
{
    int i;

    if (vg == NULL)
        return;

    for (i = 0; i < 12; i++)
        nvgDeleteImage(vg, data->images[i]);
}

void drawParagraph(NVGcontext* vg, float x, float y, float width, float height, float mx, float my)
{
    NVGtextRow       rows[3];
    NVGglyphPosition glyphs[100];

    // error C2440: 'initializing': cannot convert from 'const char8_t [176]' to 'const char *'
    const char*      text = (const char*)
        u8"This is longer chunk of text.\n  \n  Would have used lorem ipsum but she    was busy jumping over the lazy "
        u8"dog with the fox and all the men who came to the aid of the party.🎉";
    const char* start;
    const char* end;
    int         nrows, i, nglyphs, j, lnum = 0;
    float       lineh;
    float       caretx, px;
    float       bounds[4];
    float       a;
    float       gx, gy;
    int         gutter = 0;
    NVG_NOTUSED(height);

    nvgSave(vg);

    nvgFontSize(vg, 18.0f);
    nvgFontFace(vg, "sans");
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextMetrics(vg, NULL, NULL, &lineh);

    // The text break API can be used to fill a large buffer of rows,
    // or to iterate over the text just few lines (or just one) at a time.
    // The "next" variable of the last returned item tells where to continue.
    start = text;
    end = text + strlen(text);
    while ((nrows = nvgTextBreakLines(vg, start, end, width, rows, 3)))
    {
        for (i = 0; i < nrows; i++)
        {
            NVGtextRow* row = &rows[i];
            int         hit = mx > x && mx < (x + width) && my >= y && my < (y + lineh);

            nvgBeginPath(vg);
            nvgFillColor(vg, nvgRGBA(255, 255, 255, hit ? 64 : 16));
            nvgRect(vg, x, y, row->width, lineh);
            nvgFill(vg);

            nvgFillColor(vg, nvgRGBA(255, 255, 255, 255));
            nvgText(vg, x, y, row->start, row->end);

            if (hit)
            {
                caretx = (mx < x + row->width / 2) ? x : x + row->width;
                px = x;
                nglyphs = nvgTextGlyphPositions(vg, x, y, row->start, row->end, glyphs, 100);
                for (j = 0; j < nglyphs; j++)
                {
                    float x0 = glyphs[j].x;
                    float x1 = (j + 1 < nglyphs) ? glyphs[j + 1].x : x + row->width;
                    float gx = x0 * 0.3f + x1 * 0.7f;
                    if (mx >= px && mx < gx)
                        caretx = glyphs[j].x;
                    px = gx;
                }
                nvgBeginPath(vg);
                nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
                nvgRect(vg, caretx, y, 1, lineh);
                nvgFill(vg);

                gutter = lnum + 1;
                gx = x - 10;
                gy = y + lineh / 2;
            }
            lnum++;
            y += lineh;
        }
        // Keep going...
        start = rows[nrows - 1].next;
    }

    if (gutter)
    {
        char txt[16];
        snprintf(txt, sizeof(txt), "%d", gutter);
        nvgFontSize(vg, 13.0f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);

        nvgTextBounds(vg, gx, gy, txt, NULL, bounds);

        nvgBeginPath(vg);
        nvgFillColor(vg, nvgRGBA(255, 192, 0, 255));
        nvgRoundedRect(
            vg, roundf(bounds[0]) - 4, roundf(bounds[1]) - 2, roundf(bounds[2] - bounds[0]) + 8,
            roundf(bounds[3] - bounds[1]) + 4, (roundf(bounds[3] - bounds[1]) + 4) / 2 - 1);
        nvgFill(vg);

        nvgFillColor(vg, nvgRGBA(32, 32, 32, 255));
        nvgText(vg, gx, gy, txt, NULL);
    }

    y += 20.0f;

    nvgFontSize(vg, 13.0f);
    nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
    nvgTextLineHeight(vg, 1.2f);

    nvgTextBoxBounds(vg, x, y, 150, "Hover your mouse over the text to see calculated caret position.", NULL, bounds);

    // Fade the tooltip out when close to it.
    gx = fabsf((mx - (bounds[0] + bounds[2]) * 0.5f) / (bounds[0] - bounds[2]));
    gy = fabsf((my - (bounds[1] + bounds[3]) * 0.5f) / (bounds[1] - bounds[3]));
    a = maxf(gx, gy) - 0.5f;
    a = clampf(a, 0, 1);
    nvgGlobalAlpha(vg, a);

    nvgBeginPath(vg);
    nvgFillColor(vg, nvgRGBA(220, 220, 220, 255));
    nvgRoundedRect(
        vg, bounds[0] - 2, bounds[1] - 2, roundf(bounds[2] - bounds[0]) + 4, roundf(bounds[3] - bounds[1]) + 4, 3);
    px = roundf((bounds[2] + bounds[0]) / 2);
    nvgMoveTo(vg, px, bounds[1] - 10);
    nvgLineTo(vg, px + 7, bounds[1] + 1);
    nvgLineTo(vg, px - 7, bounds[1] + 1);
    nvgFill(vg);

    nvgFillColor(vg, nvgRGBA(0, 0, 0, 220));
    nvgTextBox(vg, x, y, 150, "Hover your mouse over the text to see calculated caret position.", NULL);

    nvgRestore(vg);
}

void drawWidths(NVGcontext* vg, float x, float y, float width)
{
    int i;

    nvgSave(vg);

    nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 255));

    for (i = 0; i < 20; i++)
    {
        float w = (i + 0.5f) * 0.1f;
        nvgStrokeWidth(vg, w);
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, y);
        nvgLineTo(vg, x + width, y + width * 0.3f);
        nvgStroke(vg);
        y += 10;
    }

    nvgRestore(vg);
}

void drawCaps(NVGcontext* vg, float x, float y, float width)
{
    int   i;
    int   caps[3] = { NVG_BUTT, NVG_ROUND, NVG_SQUARE };
    float lineWidth = 8.0f;

    nvgSave(vg);

    nvgBeginPath(vg);
    nvgRect(vg, x - lineWidth / 2, y, width + lineWidth, 40);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 32));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgRect(vg, x, y, width, 40);
    nvgFillColor(vg, nvgRGBA(255, 255, 255, 32));
    nvgFill(vg);

    nvgStrokeWidth(vg, lineWidth);
    for (i = 0; i < 3; i++)
    {
        nvgLineCap(vg, caps[i]);
        nvgStrokeColor(vg, nvgRGBA(0, 0, 0, 255));
        nvgBeginPath(vg);
        nvgMoveTo(vg, x, y + i * 10 + 5);
        nvgLineTo(vg, x + width, y + i * 10 + 5);
        nvgStroke(vg);
    }

    nvgRestore(vg);
}

void drawScissor(NVGcontext* vg, float x, float y, float t)
{
    nvgSave(vg);

    // Draw first rect and set scissor to it's area.
    nvgTranslate(vg, x, y);
    nvgRotate(vg, nvgDegToRad(5));
    nvgBeginPath(vg);
    nvgRect(vg, -20, -20, 60, 40);
    nvgFillColor(vg, nvgRGBA(255, 0, 0, 255));
    nvgFill(vg);
    nvgScissor(vg, -20, -20, 60, 40);

    // Draw second rectangle with offset and rotation.
    nvgTranslate(vg, 40, 0);
    nvgRotate(vg, t);

    // Draw the intended second rectangle without any scissoring.
    nvgSave(vg);
    nvgResetScissor(vg);
    nvgBeginPath(vg);
    nvgRect(vg, -20, -10, 60, 30);
    nvgFillColor(vg, nvgRGBA(255, 128, 0, 64));
    nvgFill(vg);
    nvgRestore(vg);

    // Draw second rectangle with combined scissoring.
    nvgIntersectScissor(vg, -20, -10, 60, 30);
    nvgBeginPath(vg);
    nvgRect(vg, -20, -10, 60, 30);
    nvgFillColor(vg, nvgRGBA(255, 128, 0, 255));
    nvgFill(vg);

    nvgRestore(vg);
}

void renderDemo(NVGcontext* vg, float mx, float my, float width, float height, float t, int blowup, DemoData* data)
{
    float x, y, popy;

    drawEyes(vg, width - 250, 50, 150, 100, mx, my, t);
    drawParagraph(vg, width - 450, 50, 150, 100, mx, my);
    drawGraph(vg, 0, height / 2, width, height / 2, t);
    drawColorwheel(vg, width - 300, height - 300, 250.0f, 250.0f, t);

    // Line joints
    drawLines(vg, 120, height - 50, 600, 50, t);

    // Line caps
    drawWidths(vg, 10, 50, 30);

    // Line caps
    drawCaps(vg, 10, 300, 30);

    drawScissor(vg, 50, height - 80, t);

    nvgSave(vg);
    if (blowup)
    {
        nvgRotate(vg, sinf(t * 0.3f) * 5.0f / 180.0f * NVG_PI);
        nvgScale(vg, 2.0f, 2.0f);
    }

    // Widgets
    drawWindow(vg, "Widgets `n Stuff", 50, 50, 300, 400);
    x = 60;
    y = 95;
    drawSearchBox(vg, "Search", x, y, 280, 25);
    y += 40;
    drawDropDown(vg, "Effects", x, y, 280, 28);
    popy = y + 14;
    y += 45;

    // Form
    drawLabel(vg, "Login", x, y, 280, 20);
    y += 25;
    drawEditBox(vg, "Email", x, y, 280, 28);
    y += 35;
    drawEditBox(vg, "Password", x, y, 280, 28);
    y += 38;
    drawCheckBox(vg, "Remember me", x, y, 140, 28);
    drawButton(vg, ICON_LOGIN, "Sign in", x + 138, y, 140, 28, nvgRGBA(0, 96, 128, 255));
    y += 45;

    // Slider
    drawLabel(vg, "Diameter", x, y, 280, 20);
    y += 25;
    drawEditBoxNum(vg, "123.00", "px", x + 180, y, 100, 28);
    drawSlider(vg, 0.4f, x, y, 170, 28);
    y += 55;

    drawButton(vg, ICON_TRASH, "Delete", x, y, 160, 28, nvgRGBA(128, 16, 8, 255));
    drawButton(vg, 0, "Cancel", x + 170, y, 110, 28, nvgRGBA(0, 0, 0, 0));

    // Thumbnails box
    drawThumbnails(vg, 365, popy - 30, 160, 300, data->images, 12, t);

    nvgRestore(vg);
}

void initGraph(PerfGraph* fps, int style, const char* name)
{
    memset(fps, 0, sizeof(PerfGraph));
    fps->style = style;
    strncpy(fps->name, name, sizeof(fps->name));
    fps->name[sizeof(fps->name) - 1] = '\0';
}

void updateGraph(PerfGraph* fps, float frameTime)
{
    fps->head = (fps->head + 1) % GRAPH_HISTORY_COUNT;
    fps->values[fps->head] = frameTime;
}

float getGraphAverage(PerfGraph* fps)
{
    int   i;
    float avg = 0;
    for (i = 0; i < GRAPH_HISTORY_COUNT; i++)
    {
        avg += fps->values[i];
    }
    return avg / (float)GRAPH_HISTORY_COUNT;
}

void renderGraph(NVGcontext* vg, float x, float y, PerfGraph* fps)
{
    int   i;
    float avg, w, h;
    char  str[64];

    avg = getGraphAverage(fps);

    w = 200;
    h = 35;

    nvgBeginPath(vg);
    nvgRect(vg, x, y, w, h);
    nvgFillColor(vg, nvgRGBA(0, 0, 0, 128));
    nvgFill(vg);

    nvgBeginPath(vg);
    nvgMoveTo(vg, x, y + h);
    if (fps->style == GRAPH_RENDER_FPS)
    {
        for (i = 0; i < GRAPH_HISTORY_COUNT; i++)
        {
            float v = 1.0f / (0.00001f + fps->values[(fps->head + i) % GRAPH_HISTORY_COUNT]);
            float vx, vy;
            if (v > 80.0f)
                v = 80.0f;
            vx = x + ((float)i / (GRAPH_HISTORY_COUNT - 1)) * w;
            vy = y + h - ((v / 80.0f) * h);
            nvgLineTo(vg, vx, vy);
        }
    }
    else if (fps->style == GRAPH_RENDER_PERCENT)
    {
        for (i = 0; i < GRAPH_HISTORY_COUNT; i++)
        {
            float v = fps->values[(fps->head + i) % GRAPH_HISTORY_COUNT] * 1.0f;
            float vx, vy;
            if (v > 100.0f)
                v = 100.0f;
            vx = x + ((float)i / (GRAPH_HISTORY_COUNT - 1)) * w;
            vy = y + h - ((v / 100.0f) * h);
            nvgLineTo(vg, vx, vy);
        }
    }
    else
    {
        for (i = 0; i < GRAPH_HISTORY_COUNT; i++)
        {
            float v = fps->values[(fps->head + i) % GRAPH_HISTORY_COUNT] * 1000.0f;
            float vx, vy;
            if (v > 20.0f)
                v = 20.0f;
            vx = x + ((float)i / (GRAPH_HISTORY_COUNT - 1)) * w;
            vy = y + h - ((v / 20.0f) * h);
            nvgLineTo(vg, vx, vy);
        }
    }
    nvgLineTo(vg, x + w, y + h);
    nvgFillColor(vg, nvgRGBA(255, 192, 0, 128));
    nvgFill(vg);

    nvgFontFace(vg, "sans");

    if (fps->name[0] != '\0')
    {
        nvgFontSize(vg, 14.0f);
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGBA(240, 240, 240, 192));
        nvgText(vg, x + 3, y + 1, fps->name, NULL);
    }

    if (fps->style == GRAPH_RENDER_FPS)
    {
        nvgFontSize(vg, 18.0f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGBA(240, 240, 240, 255));
        sprintf(str, "%.2f FPS", 1.0f / avg);
        nvgText(vg, x + w - 3, y + 1, str, NULL);

        nvgFontSize(vg, 15.0f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_BOTTOM);
        nvgFillColor(vg, nvgRGBA(240, 240, 240, 160));
        sprintf(str, "%.2f ms", avg * 1000.0f);
        nvgText(vg, x + w - 3, y + h - 1, str, NULL);
    }
    else if (fps->style == GRAPH_RENDER_PERCENT)
    {
        nvgFontSize(vg, 18.0f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGBA(240, 240, 240, 255));
        sprintf(str, "%.1f %%", avg * 1.0f);
        nvgText(vg, x + w - 3, y + 1, str, NULL);
    }
    else
    {
        nvgFontSize(vg, 18.0f);
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_TOP);
        nvgFillColor(vg, nvgRGBA(240, 240, 240, 255));
        sprintf(str, "%.2f ms", avg * 1000.0f);
        nvgText(vg, x + w - 3, y + 1, str, NULL);
    }
}
