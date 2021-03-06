#include <pebble.h>
#include "multi_alarm_layer.h"

#define MENU_NUM_SECTIONS           (2)    // 0:time, 1:setting
#define MENU_SECTION_INDEX_TIME     (0)
#define MENU_SECTION_INDEX_SETTING  (1)
#define MENU_NUM_ROWS_OF_SETTING    (1)
#define MENU_HEADER_HIGHT_TIME      (0)
#define MENU_HEADER_HIGHT_SETTING   (0)
#define MENU_CELL_HIGHT_TIME        (30)
#define MENU_CELL_HIGHT_SETTING     (30)

#define MENU_BOUNDS_SIZE_WIDTH      (50) 

#define INFO_ALARM_ORIGIN_X         (4)
#define INFO_TIME_MARGIN_X          (4)
#define INFO_TIME_MARGIN_Y          (4)
    
typedef struct multi_alarm_layer {
    MenuLayer *menu_layer;
    Layer *info_layer;
    Layer *abouttime_layer;
    MultiAlarmData *data;
    MultiAlarmLayerClickCallback select_handler;
    MultiAlarmLayerClickCallback long_select_handler;
}MultiAlarmLayer;

#define ICON_KEY_BELL_WHITE         (0)
#define ICON_KEY_BELL_BLACK         (1)
#define MAX_ICONS                   (2)

static struct {
    GBitmap *bitmap;
    const uint32_t resource_id;
} s_icons[MAX_ICONS] = {
    {NULL, RESOURCE_ID_BELL_WHITE},
    {NULL, RESOURCE_ID_BELL_BLACK}
};

static uint16_t s_menu_get_num_sections_callback(struct MenuLayer *menu_layer, void *callback_context);
static uint16_t s_menu_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static int16_t s_menu_get_cell_hight_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static int16_t s_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context);
static void s_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context);
static void s_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context);
static void s_menu_select_click_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void s_menu_select_long_click_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context);
static void s_menu_changed_callback(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context);
static void s_menu_data_time_update(const Layer *layer, GContext *ctx, MultiAlarmLayer *malarm, uint16_t index);
static void s_menu_data_alarm_update(const Layer *layer, GContext *ctx, MultiAlarmLayer *malarm, uint16_t index);
static void s_info_update(struct Layer *layer, GContext *ctx);
static void s_info_abouttime_update(struct Layer *layer, GContext *ctx);

static bool s_icons_init();
static void s_icons_finalize();

