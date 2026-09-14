#include "bridge_client.h"
#include "icons.h"

#include <Xm/CascadeB.h>
#include <Xm/DrawingA.h>
#include <Xm/Form.h>
#include <Xm/Frame.h>
#include <Xm/Label.h>
#include <Xm/List.h>
#include <Xm/MainW.h>
#include <Xm/Protocols.h>
#include <Xm/PushB.h>
#include <Xm/RowColumn.h>
#include <Xm/Scale.h>
#include <Xm/ScrolledW.h>
#include <Xm/SeparatoG.h>
#include <Xm/TextF.h>
#include <Xm/ToggleB.h>
#include <Xm/Xm.h>
#include <X11/Shell.h>
#include <X11/Xutil.h>
#include <X11/xpm.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

static BridgeClient bridge;
static Widget status_label;
static Widget volume_popup;
static Widget volume_button;
static Widget position_scale;
static Widget play_pause_button;
static Widget browser_list_scroller;
static Widget browser_list_canvas;
static Widget source_list_widget;
static Widget list_frame;
static Widget artwork_label;
static Widget main_artwork_label;
static Widget album_grid;
static Widget album_grid_scroller;
static Widget grid_view_button;
static Widget album_tracks_popup;
static Widget album_tracks_title;
static Widget album_tracks_list;
static Widget sidebar_frame;
static Widget sidebar_sizer;
static Widget right_pane_widget;
static Widget browser_widget;
static Widget search_widget;
static Pixmap artwork_pixmap = XmUNSPECIFIED_PIXMAP;
static Pixmap main_artwork_pixmap = XmUNSPECIFIED_PIXMAP;
static Pixmap shuffle_list_pixmap = XmUNSPECIFIED_PIXMAP;
static Pixmap repeat_list_pixmap = XmUNSPECIFIED_PIXMAP;
static Pixmap automix_list_pixmap = XmUNSPECIFIED_PIXMAP;
static Pixmap autoplay_list_pixmap = XmUNSPECIFIED_PIXMAP;
typedef struct {
    Widget widget;
    char **xpm;
    Pixmap pixmap;
    int active;
} ButtonIconState;
static ButtonIconState button_icons[12];
static unsigned int button_icon_count;
static Pixmap album_grid_pixmaps[100];
static unsigned int album_grid_pixmap_count;
static int browser_mode;
static unsigned int album_grid_columns = 1;
static int album_grid_origin_x = 8;
static int album_grid_timer_pending;
static char album_popup_catalog_ids[100][256];
static char album_popup_kinds[100][32];
static unsigned int album_popup_count;
static int player_is_playing;
static double player_duration;
static unsigned int player_poll_count;
static XtIntervalId player_poll_timer = (XtIntervalId)0;
static int prior_position_percent = -1;
static int shutting_down;
static char current_view_path[512] = "/v1/listen-now";
static char populated_view_path[512];
static char current_view_type[32] = "songs";
static char current_view_heading[512] = "Listen Now";
static char current_view_container_kind[32];
static char current_view_ids[100][256];
static char current_view_catalog_ids[100][256];
static char current_view_item_kinds[100][32];
static char current_view_labels[100][1200];
static char current_view_titles[100][512];
static char current_view_artists[100][512];
static unsigned int current_view_durations[100];
static int current_view_is_current[100];
static int current_view_is_autoplay[100];
static char current_view_artwork_urls[100][1024];
static unsigned int current_view_disc_numbers[100];
static unsigned int current_view_track_numbers[100];
static int current_view_container_rows[100];
static char source_paths[107][512];
static char source_titles[107][512];
static unsigned int source_count;
static int sidebar_drag_start_x;
static Dimension sidebar_drag_start_width;
static unsigned int sidebar_artwork_size = 160;
static int now_playing_source_visible;
static unsigned int visible_to_model[100];
static unsigned int visible_view_count;
static char current_filter[256];
static unsigned int current_view_count;
static unsigned int current_render_limit = 30;
static int current_view_is_track_detail;
static int current_view_collapsed;
static int list_lazy_loading;
static int browser_scroll_last_value = -1;
static int browser_scroll_redraw_from = -1;
static int browser_scroll_redraw_to = -1;
static int browser_scroll_redraw_pending;
static XtIntervalId browser_resize_redraw_timer = (XtIntervalId)0;
static int volume_popup_visible;
static int volume_icon_level = -1;
static int player_shuffle_enabled;
static int player_repeat_mode;
static int player_automix_enabled;
static int player_autoplay_enabled;
static int rendered_shuffle_state = -1;
static int rendered_repeat_mode = -1;
static int rendered_automix_state = -1;
static int rendered_autoplay_state = -1;
static int browser_list_selected_model = -1;
static int browser_list_last_click_model = -1;
static Time browser_list_last_click_time;
static XtAppContext application_context;

