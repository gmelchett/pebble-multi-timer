#include <pebble.h>

// Persistent storage keys
#define KEY_NUM_TIMERS              1
#define KEY_SELECTED_MENU_SECTION   2
#define KEY_SELECTED_MENU_ROW       3
#define KEY_SHUTDOWN_TIME           4
#define KEY_VERSION                 5
#define KEY_FIRST_TIMER           100

// AppMessage keys
#define KEY_COMMAND             200
#define KEY_TIMELINE_ID         201
#define KEY_TIMELINE_TIME       202
#define KEY_TIMELINE_TITLE      203

#define COMMAND_ADD_TO_TIMELINE         0
#define COMMAND_REMOVE_FROM_TIMELINE    1

#define BITMAP_W 12
#define BITMAP_H 12
#define BITMAP_PAD 3
#define ICON_BITMAP_SIZE 28

#define SECTION_NEW_TIMER       0
#define SECTION_TIMERS          1
#define SECTION_NEW_STOPWATCH   2
#define SECTION_STOPWATCHES     3
#define NUM_MENU_SECTIONS       4

#define MAX_TIMERS 10

#define NEW_TIMER       -1
#define NEW_STOPWATCH   -2

#define TYPE_TIMER      0
#define TYPE_STOPWATCH  1

static struct
{
    uint32_t total_sec;     // Seconds in timer
    uint32_t elapsed_sec;   // Seconds elapsed
    uint32_t alert_sec;     // Alert duration in seconds
    int iconIdx;            // Index into icon_ arrays
    int vibeIdx;            // Index into vibe_ arrays
    int vibeRepeat;
    bool isRunning;         // True iff timer is running
    bool isCountingUp;      // Stopwatch if true
} timers[MAX_TIMERS];

static int cur_timer;

static Window *window = NULL;
static MenuLayer *s_menu_layer = NULL;
static int num_timers = 3;
static StatusBarLayer *s_status_bar, *s_timer_status_bar;
static Layer *s_battery_layer, *s_timer_battery_layer;
static Layer *s_indicator_up_layer, *s_indicator_down_layer;
static ContentIndicator *s_indicator;
static ContentIndicatorConfig s_up_config, s_down_config;
#ifdef PBL_COLOR
static const int CompOp = GCompOpSet;
#else
static const int CompOp = GCompOpAssign;
#endif

#define TIMER_ICON_ITEMS 52
#define MAX_TIMER_CACHED_ICONS MAX_TIMERS

static char *timer_icon_labels[TIMER_ICON_ITEMS];
static int timer_icon_bitmap_resids[TIMER_ICON_ITEMS];

#define MAX_TIMER_CACHED_ICONS MAX_TIMERS

typedef struct
{
    int iconIdx;
    GBitmap *bmp;
} TimerIconCacheItem;

static TimerIconCacheItem timer_icon_cache[MAX_TIMER_CACHED_ICONS];
static int cachedNum = 0;

static GBitmap *timer_icon_cache_get(int iconIdx)
{
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_get(%d) cachedNum %d", iconIdx, cachedNum);

    for (int i = 0; i < cachedNum && i < MAX_TIMER_CACHED_ICONS; i++)
    {
        if (timer_icon_cache[i].iconIdx == iconIdx)
        {
            //APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_get(%d) foundAt:%d", iconIdx, i);
            return timer_icon_cache[i].bmp;
        }
    }

    if (cachedNum < MAX_TIMER_CACHED_ICONS)
    {
        timer_icon_cache[cachedNum].iconIdx = iconIdx;
        timer_icon_cache[cachedNum].bmp = gbitmap_create_with_resource(timer_icon_bitmap_resids[iconIdx]);
        cachedNum++;

        //APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_get(%d) addedAt:%d not full", iconIdx, cachedNum - 1);

        return timer_icon_cache[cachedNum - 1].bmp;
    }
    else
    {
        // cache is full; delete first (FIFO)
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_get(%d) deletingAt:%d full", iconIdx, 0);
        gbitmap_destroy(timer_icon_cache[0].bmp);
        int last = cachedNum - 1;

        for (int i = 0; i < last && i < MAX_TIMER_CACHED_ICONS; i++)
        {
            timer_icon_cache[i].iconIdx = timer_icon_cache[i+1].iconIdx;
            timer_icon_cache[i].bmp = timer_icon_cache[i+1].bmp;
        }

        timer_icon_cache[last].iconIdx = iconIdx;
        timer_icon_cache[last].bmp = gbitmap_create_with_resource(timer_icon_bitmap_resids[iconIdx]);
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_get(%d) addedAt:%d full", iconIdx, last);
        return timer_icon_cache[last].bmp;
    }
}

static void timer_icon_cache_init(void)
{
    for (int i = 0; i < cachedNum && i < MAX_TIMER_CACHED_ICONS; i++)
    {
        timer_icon_cache[i].iconIdx = -1;
    }

    cachedNum = 0;

    int i = 0;
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_BLANK;
    timer_icon_labels[i++] = "";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_EGG;
    timer_icon_labels[i++] = "egg" ;
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_BURGER;
    timer_icon_labels[i++] = "burger" ;
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_FISH;
    timer_icon_labels[i++] = "fish" ;
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_MEAT;
    timer_icon_labels[i++] = "meat" ;
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_POTATO;
    timer_icon_labels[i++] = "potato" ;
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_VEGGIES;
    timer_icon_labels[i++] = "veggies" ;
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_STAR;
    timer_icon_labels[i++] = "favorite";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_CAP;
    timer_icon_labels[i++] = "cappuccino";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_ESPRESSO_S;
    timer_icon_labels[i++] = "coffee";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_TEA;
    timer_icon_labels[i++] = "tea";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_BODOM;
    timer_icon_labels[i++] = "bodom";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_BREAD;
    timer_icon_labels[i++] = "bread";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_FOOD;
    timer_icon_labels[i++] = "food";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_STOVE;
    timer_icon_labels[i++] = "stove";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_TOAST;
    timer_icon_labels[i++] = "toast";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_WINE;
    timer_icon_labels[i++] = "wine";

    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_CALENDAR;
    timer_icon_labels[i++] = "calendar";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_CLOCK;
    timer_icon_labels[i++] = "clock";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_BUSINESS;
    timer_icon_labels[i++] = "business";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_LAUNDRY;
    timer_icon_labels[i++] = "laundry";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_LAUNDRY2;
    timer_icon_labels[i++] = "laundry";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_CAT;
    timer_icon_labels[i++] = "cat";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_DOG;
    timer_icon_labels[i++] = "dog";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_PEOPLE;
    timer_icon_labels[i++] = "people";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_PHONE;
    timer_icon_labels[i++] = "phone";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_PHONE2;
    timer_icon_labels[i++] = "phone";

    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_PILL;
    timer_icon_labels[i++] = "pill";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_DOCTOR;
    timer_icon_labels[i++] = "doctor";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_AMBULANCE;
    timer_icon_labels[i++] = "ambulance";

    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_AIRPLANE;
    timer_icon_labels[i++] = "airplane";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_BUS;
    timer_icon_labels[i++] = "bus";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_TAXI;
    timer_icon_labels[i++] = "taxi";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_TRAIN;
    timer_icon_labels[i++] = "train";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_WALK;
    timer_icon_labels[i++] = "walk";

    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_LOVE;
    timer_icon_labels[i++] = "heart";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_ANGER;
    timer_icon_labels[i++] = "angry";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_SLEEP;
    timer_icon_labels[i++] = "sleep";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_SMILE;
    timer_icon_labels[i++] = "smile";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_SURPRISE;
    timer_icon_labels[i++] = "surprise";

    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_BRAIN;
    timer_icon_labels[i++] = "brain";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_CALCULATOR;
    timer_icon_labels[i++] = "calculator";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_DANGER;
    timer_icon_labels[i++] = "danger";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_FIRE;
    timer_icon_labels[i++] = "fire";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_HOLIDAY;
    timer_icon_labels[i++] = "holiday";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_MUSIC;
    timer_icon_labels[i++] = "music";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_APPLE;
    timer_icon_labels[i++] = "apple";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_RASPBERRY;
    timer_icon_labels[i++] = "raspberry pi";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_PRISONER;
    timer_icon_labels[i++] = "freedom";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_COFFEEBEAN;
    timer_icon_labels[i++] = "coffee";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_LAPAVONI;
    timer_icon_labels[i++] = "lapavoni";
    timer_icon_bitmap_resids[i] = RESOURCE_ID_IMAGE_CHEMIST;
    timer_icon_labels[i++] = "laboratory";

    if (i != TIMER_ICON_ITEMS)
    {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_init not fully initialized items %d != %d (TIMER_ICON_ITEMS)", i, TIMER_ICON_ITEMS);
    }
    else
    {
        APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_init initialized items %d", i);
    }
}

static void timer_icon_cache_destroy(void)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_destory cachedNum %d", cachedNum);

    for (int i = 0; i < cachedNum && i < MAX_TIMER_CACHED_ICONS; i++)
    {
        //APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_icon_cache_destroy() deletingAt:%d", i);
        gbitmap_destroy(timer_icon_cache[i].bmp);
        timer_icon_cache[i].bmp = NULL;
        timer_icon_cache[i].iconIdx = -1;
    }

    cachedNum = 0;
}

