/* SPDX-License-Identifier: Apache-2.0 */
#include "mybot_display.h"

#include <mybot/platform/mybot_lcd.h>

#include <components/bk_display.h>
#include "mybot_platform_log.h"
#include <driver/gpio.h>
#include <driver/pwr_clk.h>
#include <gpio_driver.h>
#include <lcd_panel_devices.h>
#include <modules/pm.h>
#include <os/mem.h>
#include <os/os.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define DISPLAY_WIDTH 160
#define DISPLAY_HEIGHT 160
#define DISPLAY_COUNT 2
#define DISPLAY_BYTES_PER_PIXEL 2
#define DISPLAY_PANEL_SIZE (DISPLAY_WIDTH * DISPLAY_HEIGHT * DISPLAY_BYTES_PER_PIXEL)
#define DISPLAY_FRAME_SIZE (DISPLAY_PANEL_SIZE * DISPLAY_COUNT)

/* Keep both badges inside the circular panel, including their AA edge. */
#define DISPLAY_BADGE_LEFT_X 40
#define DISPLAY_BADGE_RIGHT_X (DISPLAY_WIDTH - DISPLAY_BADGE_LEFT_X)
#define DISPLAY_BADGE_Y 36
#define DISPLAY_BADGE_RADIUS 14

#define DISPLAY_BACKLIGHT_GPIO GPIO_25
#define DISPLAY_THREAD_PRIORITY 2
#define DISPLAY_THREAD_STACK_SIZE 4096
#define DISPLAY_QUEUE_DEPTH 4
#define DISPLAY_FLUSH_TIMEOUT_MS 5000

#define TAG "mybot_display"
#define LOGE(...) MYBOT_LOGE(TAG, ##__VA_ARGS__)
#define LOGD(...) MYBOT_LOGD(TAG, ##__VA_ARGS__)
#define LOGW(...) MYBOT_LOGW(TAG, ##__VA_ARGS__)
#define LOGI(...) MYBOT_LOGI(TAG, ##__VA_ARGS__)

#define COLOR_BLACK 0x0000
#define COLOR_NEAR_BLACK 0x0841
#define COLOR_DIM_SEGMENT 0x18E3
#define COLOR_WHITE 0xFFFF
#define COLOR_CYAN 0x3FFF
#define COLOR_BLUE 0x4A7F
#define COLOR_GREEN 0x47E8
#define COLOR_YELLOW 0xFFE0
#define COLOR_AMBER 0xFD20
#define COLOR_RED 0xF986
#define COLOR_GRAY 0x8410

typedef enum {
    DISPLAY_COMMAND_SCREEN = 0,
    DISPLAY_COMMAND_PAIR_CODE,
    DISPLAY_COMMAND_STOP,
} display_command_type_t;

typedef struct {
    display_command_type_t type;
    mybot_display_screen_t screen;
    uint32_t indicators;
    char pair_code[7];
    uint32_t trace_id;
} display_command_t;

typedef struct {
    frame_buffer_t frame;
    unsigned int slot;
    unsigned int panel;
    uint32_t trace_id;
} display_panel_frame_t;

typedef struct {
    bool power_owned;
    bool backlight_owned;
    beken_mutex_t api_lock;
    beken_mutex_t frame_lock;
    beken_queue_t command_queue;
    beken_semaphore_t command_done;
    beken_semaphore_t worker_stopped;
    beken_semaphore_t flush_done;
    beken_semaphore_t api_users_drained;
    beken_thread_t worker;
    bk_display_ctlr_handle_t controllers[DISPLAY_COUNT];
    bool controller_open[DISPLAY_COUNT];
    uint8_t *frame_pixels[2];
    display_panel_frame_t frames[2][DISPLAY_COUNT];
    bool panel_completed[2][DISPLAY_COUNT];
    unsigned int next_frame;
    uint32_t next_trace_id;
    int command_result;
} display_manager_t;

typedef enum {
    DISPLAY_LIFECYCLE_DOWN = 0,
    DISPLAY_LIFECYCLE_STARTING,
    DISPLAY_LIFECYCLE_READY,
    DISPLAY_LIFECYCLE_STOPPING,
} display_lifecycle_state_t;

static display_manager_t s_display;
static beken_mutex_t s_lifecycle_lock;
static display_lifecycle_state_t s_lifecycle;
static uint32_t s_api_users;

static bk_display_spi_ctlr_config_t s_spi_config[DISPLAY_COUNT] = {
    {
        .lcd_device = &lcd_device_gc9d01,
        .spi_id = 0,
        .dc_pin = GPIO_7,
        .reset_pin = GPIO_6,
        .te_pin = 0,
    },
    {
        .lcd_device = &lcd_device_gc9d01,
        .spi_id = 1,
        .dc_pin = GPIO_5,
        .reset_pin = GPIO_45,
        .te_pin = 0,
    },
};

static uint8_t *panel_pixels(unsigned int slot, unsigned int panel) {
    return s_display.frames[slot][panel].frame.frame;
}

static void put_pixel(uint8_t *pixels, int x, int y, uint16_t color) {
    if (x < 0 || x >= DISPLAY_WIDTH || y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }

    size_t offset = ((size_t)y * DISPLAY_WIDTH + (size_t)x) * DISPLAY_BYTES_PER_PIXEL;
    pixels[offset] = (uint8_t)(color >> 8);
    pixels[offset + 1] = (uint8_t)color;
}

static void fill_panel(uint8_t *pixels, uint16_t color) {
    uint8_t high = (uint8_t)(color >> 8);
    uint8_t low = (uint8_t)color;

    for (size_t offset = 0; offset < DISPLAY_PANEL_SIZE; offset += 2) {
        pixels[offset] = high;
        pixels[offset + 1] = low;
    }
}

