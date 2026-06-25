#include "time_layer.h"
#include "c/appendix/config.h"
#include "c/appendix/memory_log.h"
#include "c/services/watch_services.h"

// MT = Margin Top
#define MT_TIME 14
#define MT_AM_PM 7
#define MT_TIME_LECO 2
#define MT_AM_PM_LECO 2
#define COLON_UP 2  // raise the time colon by this many px
#define TIME_LEFT_SHIFT 1   // nudge the whole time block left
#define SLEEP_LEFT_SHIFT 1  // nudge the right-aligned sleep column left

// Health metrics flanking the main time (steps left, sleep right)
#define HEALTH_FONT_KEY FONT_KEY_GOTHIC_14
#define HEALTH_SIDE_GAP 4   // gap between the time and each health column
#define HEALTH_BOX_H 18     // line height for the GOTHIC_14 health text
#define HEALTH_VALUE_DY 12  // vertical offset of the value below its label
#define HEALTH_STACK_H 30   // total label+value stack height (used for centering)
#define HEALTH_Y_OFFSET 0   // fine-tune vertical alignment vs the time
#define WALK_DATA_X_SHIFT 3 // nudge the step value right of the calendar edge
#define WALK_LABEL_X_SHIFT 2 // nudge the "Walk" label right of the calendar edge
#define HEALTH_DATA_COLOR PBL_IF_COLOR_ELSE(GColorOrange, GColorWhite)

static TextLayer *s_container_layer;
static TextLayer *s_time_layer;   // hours
static TextLayer *s_colon_layer;  // ":" (drawn raised)
static TextLayer *s_min_layer;    // minutes
static TextLayer *s_am_pm_layer;
static TextLayer *s_steps_layer;
static TextLayer *s_sleep_layer;
static TextLayer *s_walk_label_layer;
static TextLayer *s_sleep_label_layer;
static char s_steps_buf[8];
static char s_sleep_buf[8];

static void health_refresh() {
    const time_t start = time_start_of_today();
    const time_t now = time(NULL);

    if (health_service_metric_accessible(HealthMetricStepCount, start, now) & HealthServiceAccessibilityMaskAvailable) {
        snprintf(s_steps_buf, sizeof(s_steps_buf), "%d", (int)health_service_sum_today(HealthMetricStepCount));
    } else {
        snprintf(s_steps_buf, sizeof(s_steps_buf), "-");
    }
    text_layer_set_text(s_steps_layer, s_steps_buf);

    if (health_service_metric_accessible(HealthMetricSleepSeconds, start, now) & HealthServiceAccessibilityMaskAvailable) {
        const int secs = (int)health_service_sum_today(HealthMetricSleepSeconds);
        snprintf(s_sleep_buf, sizeof(s_sleep_buf), "%dh%02dm", secs / 3600, (secs % 3600) / 60);
    } else {
        snprintf(s_sleep_buf, sizeof(s_sleep_buf), "-");
    }
    text_layer_set_text(s_sleep_layer, s_sleep_buf);
}