/*
 APP_MSG_OK(0) All good, operation was successful.
 APP_MSG_SEND_TIMEOUT(2) The other end did not confirm receiving the sent data with an (n)ack in time.
 APP_MSG_SEND_REJECTED(4) The other end rejected the sent data, with a "nack" reply.
 APP_MSG_NOT_CONNECTED(8) The other end was not connected.
 APP_MSG_APP_NOT_RUNNING(16) The local application was not running.
 APP_MSG_INVALID_ARGS(32) The function was called with invalid arguments.
 APP_MSG_BUSY(64) There are pending (in or outbound) messages that need to be processed first before new ones can be received or sent.
 APP_MSG_BUFFER_OVERFLOW(128) The buffer was too small to contain the incoming message.
 APP_MSG_ALREADY_RELEASED(512) The resource had already been released.
 APP_MSG_CALLBACK_ALREADY_REGISTERED(1024) The callback was already registered.
 APP_MSG_CALLBACK_NOT_REGISTERED(2048) The callback could not be deregistered, because it had not been registered before.
 APP_MSG_OUT_OF_MEMORY(4096) The system did not have sufficient application memory to perform the requested operation.
 APP_MSG_CLOSED(8192) App message was closed.
 APP_MSG_INTERNAL_ERROR(16384) An internal OS error prevented AppMessage from completing an operation.
 APP_MSG_INVALID_STATE(32768) The function was called while App Message was not in the appropriate state.
 */
char *translate_error(AppMessageResult result)
{
    switch (result) {
        case APP_MSG_OK: return "APP_MSG_OK";
        case APP_MSG_SEND_TIMEOUT: return "APP_MSG_SEND_TIMEOUT";
        case APP_MSG_SEND_REJECTED: return "APP_MSG_SEND_REJECTED";
        case APP_MSG_NOT_CONNECTED: return "APP_MSG_NOT_CONNECTED";
        case APP_MSG_APP_NOT_RUNNING: return "APP_MSG_APP_NOT_RUNNING";
        case APP_MSG_INVALID_ARGS: return "APP_MSG_INVALID_ARGS";
        case APP_MSG_BUSY: return "APP_MSG_BUSY";
        case APP_MSG_BUFFER_OVERFLOW: return "APP_MSG_BUFFER_OVERFLOW";
        case APP_MSG_ALREADY_RELEASED: return "APP_MSG_ALREADY_RELEASED";
        case APP_MSG_CALLBACK_ALREADY_REGISTERED: return "APP_MSG_CALLBACK_ALREADY_REGISTERED";
        case APP_MSG_CALLBACK_NOT_REGISTERED: return "APP_MSG_CALLBACK_NOT_REGISTERED";
        case APP_MSG_OUT_OF_MEMORY: return "APP_MSG_OUT_OF_MEMORY";
        case APP_MSG_CLOSED: return "APP_MSG_CLOSED";
        case APP_MSG_INTERNAL_ERROR: return "APP_MSG_INTERNAL_ERROR";
        //case APP_MSG_INVALID_STATE: return "APP_MSG_INVALID_STATE";
        default: return "UNKNOWN ERROR";
    }
}

static char timeline_title[20];

static void addToTimeLine(int idx)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "addToTimeLine");

    if (timers[idx].isCountingUp) {
        return;
    }

    DictionaryIterator *iter;
    AppMessageResult result = app_message_outbox_begin(&iter);
    if (result == APP_MSG_OK && iter) {
        dict_write_uint8(iter, KEY_COMMAND, COMMAND_ADD_TO_TIMELINE);
        dict_write_uint8(iter, KEY_TIMELINE_ID, idx);
        dict_write_uint32(iter, KEY_TIMELINE_TIME, timers[idx].total_sec - timers[idx].elapsed_sec);
        snprintf(timeline_title, sizeof(timeline_title), "Multi-Timer+ %s", timer_icon_labels[timers[idx].iconIdx]);
        dict_write_cstring(iter, KEY_TIMELINE_TITLE, timeline_title);
        result = app_message_outbox_send();
        if (result != APP_MSG_OK) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "addToTimeLine app_message_outbox_send failed. %s", translate_error(result));
        }
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "addToTimeLine app_message_outbox_begin failed. %s", translate_error(result));
    }
}

static void removeFromTimeLine(int idx)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "removeFromTimeLine");

    if (timers[idx].isCountingUp) {
        return;
    }

    DictionaryIterator *iter;
    AppMessageResult result = app_message_outbox_begin(&iter);
    if (result == APP_MSG_OK && iter) {
        dict_write_uint8(iter, KEY_COMMAND, COMMAND_REMOVE_FROM_TIMELINE);
        dict_write_uint8(iter, KEY_TIMELINE_ID, idx);
        result = app_message_outbox_send();
        if (result != APP_MSG_OK) {
            APP_LOG(APP_LOG_LEVEL_ERROR, "removeFromTimeLine app_message_outbox_send failed. %s", translate_error(result));
        }
    } else {
        APP_LOG(APP_LOG_LEVEL_ERROR, "removeFromTimeLine app_message_outbox_begin failed. %s", translate_error(result));
    }
}

static void outbox_failed_callback(DictionaryIterator *iterator, AppMessageResult reason, void *context)
{
    APP_LOG(APP_LOG_LEVEL_ERROR, "Outbox send failed. %s", translate_error(reason));
}

static void outbox_sent_callback(DictionaryIterator *iterator, void *context)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "Outbox send success!");
}

#define TIMER_VIBE_ITEMS 4
#define TIMER_VIBE_REPEATS 5

static char *timer_vibe_labels[TIMER_VIBE_ITEMS] = { "short+short", "long", "short", "short+long" };
static char *timer_vibe_repeat_labels[TIMER_VIBE_REPEATS] = { "5", "4", "3", "2", "1" };

#define KEY_TOTAL        0
#define KEY_ELAPSED      1
#define KEY_ISRUNNING    2
#define KEY_ICON         3
#define KEY_TYPE         4
#define KEY_VIBE         5
#define KEY_VIBE_REPEAT  6

#define TimerItemKey(timer, offset) (KEY_FIRST_TIMER + MAX_TIMERS * (offset) + (timer))

static int timerIndex(MenuIndex *cell_index)
{
    int row = -1;
    int index = -1;

    if (cell_index->section == SECTION_STOPWATCHES)
    {
        for (int i = 0; i < num_timers && row < cell_index->row; i++)
        {
            if (timers[i].isCountingUp)
            {
                row++;
                index = i;
            }
        }
    }
    else
    {
        for (int i = 0; i < num_timers && row < cell_index->row; i++)
        {
            if (!timers[i].isCountingUp)
            {
                row++;
                index = i;
            }
        }
    }

    return index;
}

static MenuIndex timerMenuIndex(int timerIndex)
{
    int row = 0;

    for (int i = 0; i < num_timers && i < timerIndex; i++)
    {
        if (timers[i].isCountingUp == timers[timerIndex].isCountingUp)
        {
            row++;
        }
    }

    MenuIndex index = (MenuIndex){ .row = row, .section = timers[timerIndex].isCountingUp ? SECTION_STOPWATCHES : SECTION_TIMERS};
    return index;
}

// ------------------------- Delete Confirmation Window ---------

#define YES_NO_W 28
#define YES_NO_H 18

static Window *delete_window = 0;
static TextLayer *delete_text_layer = 0, *delete_yes_text_layer = 0, *delete_no_text_layer = 0, *delete_icon_label_text_layer = 0;
static BitmapLayer *delete_icon_bitmap_layer = 0;
static int delete_window_pop_cnt = 0;

static void delete_window_yes_click_handler(ClickRecognizerRef recognizer, void *context)
{
    for (int i = cur_timer; i < num_timers - 1; i++)
    {
        timers[i] = timers[i+1];
    }

    num_timers--;

    if (cur_timer >= num_timers)
    {
        cur_timer = num_timers - 1;
    }

    for (int i = 0; i < delete_window_pop_cnt; i++)
    {
        window_stack_pop(false);
    }

    menu_layer_reload_data(s_menu_layer);
    menu_layer_set_selected_index(s_menu_layer, timerMenuIndex(cur_timer), MenuRowAlignCenter, false);
    window_stack_pop(true);
}

static void delete_window_no_click_handler(ClickRecognizerRef recognizer, void *context)
{
    window_stack_pop(true);
}

static void delete_window_click_config_provider(void *context)
{
    window_single_click_subscribe(BUTTON_ID_UP, delete_window_yes_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, delete_window_no_click_handler);
}

static void delete_window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);

    delete_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x, bounds.origin.y + YES_NO_H * 2 }, .size = { bounds.size.w, bounds.size.h - YES_NO_H * 3 } } );
    text_layer_set_font(delete_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_28_BOLD));
    text_layer_set_text_alignment(delete_text_layer, GTextAlignmentCenter);
    text_layer_set_overflow_mode(delete_text_layer, GTextOverflowModeWordWrap);

    static char title[48];
    uint32_t time = timers[cur_timer].total_sec - timers[cur_timer].elapsed_sec;
    int days = time / 60 / 60 / 24;
    int hours = time / 60 / 60 - days * 24;
    int minutes = time / 60 - days * 24 * 60 - hours * 60;
    int seconds = time - days * 24 * 60 * 60 - hours * 60 * 60 - minutes * 60;

    if (days > 0)
    {
        snprintf(title, sizeof(title), "Delete\n%2d %02d:%02d:%02d?", days, hours, minutes, seconds);
    }
    else if (hours > 0)
    {
        snprintf(title, sizeof(title), "Delete\n%2d:%02d:%02d?", hours, minutes, seconds);
    }
    else
    {
        snprintf(title, sizeof(title), "Delete\n%2d:%02d?", minutes, seconds);
    }

    text_layer_set_text(delete_text_layer, title);
    layer_add_child(window_layer, text_layer_get_layer(delete_text_layer));

#ifdef PBL_PLATFORM_CHALK
    const int insetX = 20;
    const int insetY = 30;
#else
    const int insetX = 0;
    const int insetY = 0;