static void fill_rect(uint8_t *pixels, int x, int y, int width, int height, uint16_t color) {
    for (int row = 0; row < height; ++row) {
        for (int column = 0; column < width; ++column) {
            put_pixel(pixels, x + column, y + row, color);
        }
    }
}

/* Coverage rasterization adapted from the BK7259 MyBot LCD renderer
 * (platforms/bk7259/bk7259_lcd.c, Apache-2.0).
 * Keep BK7258 SPI RGB565 pixels in their existing high-byte-first order. */
static void blend_pixel(uint8_t *pixels, int x, int y, uint16_t color, uint8_t alpha) {
    if (!pixels || alpha == 0 || x < 0 || x >= DISPLAY_WIDTH ||
        y < 0 || y >= DISPLAY_HEIGHT) {
        return;
    }
    size_t offset = ((size_t)y * DISPLAY_WIDTH + (size_t)x) * DISPLAY_BYTES_PER_PIXEL;
    uint16_t background = ((uint16_t)pixels[offset] << 8) | pixels[offset + 1];
    uint32_t inverse = 255U - alpha;
    uint32_t red = (((color >> 11) & 31U) * alpha +
                    ((background >> 11) & 31U) * inverse + 127U) / 255U;
    uint32_t green = (((color >> 5) & 63U) * alpha +
                      ((background >> 5) & 63U) * inverse + 127U) / 255U;
    uint32_t blue = ((color & 31U) * alpha + (background & 31U) * inverse + 127U) / 255U;
    put_pixel(pixels, x, y, (uint16_t)((red << 11) | (green << 5) | blue));
}

static bool point_in_capsule(int point_x8, int point_y8, int start_x8, int start_y8,
                             int end_x8, int end_y8, int radius_squared)
{
    int vector_x = end_x8 - start_x8;
    int vector_y = end_y8 - start_y8;
    int point_vector_x = point_x8 - start_x8;
    int point_vector_y = point_y8 - start_y8;
    int length_squared = vector_x * vector_x + vector_y * vector_y;
    int projection = point_vector_x * vector_x + point_vector_y * vector_y;

    if (projection <= 0 || length_squared == 0) {
        return point_vector_x * point_vector_x + point_vector_y * point_vector_y <=
               radius_squared;
    }
    if (projection >= length_squared) {
        int end_dx = point_x8 - end_x8;
        int end_dy = point_y8 - end_y8;
        return end_dx * end_dx + end_dy * end_dy <= radius_squared;
    }

    int64_t cross = (int64_t)point_vector_x * vector_y -
                    (int64_t)point_vector_y * vector_x;
    return cross * cross <= (int64_t)radius_squared * length_squared;
}

static void draw_line(uint8_t *pixels, int x0, int y0, int x1, int y1, int thickness,
                      uint16_t color)
{
    static const int sample_offsets[4] = {-3, -1, 1, 3};
    if (!pixels || thickness <= 0) {
        return;
    }

    int padding = (thickness + 1) / 2 + 1;
    int min_x = (x0 < x1 ? x0 : x1) - padding;
    int max_x = (x0 > x1 ? x0 : x1) + padding;
    int min_y = (y0 < y1 ? y0 : y1) - padding;
    int max_y = (y0 > y1 ? y0 : y1) + padding;
    if (min_x < 0) {
        min_x = 0;
    }
    if (max_x >= DISPLAY_WIDTH) {
        max_x = DISPLAY_WIDTH - 1;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_y >= DISPLAY_HEIGHT) {
        max_y = DISPLAY_HEIGHT - 1;
    }

    int start_x8 = x0 * 8;
    int start_y8 = y0 * 8;
    int end_x8 = x1 * 8;
    int end_y8 = y1 * 8;
    int radius8 = thickness * 4;
    int radius_squared = radius8 * radius8;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            unsigned int covered = 0;
            for (size_t sample_y = 0; sample_y < 4; ++sample_y) {
                int point_y8 = y * 8 + sample_offsets[sample_y];
                for (size_t sample_x = 0; sample_x < 4; ++sample_x) {
                    int point_x8 = x * 8 + sample_offsets[sample_x];
                    if (point_in_capsule(point_x8, point_y8, start_x8, start_y8, end_x8,
                                         end_y8, radius_squared)) {
                        ++covered;
                    }
                }
            }
            blend_pixel(pixels, x, y, color, (uint8_t)((covered * 255U + 8U) >> 4));
        }
    }
}

static void draw_radial(uint8_t *pixels, int center_x, int center_y, int inner_radius,
                        int outer_radius, uint16_t color)
{
    static const int sample_offsets[4] = {-3, -1, 1, 3};
    if (!pixels || outer_radius <= 0) {
        return;
    }

    int min_x = center_x - outer_radius;
    int max_x = center_x + outer_radius;
    int min_y = center_y - outer_radius;
    int max_y = center_y + outer_radius;
    if (min_x < 0) {
        min_x = 0;
    }
    if (max_x >= DISPLAY_WIDTH) {
        max_x = DISPLAY_WIDTH - 1;
    }
    if (min_y < 0) {
        min_y = 0;
    }
    if (max_y >= DISPLAY_HEIGHT) {
        max_y = DISPLAY_HEIGHT - 1;
    }

    int outer8 = outer_radius * 8;
    int outer_squared = outer8 * outer8;
    int inner8 = inner_radius > 0 ? inner_radius * 8 : 0;
    int inner_squared = inner8 * inner8;

    for (int y = min_y; y <= max_y; ++y) {
        for (int x = min_x; x <= max_x; ++x) {
            unsigned int covered = 0;
            for (size_t sample_y = 0; sample_y < 4; ++sample_y) {
                int dy8 = (y - center_y) * 8 + sample_offsets[sample_y];
                for (size_t sample_x = 0; sample_x < 4; ++sample_x) {
                    int dx8 = (x - center_x) * 8 + sample_offsets[sample_x];
                    int distance_squared = dx8 * dx8 + dy8 * dy8;
                    if (distance_squared <= outer_squared &&
                        (inner_radius <= 0 || distance_squared >= inner_squared)) {
                        ++covered;
                    }
                }
            }
            blend_pixel(pixels, x, y, color, (uint8_t)((covered * 255U + 8U) >> 4));
        }
    }
}