#define GRID_CELL 112
#define GRID_ARTWORK 96
#define GRID_MARGIN 8
#define LIST_TITLE_HEIGHT 26
#define LIST_ROW_HEIGHT 19
#define LIST_SIDE_PAD 8
#define LIST_ICON_SIZE 20
#define LIST_SMALL_ICON_SIZE 16
#define LIST_ICON_GAP 6

static const char *json_key(const char *json, const char *key)
{
    char needle[96];
    const char *value;
    snprintf(needle, sizeof(needle), "\"%s\"", key);
    value = strstr(json, needle);
    if (value == NULL) return NULL;
    value = strchr(value + strlen(needle), ':');
    if (value == NULL) return NULL;
    do { value++; } while (*value == ' ' || *value == '\t');
    return value;
}

static int json_string(const char *json, const char *key, char *output, size_t size)
{
    const char *value = json_key(json, key);
    size_t used = 0;
    if (value == NULL || *value != '"' || size == 0) return 0;
    value++;
    while (*value != '\0' && *value != '"' && used + 1 < size) {
        if (*value == '\\' && value[1] != '\0') {
            value++;
            if (*value == 'n') output[used++] = ' ';
            else if (*value == 't') output[used++] = ' ';
            else output[used++] = *value;
        } else output[used++] = *value;
        value++;
    }
    output[used] = '\0';
    return 1;
}

static int json_number(const char *json, const char *key, double *output)
{
    const char *value = json_key(json, key);
    char *end;
    if (value == NULL) return 0;
    *output = strtod(value, &end);
    return end != value;
}

static int json_boolean(const char *json, const char *key, int *output)
{
    const char *value = json_key(json, key);
    if (value == NULL) return 0;
    if (strncmp(value, "true", 4) == 0) { *output = 1; return 1; }
    if (strncmp(value, "false", 5) == 0) { *output = 0; return 1; }
    return 0;
}

static Pixmap make_button_pixmap_color(Widget widget, char **xpm, Pixel background)
{
    Display *display = XtDisplay(widget);
    Screen *screen = XtScreen(widget);
    Pixmap source = None, mask = None, result;
    GC gc;
    XGCValues values;
    XpmAttributes attributes;

    memset(&attributes, 0, sizeof(attributes));
    attributes.valuemask = XpmSize;
    if (XpmCreatePixmapFromData(display, RootWindowOfScreen(screen), xpm,
                                &source, &mask, &attributes) != XpmSuccess)
        return XmUNSPECIFIED_PIXMAP;

    result = XCreatePixmap(display, RootWindowOfScreen(screen),
                           attributes.width, attributes.height,
                           DefaultDepthOfScreen(screen));
    values.foreground = background;
    gc = XCreateGC(display, result, GCForeground, &values);
    XFillRectangle(display, result, gc, 0, 0, attributes.width, attributes.height);
    if (mask != None) {
        XSetClipMask(display, gc, mask);
        XSetClipOrigin(display, gc, 0, 0);
    }
    XCopyArea(display, source, result, gc, 0, 0,
              attributes.width, attributes.height, 0, 0);
    XFreeGC(display, gc);
    XFreePixmap(display, source);
    if (mask != None) XFreePixmap(display, mask);
    return result;
}