#endif
    delete_yes_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x + bounds.size.w - YES_NO_W - 4 - insetX, bounds.origin.y + insetY}, .size = { YES_NO_W, YES_NO_H } });
    text_layer_set_font(delete_yes_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(delete_yes_text_layer, GTextAlignmentRight);
    text_layer_set_text(delete_yes_text_layer, "Yes");
#ifdef PBL_COLOR
    text_layer_set_text_color(delete_yes_text_layer, GColorRed);
#endif
    text_layer_set_background_color(delete_yes_text_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(delete_yes_text_layer));

    delete_no_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x + bounds.size.w - YES_NO_W - 4 - insetX, bounds.origin.y + bounds.size.h - YES_NO_H - 8 - insetY}, .size = { YES_NO_W, YES_NO_H } });
    text_layer_set_font(delete_no_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
    text_layer_set_text_alignment(delete_no_text_layer, GTextAlignmentRight);
    text_layer_set_text(delete_no_text_layer, "No");
    text_layer_set_background_color(delete_no_text_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(delete_no_text_layer));

    delete_icon_bitmap_layer = bitmap_layer_create((GRect) { .origin = { bounds.origin.x + 10 + insetX, bounds.origin.y + YES_NO_H * 2 + 60}, .size = { ICON_BITMAP_SIZE, ICON_BITMAP_SIZE } });
    bitmap_layer_set_bitmap(delete_icon_bitmap_layer, timer_icon_cache_get(timers[cur_timer].iconIdx));
    bitmap_layer_set_compositing_mode(delete_icon_bitmap_layer, CompOp);
    bitmap_layer_set_alignment(delete_icon_bitmap_layer, GAlignCenter);
    layer_add_child(window_layer, bitmap_layer_get_layer(delete_icon_bitmap_layer));

    delete_icon_label_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x + insetX + ICON_BITMAP_SIZE + 12, bounds.origin.y + YES_NO_H * 2 + 60}, .size = { bounds.size.w, 28 } });
    text_layer_set_text(delete_icon_label_text_layer, timer_icon_labels[timers[cur_timer].iconIdx]);
    text_layer_set_font(delete_icon_label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    text_layer_set_text_alignment(delete_icon_label_text_layer, GTextAlignmentLeft);
    text_layer_set_background_color(delete_icon_label_text_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(delete_icon_label_text_layer));
}

static void delete_window_unload(Window *window)
{
    text_layer_destroy(delete_text_layer);
    text_layer_destroy(delete_yes_text_layer);
    text_layer_destroy(delete_no_text_layer);
    text_layer_destroy(delete_icon_label_text_layer);
    bitmap_layer_destroy(delete_icon_bitmap_layer);
    window_destroy(delete_window);
}

static void setupContentIndicators(Layer *window_layer, GRect bounds, MenuLayer *menu_layer, ContentIndicator **indicator, Layer **indicator_up_layer, Layer **indicator_down_layer, ContentIndicatorConfig *up_config, ContentIndicatorConfig *down_config)
{
    *indicator = scroll_layer_get_content_indicator(menu_layer_get_scroll_layer(menu_layer));
    // Create two Layers to draw the arrows
    *indicator_up_layer = layer_create(GRect(0, bounds.origin.y, bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
    *indicator_down_layer = layer_create(GRect(0, bounds.origin.y + bounds.size.h - STATUS_BAR_LAYER_HEIGHT, bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
    layer_add_child(window_layer, *indicator_up_layer);
    layer_add_child(window_layer, *indicator_down_layer);

    // Configure the properties of each indicator
    *up_config = (ContentIndicatorConfig) {
        .layer = *indicator_up_layer,
        .times_out = true,
        .alignment = GAlignCenter,
        .colors = {
            .foreground = GColorBlue,
            .background = GColorWhite
        }
    };
    content_indicator_configure_direction(*indicator, ContentIndicatorDirectionUp, up_config);

    *down_config = (ContentIndicatorConfig) {
        .layer = *indicator_down_layer,
        .times_out = true,
        .alignment = GAlignCenter,
        .colors = {
            .foreground = GColorBlue,
            .background = GColorWhite
        }
    };
    content_indicator_configure_direction(*indicator, ContentIndicatorDirectionDown, down_config);
}

// ------------------------- Timer Window -----------------------

static Window *timer_window = 0;
static TextLayer *days_text_layer = 0, *days_label_text_layer = 0;
static TextLayer *hours_text_layer = 0, *hours_label_text_layer = 0;
static TextLayer *time_text_layer = 0;
static TextLayer *minutes_label_text_layer = 0, *seconds_label_text_layer = 0, *icon_label_text_layer = 0;
static BitmapLayer *up_bitmap_layer = 0, *select_bitmap_layer = 0, *down_bitmap_layer = 0, *icon_bitmap_layer = 0;
static GBitmap *setup_bitmap = 0, *start_bitmap = 0, *pause_bitmap = 0, *reset_bitmap = 0, *running_bitmap = 0, *trash_bitmap = 0, *stopwatch_bitmap = 0, *vibe_bitmap = 0;

static void timer_stop(int timer_num)
{
    //tick_timer_service_unsubscribe();
    timers[timer_num].isRunning = false;

    if (timer_num == cur_timer)
    {
        bitmap_layer_set_bitmap(select_bitmap_layer, start_bitmap);
        layer_set_hidden((Layer *)up_bitmap_layer, false);
        layer_set_hidden((Layer *)down_bitmap_layer, false);
    }
}

static void vibe(int timer)
{
    switch (timers[timer].vibeIdx) {
        case 0:
            vibes_double_pulse();
            break;

        case 1:
            vibes_long_pulse();
            break;

        case 2:
            vibes_short_pulse();
            break;

        case 3:
        {
            // Vibe pattern: {on, off, on, ...}
            static const uint32_t segments[] = { 200, 100, 400 };
            VibePattern pat = {
                .durations = segments,
                .num_segments = ARRAY_LENGTH(segments),
            };
            vibes_enqueue_custom_pattern(pat);
            break;
        }
        default:
            vibes_double_pulse();
            break;
    }
}

static void timer_update_time(void)
{
    static char days_title[] = "ddddddd d";
    static char hours_title[10];
    static char time_title[20];
    int alert_timer = -1;

    for (int i = 0; i < MAX_TIMERS; i++)
    {
        int32_t time_delta = 0;

        if (timers[i].isCountingUp)
        {
            time_delta = timers[i].elapsed_sec;
        }
        else
        {
            time_delta = timers[i].total_sec - timers[i].elapsed_sec;
        }

        if (i == cur_timer)
        {
            uint32_t time = time_delta;

            int days = time / 60 / 60 / 24;
            int hours = time / 60 / 60 - days * 24;
            int minutes = time / 60 - days * 24 * 60 - hours * 60;
            int seconds = time - days * 24 * 60 * 60 - hours * 60 * 60 - minutes * 60;

            if (days > 0)
            {
                snprintf(days_title, sizeof(days_title), "%d", days);
                layer_set_hidden((Layer *)days_text_layer, false);
                layer_set_hidden((Layer *)days_label_text_layer, false);
                text_layer_set_text(days_text_layer, days_title);
            }
            else
            {
                layer_set_hidden((Layer *)days_text_layer, true);
                layer_set_hidden((Layer *)days_label_text_layer, true);
            }

            if (hours > 0 || days > 0)
            {
                snprintf(hours_title, sizeof(hours_title), "%02d", hours);
                layer_set_hidden((Layer *)hours_text_layer, false);
                layer_set_hidden((Layer *)hours_label_text_layer, false);
                text_layer_set_text(hours_text_layer, hours_title);
            }
            else
            {
                layer_set_hidden((Layer *)hours_text_layer, true);
                layer_set_hidden((Layer *)hours_label_text_layer, true);
            }

            snprintf(time_title, sizeof(time_title), "%02d:%02d", minutes, seconds);

            text_layer_set_text(time_text_layer, time_title);
        }

        if (!timers[i].isCountingUp)
        {
            if (timers[i].isRunning && time_delta <= 0)
            {
                timers[i].elapsed_sec = 0;
                timers[i].alert_sec = 5 - timers[i].vibeRepeat; // vibe repeat
                timer_stop(i);
                vibe(i);
                menu_layer_set_selected_index(s_menu_layer, timerMenuIndex(i), MenuRowAlignCenter, false);
                timer_update_time();
            }
            else if (timers[i].alert_sec > 0)
            {
                timers[i].alert_sec--;
                alert_timer = i;
            }
        }
    }

    if (alert_timer >= 0)
    {
        vibe(alert_timer);
    }
}

static void timer_handle_tick(struct tm* tick_time, TimeUnits units_changed)
{
    bool isRunning = false;

    for (int i = 0; i < MAX_TIMERS; i++)
    {
        if (timers[i].isRunning)
        {
            timers[i].elapsed_sec++;
            isRunning = true;
        }
        else if (timers[i].alert_sec > 0)
        {
            isRunning = true;
        }
    }

    timer_update_time();

    if (isRunning)
    {
        layer_mark_dirty(menu_layer_get_layer(s_menu_layer));
    }
}


// ------------------ Number Picker Window ------------------------

#define NUM_WIN_MODE_DAYS       0
#define NUM_WIN_MODE_HOURS      1
#define NUM_WIN_MODE_MINUTES    2
#define NUM_WIN_MODE_SECONDS    3
#define NUM_WIN_MODE_DONE       4

static int number_window_value[] = { 0, 0, 0, 0 };
static int number_window_max_value[] = { 1000, 24, 60, 60 };
static char *number_window_labels[] = { "Set Days", "Set Hours", "Set Minutes", "Set Seconds" };
static NumberWindow *number_window[NUM_WIN_MODE_DONE];

static void number_window_build(int mode);
static void number_window_selected(NumberWindow *this_window, void *context)
{
    int mode = (int)context;
    number_window_value[mode] = number_window_get_value(this_window);
    timers[cur_timer].total_sec = number_window_value[NUM_WIN_MODE_DAYS] * 24 * 60 * 60 + number_window_value[NUM_WIN_MODE_HOURS] * 60 * 60 +
    number_window_value[NUM_WIN_MODE_MINUTES] * 60 + number_window_value[NUM_WIN_MODE_SECONDS];
    timers[cur_timer].elapsed_sec = 0;
    timer_update_time();
    mode++;

    if (mode == NUM_WIN_MODE_DONE)
    {
        for (int i = mode - 1; i > 0; i--)
            window_stack_pop(false);

        for (int i = 0; i < NUM_WIN_MODE_DONE; i++)
            number_window_destroy(number_window[i]);

        window_stack_pop(true);
    }
    else
    {
        number_window_build(mode);
    }
}

static void number_window_decremented(NumberWindow *this_window, void *context)
{
    int mode = (int)context;

    if (number_window_get_value(this_window) == -1) {
	    number_window_set_value(this_window, number_window_max_value[mode]-1);
    }
}

static void number_window_incremented(NumberWindow *this_window, void *context)
{
    int mode = (int)context;

    if (number_window_get_value(this_window) == number_window_max_value[mode]) {
	    number_window_set_value(this_window, 0);
    }
}

static void number_window_build(int mode)
{
    number_window[mode] = number_window_create(number_window_labels[mode], (NumberWindowCallbacks) { .incremented = number_window_incremented, .decremented = number_window_decremented, .selected = number_window_selected }, (void *)mode);
    number_window_set_value(number_window[mode], number_window_value[mode]);
    number_window_set_min(number_window[mode], -1);
    number_window_set_max(number_window[mode], number_window_max_value[mode]);

    window_stack_push((Window *)number_window[mode], true);
}

static void number_window_init(void)
{
    for (int i = 0; i < NUM_WIN_MODE_DONE; i++)
        number_window[i] = NULL;

    uint32_t time = timers[cur_timer].total_sec - timers[cur_timer].elapsed_sec;
    int days = number_window_value[NUM_WIN_MODE_DAYS] = time / 60 / 60 / 24;
    int hours = number_window_value[NUM_WIN_MODE_HOURS] = time / 60 / 60 - days * 24;
    int minutes = number_window_value[NUM_WIN_MODE_MINUTES] = time / 60 - days * 24 * 60 - hours * 60;
    number_window_value[NUM_WIN_MODE_SECONDS] = time - days * 24 * 60 * 60 - hours * 60 * 60 - minutes * 60;

    number_window_build(NUM_WIN_MODE_DAYS);
}

static void number_window_init_timer_callback(void *data)
{
    number_window_init();
}

// --------------------- Setup Icon Menu -----------------------------

static Window *setup_icon_window = 0;
static MenuLayer *setup_icon_menu_layer = 0;
static Layer *s_setup_icon_indicator_up_layer, *s_setup_icon_indicator_down_layer;
static ContentIndicator *s_setup_icon_indicator;
static ContentIndicatorConfig s_setup_icon_up_config, s_setup_icon_down_config;

static uint16_t setup_icon_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
    return TIMER_ICON_ITEMS;
}

#ifdef PBL_PLATFORM_CHALK
static int16_t setup_icon_menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    return 60;
}
#endif

static void setup_icon_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
    menu_cell_basic_draw(ctx, cell_layer, timer_icon_labels[cell_index->row], NULL, timer_icon_cache_get(cell_index->row));
}

void setup_icon_menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    timers[cur_timer].iconIdx = cell_index->row;

    window_stack_pop(true);
}