static void draw_ring(uint8_t *pixels, int radius, int thickness, uint16_t color) {
    draw_radial(pixels, DISPLAY_WIDTH / 2, DISPLAY_HEIGHT / 2,
                radius - thickness, radius, color);
}

static void draw_circle(uint8_t *pixels, int center_x, int center_y, int radius,
                        uint16_t color) {
    draw_radial(pixels, center_x, center_y, 0, radius, color);
}

static void draw_vp_indicator(uint8_t *pixels) {
    const int x = DISPLAY_BADGE_RIGHT_X;
    const int y = DISPLAY_BADGE_Y;
    draw_circle(pixels, x, y, DISPLAY_BADGE_RADIUS, COLOR_GREEN);
    draw_line(pixels, x - 8, y, x - 2, y + 6, 3, COLOR_WHITE);
    draw_line(pixels, x - 2, y + 6, x + 9, y - 7, 3, COLOR_WHITE);
}

static void draw_vp_pending_indicator(uint8_t *pixels) {
    const int x = DISPLAY_BADGE_RIGHT_X;
    const int y = DISPLAY_BADGE_Y;
    draw_circle(pixels, x, y, DISPLAY_BADGE_RADIUS, COLOR_RED);
    draw_line(pixels, x - 7, y - 7, x + 7, y + 7, 3, COLOR_WHITE);
    draw_line(pixels, x + 7, y - 7, x - 7, y + 7, 3, COLOR_WHITE);
}

static mybot_lcd_indicator_t server_indicator(uint32_t indicators)
{
    /* The SDK guarantees mutual exclusion. Keep a stable precedence for a
     * malformed bitmask received from an older/custom caller. */
    if (indicators & MYBOT_LCD_INDICATOR_LISTENING) {
        return MYBOT_LCD_INDICATOR_LISTENING;
    }
    if (indicators & MYBOT_LCD_INDICATOR_THINKING) {
        return MYBOT_LCD_INDICATOR_THINKING;
    }
    if (indicators & MYBOT_LCD_INDICATOR_SPEAKING) {
        return MYBOT_LCD_INDICATOR_SPEAKING;
    }
    return MYBOT_LCD_INDICATOR_NONE;
}

static uint16_t server_indicator_color(mybot_lcd_indicator_t indicator)
{
    switch (indicator) {
    case MYBOT_LCD_INDICATOR_LISTENING:
        return COLOR_CYAN;
    case MYBOT_LCD_INDICATOR_THINKING:
        return COLOR_AMBER;
    case MYBOT_LCD_INDICATOR_SPEAKING:
        return COLOR_GREEN;
    case MYBOT_LCD_INDICATOR_NONE:
    case MYBOT_LCD_INDICATOR_VP_REGISTERED:
    default:
        return COLOR_CYAN;
    }
    return COLOR_CYAN;
}

static void draw_server_state_overlay(uint8_t *pixels, mybot_lcd_indicator_t indicator)
{
    const int center_x = DISPLAY_BADGE_LEFT_X;
    const int center_y = DISPLAY_BADGE_Y;

    if (indicator == MYBOT_LCD_INDICATOR_NONE ||
        indicator == MYBOT_LCD_INDICATOR_VP_REGISTERED) {
        return;
    }

    draw_circle(pixels, center_x, center_y, DISPLAY_BADGE_RADIUS,
                server_indicator_color(indicator));
    switch (indicator) {
    case MYBOT_LCD_INDICATOR_LISTENING:
        draw_line(pixels, center_x, center_y - 6, center_x, center_y + 1, 6, COLOR_BLACK);
        draw_line(pixels, center_x - 7, center_y - 1, center_x - 7, center_y + 3, 2,
                  COLOR_BLACK);
        draw_line(pixels, center_x - 7, center_y + 3, center_x, center_y + 7, 2, COLOR_BLACK);
        draw_line(pixels, center_x, center_y + 7, center_x + 7, center_y + 3, 2, COLOR_BLACK);
        draw_line(pixels, center_x + 7, center_y + 3, center_x + 7, center_y - 1, 2,
                  COLOR_BLACK);
        draw_line(pixels, center_x, center_y + 7, center_x, center_y + 11, 2, COLOR_BLACK);
        break;
    case MYBOT_LCD_INDICATOR_THINKING:
        draw_circle(pixels, center_x - 8, center_y, 3, COLOR_BLACK);
        draw_circle(pixels, center_x, center_y, 3, COLOR_BLACK);
        draw_circle(pixels, center_x + 8, center_y, 3, COLOR_BLACK);
        break;
    case MYBOT_LCD_INDICATOR_SPEAKING:
        draw_line(pixels, center_x - 8, center_y - 3, center_x - 8, center_y + 3, 5,
                  COLOR_BLACK);
        draw_line(pixels, center_x - 5, center_y - 4, center_x, center_y - 8, 3, COLOR_BLACK);
        draw_line(pixels, center_x - 5, center_y + 4, center_x, center_y + 8, 3, COLOR_BLACK);
        draw_line(pixels, center_x, center_y - 8, center_x, center_y + 8, 3, COLOR_BLACK);
        draw_line(pixels, center_x + 5, center_y - 5, center_x + 9, center_y - 9, 2,
                  COLOR_BLACK);
        draw_line(pixels, center_x + 5, center_y + 5, center_x + 9, center_y + 9, 2,
                  COLOR_BLACK);
        break;
    case MYBOT_LCD_INDICATOR_NONE:
    case MYBOT_LCD_INDICATOR_VP_REGISTERED:
    default:
        break;
    }
}

