#include "battery_layer.h"
#include "c/appendix/persist.h"
#include "c/appendix/memory_log.h"
#include "c/services/watch_services.h"

#define BATTERY_NUB_W 2
#define BATTERY_NUB_H 6
#define BATTERY_STROKE 1
#define FILL_PADDING 1
#define ICON_SPACING 3
#define BATTERY_POWER_ICON_W 7

// The battery glyph (charging icon + outline + nub) keeps its original size and
// hugs the right edge of the layer; the percentage number is drawn to its left.
#define BATTERY_GLYPH_W 29
#define BATTERY_GLYPH_H 10
#define PCT_TEXT_GAP 2
#define PCT_TEXT_Y_NUDGE 5
#define BATTERY_PCT_FONT_KEY FONT_KEY_GOTHIC_14


static Layer *s_battery_layer;
static GBitmap *s_battery_power_bitmap;
static GColor s_battery_palette[2];
static bool s_battery_subscribed;

static void battery_state_handler(BatteryChargeState charge) {
    battery_layer_refresh();
}

#ifdef PBL_COLOR
static GColor get_battery_color(int level) {
    if (level >= 50)
        return GColorGreen;
    else if (level >= 30)
        return GColorYellow;
    else
        return GColorRed;
}
#endif

static void ensure_battery_power_bitmap_loaded(void) {
    if (!s_battery_power_bitmap) {
        s_battery_power_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_BATTERY_CHARGING);
        s_battery_palette[0] = GColorWhite;
        s_battery_palette[1] = GColorClear;
        gbitmap_set_palette(s_battery_power_bitmap, s_battery_palette, false);
    }
}

static void maybe_unload_battery_power_bitmap(bool show_power_icon) {
    if (!show_power_icon && s_battery_power_bitmap) {
        gbitmap_destroy(s_battery_power_bitmap);
        s_battery_power_bitmap = NULL;
    }
}

static void draw_power_icon(GContext *ctx, int base_x, int region_top, int region_h, GBitmap *icon_bitmap) {
    GRect icon_bounds = gbitmap_get_bounds(icon_bitmap);
    int icon_x = base_x;
    int icon_y = region_top + (region_h - icon_bounds.size.h) / 2;

    graphics_context_set_compositing_mode(ctx, GCompOpSet);
    graphics_draw_bitmap_in_rect(
        ctx,
        icon_bitmap,
        GRect(icon_x, icon_y, icon_bounds.size.w, icon_bounds.size.h));
    graphics_context_set_compositing_mode(ctx, GCompOpAssign);
}

static void battery_update_proc(Layer *layer, GContext *ctx) {
    GRect bounds = layer_get_bounds(layer);
    int w = bounds.size.w;
    int h = bounds.size.h;
    BatteryChargeState battery_state = watch_services_battery_state();
    int battery_level = battery_state.charge_percent;
    bool show_power_icon = battery_state.is_charging || battery_state.is_plugged;

    maybe_unload_battery_power_bitmap(show_power_icon);

    // Battery glyph hugs the right edge; the percentage number goes to its left.
    int glyph_left = w - BATTERY_GLYPH_W;
    int glyph_top = (h - BATTERY_GLYPH_H) / 2;

    // Draw the battery percentage, right-aligned just left of the glyph
    char pct_buffer[6];
    snprintf(pct_buffer, sizeof(pct_buffer), "%d%%", battery_level);
    graphics_context_set_text_color(ctx, GColorWhite);
    graphics_draw_text(
        ctx, pct_buffer, fonts_get_system_font(BATTERY_PCT_FONT_KEY),
        GRect(0, glyph_top - PCT_TEXT_Y_NUDGE, glyph_left - PCT_TEXT_GAP, BATTERY_GLYPH_H + PCT_TEXT_Y_NUDGE * 2),
        GTextOverflowModeFill, GTextAlignmentRight, NULL);

    int battery_x = glyph_left + BATTERY_POWER_ICON_W + ICON_SPACING;
    int battery_total_w = BATTERY_GLYPH_W - (BATTERY_POWER_ICON_W + ICON_SPACING);
    int battery_w = battery_total_w - BATTERY_NUB_W;

    // Fill the battery level
    GRect color_bounds = GRect(
        battery_x + BATTERY_STROKE + FILL_PADDING, glyph_top + BATTERY_STROKE + FILL_PADDING,
        battery_w - (BATTERY_STROKE + FILL_PADDING) * 2, BATTERY_GLYPH_H - (BATTERY_STROKE + FILL_PADDING) * 2);
    GRect color_area = GRect(
        color_bounds.origin.x, color_bounds.origin.y,
        color_bounds.size.w * (battery_level + 10) / 110, color_bounds.size.h);
#ifdef PBL_COLOR
    graphics_context_set_fill_color(ctx, get_battery_color(battery_level));
#else
    graphics_context_set_fill_color(ctx, GColorWhite);
#endif
    graphics_fill_rect(ctx, color_area, 0, GCornerNone);

    if (show_power_icon) {
        ensure_battery_power_bitmap_loaded();
        draw_power_icon(ctx, glyph_left, glyph_top, BATTERY_GLYPH_H, s_battery_power_bitmap);
    }

    // Draw the white battery outline
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_context_set_stroke_width(ctx, BATTERY_STROKE);
    graphics_draw_rect(ctx, GRect(battery_x, glyph_top, battery_w, BATTERY_GLYPH_H));

    // Draw the battery nub on the right
    graphics_draw_rect(
        ctx,
        GRect(battery_x + battery_w - 1, glyph_top + BATTERY_GLYPH_H / 2 - BATTERY_NUB_H / 2, BATTERY_NUB_W + 1, BATTERY_NUB_H));
}

void battery_layer_create(Layer* parent_layer, GRect frame) {
    MemoryHeapProbe probe = MEMORY_HEAP_PROBE_START("battery_layer_create");

    s_battery_layer = layer_create(frame);
    MEMORY_HEAP_PROBE_SAMPLE("after_layer_create", &probe);

    layer_set_update_proc(s_battery_layer, battery_update_proc);
    if (!watch_services_battery_is_fixture()) {
        battery_state_service_subscribe(battery_state_handler);
        s_battery_subscribed = true;
    } else {
        s_battery_subscribed = false;
    }
    MEMORY_HEAP_PROBE_SAMPLE("after_battery_subscribe", &probe);
    layer_add_child(parent_layer, s_battery_layer);
    MEMORY_HEAP_PROBE_SAMPLE("after_layer_add_child", &probe);
    MEMORY_LOG_HEAP("after_battery_layer_create");
    MEMORY_HEAP_PROBE_LOG_MIN(&probe);
}

void battery_layer_refresh() {
    layer_mark_dirty(s_battery_layer);
}

void battery_layer_destroy() {
    MEMORY_LOG_HEAP("battery_layer_destroy:before");
    if (s_battery_subscribed) {
        battery_state_service_unsubscribe();
        s_battery_subscribed = false;
    }
    if (s_battery_power_bitmap) {
        gbitmap_destroy(s_battery_power_bitmap);
        s_battery_power_bitmap = NULL;
    }
    layer_destroy(s_battery_layer);
    MEMORY_LOG_HEAP("battery_layer_destroy:after");
}