static void setup_icon_window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    setup_icon_menu_layer = menu_layer_create(bounds);

    menu_layer_set_callbacks(setup_icon_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_rows = setup_icon_menu_get_num_rows_callback,
        .draw_row = setup_icon_menu_draw_row_callback,
        .select_click = setup_icon_menu_select_click_callback,
#ifdef PBL_PLATFORM_CHALK
        .get_cell_height = setup_icon_menu_get_cell_height,
#endif
    });

    menu_layer_set_click_config_onto_window(setup_icon_menu_layer, window);
    MenuIndex index = (MenuIndex){ .row = timers[cur_timer].iconIdx, .section = 0};
    menu_layer_set_selected_index(setup_icon_menu_layer, index, MenuRowAlignCenter, false);
#ifdef PBL_COLOR
    menu_layer_set_highlight_colors(setup_icon_menu_layer, GColorVividCerulean, GColorBlack);
#endif
    layer_add_child(window_layer, menu_layer_get_layer(setup_icon_menu_layer));
    setupContentIndicators(window_layer, bounds, setup_icon_menu_layer, &s_setup_icon_indicator, &s_setup_icon_indicator_up_layer, &s_setup_icon_indicator_down_layer, &s_setup_icon_up_config, &s_setup_icon_down_config);
}

static void setup_icon_window_appear(Window *window)
{
    menu_layer_reload_data(setup_icon_menu_layer);
}

static void setup_icon_window_unload(Window *window)
{
    menu_layer_destroy(setup_icon_menu_layer);
    layer_destroy(s_setup_icon_indicator_up_layer);
    layer_destroy(s_setup_icon_indicator_down_layer);
    bitmap_layer_set_bitmap(icon_bitmap_layer, timer_icon_cache_get(timers[cur_timer].iconIdx));
    text_layer_set_text(icon_label_text_layer, timer_icon_labels[timers[cur_timer].iconIdx]);
    window_destroy(setup_icon_window);
}

// --------------------- Setup Vibe Menu -----------------------------

static Window *setup_vibe_window = 0;
static MenuLayer *setup_vibe_menu_layer = 0;
static Layer *s_setup_vibe_indicator_up_layer, *s_setup_vibe_indicator_down_layer;
static ContentIndicator *s_setup_vibe_indicator;
static ContentIndicatorConfig s_setup_vibe_up_config, s_setup_vibe_down_config;

static uint16_t setup_vibe_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
    return TIMER_VIBE_ITEMS;
}

static int16_t setup_vibe_menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *data) {
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    return bounds.size.h / TIMER_VIBE_ITEMS - 5;
}

static void setup_vibe_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
    menu_cell_basic_draw(ctx, cell_layer, timer_vibe_labels[cell_index->row], NULL, NULL);
}

static void setup_vibe_menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    timers[cur_timer].vibeIdx = cell_index->row;
    window_stack_pop(true);
}

static void setup_vibe_window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    setup_vibe_menu_layer = menu_layer_create(bounds);

    menu_layer_set_callbacks(setup_vibe_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_rows = setup_vibe_menu_get_num_rows_callback,
        .draw_row = setup_vibe_menu_draw_row_callback,
        .select_click = setup_vibe_menu_select_click_callback,
        .get_cell_height = setup_vibe_menu_get_cell_height,
    });

    menu_layer_set_click_config_onto_window(setup_vibe_menu_layer, window);
    MenuIndex index = (MenuIndex){ .row = timers[cur_timer].vibeIdx, .section = 0};
    menu_layer_set_selected_index(setup_vibe_menu_layer, index, MenuRowAlignCenter, false);
#ifdef PBL_COLOR
    menu_layer_set_highlight_colors(setup_vibe_menu_layer, GColorVividCerulean, GColorBlack);
#endif
    layer_add_child(window_layer, menu_layer_get_layer(setup_vibe_menu_layer));
    setupContentIndicators(window_layer, bounds, setup_vibe_menu_layer, &s_setup_vibe_indicator, &s_setup_vibe_indicator_up_layer, &s_setup_vibe_indicator_down_layer, &s_setup_vibe_up_config, &s_setup_vibe_down_config);
}

static void setup_vibe_window_appear(Window *window)
{
    menu_layer_reload_data(setup_vibe_menu_layer);
}

static void setup_vibe_window_unload(Window *window)
{
    menu_layer_destroy(setup_vibe_menu_layer);
    layer_destroy(s_setup_vibe_indicator_up_layer);
    layer_destroy(s_setup_vibe_indicator_down_layer);
    window_destroy(setup_vibe_window);
}

// --------------------- Setup Vibe Repeat Menu -----------------------------

static Window *setup_vibe_repeat_window = 0;
static MenuLayer *setup_vibe_repeat_menu_layer = 0;
static Layer *s_setup_vibe_repeat_indicator_up_layer, *s_setup_vibe_repeat_indicator_down_layer;
static ContentIndicator *s_setup_vibe_repeat_indicator;
static ContentIndicatorConfig s_setup_vibe_repeat_up_config, s_setup_vibe_repeat_down_config;

static uint16_t setup_vibe_repeat_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
    return TIMER_VIBE_REPEATS;
}

static int16_t setup_vibe_repeat_menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);
    return bounds.size.h / TIMER_VIBE_REPEATS -5;
}

static void setup_vibe_repeat_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
    menu_cell_basic_draw(ctx, cell_layer, timer_vibe_repeat_labels[cell_index->row], NULL, NULL);
}

void setup_vibe_repeat_menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    timers[cur_timer].vibeRepeat = cell_index->row;
    window_stack_pop(true);
}

static void setup_vibe_repeat_window_load(Window *window)
{
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    setup_vibe_repeat_menu_layer = menu_layer_create(bounds);

    menu_layer_set_callbacks(setup_vibe_repeat_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_rows = setup_vibe_repeat_menu_get_num_rows_callback,
        .draw_row = setup_vibe_repeat_menu_draw_row_callback,
        .select_click = setup_vibe_repeat_menu_select_click_callback,
        .get_cell_height = setup_vibe_repeat_menu_get_cell_height,
    });

    menu_layer_set_click_config_onto_window(setup_vibe_repeat_menu_layer, window);
    MenuIndex index = (MenuIndex){ .row = timers[cur_timer].vibeRepeat, .section = 0};
    menu_layer_set_selected_index(setup_vibe_repeat_menu_layer, index, MenuRowAlignCenter, false);
#ifdef PBL_COLOR
    menu_layer_set_highlight_colors(setup_vibe_repeat_menu_layer, GColorVividCerulean, GColorBlack);
#endif
    layer_add_child(window_layer, menu_layer_get_layer(setup_vibe_repeat_menu_layer));
    setupContentIndicators(window_layer, bounds, setup_vibe_repeat_menu_layer, &s_setup_vibe_repeat_indicator, &s_setup_vibe_repeat_indicator_up_layer, &s_setup_vibe_repeat_indicator_down_layer, &s_setup_vibe_repeat_up_config, &s_setup_vibe_repeat_down_config);
}