static uint16_t screen_color(mybot_display_screen_t screen) {
    switch (screen) {
    case MYBOT_DISPLAY_SCREEN_STARTING:
        return COLOR_CYAN;
    case MYBOT_DISPLAY_SCREEN_WIFI_PROVISIONING:
        return COLOR_AMBER;
    case MYBOT_DISPLAY_SCREEN_WIFI_DISCONNECTED:
        return COLOR_RED;
    case MYBOT_DISPLAY_SCREEN_STARTING_SERVICES:
        return COLOR_BLUE;
    case MYBOT_DISPLAY_SCREEN_PAIRING:
        return COLOR_YELLOW;
    case MYBOT_DISPLAY_SCREEN_READY:
        return COLOR_GREEN;
    case MYBOT_DISPLAY_SCREEN_IN_CONVERSATION:
        return COLOR_CYAN;
    case MYBOT_DISPLAY_SCREEN_FAILED:
        return COLOR_RED;
    case MYBOT_DISPLAY_SCREEN_STOPPING:
    case MYBOT_DISPLAY_SCREEN_COUNT:
        return COLOR_GRAY;
    }
    return COLOR_GRAY;
}

static void draw_state_icon(uint8_t *pixels, mybot_display_screen_t screen, uint16_t color) {
    switch (screen) {
    case MYBOT_DISPLAY_SCREEN_READY:
        draw_line(pixels, 52, 82, 71, 101, 8, color);
        draw_line(pixels, 70, 101, 111, 58, 8, color);
        break;
    case MYBOT_DISPLAY_SCREEN_FAILED:
    case MYBOT_DISPLAY_SCREEN_WIFI_DISCONNECTED:
        draw_line(pixels, 56, 56, 104, 104, 8, color);
        draw_line(pixels, 104, 56, 56, 104, 8, color);
        break;
    case MYBOT_DISPLAY_SCREEN_IN_CONVERSATION:
        draw_line(pixels, 59, 69, 59, 90, 9, color);
        draw_line(pixels, 80, 58, 80, 101, 10, color);
        draw_line(pixels, 100, 69, 100, 90, 9, color);
        break;
    case MYBOT_DISPLAY_SCREEN_WIFI_PROVISIONING:
    case MYBOT_DISPLAY_SCREEN_PAIRING:
        fill_rect(pixels, 50, 75, 14, 14, color);
        fill_rect(pixels, 73, 75, 14, 14, color);
        fill_rect(pixels, 96, 75, 14, 14, color);
        break;
    case MYBOT_DISPLAY_SCREEN_STARTING:
    case MYBOT_DISPLAY_SCREEN_STARTING_SERVICES:
        draw_line(pixels, 80, 51, 80, 80, 8, color);
        draw_line(pixels, 80, 80, 101, 95, 8, color);
        break;
    case MYBOT_DISPLAY_SCREEN_STOPPING:
    case MYBOT_DISPLAY_SCREEN_COUNT:
        fill_rect(pixels, 55, 76, 50, 8, color);
        break;
    }
}

static void draw_state_panel(uint8_t *pixels, mybot_display_screen_t screen) {
    uint16_t color = screen_color(screen);
    fill_panel(pixels, COLOR_NEAR_BLACK);
    draw_ring(pixels, 64, 5, color);
    draw_state_icon(pixels, screen, color);
}

static const uint8_t s_digit_segments[10] = {
    0x3F, 0x06, 0x5B, 0x4F, 0x66, 0x6D, 0x7D, 0x07, 0x7F, 0x6F,
};

static void draw_digit(uint8_t *pixels, int x, int y, char digit) {
    const int width = 36;
    const int height = 84;
    const int thickness = 7;
    const int half = height / 2;
    uint8_t mask = s_digit_segments[digit - '0'];

#define DRAW_SEGMENT(bit, sx, sy, sw, sh)                                                     \
    fill_rect(pixels, (sx), (sy), (sw), (sh),                                                \
              (mask & (1u << (bit))) ? COLOR_WHITE : COLOR_DIM_SEGMENT)
    DRAW_SEGMENT(0, x + thickness, y, width - thickness * 2, thickness);
    DRAW_SEGMENT(1, x + width - thickness, y + thickness, thickness, half - thickness);
    DRAW_SEGMENT(2, x + width - thickness, y + half, thickness, half - thickness);
    DRAW_SEGMENT(3, x + thickness, y + height - thickness, width - thickness * 2, thickness);
    DRAW_SEGMENT(4, x, y + half, thickness, half - thickness);
    DRAW_SEGMENT(5, x, y + thickness, thickness, half - thickness);
    DRAW_SEGMENT(6, x + thickness, y + half - thickness / 2, width - thickness * 2, thickness);
#undef DRAW_SEGMENT
}

static void draw_pair_code_panel(uint8_t *pixels, const char *digits) {
    const int digit_width = 36;
    const int digit_gap = 7;
    const int group_width = digit_width * 3 + digit_gap * 2;
    const int start_x = (DISPLAY_WIDTH - group_width) / 2;

    fill_panel(pixels, COLOR_BLACK);
    draw_ring(pixels, 76, 2, COLOR_CYAN);
    for (int i = 0; i < 3; ++i) {
        draw_digit(pixels, start_x + i * (digit_width + digit_gap), 38, digits[i]);
    }
}

