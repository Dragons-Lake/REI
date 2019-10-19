#include "sample.h"

struct sample_input_state_t
{
    int32_t     width;
    int32_t     height;

    int32_t     mouse_x;
    int32_t     mouse_y;
    int32_t     delta_x;
    int32_t     delta_y;
    uint8_t     key_state_prev[SDL_NUM_SCANCODES];
    uint8_t     key_state_curr[SDL_NUM_SCANCODES];
    uint32_t    mouse_state_prev;
    uint32_t    mouse_state_curr;
    bool        mouse_wheel_up;
    bool        mouse_wheel_down;

    bool        fullscreen;
    bool        is_window_resizable;
    SDL_Window* window;
} sample_input_state;

void sample_input_init(SDL_Window* window)
{
    memset(&sample_input_state, 0, sizeof(sample_input_state));

    sample_input_state.window = window;
    sample_input_state.is_window_resizable = (SDL_GetWindowFlags(window) & SDL_WINDOW_RESIZABLE) != 0;
    SDL_GetWindowSize(window, &sample_input_state.width, &sample_input_state.height);
}

void sample_input_fini()
{
}

void sample_input_resize()
{
    SDL_GetWindowSize(window, &sample_input_state.width, &sample_input_state.height);
}

void sample_input_update()
{
    memcpy(sample_input_state.key_state_prev, sample_input_state.key_state_curr, SDL_NUM_SCANCODES);
    memcpy(sample_input_state.key_state_curr, SDL_GetKeyboardState(NULL), SDL_NUM_SCANCODES);

    sample_input_state.mouse_state_prev = sample_input_state.mouse_state_curr;
    sample_input_state.mouse_state_curr = SDL_GetMouseState(&sample_input_state.mouse_x, &sample_input_state.mouse_y);
    SDL_GetRelativeMouseState(&sample_input_state.delta_x, &sample_input_state.delta_y);

    sample_input_state.mouse_wheel_up = sample_input_state.mouse_wheel_down = false;
}

void sample_input_on_event(SDL_Event* event)
{
    if (event->type == SDL_MOUSEWHEEL)
    {
        sample_input_state.mouse_wheel_up = event->wheel.y > 0;
        sample_input_state.mouse_wheel_down = event->wheel.y < 0;
    }
}

bool sample_mouse_was_wheel_up()
{
    return sample_input_state.mouse_wheel_up;
}

bool sample_mouse_was_wheel_down()
{
    return sample_input_state.mouse_wheel_down;
}

bool sample_key_is_pressed(int32_t key)
{
    REI_ASSERT(key<SDL_NUM_SCANCODES);
    return sample_input_state.key_state_curr[key] != 0;
}

bool sample_key_was_pressed(int32_t key)
{
    REI_ASSERT(key<SDL_NUM_SCANCODES);
    return !sample_input_state.key_state_prev[key] && sample_input_state.key_state_curr[key];
}

bool sample_key_is_released(int32_t key)
{
    REI_ASSERT(key<SDL_NUM_SCANCODES);
    return !sample_input_state.key_state_curr[key];
}

bool sample_key_was_released(int32_t key)
{
    REI_ASSERT(key<SDL_NUM_SCANCODES);
    return sample_input_state.key_state_prev[key] && !sample_input_state.key_state_curr[key];
}

bool sample_mouse_is_pressed(int32_t button)
{
    return (sample_input_state.mouse_state_curr&SDL_BUTTON(button)) != 0;
}

bool sample_mouse_was_pressed(int32_t button)
{
    return !(sample_input_state.mouse_state_prev&SDL_BUTTON(button)) && (sample_input_state.mouse_state_curr&SDL_BUTTON(button));
}

bool sample_mouse_is_released(int32_t button)
{
    return !(sample_input_state.mouse_state_curr&SDL_BUTTON(button));
}

bool sample_mouse_was_released(int32_t button)
{
    return (sample_input_state.mouse_state_prev&SDL_BUTTON(button)) && !(sample_input_state.mouse_state_curr&SDL_BUTTON(button));
}

void sample_mouse_rel_offset(int32_t* dx, int32_t* dy)
{
    if (dx) *dx = sample_input_state.delta_x;
    if (dy) *dy = sample_input_state.delta_y;
}

void sample_mouse_abs_offset(int32_t* x, int32_t* y)
{
    if (x) *x = sample_input_state.mouse_x;
    if (y) *y = sample_input_state.mouse_y;
}

void sample_capture_mouse()
{
    SDL_ShowCursor(FALSE);
    SDL_SetWindowGrab(window, SDL_TRUE);
    SDL_SetRelativeMouseMode(SDL_TRUE);
}

void sample_release_mouse()
{
    SDL_ShowCursor(TRUE);
    SDL_SetWindowGrab(window, SDL_FALSE);
    SDL_SetRelativeMouseMode(SDL_FALSE);
}

bool sample_mouse_is_captured()
{
    return SDL_GetWindowGrab(window) == SDL_TRUE;
}

#include "REI_Integration/SimpleCamera.h"

void sample_update_simple_camera_with_input(SimpleCamera* cam)
{
    if (sample_mouse_was_pressed(SDL_BUTTON_LEFT))
    {
        sample_capture_mouse();
    }
    if (sample_mouse_was_released(SDL_BUTTON_LEFT))
    {
        sample_release_mouse();
    }
    if (sample_mouse_is_captured())
    {
        int32_t mouse_dx, mouse_dy;
        sample_mouse_rel_offset(&mouse_dx, &mouse_dy);
        SimpleCamera_rotate(cam, mouse_dx * 0.001f, mouse_dy * 0.001f );
    }

    float dx = (sample_key_is_pressed(SDL_SCANCODE_D)?1.0f:0.0f) - (sample_key_is_pressed(SDL_SCANCODE_A)?1.0f:0.0f);
    float dy = (sample_key_is_pressed(SDL_SCANCODE_E)?1.0f:0.0f) - (sample_key_is_pressed(SDL_SCANCODE_Q)?1.0f:0.0f);
    float dz = (sample_key_is_pressed(SDL_SCANCODE_W)?1.0f:0.0f) - (sample_key_is_pressed(SDL_SCANCODE_S)?1.0f:0.0f);
    SimpleCamera_move(cam, dx, dy, dz);
}