static void setup_vibe_repeat_window_appear(Window *window)
{
    menu_layer_reload_data(setup_vibe_repeat_menu_layer);
}

static void setup_vibe_repeat_window_unload(Window *window)
{
    menu_layer_destroy(setup_vibe_repeat_menu_layer);
    layer_destroy(s_setup_vibe_repeat_indicator_up_layer);
    layer_destroy(s_setup_vibe_repeat_indicator_down_layer);
    window_destroy(setup_vibe_repeat_window);
}

// --------------------- Setup Menu -----------------------------

static Window *setup_window = 0;
static MenuLayer *setup_menu_layer = 0;
static Layer *s_setup_indicator_up_layer, *s_setup_indicator_down_layer;
static ContentIndicator *s_setup_indicator;
static ContentIndicatorConfig s_setup_up_config, s_setup_down_config;

#define SETUP_MENU_TIMER     0
#define SETUP_MEMU_STOPWATCH 1
static int setup_menu_labels_count[2] = {5, 2};
static char *setup_menu_labels[2][5]= {
    {"Time", "Icon", "Vibe", "Vibe Repeat", "Delete"},
    {"Icon", "Delete", "", ""}
};

static uint16_t setup_menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
    if (timers[cur_timer].isCountingUp)
    {
        return setup_menu_labels_count[SETUP_MEMU_STOPWATCH];
    }
    else
    {
        return setup_menu_labels_count[SETUP_MENU_TIMER];
    }
}

#ifdef PBL_PLATFORM_CHALK
static int16_t setup_menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    return 76;
}
#endif

static void setup_menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
    static char title[36];

    if (timers[cur_timer].isCountingUp)
    {
        switch (cell_index->row)
        {
            case 0: // Icon
                menu_cell_basic_draw(ctx, cell_layer, setup_menu_labels[SETUP_MEMU_STOPWATCH][cell_index->row], timer_icon_labels[timers[cur_timer].iconIdx], timer_icon_cache_get(timers[cur_timer].iconIdx));
                break;
            case 1: // Delete
                menu_cell_basic_draw(ctx, cell_layer, setup_menu_labels[SETUP_MEMU_STOPWATCH][cell_index->row], NULL, trash_bitmap);
                break;
        }
    }
    else
    {
        switch (cell_index->row)
        {
            case 0: // Time
            {
                uint32_t time = timers[cur_timer].total_sec;
                int days = time / 60 / 60 / 24;
                int hours = time / 60 / 60 - days * 24;
                int minutes = time / 60 - days * 24 * 60 - hours * 60;
                int seconds = time - days * 24 * 60 * 60 - hours * 60 * 60 - minutes * 60;

                if (days > 0)
                {
                    snprintf(title, sizeof(title), "%2d %02d:%02d:%02d", days, hours, minutes, seconds);
                }
                else if (hours > 0)
                {
                    snprintf(title, sizeof(title), "%2d:%02d:%02d", hours, minutes, seconds);
                }
                else
                {
                    snprintf(title, sizeof(title), "%2d:%02d", minutes, seconds);
                }

                menu_cell_basic_draw(ctx, cell_layer, setup_menu_labels[SETUP_MENU_TIMER][cell_index->row], title, running_bitmap);
                break;
            }
            case 1: // Icon
                menu_cell_basic_draw(ctx, cell_layer, setup_menu_labels[SETUP_MENU_TIMER][cell_index->row], timer_icon_labels[timers[cur_timer].iconIdx], timer_icon_cache_get(timers[cur_timer].iconIdx));
                break;
            case 2:  // Vibe
                menu_cell_basic_draw(ctx, cell_layer, setup_menu_labels[SETUP_MENU_TIMER][cell_index->row], timer_vibe_labels[timers[cur_timer].vibeIdx], vibe_bitmap);
                break;
            case 3:  // Vibe Repeat
                menu_cell_basic_draw(ctx, cell_layer, setup_menu_labels[SETUP_MENU_TIMER][cell_index->row], timer_vibe_repeat_labels[timers[cur_timer].vibeRepeat], vibe_bitmap);
                break;
            case 4: // Delete
                menu_cell_basic_draw(ctx, cell_layer, setup_menu_labels[SETUP_MENU_TIMER][cell_index->row], NULL, trash_bitmap);
                break;
        }
    }
}

void setup_menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    if (timers[cur_timer].isCountingUp)
    {
        switch (cell_index->row) {
            case 0: // Icon
                setup_icon_window = window_create();
                window_set_window_handlers(setup_icon_window, (WindowHandlers)
                                           {
                                               .load = setup_icon_window_load,
                                               .unload = setup_icon_window_unload,
                                               .appear = setup_icon_window_appear,
                                           });
                window_stack_push(setup_icon_window, true);
                break;
            case 1: // Delete
                delete_window = window_create();
                delete_window_pop_cnt = 2;
                window_set_click_config_provider(delete_window, delete_window_click_config_provider);
                window_set_window_handlers(delete_window, (WindowHandlers) {
                    .load = delete_window_load,
                    .unload = delete_window_unload,
                });
                window_stack_push(delete_window, true);
                break;
        }
    }
    else
    {
        switch (cell_index->row) {
            case 0: // Time
                // timer_setup()
                number_window_init();
                break;
            case 1: // Icon
                setup_icon_window = window_create();
                window_set_window_handlers(setup_icon_window, (WindowHandlers)
                                           {
                                               .load = setup_icon_window_load,
                                               .unload = setup_icon_window_unload,
                                               .appear = setup_icon_window_appear,
                                           });
                window_stack_push(setup_icon_window, true);
                break;
            case 2: // Vibe
                setup_vibe_window = window_create();
                window_set_window_handlers(setup_vibe_window, (WindowHandlers)
                                           {
                                               .load = setup_vibe_window_load,
                                               .unload = setup_vibe_window_unload,
                                               .appear = setup_vibe_window_appear,
                                           });
                window_stack_push(setup_vibe_window, true);
                break;
            case 3: // Vibe Repeat
                setup_vibe_repeat_window = window_create();
                window_set_window_handlers(setup_vibe_repeat_window, (WindowHandlers)
                                           {
                                               .load = setup_vibe_repeat_window_load,
                                               .unload = setup_vibe_repeat_window_unload,
                                               .appear = setup_vibe_repeat_window_appear,
                                           });
                window_stack_push(setup_vibe_repeat_window, true);
                break;
            case 4: // Delete
                delete_window = window_create();
                delete_window_pop_cnt = 2;
                window_set_click_config_provider(delete_window, delete_window_click_config_provider);
                window_set_window_handlers(delete_window, (WindowHandlers) {
                    .load = delete_window_load,
                    .unload = delete_window_unload,
                });
                window_stack_push(delete_window, true);
                break;
        }
    }
}

static void setup_window_load(Window *window)
{
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "setup_window_load() free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    setup_menu_layer = menu_layer_create(bounds);

    menu_layer_set_callbacks(setup_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_rows = setup_menu_get_num_rows_callback,
        .draw_row = setup_menu_draw_row_callback,
        .select_click = setup_menu_select_click_callback,
#ifdef PBL_PLATFORM_CHALK
        .get_cell_height = setup_menu_get_cell_height,
#endif
    });

    menu_layer_set_click_config_onto_window(setup_menu_layer, window);
#ifdef PBL_COLOR
    menu_layer_set_highlight_colors(setup_menu_layer, GColorVividCerulean, GColorBlack);
#endif
    layer_add_child(window_layer, menu_layer_get_layer(setup_menu_layer));
    setupContentIndicators(window_layer, bounds, setup_menu_layer, &s_setup_indicator, &s_setup_indicator_up_layer, &s_setup_indicator_down_layer, &s_setup_up_config, &s_setup_down_config);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "setup_window_load() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
}

static void setup_window_appear(Window *window)
{
    menu_layer_reload_data(setup_menu_layer);
}

static void setup_window_unload(Window *window)
{
    menu_layer_destroy(setup_menu_layer);
    layer_destroy(s_setup_indicator_up_layer);
    layer_destroy(s_setup_indicator_down_layer);
    window_destroy(setup_window);
}

// ------------------ Timer Window --------------------------

static void timer_up_click_handler(ClickRecognizerRef recognizer, void *context)
{
    if (!timers[cur_timer].isRunning)
    {
        setup_window = window_create();
        window_set_window_handlers(setup_window, (WindowHandlers)
        {
            .load = setup_window_load,
            .unload = setup_window_unload,
            .appear = setup_window_appear
        });
        window_stack_push(setup_window, true);
    }
}

static void timer_up_long_click_handler(ClickRecognizerRef recognizer, void *context)
{
    if (!timers[cur_timer].isRunning)
    {
        delete_window_pop_cnt = 1;
        delete_window = window_create();
        window_set_click_config_provider(delete_window, delete_window_click_config_provider);
        window_set_window_handlers(delete_window, (WindowHandlers) {
            .load = delete_window_load,
            .unload = delete_window_unload,
        });
        window_stack_push(delete_window, true);
    }
}

static void timer_down_click_handler(ClickRecognizerRef recognizer, void *context)
{
    if (!timers[cur_timer].isRunning)
    {
        // timer_reset()
        timers[cur_timer].elapsed_sec = 0;
        timer_update_time();
    }
}

static bool timer_toggle(int timer)
{
    if (timers[timer].isRunning)
    {
        timer_stop(timer);
        removeFromTimeLine(timer);
        return false;
    }
    else if (!timers[timer].isCountingUp && timers[timer].total_sec == 0)
    {
        vibes_double_pulse();
        removeFromTimeLine(timer);
        return false;
    }
    else
    {
        // timer_start
        timers[timer].isRunning = true;
        timers[timer].alert_sec = 0;
        addToTimeLine(timer);
        return true;
    }
}