static void render_command(unsigned int slot, const display_command_t *command) {
    if (command->type == DISPLAY_COMMAND_PAIR_CODE) {
        draw_pair_code_panel(panel_pixels(slot, 0), command->pair_code);
        draw_pair_code_panel(panel_pixels(slot, 1), command->pair_code + 3);
        return;
    }

    draw_state_panel(panel_pixels(slot, 0), command->screen);
    draw_state_panel(panel_pixels(slot, 1), command->screen);
    if (command->screen == MYBOT_DISPLAY_SCREEN_IN_CONVERSATION) {
        mybot_lcd_indicator_t indicator = server_indicator(command->indicators);
        draw_server_state_overlay(panel_pixels(slot, 0), indicator);
        draw_server_state_overlay(panel_pixels(slot, 1), indicator);
    }
    if (command->screen == MYBOT_DISPLAY_SCREEN_IN_CONVERSATION &&
        (command->indicators & MYBOT_LCD_INDICATOR_VP_REGISTERED)) {
        draw_vp_indicator(panel_pixels(slot, 0));
        draw_vp_indicator(panel_pixels(slot, 1));
    } else if (command->screen == MYBOT_DISPLAY_SCREEN_IN_CONVERSATION) {
        draw_vp_pending_indicator(panel_pixels(slot, 0));
        draw_vp_pending_indicator(panel_pixels(slot, 1));
    }
}

static bk_err_t flush_complete_callback(void *frame) {
    display_panel_frame_t *panel_frame = frame;
    uint32_t trace_id;

    if (!panel_frame || panel_frame->slot >= 2 || panel_frame->panel >= DISPLAY_COUNT) {
        return BK_FAIL;
    }
    if (rtos_lock_mutex(&s_display.frame_lock) != BK_OK) {
        return BK_FAIL;
    }
    trace_id = panel_frame->trace_id;
    s_display.panel_completed[panel_frame->slot][panel_frame->panel] = true;
    rtos_unlock_mutex(&s_display.frame_lock);
    LOGD("trace=%u flush complete: slot=%u LCD%u", (unsigned int)trace_id,
         panel_frame->slot, panel_frame->panel);
    rtos_set_semaphore(&s_display.flush_done);
    return BK_OK;
}

static bool frame_is_available(unsigned int index) {
    bool available = true;

    if (rtos_lock_mutex(&s_display.frame_lock) != BK_OK) {
        return false;
    }
    for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
        if (!s_display.panel_completed[index][panel]) {
            available = false;
            break;
        }
    }
    rtos_unlock_mutex(&s_display.frame_lock);
    return available;
}

static int wait_until_frame_available(unsigned int index) {
    uint32_t start = rtos_get_time();

    for (;;) {
        if (frame_is_available(index)) {
            return 0;
        }

        uint32_t elapsed = rtos_get_time() - start;
        if (elapsed >= DISPLAY_FLUSH_TIMEOUT_MS) {
            LOGE("timed out waiting for LCD frame %u", index);
            return -1;
        }

        if (rtos_get_semaphore(&s_display.flush_done,
                               DISPLAY_FLUSH_TIMEOUT_MS - elapsed) != BK_OK) {
            LOGE("timed out waiting for LCD frame %u", index);
            return -1;
        }
    }
}

static int submit_frame(unsigned int index, uint32_t trace_id) {
    int result = 0;

    if (rtos_lock_mutex(&s_display.frame_lock) != BK_OK) {
        return -1;
    }
    for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
        s_display.frames[index][panel].trace_id = trace_id;
        s_display.panel_completed[index][panel] = false;
    }
    rtos_unlock_mutex(&s_display.frame_lock);
    for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
        LOGD("trace=%u flush submit: slot=%u LCD%u", (unsigned int)trace_id, index,
             panel);
        if (bk_display_flush(s_display.controllers[panel],
                             &s_display.frames[index][panel].frame,
                             flush_complete_callback) != BK_OK) {
            LOGE("trace=%u failed to flush: slot=%u LCD%u",
                 (unsigned int)trace_id, index, panel);
            rtos_lock_mutex(&s_display.frame_lock);
            s_display.panel_completed[index][panel] = true;
            rtos_unlock_mutex(&s_display.frame_lock);
            result = -1;
        } else {
            LOGD("trace=%u flush accepted: slot=%u LCD%u",
                 (unsigned int)trace_id, index, panel);
        }
    }
    return result;
}

static int render_next_frame(const display_command_t *command) {
    unsigned int index = s_display.next_frame;
    int result;

    LOGD("trace=%u frame selected: slot=%u", (unsigned int)command->trace_id, index);
    if (wait_until_frame_available(index) < 0) {
        return -1;
    }
    render_command(index, command);
    LOGD("trace=%u render complete: slot=%u", (unsigned int)command->trace_id, index);
    result = submit_frame(index, command->trace_id);
    s_display.next_frame ^= 1u;
    return result;
}

static const char *screen_name(mybot_display_screen_t screen) {
    static const char *const names[MYBOT_DISPLAY_SCREEN_COUNT] = {
        [MYBOT_DISPLAY_SCREEN_STARTING] = "starting",
        [MYBOT_DISPLAY_SCREEN_WIFI_PROVISIONING] = "wifi_provisioning",
        [MYBOT_DISPLAY_SCREEN_WIFI_DISCONNECTED] = "wifi_disconnected",
        [MYBOT_DISPLAY_SCREEN_STARTING_SERVICES] = "starting_services",
        [MYBOT_DISPLAY_SCREEN_PAIRING] = "pairing",
        [MYBOT_DISPLAY_SCREEN_READY] = "ready",
        [MYBOT_DISPLAY_SCREEN_IN_CONVERSATION] = "in_conversation",
        [MYBOT_DISPLAY_SCREEN_FAILED] = "failed",
        [MYBOT_DISPLAY_SCREEN_STOPPING] = "stopping",
    };
    return screen >= MYBOT_DISPLAY_SCREEN_STARTING && screen < MYBOT_DISPLAY_SCREEN_COUNT
               ? names[screen]
               : "unknown";
}