MultiAlarmLayer *multi_alarm_layer_create(GRect frame) {
    MultiAlarmLayer *malarm = NULL;
    
    malarm = calloc(sizeof(MultiAlarmLayer), 1);
    if (malarm != NULL) {
        // menu layer
        malarm->menu_layer = menu_layer_create((GRect){.origin = {0, 0}, .size = {MENU_BOUNDS_SIZE_WIDTH, frame.size.h}});
        if (malarm->menu_layer != NULL) {
            scroll_layer_set_shadow_hidden(menu_layer_get_scroll_layer(malarm->menu_layer), true);

            menu_layer_set_callbacks(
                malarm->menu_layer,
                (void*)malarm,
                (MenuLayerCallbacks){
                    .get_num_sections  = s_menu_get_num_sections_callback,
                    .get_num_rows      = s_menu_get_num_rows_callback,
                    .get_cell_height   = s_menu_get_cell_hight_callback,
                    .get_header_height = s_menu_get_header_height_callback,
                    .draw_row          = s_menu_draw_row_callback,
                    .draw_header       = s_menu_draw_header_callback,
                    .select_click      = s_menu_select_click_callback,
                    .select_long_click = s_menu_select_long_click_callback,
                    .selection_changed = s_menu_changed_callback,});
            
            // info layer
            malarm->info_layer = layer_create_with_data(
                (GRect){
                    .origin = {MENU_BOUNDS_SIZE_WIDTH, 0},
                    .size = {frame.size.w - MENU_BOUNDS_SIZE_WIDTH, frame.size.h}},
                sizeof(MultiAlarmLayer*));
            if (malarm->info_layer != NULL) {
                *((MultiAlarmLayer**)layer_get_data(malarm->info_layer)) = malarm;
                layer_set_update_proc(malarm->info_layer, s_info_update);
                
                // abouttime layer
                malarm->abouttime_layer = layer_create_with_data(
                    (GRect){
                        .origin = {0, (frame.size.h / 2) - (28 / 2) - 5},
                        .size ={(frame.size.w - MENU_BOUNDS_SIZE_WIDTH) - 7, 28}},
                    sizeof(MultiAlarmLayer*));
                if (malarm->abouttime_layer != NULL) {
                    *((MultiAlarmLayer**)layer_get_data(malarm->abouttime_layer)) = malarm;
                    layer_set_update_proc(malarm->abouttime_layer, s_info_abouttime_update);

                    layer_add_child(malarm->info_layer, malarm->abouttime_layer);
                    
                    // icons
                    if (s_icons_init() == false) {
                        multi_alarm_layer_destroy(malarm);
                        malarm = NULL;
                    }
                } else {
                    multi_alarm_layer_destroy(malarm);
                    malarm = NULL;
                }
            } else {
                multi_alarm_layer_destroy(malarm);
                malarm = NULL;
            }
        } else {
            multi_alarm_layer_destroy(malarm);
            malarm = NULL;
        }
    }
    return malarm;
}

void multi_alarm_layer_destroy(MultiAlarmLayer *malarm) {
    if (malarm != NULL) {
        s_icons_finalize();
        if (malarm->abouttime_layer != NULL) {
            layer_destroy(malarm->abouttime_layer);
        }
        if (malarm->info_layer != NULL) {
            layer_destroy(malarm->info_layer);
        }
        if (malarm->menu_layer != NULL) {
            menu_layer_destroy(malarm->menu_layer);
        }
        free(malarm);
    }
}

void multi_alarm_layer_set_click_config_onto_window(MultiAlarmLayer *malarm, Window *window) {
    menu_layer_set_click_config_onto_window(malarm->menu_layer, window);
}

void multi_alarm_layer_add_child_to_layer(MultiAlarmLayer *malarm, Layer *layer) {
    layer_add_child(layer, menu_layer_get_layer(malarm->menu_layer));
    layer_add_child(layer, malarm->info_layer);
}

void multi_alarm_layer_mark_dirty(MultiAlarmLayer *malarm) {
    layer_mark_dirty(menu_layer_get_layer(malarm->menu_layer));
}

void multi_alarm_layer_set_data_pointer(MultiAlarmLayer *malarm, MultiAlarmData *data) {
    malarm->data = data;
    menu_layer_reload_data(malarm->menu_layer);
    menu_layer_set_selected_index(malarm->menu_layer, (MenuIndex){0, 0}, MenuRowAlignCenter, true);
}

void multi_alarm_layer_set_data_index(MultiAlarmLayer *malarm, index_t index) {
    if (index == MA_INDEX_NEAR_NOW_TIME) {
         index = multi_alarm_data_get_index_near_now(malarm->data);
    } else {
        if (multi_alarm_data_get_num_usable(malarm->data) <= index) {
            index = 0;
        }
    }
    menu_layer_set_selected_index(malarm->menu_layer, (MenuIndex){0, index}, MenuRowAlignCenter, true);
}

void multi_alarm_layer_update_abouttime(MultiAlarmLayer *malarm) {
    MenuIndex cell_index = menu_layer_get_selected_index(malarm->menu_layer);

    if (cell_index.section == MENU_SECTION_INDEX_TIME) {
        layer_mark_dirty(malarm->abouttime_layer);
    }
}

void multi_alarm_layer_select_click_subscribe(MultiAlarmLayer *malarm, MultiAlarmLayerClickCallback handler) {
    malarm->select_handler = handler;
}