static void timer_select_click_handler(ClickRecognizerRef recognizer, void *context)
{
    if (timer_toggle(cur_timer))
    {
        layer_set_hidden((Layer *)up_bitmap_layer, true);
        layer_set_hidden((Layer *)down_bitmap_layer, true);
        bitmap_layer_set_bitmap(select_bitmap_layer, pause_bitmap);
    }
}

static void timer_click_config_provider(void *context)
{
    window_long_click_subscribe(BUTTON_ID_UP, 0, timer_up_long_click_handler, NULL);
    window_single_click_subscribe(BUTTON_ID_SELECT, timer_select_click_handler);
    window_single_click_subscribe(BUTTON_ID_UP, timer_up_click_handler);
    window_single_click_subscribe(BUTTON_ID_DOWN, timer_down_click_handler);
}

static void battery_proc(Layer *layer, GContext *ctx)
{
#ifndef PBL_PLATFORM_CHALK
    // Emulate battery meter on Aplite
    graphics_context_set_stroke_color(ctx, GColorWhite);
    graphics_draw_rect(ctx, GRect(126, 4, 14, 8));
    graphics_draw_line(ctx, GPoint(140, 6), GPoint(140, 9));

    BatteryChargeState state = battery_state_service_peek();
    int width = (int)(float)(((float)state.charge_percent / 100.0F) * 10.0F);
    graphics_context_set_fill_color(ctx, GColorWhite);
    graphics_fill_rect(ctx, GRect(128, 6, width, 4), 0, GCornerNone);
#endif
}

static void timer_window_load(Window *window)
{
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "timer_window_load() free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_frame(window_layer);

#ifdef PBL_PLATFORM_CHALK
    bounds.origin.x = 18;
    bounds.size.w -= 36;
#endif
    bounds.origin.y += STATUS_BAR_LAYER_HEIGHT;
    bounds.size.h -= STATUS_BAR_LAYER_HEIGHT;

    const char * timeFont =
#ifdef PBL_PLATFORM_CHALK
    FONT_KEY_LECO_42_NUMBERS;
#else
    FONT_KEY_ROBOTO_BOLD_SUBSET_49;
#endif
    ;
    days_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x, bounds.origin.y + -10 }, .size = { bounds.size.w - BITMAP_W - BITMAP_PAD, 50 } });
    text_layer_set_font(days_text_layer, fonts_get_system_font(timeFont));
    text_layer_set_text_alignment(days_text_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(days_text_layer));

    hours_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x, bounds.origin.y + 40 }, .size = { bounds.size.w - BITMAP_W - BITMAP_PAD, 50 } });
    text_layer_set_font(hours_text_layer, fonts_get_system_font(timeFont));
    text_layer_set_text_alignment(hours_text_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(hours_text_layer));

    time_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x, bounds.origin.y + 90 }, .size = { bounds.size.w - BITMAP_W - BITMAP_PAD, 50 } });
    text_layer_set_font(time_text_layer, fonts_get_system_font(timeFont));
    text_layer_set_text_alignment(time_text_layer, GTextAlignmentRight);
    layer_add_child(window_layer, text_layer_get_layer(time_text_layer));

    days_label_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x, bounds.origin.y + 36 }, .size = { bounds.size.w - BITMAP_W - BITMAP_PAD, 14 } });
    text_layer_set_text(days_label_text_layer, "days");
    text_layer_set_font(days_label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(days_label_text_layer, GTextAlignmentRight);
    text_layer_set_background_color(days_label_text_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(days_label_text_layer));

    hours_label_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x, bounds.origin.y + 86 }, .size = { bounds.size.w - BITMAP_W - BITMAP_PAD, 14 } });
    text_layer_set_text(hours_label_text_layer, "hours");
    text_layer_set_font(hours_label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(hours_label_text_layer, GTextAlignmentRight);
    text_layer_set_background_color(hours_label_text_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(hours_label_text_layer));

    minutes_label_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x + 20, bounds.origin.y + 136 }, .size = { bounds.size.w / 4, 14 } });
    text_layer_set_text(minutes_label_text_layer, "min");
    text_layer_set_font(minutes_label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(minutes_label_text_layer, GTextAlignmentRight);
    text_layer_set_background_color(minutes_label_text_layer, GColorClear);
    //layer_add_child(window_layer, text_layer_get_layer(minutes_label_text_layer));

    seconds_label_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x, bounds.origin.y + 136 }, .size = { bounds.size.w - BITMAP_W - BITMAP_PAD, 14 } });
    text_layer_set_text(seconds_label_text_layer, "sec");
    text_layer_set_font(seconds_label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_14));
    text_layer_set_text_alignment(seconds_label_text_layer, GTextAlignmentRight);
    text_layer_set_background_color(seconds_label_text_layer, GColorClear);
    //layer_add_child(window_layer, text_layer_get_layer(seconds_label_text_layer));

    up_bitmap_layer = bitmap_layer_create((GRect) { .origin = { bounds.origin.x + bounds.size.w - BITMAP_W - 1, bounds.origin.y + 35 }, .size = { BITMAP_W, BITMAP_H } });
    bitmap_layer_set_bitmap(up_bitmap_layer, setup_bitmap);
    bitmap_layer_set_compositing_mode(up_bitmap_layer, CompOp);
    bitmap_layer_set_alignment(up_bitmap_layer, GAlignCenter);
    layer_add_child(window_layer, bitmap_layer_get_layer(up_bitmap_layer));

    select_bitmap_layer = bitmap_layer_create((GRect) { .origin = { bounds.origin.x + bounds.size.w - BITMAP_W - 1, bounds.origin.y + (bounds.size.h - BITMAP_H) / 2 }, .size = { BITMAP_W, BITMAP_H } });
    bitmap_layer_set_bitmap(select_bitmap_layer, start_bitmap);
    bitmap_layer_set_compositing_mode(select_bitmap_layer, CompOp);
    bitmap_layer_set_alignment(select_bitmap_layer, GAlignCenter);
    layer_add_child(window_layer, bitmap_layer_get_layer(select_bitmap_layer));

    down_bitmap_layer = bitmap_layer_create((GRect) { .origin = { bounds.origin.x + bounds.size.w - BITMAP_W - 1, bounds.origin.y + bounds.size.h - BITMAP_H - 35 }, .size = { BITMAP_W, BITMAP_H } });
    bitmap_layer_set_bitmap(down_bitmap_layer, reset_bitmap);
    bitmap_layer_set_compositing_mode(down_bitmap_layer, CompOp);
    bitmap_layer_set_alignment(down_bitmap_layer, GAlignCenter);
    layer_add_child(window_layer, bitmap_layer_get_layer(down_bitmap_layer));

    if (timers[cur_timer].isRunning)
    {
        layer_set_hidden((Layer *)up_bitmap_layer, true);
        layer_set_hidden((Layer *)down_bitmap_layer, true);
        bitmap_layer_set_bitmap(select_bitmap_layer, pause_bitmap);
    }

    const int inset =
#ifdef PBL_PLATFORM_CHALK
    -10;
#else
    0;