static void display_worker_main(beken_thread_arg_t arg) {
    (void)arg;
    LOGI("worker started");

    for (;;) {
        display_command_t command;
        if (rtos_pop_from_queue(&s_display.command_queue, &command, BEKEN_WAIT_FOREVER) !=
            BK_OK) {
            continue;
        }
        if (command.type == DISPLAY_COMMAND_STOP) {
            LOGI("worker stop received");
            break;
        }

        if (command.type == DISPLAY_COMMAND_PAIR_CODE) {
            LOGD("trace=%u dequeued: state=pair_code",
                 (unsigned int)command.trace_id);
        } else {
            LOGD("trace=%u dequeued: state=%s", (unsigned int)command.trace_id,
                 screen_name(command.screen));
        }
        s_display.command_result = render_next_frame(&command);
        if (s_display.command_result < 0) {
            LOGE("trace=%u command failed, type=%d",
                 (unsigned int)command.trace_id, command.type);
        } else if (command.type == DISPLAY_COMMAND_PAIR_CODE) {
            LOGD("trace=%u render submitted: state=pair_code",
                 (unsigned int)command.trace_id);
        } else {
            LOGD("trace=%u render submitted: state=%s",
                 (unsigned int)command.trace_id, screen_name(command.screen));
        }
        rtos_set_semaphore(&s_display.command_done);
    }

    LOGI("worker stopped");
    rtos_set_semaphore(&s_display.worker_stopped);
    rtos_delete_thread(NULL);
}

static void backlight_close(void);

static int display_power_open(void) {
#if CONFIG_LDO3V3_ENABLE
    if (bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_LCD,
                                             CONFIG_LDO3V3_CTRL_GPIO,
                                             GPIO_OUTPUT_STATE_HIGH) != BK_OK) {
        LOGE("failed to enable display 3.3V supply on GPIO%d",
             CONFIG_LDO3V3_CTRL_GPIO);
        return -1;
    }
    s_display.power_owned = true;
    rtos_delay_milliseconds(10);
    LOGI("display 3.3V supply enabled on GPIO%d", CONFIG_LDO3V3_CTRL_GPIO);
#endif
    return 0;
}

static void display_power_close(void) {
#if CONFIG_LDO3V3_ENABLE
    if (!s_display.power_owned) {
        return;
    }
    if (bk_pm_module_vote_ctrl_external_ldo(GPIO_CTRL_LDO_MODULE_LCD,
                                             CONFIG_LDO3V3_CTRL_GPIO,
                                             GPIO_OUTPUT_STATE_LOW) != BK_OK) {
        LOGW("failed to release display 3.3V supply on GPIO%d",
             CONFIG_LDO3V3_CTRL_GPIO);
        return;
    }
    s_display.power_owned = false;
    LOGI("display 3.3V supply released");
#endif
}

static int backlight_open(void) {
    gpio_dev_unmap(DISPLAY_BACKLIGHT_GPIO);
    s_display.backlight_owned = true;
    if (bk_gpio_enable_output(DISPLAY_BACKLIGHT_GPIO) != BK_OK ||
        bk_gpio_enable_pull(DISPLAY_BACKLIGHT_GPIO) != BK_OK ||
        bk_gpio_pull_up(DISPLAY_BACKLIGHT_GPIO) != BK_OK ||
        bk_gpio_set_output_high(DISPLAY_BACKLIGHT_GPIO) != BK_OK) {
        LOGE("failed to enable LCD backlight");
        backlight_close();
        return -1;
    }
    LOGI("backlight enabled on GPIO%d", DISPLAY_BACKLIGHT_GPIO);
    return 0;
}

static void backlight_close(void) {
    if (!s_display.backlight_owned) {
        return;
    }
    bk_gpio_pull_down(DISPLAY_BACKLIGHT_GPIO);
    bk_gpio_set_output_low(DISPLAY_BACKLIGHT_GPIO);
    gpio_dev_unmap(DISPLAY_BACKLIGHT_GPIO);
    s_display.backlight_owned = false;
    LOGI("backlight disabled");
}

static int allocate_frames(void) {
    for (unsigned int slot = 0; slot < 2; ++slot) {
        s_display.frame_pixels[slot] = psram_malloc(DISPLAY_FRAME_SIZE);
        if (!s_display.frame_pixels[slot]) {
            return -1;
        }
        for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
            display_panel_frame_t *panel_frame = &s_display.frames[slot][panel];
            memset(panel_frame, 0, sizeof(*panel_frame));
            panel_frame->frame.frame =
                s_display.frame_pixels[slot] + panel * DISPLAY_PANEL_SIZE;
            panel_frame->frame.size = DISPLAY_PANEL_SIZE;
            panel_frame->frame.width = DISPLAY_WIDTH;
            panel_frame->frame.height = DISPLAY_HEIGHT;
            panel_frame->frame.fmt = PIXEL_FMT_RGB565;
            panel_frame->slot = slot;
            panel_frame->panel = panel;
            fill_panel(panel_frame->frame.frame, COLOR_BLACK);
        }
        for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
            s_display.panel_completed[slot][panel] = true;
        }
    }
    return 0;
}

static void free_frames(void) {
    for (unsigned int slot = 0; slot < 2; ++slot) {
        if (s_display.frame_pixels[slot]) {
            psram_free(s_display.frame_pixels[slot]);
            s_display.frame_pixels[slot] = NULL;
        }
    }
}