void multi_alarm_layer_select_long_click_subscribe(MultiAlarmLayer *malarm, MultiAlarmLayerClickCallback handler) {
    malarm->long_select_handler = handler;
}

static uint16_t s_menu_get_num_sections_callback(struct MenuLayer *menu_layer, void *callback_context) {
    (void)menu_layer;
    (void)callback_context;

    return MENU_NUM_SECTIONS;
}

static uint16_t s_menu_get_num_rows_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
    (void)menu_layer;
    MultiAlarmLayer *malarm = (MultiAlarmLayer*)callback_context;
    uint16_t num_rows[] = {
        multi_alarm_data_get_num_usable(malarm->data),
        MENU_NUM_ROWS_OF_SETTING
    };
    
    return num_rows[section_index];
}

static int16_t s_menu_get_cell_hight_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    (void)menu_layer;
    (void)cell_index;
    (void)callback_context;
    uint16_t hight[] = {
        MENU_CELL_HIGHT_TIME,
        MENU_CELL_HIGHT_SETTING
    };
    
    return hight[cell_index->section];    
}

static int16_t s_menu_get_header_height_callback(struct MenuLayer *menu_layer, uint16_t section_index, void *callback_context) {
    (void)menu_layer;
    (void)section_index;
    (void)callback_context;
    uint16_t hight[] = {
        MENU_HEADER_HIGHT_TIME,
        MENU_HEADER_HIGHT_SETTING
    };

    return hight[section_index];  
}

static void s_menu_draw_row_callback(GContext *ctx, const Layer *cell_layer, MenuIndex *cell_index, void *callback_context) {
    MultiAlarmLayer *malarm = (MultiAlarmLayer*)callback_context;

    if (cell_index->section == MENU_SECTION_INDEX_TIME) {
        uint16_t index = cell_index->row;

        s_menu_data_time_update(cell_layer, ctx, malarm, index);
        s_menu_data_alarm_update(cell_layer, ctx, malarm, index);
    } else {
        GPoint center = (GPoint){MENU_BOUNDS_SIZE_WIDTH / 2, MENU_CELL_HIGHT_SETTING / 2};

        graphics_context_set_stroke_color(ctx, GColorBlack);
        graphics_draw_line(ctx,
                           (GPoint){center.x, center.y - 4},
                           (GPoint){center.x, center.y + 4});
        graphics_draw_line(ctx,
                           (GPoint){center.x - 4, center.y},
                           (GPoint){center.x + 4, center.y});
    }
}

static void s_menu_draw_header_callback(GContext *ctx, const Layer *cell_layer, uint16_t section_index, void *callback_context) {
    (void)ctx;
    (void)cell_layer;
    (void)section_index;
    (void)callback_context;
}

static void s_menu_select_click_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    (void)menu_layer;
    MultiAlarmLayer *malarm = (MultiAlarmLayer*)callback_context;

    if (malarm->select_handler != NULL) {
        if (cell_index->section == MENU_SECTION_INDEX_TIME) {
            (*malarm->select_handler)(malarm->data, cell_index->row);
        } else {
            (*malarm->select_handler)(malarm->data, MA_INDEX_INVALID);
        }
    }
}

static void s_menu_select_long_click_callback(struct MenuLayer *menu_layer, MenuIndex *cell_index, void *callback_context) {
    (void)menu_layer;
    MultiAlarmLayer *malarm = (MultiAlarmLayer*)callback_context;

    if (malarm->long_select_handler != NULL) {
        if (cell_index->section == MENU_SECTION_INDEX_TIME) {
            (*malarm->long_select_handler)(malarm->data, cell_index->row);
        } else {
            (*malarm->long_select_handler)(malarm->data, MA_INDEX_INVALID);
        }
    }
}