void time_layer_create(Layer* parent_layer, GRect frame) {
    s_container_layer = text_layer_create(frame);
    s_time_layer = text_layer_create(GRect(0, 0, frame.size.w, frame.size.h));
    s_colon_layer = text_layer_create(GRect(0, 0, frame.size.w, frame.size.h));
    s_min_layer = text_layer_create(GRect(0, 0, frame.size.w, frame.size.h));
    s_am_pm_layer = text_layer_create(GRect(0, 0, 30, frame.size.h));

    text_layer_set_background_color(s_container_layer, GColorClear);

    // Main time split into hours / colon / minutes so the colon can be raised.
    text_layer_set_background_color(s_time_layer, GColorClear);
    text_layer_set_text(s_time_layer, "00");
    text_layer_set_text_alignment(s_time_layer, GTextAlignmentLeft);
    text_layer_set_background_color(s_colon_layer, GColorClear);
    text_layer_set_text(s_colon_layer, ":");
    text_layer_set_text_alignment(s_colon_layer, GTextAlignmentLeft);
    text_layer_set_background_color(s_min_layer, GColorClear);
    text_layer_set_text(s_min_layer, "00");
    text_layer_set_text_alignment(s_min_layer, GTextAlignmentLeft);

    // AM/PM formatting
    text_layer_set_font(s_am_pm_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18));
    text_layer_set_background_color(s_am_pm_layer, GColorClear);
    text_layer_set_text_color(s_am_pm_layer, GColorWhite);
    text_layer_set_text(s_am_pm_layer, "PM");
    text_layer_set_text_alignment(s_am_pm_layer, GTextAlignmentLeft);

    // Steps (left of time) and sleep (right of time), smallest font
    s_steps_layer = text_layer_create(GRect(0, 0, 40, HEALTH_BOX_H));
    text_layer_set_background_color(s_steps_layer, GColorClear);
    text_layer_set_text_color(s_steps_layer, HEALTH_DATA_COLOR);
    text_layer_set_font(s_steps_layer, fonts_get_system_font(HEALTH_FONT_KEY));
    text_layer_set_text_alignment(s_steps_layer, GTextAlignmentLeft);

    s_sleep_layer = text_layer_create(GRect(0, 0, 40, HEALTH_BOX_H));
    text_layer_set_background_color(s_sleep_layer, GColorClear);
    text_layer_set_text_color(s_sleep_layer, HEALTH_DATA_COLOR);
    text_layer_set_font(s_sleep_layer, fonts_get_system_font(HEALTH_FONT_KEY));
    text_layer_set_text_alignment(s_sleep_layer, GTextAlignmentRight);

    // "Walk" / "Sleep" labels above their values
    s_walk_label_layer = text_layer_create(GRect(0, 0, 40, HEALTH_BOX_H));
    text_layer_set_background_color(s_walk_label_layer, GColorClear);
    text_layer_set_text_color(s_walk_label_layer, GColorWhite);
    text_layer_set_font(s_walk_label_layer, fonts_get_system_font(HEALTH_FONT_KEY));
    text_layer_set_text_alignment(s_walk_label_layer, GTextAlignmentLeft);
    text_layer_set_text(s_walk_label_layer, "Walk");

    s_sleep_label_layer = text_layer_create(GRect(0, 0, 40, HEALTH_BOX_H));
    text_layer_set_background_color(s_sleep_label_layer, GColorClear);
    text_layer_set_text_color(s_sleep_label_layer, GColorWhite);
    text_layer_set_font(s_sleep_label_layer, fonts_get_system_font(HEALTH_FONT_KEY));
    text_layer_set_text_alignment(s_sleep_label_layer, GTextAlignmentRight);
    text_layer_set_text(s_sleep_label_layer, "Sleep");

    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_time_layer));
    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_colon_layer));
    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_min_layer));
    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_am_pm_layer));
    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_steps_layer));
    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_sleep_layer));
    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_walk_label_layer));
    layer_add_child(text_layer_get_layer(s_container_layer), text_layer_get_layer(s_sleep_label_layer));
    layer_add_child(parent_layer, text_layer_get_layer(s_container_layer));
    MEMORY_LOG_HEAP("after_time_layer_create");

}

// 12:30 -> 12:30
// 13:30 -> 1:30
// 00:30 -> 12:30

static void text_layer_move_frame(TextLayer *text_layer, GRect frame) {
    layer_set_frame(text_layer_get_layer(text_layer), frame);
}