static Pixmap make_button_pixmap(Widget widget, char **xpm)
{
    Pixel background;
    XtVaGetValues(widget, XmNbackground, &background, NULL);
    return make_button_pixmap_color(widget, xpm, background);
}

static ButtonIconState *find_button_icon(Widget widget)
{
    unsigned int i;
    for (i = 0; i < button_icon_count; ++i)
        if (button_icons[i].widget == widget) return &button_icons[i];
    return NULL;
}

static void render_button_icon(ButtonIconState *state)
{
    Pixel background;
    Pixmap replacement;
    XtVaGetValues(state->widget,
                  state->active ? XmNarmColor : XmNbackground, &background, NULL);
    replacement = make_button_pixmap_color(state->widget, state->xpm, background);
    if (replacement == XmUNSPECIFIED_PIXMAP) return;
    XtVaSetValues(state->widget, XmNlabelPixmap, replacement, NULL);
    if (state->pixmap != XmUNSPECIFIED_PIXMAP && state->pixmap != None)
        XFreePixmap(XtDisplay(state->widget), state->pixmap);
    state->pixmap = replacement;
}

static void button_icon_event(Widget widget, XtPointer client_data,
                              XEvent *event, Boolean *continue_dispatch)
{
    ButtonIconState *state = (ButtonIconState *)client_data;
    (void)widget; (void)continue_dispatch;
    state->active = event->type == EnterNotify || event->type == ButtonPress;
    if (event->type == LeaveNotify) state->active = 0;
    render_button_icon(state);
}

static void register_button_icon(Widget widget, char **xpm, Pixmap pixmap)
{
    ButtonIconState *state;
    if (button_icon_count >= 8) return;
    state = &button_icons[button_icon_count++];
    state->widget = widget; state->xpm = xpm; state->pixmap = pixmap; state->active = 0;
    XtAddEventHandler(widget, EnterWindowMask | LeaveWindowMask | ButtonPressMask,
                      False, button_icon_event, state);
}

static void set_button_icon(Widget widget, char **xpm)
{
    ButtonIconState *state = find_button_icon(widget);
    if (state == NULL || state->xpm == xpm) return;
    state->xpm = xpm;
    render_button_icon(state);
}

static void update_artwork_widget(Widget widget, unsigned int size, Pixmap *stored_pixmap);
static void browser_list_reflow(void);
static void refresh_browser_list(void);
static void browser_list_event(Widget widget, XtPointer client_data,
                               XEvent *event, Boolean *continue_dispatch);
static void shuffle_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
static void repeat_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
static void automix_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
static void autoplay_toggled(Widget widget, XtPointer client_data, XtPointer call_data);

static const char *repeat_mode_name(int mode)
{
    return mode == 2 ? "one" : (mode == 1 ? "all" : "off");
}

static void damage_list_title_row(void)
{
    if (browser_list_canvas != NULL && XtIsRealized(browser_list_canvas))
        XClearArea(XtDisplay(browser_list_canvas), XtWindow(browser_list_canvas),
                   0, 0, 0, LIST_TITLE_HEIGHT, True);
}

static void update_shuffle_button(void)
{
    if (browser_list_canvas == NULL || rendered_shuffle_state == player_shuffle_enabled) return;
    rendered_shuffle_state = player_shuffle_enabled;
    if (shuffle_list_pixmap != XmUNSPECIFIED_PIXMAP && shuffle_list_pixmap != None)
        XFreePixmap(XtDisplay(browser_list_canvas), shuffle_list_pixmap);
    shuffle_list_pixmap = make_button_pixmap(browser_list_canvas,
        player_shuffle_enabled ? shuffle_on_icon_xpm : shuffle_off_icon_xpm);
    damage_list_title_row();
}