static void s_menu_changed_callback(struct MenuLayer *menu_layer, MenuIndex new_index, MenuIndex old_index, void *callback_context) {
    (void)menu_layer;
    (void)new_index;
    (void)old_index;
    MultiAlarmLayer *malarm = (MultiAlarmLayer*)callback_context;

    if (menu_index_compare(&new_index, &old_index) != 0) {
        layer_mark_dirty(malarm->info_layer);
    }
}

static void s_menu_data_time_update(const Layer *layer, GContext *ctx, MultiAlarmLayer *malarm, uint16_t index) {
    (void)layer;
    MATime time;
    char str[6];

    if (multi_alarm_data_get_MATime(malarm->data, index, &time) == 0) {
        snprintf(str, 6, "%2d:%02d", time.hour, time.min);
    
        graphics_context_set_text_color(ctx, GColorBlack);
        graphics_draw_text(
            ctx,
            str,
            fonts_get_system_font(FONT_KEY_GOTHIC_18),
            (GRect){
                .origin = {0, (MENU_CELL_HIGHT_TIME / 2) - (18 / 2) - INFO_TIME_MARGIN_Y},
                .size = {MENU_BOUNDS_SIZE_WIDTH - INFO_TIME_MARGIN_X, 18}},
            GTextOverflowModeWordWrap,
            GTextAlignmentRight,
            NULL);
    }
}

static void s_menu_data_alarm_update(const Layer *layer, GContext *ctx, MultiAlarmLayer *malarm, uint16_t index) {
    bool enable;

    if (multi_alarm_data_get_alarm_enable(malarm->data, index, &enable) == 0) {
        int key = enable == true ? ICON_KEY_BELL_BLACK : ICON_KEY_BELL_WHITE;
    
        graphics_draw_bitmap_in_rect(
            ctx,
            s_icons[key].bitmap,
            (GRect){
                .origin = {INFO_ALARM_ORIGIN_X, (MENU_CELL_HIGHT_TIME / 2) - (gbitmap_get_bounds(s_icons[key].bitmap).size.h / 2)},
                .size = gbitmap_get_bounds(s_icons[key].bitmap).size});
    }
}

static void s_info_update(struct Layer *layer, GContext *ctx) {
    // background
    graphics_context_set_fill_color(ctx, GColorBlack);
    graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);
}

static void s_info_abouttime_update(struct Layer *layer, GContext *ctx) {
    MultiAlarmLayer *malarm = *((MultiAlarmLayer**)layer_get_data(layer));
    MenuIndex cell_index = menu_layer_get_selected_index(malarm->menu_layer);

    if (cell_index.section == MENU_SECTION_INDEX_TIME) {
        uint16_t index = cell_index.row;

        // calc different between now to the selected time
        time_t set_time;
        if (multi_alarm_data_get_time_t_of_after24h(malarm->data, index, &set_time) == 0) {
            uint32_t diff_time = (uint32_t)(set_time - time(NULL));
    
            // draw
            char str[16];
            snprintf(str,
                     16,
                     "%2d:%02d:%02d",
                     (int)(diff_time / (60 * 60)),
                     (int)((diff_time / 60) % 60),
                     (int)(diff_time % 60));
    
            graphics_context_set_text_color(ctx, GColorWhite);
            graphics_draw_text(
                ctx,
                str,
                fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD),
                layer_get_bounds(layer),
                GTextOverflowModeWordWrap,
                GTextAlignmentRight,
                NULL);
        }
    }
}

static bool s_icons_init() {
    bool ret = true;
    
    for (int key = 0; key < MAX_ICONS; key++) {
        s_icons[key].bitmap = gbitmap_create_with_resource(s_icons[key].resource_id);
        if (s_icons[key].bitmap == NULL) {
            ret = false;
            break;            
        }
    }
    return ret;
}

static void s_icons_finalize() {
    for (int key = 0; key < MAX_ICONS; key++) {
        if (s_icons[key].bitmap != NULL) {
            gbitmap_destroy(s_icons[key].bitmap);
            s_icons[key].bitmap = NULL;
        }
    }
}