void time_layer_tick() {
    // Get a tm structure
    struct tm tick_time = watch_services_localtime();

    // Format the time into a buffer
    static char s_buffer[8];
    config_format_time(s_buffer, 8, &tick_time);

    // Split "H:MM" into hours / minutes (colon drawn separately so it can be raised)
    static char s_hh[6];
    static char s_mm[4];
    int ci = 0;
    while (s_buffer[ci] && s_buffer[ci] != ':') ci++;
    int hl = ci < (int)sizeof(s_hh) - 1 ? ci : (int)sizeof(s_hh) - 1;
    memcpy(s_hh, s_buffer, hl);
    s_hh[hl] = '\0';
    snprintf(s_mm, sizeof(s_mm), "%s", s_buffer[ci] == ':' ? s_buffer + ci + 1 : "");

    text_layer_set_text(s_time_layer, s_hh);
    text_layer_set_text(s_min_layer, s_mm);
    if (g_config->show_am_pm)
        text_layer_set_text(s_am_pm_layer, tick_time.tm_hour < 12 ? "AM" : "PM");

    // Reset frames wide, then measure each part
    GRect bounds = layer_get_bounds(text_layer_get_layer(s_container_layer));
    const GRect measure_box = GRect(0, 0, bounds.size.w, bounds.size.h);
    text_layer_move_frame(s_time_layer, measure_box);
    text_layer_move_frame(s_colon_layer, measure_box);
    text_layer_move_frame(s_min_layer, measure_box);
    GSize hh_size = text_layer_get_content_size(s_time_layer);
    GSize colon_size = text_layer_get_content_size(s_colon_layer);
    GSize mm_size = text_layer_get_content_size(s_min_layer);
    GSize am_pm_size = text_layer_get_content_size(s_am_pm_layer);

    // Calculate some landmarks
    int time_w = hh_size.w + colon_size.w + mm_size.w;
    int time_h = hh_size.h;
    int content_w = time_w + (g_config->show_am_pm ? am_pm_size.w : 0);
    int text_h = time_h - MT_TIME; // Remove top margin, approximately
    int text_top = -MT_TIME + (bounds.size.h/2 - text_h/2);
    int text_left = bounds.size.w / 2 - content_w / 2;

    // emery: nudge LECO time text upward slightly to keep optical centering.
#ifdef PBL_PLATFORM_EMERY
    if (g_config->time_font == TIME_FONT_LECO) {
        text_top -= MT_TIME_LECO;
    }
#endif

    // Position hours, colon (raised), minutes left-to-right (nudged left)
    int x = text_left - TIME_LEFT_SHIFT;
    text_layer_move_frame(s_time_layer, GRect(x, text_top, hh_size.w + 2, time_h));
    x += hh_size.w;
    text_layer_move_frame(s_colon_layer, GRect(x, text_top - COLON_UP, colon_size.w + 2, time_h));
    x += colon_size.w;
    text_layer_move_frame(s_min_layer, GRect(x, text_top, mm_size.w + 2, time_h));

    if (g_config->show_am_pm) {
        int am_pm_y = text_top + MT_TIME - MT_AM_PM;
        // emery: nudge LECO AM/PM down slightly to align with larger time numerals.
#ifdef PBL_PLATFORM_EMERY
        if (g_config->time_font == TIME_FONT_LECO) {
            am_pm_y += MT_AM_PM_LECO;
        }
#endif
        text_layer_move_frame(s_am_pm_layer, GRect(text_left - TIME_LEFT_SHIFT + time_w, am_pm_y, 30, time_h));
    }
    layer_set_hidden(text_layer_get_layer(s_am_pm_layer), !g_config->show_am_pm);

    // Update and position the flanking health columns (label over value),
    // centered vertically against the time, in the gaps beside it.
    health_refresh();
    const int label_y = bounds.size.h / 2 - HEALTH_STACK_H / 2 + HEALTH_Y_OFFSET;
    const int value_y = label_y + HEALTH_VALUE_DY;
    const int steps_w = text_left - HEALTH_SIDE_GAP;
    const int left_w = steps_w > 0 ? steps_w : 0;
    const int walk_label_x = left_w > WALK_LABEL_X_SHIFT ? WALK_LABEL_X_SHIFT : 0;
    text_layer_move_frame(s_walk_label_layer, GRect(walk_label_x, label_y, left_w - walk_label_x, HEALTH_BOX_H));
    const int steps_x = left_w > WALK_DATA_X_SHIFT ? WALK_DATA_X_SHIFT : 0;
    text_layer_move_frame(s_steps_layer, GRect(steps_x, value_y, left_w - steps_x, HEALTH_BOX_H));
    const int sleep_x = text_left + content_w + HEALTH_SIDE_GAP;
    const int sleep_w = bounds.size.w - sleep_x;
    const int right_w = sleep_w > 0 ? sleep_w : 0;
    const int sleep_right_w = right_w > SLEEP_LEFT_SHIFT ? right_w - SLEEP_LEFT_SHIFT : right_w;
    text_layer_move_frame(s_sleep_label_layer, GRect(sleep_x, label_y, sleep_right_w, HEALTH_BOX_H));
    text_layer_move_frame(s_sleep_layer, GRect(sleep_x, value_y, sleep_right_w, HEALTH_BOX_H));
}

void time_layer_refresh() {
    GFont time_font = config_time_font();
    GColor time_color = PBL_IF_COLOR_ELSE(g_config->color_time, GColorWhite);
    text_layer_set_font(s_time_layer, time_font);
    text_layer_set_font(s_colon_layer, time_font);
    text_layer_set_font(s_min_layer, time_font);
    text_layer_set_text_color(s_time_layer, time_color);
    text_layer_set_text_color(s_colon_layer, time_color);
    text_layer_set_text_color(s_min_layer, time_color);
    time_layer_tick();  // Update main time text and layer positions
}

void time_layer_destroy() {
    MEMORY_LOG_HEAP("time_layer_destroy:before");
    text_layer_destroy(s_steps_layer);
    text_layer_destroy(s_sleep_layer);
    text_layer_destroy(s_walk_label_layer);
    text_layer_destroy(s_sleep_label_layer);
    text_layer_destroy(s_time_layer);
    text_layer_destroy(s_colon_layer);
    text_layer_destroy(s_min_layer);
    text_layer_destroy(s_container_layer);
    MEMORY_LOG_HEAP("time_layer_destroy:after");
}