#endif
    icon_bitmap_layer = bitmap_layer_create((GRect) { .origin = { bounds.origin.x + 1 + inset, bounds.origin.y + 50 }, .size = { ICON_BITMAP_SIZE, ICON_BITMAP_SIZE } });
    bitmap_layer_set_bitmap(icon_bitmap_layer, timer_icon_cache_get(timers[cur_timer].iconIdx));
    bitmap_layer_set_compositing_mode(icon_bitmap_layer, CompOp);
    bitmap_layer_set_alignment(icon_bitmap_layer, GAlignCenter);
    layer_add_child(window_layer, bitmap_layer_get_layer(icon_bitmap_layer));

    icon_label_text_layer = text_layer_create((GRect) { .origin = { bounds.origin.x + 2 + inset, bounds.origin.y + 50 + ICON_BITMAP_SIZE / 2 + 6 }, .size = { bounds.size.w, 28 } });
    text_layer_set_text(icon_label_text_layer, timer_icon_labels[timers[cur_timer].iconIdx]);
    text_layer_set_font(icon_label_text_layer, fonts_get_system_font(FONT_KEY_GOTHIC_24));
    text_layer_set_text_alignment(icon_label_text_layer, GTextAlignmentLeft);
    text_layer_set_background_color(icon_label_text_layer, GColorClear);
    layer_add_child(window_layer, text_layer_get_layer(icon_label_text_layer));

    bounds = layer_get_frame(window_layer);
    // Set up the status bar last to ensure it is on top of other Layers
    s_timer_status_bar = status_bar_layer_create();
    status_bar_layer_set_colors(s_timer_status_bar, GColorCobaltBlue, GColorWhite);
    layer_add_child(window_layer, status_bar_layer_get_layer(s_timer_status_bar));

    // Show legacy battery meter
    s_timer_battery_layer = layer_create(GRect(0, 0, bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
    layer_set_update_proc(s_timer_battery_layer, battery_proc);
    layer_add_child(window_layer, s_timer_battery_layer);

    timer_update_time();

    if (timers[cur_timer].total_sec == 0 && !timers[cur_timer].isCountingUp)
    {
        app_timer_register(50, number_window_init_timer_callback, 0);
    }

    //APP_LOG(APP_LOG_LEVEL_DEBUG, "timer_window_load() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
}

static void timer_window_unload(Window *window)
{
    persist_write_int(KEY_FIRST_TIMER + cur_timer, timers[cur_timer].total_sec);

    text_layer_destroy(days_text_layer);
    text_layer_destroy(hours_text_layer);
    text_layer_destroy(time_text_layer);
    text_layer_destroy(days_label_text_layer);
    text_layer_destroy(hours_label_text_layer);
    text_layer_destroy(minutes_label_text_layer);
    text_layer_destroy(seconds_label_text_layer);
    text_layer_destroy(icon_label_text_layer);
    bitmap_layer_destroy(up_bitmap_layer);
    bitmap_layer_destroy(select_bitmap_layer);
    bitmap_layer_destroy(down_bitmap_layer);
    bitmap_layer_destroy(icon_bitmap_layer);
    layer_destroy(s_timer_battery_layer);
    status_bar_layer_destroy(s_timer_status_bar);

    menu_layer_reload_data(s_menu_layer);
    menu_layer_set_selected_index(s_menu_layer, timerMenuIndex(cur_timer), MenuRowAlignCenter, false);

    cur_timer = -999999; // invalid

    window_destroy(timer_window);
}

static void timer_window_init(int timer_num)
{
    APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_window_init(%d)", timer_num);

    if (timer_num >= 0)
    {
        // use existing timer
        cur_timer = timer_num;
        timers[cur_timer].total_sec = persist_read_int(KEY_FIRST_TIMER + cur_timer);
    }
    else
    {
        // create a new timer
        cur_timer = num_timers;
        timers[cur_timer].total_sec = 0;
        persist_write_int(TimerItemKey(cur_timer, KEY_TOTAL), timers[cur_timer].total_sec);
        timers[cur_timer].isCountingUp = (timer_num == NEW_STOPWATCH);
        persist_write_int(TimerItemKey(cur_timer, KEY_TYPE), timers[cur_timer].isCountingUp);
        APP_LOG(APP_LOG_LEVEL_DEBUG, "@@ timer_window_init(%d) up:%d", timer_num, timers[cur_timer].isCountingUp);

        num_timers++;
        persist_write_int(KEY_NUM_TIMERS, num_timers);
    }

    timer_window = window_create();
    window_set_click_config_provider(timer_window, timer_click_config_provider);
    window_set_window_handlers(timer_window, (WindowHandlers) {
        .load = timer_window_load,
        .unload = timer_window_unload,
    });
    window_stack_push(timer_window, true);
}

static uint16_t menu_get_num_sections_callback(MenuLayer *menu_layer, void *data)
{
    return NUM_MENU_SECTIONS;
}

static uint16_t menu_get_num_rows_callback(MenuLayer *menu_layer, uint16_t section_index, void *data)
{
    switch (section_index) {
        case SECTION_TIMERS:
        {
            int count = 0;

            for (int i = 0; i < num_timers; i++)
            {
                if (!timers[i].isCountingUp)
                {
                    count++;
                }
            }

            return count;
        }
        case SECTION_STOPWATCHES:
        {
            int count = 0;

            for (int i = 0; i < num_timers; i++)
            {
                if (timers[i].isCountingUp)
                {
                    count++;
                }
            }

            return count;
        }
        case SECTION_NEW_TIMER:
        case SECTION_NEW_STOPWATCH:
            return 1;

        default:
            return 0;
    }
}

static int16_t menu_get_cell_height(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    switch (cell_index->section) {
        case SECTION_TIMERS:
        case SECTION_STOPWATCHES:
            return 30;

        case SECTION_NEW_TIMER:
            if (num_timers < MAX_TIMERS)
                return 30;
            else
                return 0;

        case SECTION_NEW_STOPWATCH:
            if (num_timers < MAX_TIMERS)
                return 30;
            else
                return 0;

        default:
            return 20;
    }
}

static bool s_tick = true;

static void menu_draw_row_callback(GContext* ctx, const Layer *cell_layer, MenuIndex *cell_index, void *data)
{
#ifdef PBL_COLOR
    graphics_context_set_compositing_mode(ctx, GCompOpSet);
#endif
    GRect layer_frame = layer_get_frame((Layer*) cell_layer);
    GSize layer_size = layer_frame.size;
    char title[30];

    switch (cell_index->section) {
        case SECTION_TIMERS:
        case SECTION_STOPWATCHES:
        {
            int32_t time_delta = 0;
            GBitmap *bmp = 0;
            int bmpSize = 12;
            int index = timerIndex(cell_index);

            if (cell_index->section == SECTION_STOPWATCHES)
            {
                time_delta = timers[index].elapsed_sec;

                if (timers[index].isRunning)
                {
                    bmp = start_bitmap;
                }
                else if (timers[index].elapsed_sec > 0)
                {
                    bmp = pause_bitmap;
                }
                else
                {
                    bmp = 0;
                }
            }
            else
            {
                if (timers[index].isRunning)
                {
                    time_delta = timers[index].total_sec - timers[index].elapsed_sec;
                    bmp = start_bitmap;
                }
                else
                {
                    time_delta = timers[index].total_sec;

                    if (timers[index].alert_sec > 0)
                    {
                        if (s_tick)
                        {
                            bmp = running_bitmap;
                            bmpSize = 28;
                        }
                    }
                    else if (timers[index].elapsed_sec > 0)
                    {
                        time_delta -= timers[index].elapsed_sec;
                        bmp = pause_bitmap;
                    }
                    else
                    {
                        bmp = 0;
                    }
                }
            }

            if (s_tick && timers[index].alert_sec > 0)
            {
                snprintf(title, sizeof(title), "%s", timer_icon_labels[timers[index].iconIdx]);
            }
            else
            {
                uint32_t time = time_delta;
                int days = time / 60 / 60 / 24;
                int hours = time / 60 / 60 - days * 24;
                int minutes = time / 60 - days * 24 * 60 - hours * 60;
                int seconds = time - days * 24 * 60 * 60 - hours * 60 * 60 - minutes * 60;

                if (days > 99)
                {
                    snprintf(title, sizeof(title), "%dd%02d", days, hours);
                }
                else if (days > 0)
                {
                    snprintf(title, sizeof(title), "%dd%02d:%02d", days, hours, minutes);
                }
                else if (hours > 0)
                {
                    snprintf(title, sizeof(title), "%2d:%02d:%02d", hours, minutes, seconds);
                }
                else
                {
                    snprintf(title, sizeof(title), "%2d:%02d", minutes, seconds);
                }
            }

            GRect r;
            GSize tsize = graphics_text_layout_get_content_size("999:00:00", fonts_get_system_font(FONT_KEY_GOTHIC_28), layer_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentLeft);
            const int bmpIconSize = 28;

#ifndef PBL_PLATFORM_CHALK
            if (bmp)
            {
                r.origin.x = 10;
                r.origin.y = (layer_size.h - bmpSize) / 2;
                r.size.w = bmpSize;
                r.size.h = r.size.w;
                graphics_draw_bitmap_in_rect(ctx, bmp, r);
            }
            const int rightPad = 2;
#else
            const int rightPad = (layer_size.w - tsize.w - bmpIconSize - bmpSize - 2 * 2) / 2;

#endif
            r.origin.x = layer_size.w - bmpIconSize - rightPad;
            r.origin.y = (layer_size.h - bmpIconSize) / 2;
            r.size.w = bmpIconSize;
            r.size.h = bmpIconSize;

            GBitmap *bmpIcon = timer_icon_cache_get(timers[index].iconIdx);
            if (bmpIcon)
            {
                graphics_draw_bitmap_in_rect(ctx, bmpIcon, r);
            }

#ifdef PBL_PLATFORM_CHALK
            r.origin.x = r.origin.x - bmpSize - 2;
            r.origin.y = (layer_size.h - bmpSize) / 2;
            r.size.w = bmpSize;
            r.size.h = r.size.w;

            if (bmp)
            {
                graphics_draw_bitmap_in_rect(ctx, bmp, r);
            }
#endif

            menu_cell_title_draw(ctx, cell_layer, ""); // HACK: needed for some reason to draw text below

#ifdef PBL_PLATFORM_CHALK
            r.origin.x = 0;
            r.size.w = layer_size.w - bmpSize - bmpIconSize - rightPad - 2;
#else
            r.origin.x = layer_size.w - r.origin.x - 2;
            r.size.w = layer_size.w - r.origin.x - r.size.w - 2;
#endif
            r.origin.y = -4;
            r.size.h = tsize.h;
            graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_28), r, GTextOverflowModeTrailingEllipsis, GTextAlignmentRight, NULL);

            break;
        }

        case SECTION_NEW_TIMER:
            switch (cell_index->row) {
                case 0:
                    if (num_timers < MAX_TIMERS) {
                        strncpy(title, "+ Timer", sizeof(title));
                    } else {
                        strncpy(title, "Max Timers", sizeof(title));
                    }

                    GSize tsize = graphics_text_layout_get_content_size(title, fonts_get_system_font(FONT_KEY_GOTHIC_28), layer_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
                    const int pad = (layer_size.w - tsize.w) / 2;
                    GRect r;
                    r.origin.x =  pad;
                    r.size.w = tsize.w;
                    r.origin.y = -4;
                    r.size.h = tsize.h;

                    menu_cell_title_draw(ctx, cell_layer, ""); // HACK: needed for some reason to draw text below
                    graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_28), r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
                    break;
            }
            break;

        case SECTION_NEW_STOPWATCH:
            switch (cell_index->row) {
                case 0:
                    if (num_timers < MAX_TIMERS) {
                        strncpy(title, "+ Stopwatch", sizeof(title));
                    } else {
                        strncpy(title, "Max Timers", sizeof(title));
                    }

                    GSize tsize = graphics_text_layout_get_content_size(title, fonts_get_system_font(FONT_KEY_GOTHIC_28), layer_frame, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter);
                    const int pad = (layer_size.w - tsize.w) / 2;
                    GRect r;
                    r.origin.x =  pad;
                    r.size.w = tsize.w;
                    r.origin.y = -4;
                    r.size.h = tsize.h;

                    menu_cell_title_draw(ctx, cell_layer, ""); // HACK: needed for some reason to draw text below
                    graphics_draw_text(ctx, title, fonts_get_system_font(FONT_KEY_GOTHIC_28), r, GTextOverflowModeTrailingEllipsis, GTextAlignmentCenter, NULL);
                    break;
            }
            break;
    }
    s_tick = !s_tick;
}