static void release_resources(void) {
    bool bus_owned[DISPLAY_COUNT];

    for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
        bus_owned[panel] = s_display.controllers[panel] != NULL;
    }
    for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
        if (s_display.controller_open[panel]) {
            bk_display_close(s_display.controllers[panel]);
            s_display.controller_open[panel] = false;

            for (unsigned int remaining = panel + 1; remaining < DISPLAY_COUNT;
                 ++remaining) {
                if (s_display.controller_open[remaining]) {
                    bk_pm_module_vote_psram_ctrl(PM_POWER_PSRAM_MODULE_NAME_VIDP_LCD,
                                                 PM_POWER_MODULE_STATE_ON);
                    bk_pm_module_vote_cpu_freq(PM_DEV_ID_DISP, PM_CPU_FRQ_480M);
                    break;
                }
            }
        }
        if (s_display.controllers[panel]) {
            bk_display_delete(s_display.controllers[panel]);
            s_display.controllers[panel] = NULL;
        }
    }
    if (bus_owned[0]) {
        gpio_dev_unmap(GPIO_22);
        gpio_dev_unmap(GPIO_23);
        gpio_dev_unmap(GPIO_24);
    }
    if (bus_owned[1]) {
        gpio_dev_unmap(GPIO_2);
        gpio_dev_unmap(GPIO_3);
        gpio_dev_unmap(GPIO_4);
    }
    backlight_close();
    free_frames();
    display_power_close();
    if (s_display.api_users_drained) {
        rtos_deinit_semaphore(&s_display.api_users_drained);
    }
    if (s_display.flush_done) {
        rtos_deinit_semaphore(&s_display.flush_done);
    }
    if (s_display.worker_stopped) {
        rtos_deinit_semaphore(&s_display.worker_stopped);
    }
    if (s_display.command_done) {
        rtos_deinit_semaphore(&s_display.command_done);
    }
    if (s_display.command_queue) {
        rtos_deinit_queue(&s_display.command_queue);
    }
    if (s_display.frame_lock) {
        rtos_deinit_mutex(&s_display.frame_lock);
    }
    if (s_display.api_lock) {
        rtos_deinit_mutex(&s_display.api_lock);
    }
    s_display = (display_manager_t){0};
}

/* The controller serializes first initialization. Keep this mutex alive across restarts. */
static int ensure_lifecycle_lock(void) {
    if (s_lifecycle_lock) {
        return 0;
    }
    return rtos_init_mutex(&s_lifecycle_lock) == BK_OK ? 0 : -1;
}

static bool display_api_acquire(void) {
    bool acquired = false;

    if (!s_lifecycle_lock || rtos_lock_mutex(&s_lifecycle_lock) != BK_OK) {
        return false;
    }
    if (s_lifecycle == DISPLAY_LIFECYCLE_READY) {
        ++s_api_users;
        acquired = true;
    }
    rtos_unlock_mutex(&s_lifecycle_lock);
    return acquired;
}

static void display_api_release(void) {
    bool drained = false;

    if (rtos_lock_mutex(&s_lifecycle_lock) != BK_OK) {
        return;
    }
    if (s_api_users > 0) {
        --s_api_users;
    }
    drained = s_lifecycle == DISPLAY_LIFECYCLE_STOPPING && s_api_users == 0;
    rtos_unlock_mutex(&s_lifecycle_lock);
    if (drained) {
        rtos_set_semaphore(&s_display.api_users_drained);
    }
}

int mybot_display_init(void) {
    if (ensure_lifecycle_lock() < 0 || rtos_lock_mutex(&s_lifecycle_lock) != BK_OK) {
        return -1;
    }
    if (s_lifecycle == DISPLAY_LIFECYCLE_READY) {
        rtos_unlock_mutex(&s_lifecycle_lock);
        return 0;
    }
    if (s_lifecycle != DISPLAY_LIFECYCLE_DOWN) {
        LOGW("initialization already in progress");
        rtos_unlock_mutex(&s_lifecycle_lock);
        return -1;
    }
    s_lifecycle = DISPLAY_LIFECYCLE_STARTING;
    rtos_unlock_mutex(&s_lifecycle_lock);

    LOGI("initializing dual GC9D01 display");
    if (display_power_open() < 0) {
        goto failed;
    }
    if (rtos_init_mutex(&s_display.api_lock) != BK_OK ||
        rtos_init_mutex(&s_display.frame_lock) != BK_OK ||
        rtos_init_semaphore(&s_display.command_done, 1) != BK_OK ||
        rtos_init_semaphore(&s_display.worker_stopped, 1) != BK_OK ||
        rtos_init_semaphore(&s_display.flush_done, DISPLAY_COUNT * 2) != BK_OK ||
        rtos_init_semaphore(&s_display.api_users_drained, 1) != BK_OK ||
        rtos_init_queue(&s_display.command_queue, "mybot_display", sizeof(display_command_t),
                        DISPLAY_QUEUE_DEPTH) != BK_OK) {
        LOGE("failed to create synchronization resources");
        goto failed;
    }
    LOGI("synchronization resources ready");
    if (allocate_frames() < 0) {
        LOGE("failed to allocate dual-screen buffers");
        goto failed;
    }
    LOGI("dual-screen PSRAM frames ready: slots=2 bytes=%u",
         (unsigned int)(DISPLAY_FRAME_SIZE * 2));
    for (unsigned int panel = 0; panel < DISPLAY_COUNT; ++panel) {
        if (bk_display_spi_new(&s_display.controllers[panel], &s_spi_config[panel]) != BK_OK) {
            LOGE("failed to create GC9D01 LCD%u", panel);
            goto failed;
        }
        if (bk_display_open(s_display.controllers[panel]) != BK_OK) {
            LOGE("failed to open GC9D01 LCD%u", panel);
            goto failed;
        }
        s_display.controller_open[panel] = true;
        LOGI("GC9D01 LCD%u opened", panel);
    }

    /* Prime both asynchronous SPI paths with black frames before enabling the backlight. */
    LOGI("priming dual display with black frames");
    if (submit_frame(0, 0) < 0 || submit_frame(1, 0) < 0 ||
        wait_until_frame_available(0) < 0 ||
        backlight_open() < 0) {
        LOGE("failed to prime dual display");
        goto failed;
    }
    s_display.next_frame = 0;
    LOGI("dual display primed");

    if (rtos_create_psram_thread(&s_display.worker, DISPLAY_THREAD_PRIORITY, "mybot_display",
                                 display_worker_main, DISPLAY_THREAD_STACK_SIZE, NULL) != BK_OK) {
        LOGE("failed to create display worker");
        goto failed;
    }
    LOGI("display worker created");
    rtos_lock_mutex(&s_lifecycle_lock);
    s_lifecycle = DISPLAY_LIFECYCLE_READY;
    rtos_unlock_mutex(&s_lifecycle_lock);
    LOGI("dual GC9D01 display ready");
    return 0;

failed:
    LOGE("dual display initialization failed");
    release_resources();
    rtos_lock_mutex(&s_lifecycle_lock);
    s_lifecycle = DISPLAY_LIFECYCLE_DOWN;
    rtos_unlock_mutex(&s_lifecycle_lock);
    return -1;
}