void menu_select_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    switch (cell_index->section) {
        case SECTION_TIMERS:
        case SECTION_STOPWATCHES:
            timer_window_init(timerIndex(cell_index));
            break;

        case SECTION_NEW_TIMER:
            if (cell_index->row == 0) {
                if (num_timers < MAX_TIMERS)
                {
                    timer_window_init(NEW_TIMER);
                }
            }
            break;

        case SECTION_NEW_STOPWATCH:
            if (cell_index->row == 0) {
                if (num_timers < MAX_TIMERS)
                {
                    timer_window_init(NEW_STOPWATCH);
                }
            }
            break;
    }
}

void menu_select_long_click_callback(MenuLayer *menu_layer, MenuIndex *cell_index, void *data)
{
    switch (cell_index->section) {
        case SECTION_TIMERS:
        case SECTION_STOPWATCHES:
            if (!timer_toggle(timerIndex(cell_index)))
            {
                menu_layer_reload_data(menu_layer);
            }
            break;

        case SECTION_NEW_TIMER:
        case SECTION_NEW_STOPWATCH:
            break;
    }
}

static void window_load(Window *window)
{
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "window_load() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
    Layer *window_layer = window_get_root_layer(window);
    GRect bounds = layer_get_bounds(window_layer);

    running_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RUNNING);
    trash_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_TRASH);
    start_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_START);
    pause_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_PAUSE);
    setup_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_SETUP);
    reset_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_RESET);
    vibe_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_VIBRATION);
    stopwatch_bitmap = gbitmap_create_with_resource(RESOURCE_ID_IMAGE_STOPWATCH);

    timer_icon_cache_init();

    cur_timer = -999999; // invalid

    if (!persist_exists(KEY_NUM_TIMERS))
    {
        persist_write_int(TimerItemKey(0, KEY_TOTAL), 60);
        persist_write_int(TimerItemKey(1, KEY_TOTAL), 5*60); // 5 min
        persist_write_int(TimerItemKey(2, KEY_TOTAL), 10*60); // 10 min
        persist_write_int(KEY_NUM_TIMERS, 3);
    }

    num_timers = persist_read_int(KEY_NUM_TIMERS);

    int version = 0;

    if (persist_exists(KEY_VERSION))
    {
        version = persist_read_int(KEY_VERSION);
    }

    if (version >= 2 && persist_exists(KEY_SHUTDOWN_TIME))
    {
        time_t now_time = time(NULL);
        time_t shutdown_time = persist_read_int(KEY_SHUTDOWN_TIME);
        uint32_t elapsed = difftime(now_time, shutdown_time);

        for (int i = 0; i < num_timers; i++)
        {
            timers[i].total_sec = persist_read_int(TimerItemKey(i, KEY_TOTAL));
            timers[i].elapsed_sec = persist_read_int(TimerItemKey(i, KEY_ELAPSED));

            if (version == 2)
            {
                if (timers[i].elapsed_sec > 0)
                {
                    timers[i].elapsed_sec += elapsed;
                }

                if (timers[i].elapsed_sec > 0)
                {
                    timers[i].isRunning = true;
                }
            }
            if (version >= 3)
            {
                timers[i].isRunning = persist_read_int(TimerItemKey(i, KEY_ISRUNNING));
                if (timers[i].isRunning)
                {
                    timers[i].elapsed_sec += elapsed;
                }
            }
            if (version >= 4)
            {
                timers[i].iconIdx = persist_read_int(TimerItemKey(i, KEY_ICON));
            }
            if (version >= 5)
            {
                timers[i].isCountingUp = persist_read_int(TimerItemKey(i, KEY_TYPE));
                timers[i].vibeIdx = persist_read_int(TimerItemKey(i, KEY_VIBE));
                timers[i].vibeRepeat = persist_read_int(TimerItemKey(i, KEY_VIBE_REPEAT));
            }
        }

        if (launch_reason() == APP_LAUNCH_WAKEUP)
        {
            vibes_short_pulse();
        }

        persist_delete(KEY_SHUTDOWN_TIME);
    }
    else
    {
        for (int i = 0; i < num_timers; i++)
        {
            timers[i].total_sec = persist_read_int(TimerItemKey(i, KEY_TOTAL));
        }
    }

    wakeup_cancel_all();

    tick_timer_service_subscribe(SECOND_UNIT, &timer_handle_tick);

    // Set up the status bar last to ensure it is on top of other Layers
    s_status_bar = status_bar_layer_create();
    status_bar_layer_set_colors(s_status_bar, GColorCobaltBlue, GColorWhite);
    layer_add_child(window_layer, status_bar_layer_get_layer(s_status_bar));

    // Show legacy battery meter
    s_battery_layer = layer_create(GRect(bounds.origin.x, bounds.origin.y, bounds.size.w, STATUS_BAR_LAYER_HEIGHT));
    layer_set_update_proc(s_battery_layer, battery_proc);
    layer_add_child(window_layer, s_battery_layer);
    bounds.origin.y += STATUS_BAR_LAYER_HEIGHT;
    bounds.size.h -= STATUS_BAR_LAYER_HEIGHT;

    s_menu_layer = menu_layer_create(bounds);

    menu_layer_set_callbacks(s_menu_layer, NULL, (MenuLayerCallbacks){
        .get_num_sections = menu_get_num_sections_callback,
        .get_num_rows = menu_get_num_rows_callback,
        .get_cell_height = menu_get_cell_height,
        .draw_row = menu_draw_row_callback,
        .select_click = menu_select_click_callback,
        .select_long_click = menu_select_long_click_callback,
    });

    menu_layer_set_click_config_onto_window(s_menu_layer, window);
#ifdef PBL_COLOR
    menu_layer_set_highlight_colors(s_menu_layer, GColorVividCerulean, GColorBlack);
#endif
    layer_add_child(window_layer, menu_layer_get_layer(s_menu_layer));
    setupContentIndicators(window_layer, bounds, s_menu_layer, &s_indicator, &s_indicator_up_layer, &s_indicator_down_layer, &s_up_config, &s_down_config);
    MenuIndex index = (MenuIndex){ .row = persist_read_int(KEY_SELECTED_MENU_ROW), .section = persist_read_int(KEY_SELECTED_MENU_SECTION)};
    menu_layer_set_selected_index(s_menu_layer, index, MenuRowAlignCenter, false);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "window_load() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
}

static void window_appear(Window *window)
{
    menu_layer_reload_data(s_menu_layer);
}

static void window_unload(Window *window)
{
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "window_unload() free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
    MenuIndex index = menu_layer_get_selected_index(s_menu_layer);
    persist_write_int(KEY_SELECTED_MENU_SECTION, index.section);
    persist_write_int(KEY_SELECTED_MENU_ROW, index.row);
    persist_write_int(KEY_VERSION, 5);
    persist_write_int(KEY_NUM_TIMERS, num_timers);

    time_t shutdown_time = time(NULL);
    persist_write_int(KEY_SHUTDOWN_TIME, shutdown_time);

    bool isRunning = false;
    uint32_t running_remaining_sec = 1999999999;

    for (int i = 0; i < num_timers; i++)
    {
        persist_write_int(TimerItemKey(i, KEY_TOTAL), timers[i].total_sec);
        persist_write_int(TimerItemKey(i, KEY_ELAPSED), timers[i].elapsed_sec);
        persist_write_int(TimerItemKey(i, KEY_ISRUNNING), timers[i].isRunning);
        persist_write_int(TimerItemKey(i, KEY_ICON), timers[i].iconIdx);
        persist_write_int(TimerItemKey(i, KEY_TYPE), timers[i].isCountingUp);
        persist_write_int(TimerItemKey(i, KEY_VIBE), timers[i].vibeIdx);
        persist_write_int(TimerItemKey(i, KEY_VIBE_REPEAT), timers[i].vibeRepeat);

        if (timers[i].isRunning && !timers[i].isCountingUp)
        {
            isRunning = true;
            uint32_t remaining_sec = timers[i].total_sec - timers[i].elapsed_sec;
            if (remaining_sec < running_remaining_sec)
            {
                running_remaining_sec = remaining_sec;
            }
        }
    }

    gbitmap_destroy(start_bitmap);
    gbitmap_destroy(pause_bitmap);
    gbitmap_destroy(setup_bitmap);
    gbitmap_destroy(vibe_bitmap);
    gbitmap_destroy(stopwatch_bitmap);
    gbitmap_destroy(running_bitmap);
    gbitmap_destroy(trash_bitmap);
    gbitmap_destroy(reset_bitmap);

    timer_icon_cache_destroy();
    menu_layer_destroy(s_menu_layer);
    layer_destroy(s_battery_layer);
    status_bar_layer_destroy(s_status_bar);
    layer_destroy(s_indicator_up_layer);
    layer_destroy(s_indicator_down_layer);

    if (isRunning)
    {
        time_t wake_time = shutdown_time + running_remaining_sec - 8;
        time_t now = time(NULL);

        if (wake_time < now)
        {
            // took too long to shutdown so wakeup time is now in the past
            wake_time = now + 5;
        }

        wakeup_schedule(wake_time, 0, true);
    }
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "window_unload() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
}

static void init(void)
{
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "init() free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
    window = window_create();
    window_set_window_handlers(window, (WindowHandlers) {
        .load = window_load,
        .unload = window_unload,
        .appear = window_appear
    });
    window_stack_push(window, true);

    app_message_register_outbox_failed(outbox_failed_callback);
    app_message_register_outbox_sent(outbox_sent_callback);
    AppMessageResult result = app_message_open(64, 100);
    if (result != APP_MSG_OK) {
        APP_LOG(APP_LOG_LEVEL_ERROR, "init app_message_open failed. %s", translate_error(result));
    }
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "init() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
}

static void deinit(void)
{
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit() free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
    app_message_deregister_callbacks();
    window_destroy(window);
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "deinit() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
}

int main(void) {
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "init() free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
    init();
    app_event_loop();
    deinit();
    //APP_LOG(APP_LOG_LEVEL_DEBUG, "init() END free:%d, used:%d", (int) heap_bytes_free(), heap_bytes_used());
}