void mybot_display_deinit(void) {
    bool owner = false;
    bool wait_for_users = false;

    if (!s_lifecycle_lock) {
        return;
    }
    LOGI("deinit requested");

    while (!owner) {
        if (rtos_lock_mutex(&s_lifecycle_lock) != BK_OK) {
            return;
        }
        if (s_lifecycle == DISPLAY_LIFECYCLE_READY) {
            s_lifecycle = DISPLAY_LIFECYCLE_STOPPING;
            wait_for_users = s_api_users != 0;
            owner = true;
        }
        if (s_lifecycle == DISPLAY_LIFECYCLE_DOWN) {
            rtos_unlock_mutex(&s_lifecycle_lock);
            return;
        }
        rtos_unlock_mutex(&s_lifecycle_lock);
        if (!owner) {
            rtos_delay_milliseconds(1);
        }
    }

    if (wait_for_users) {
        LOGI("waiting for active display requests");
        rtos_get_semaphore(&s_display.api_users_drained, BEKEN_NEVER_TIMEOUT);
    }

    display_command_t stop = {.type = DISPLAY_COMMAND_STOP};
    LOGI("sending worker stop");
    rtos_push_to_queue_front(&s_display.command_queue, &stop, BEKEN_WAIT_FOREVER);
    rtos_get_semaphore(&s_display.worker_stopped, BEKEN_NEVER_TIMEOUT);
    rtos_thread_join(&s_display.worker);
    s_display.worker = NULL;

    LOGI("releasing display resources");
    release_resources();
    rtos_lock_mutex(&s_lifecycle_lock);
    s_api_users = 0;
    s_lifecycle = DISPLAY_LIFECYCLE_DOWN;
    rtos_unlock_mutex(&s_lifecycle_lock);
    LOGI("dual display deinitialized");
}

bool mybot_display_is_ready(void) {
    bool ready = false;

    if (!s_lifecycle_lock || rtos_lock_mutex(&s_lifecycle_lock) != BK_OK) {
        return false;
    }
    ready = s_lifecycle == DISPLAY_LIFECYCLE_READY;
    rtos_unlock_mutex(&s_lifecycle_lock);
    return ready;
}

static int send_command(const display_command_t *command) {
    display_command_t queued_command;
    int result = -1;

    if (!command || !display_api_acquire()) {
        return -1;
    }
    rtos_lock_mutex(&s_display.api_lock);
    queued_command = *command;
    ++s_display.next_trace_id;
    if (s_display.next_trace_id == 0) {
        ++s_display.next_trace_id;
    }
    queued_command.trace_id = s_display.next_trace_id;
    if (queued_command.type == DISPLAY_COMMAND_PAIR_CODE) {
        LOGD("trace=%u request: state=pair_code",
             (unsigned int)queued_command.trace_id);
    } else {
        LOGD("trace=%u request: state=%s", (unsigned int)queued_command.trace_id,
             screen_name(queued_command.screen));
    }
    if (rtos_push_to_queue(&s_display.command_queue, &queued_command, BEKEN_WAIT_FOREVER) ==
        BK_OK) {
        LOGD("trace=%u enqueued", (unsigned int)queued_command.trace_id);
        rtos_get_semaphore(&s_display.command_done, BEKEN_NEVER_TIMEOUT);
        result = s_display.command_result;
        if (queued_command.type == DISPLAY_COMMAND_PAIR_CODE) {
            LOGD("trace=%u request returned: state=pair_code result=%d",
                 (unsigned int)queued_command.trace_id, result);
        } else {
            LOGD("trace=%u request returned: state=%s result=%d",
                 (unsigned int)queued_command.trace_id,
                 screen_name(queued_command.screen), result);
        }
    } else {
        LOGE("trace=%u command transport failed, type=%d",
             (unsigned int)queued_command.trace_id, queued_command.type);
    }
    rtos_unlock_mutex(&s_display.api_lock);
    display_api_release();
    return result;
}

int mybot_display_show_screen(mybot_display_screen_t screen, uint32_t indicators) {
    if (screen < MYBOT_DISPLAY_SCREEN_STARTING || screen >= MYBOT_DISPLAY_SCREEN_COUNT) {
        return -1;
    }

    display_command_t command = {
        .type = DISPLAY_COMMAND_SCREEN,
        .screen = screen,
        .indicators = indicators,
    };
    return send_command(&command);
}

int mybot_display_show_pair_code(const char *pair_code) {
    if (!pair_code || strlen(pair_code) != 6) {
        return -1;
    }
    for (size_t i = 0; i < 6; ++i) {
        if (pair_code[i] < '0' || pair_code[i] > '9') {
            return -1;
        }
    }

    display_command_t command = {.type = DISPLAY_COMMAND_PAIR_CODE};
    memcpy(command.pair_code, pair_code, sizeof(command.pair_code));
    return send_command(&command);
}
