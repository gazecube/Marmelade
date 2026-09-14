#ifndef MARMELADE_APP_INTERNAL_H
#define MARMELADE_APP_INTERNAL_H

#include "bridge_client.h"

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

typedef struct {
    Widget widget;
    char **xpm;
    Pixmap pixmap;
    int active;
} ButtonIconState;

#define GRID_CELL 112
#define GRID_ARTWORK 96
#define GRID_MARGIN 8
#define LIST_TITLE_HEIGHT 26
#define LIST_ROW_HEIGHT 19
#define LIST_SIDE_PAD 8
#define LIST_ICON_SIZE 20
#define LIST_SMALL_ICON_SIZE 16
#define LIST_ICON_GAP 6

extern BridgeClient bridge;
extern Widget status_label;
extern Widget volume_popup;
extern Widget volume_button;
extern Widget position_scale;
extern Widget play_pause_button;
extern Widget browser_list_scroller;
extern Widget browser_list_canvas;
extern Widget source_list_widget;
extern Widget list_frame;
extern Widget artwork_label;
extern Widget main_artwork_label;
extern Widget album_grid;
extern Widget album_grid_scroller;
extern Widget grid_view_button;
extern Widget album_tracks_popup;
extern Widget album_tracks_title;
extern Widget album_tracks_list;
extern Widget sidebar_frame;
extern Widget sidebar_sizer;
extern Widget right_pane_widget;
extern Widget browser_widget;
extern Widget search_widget;
extern Pixmap artwork_pixmap;
extern Pixmap main_artwork_pixmap;
extern Pixmap shuffle_list_pixmap;
extern Pixmap repeat_list_pixmap;
extern Pixmap automix_list_pixmap;
extern Pixmap autoplay_list_pixmap;
extern ButtonIconState button_icons[12];
extern unsigned int button_icon_count;
extern Pixmap album_grid_pixmaps[100];
extern unsigned int album_grid_pixmap_count;
extern int browser_mode;
extern unsigned int album_grid_columns;
extern int album_grid_origin_x;
extern int album_grid_timer_pending;
extern char album_popup_catalog_ids[100][256];
extern char album_popup_kinds[100][32];
extern unsigned int album_popup_count;
extern int player_is_playing;
extern double player_duration;
extern unsigned int player_poll_count;
extern XtIntervalId player_poll_timer;
extern int prior_position_percent;
extern int shutting_down;
extern char current_view_path[512];
extern char populated_view_path[512];
extern char current_view_type[32];
extern char current_view_heading[512];
extern char current_view_container_kind[32];
extern char current_view_ids[100][256];
extern char current_view_catalog_ids[100][256];
extern char current_view_item_kinds[100][32];
extern char current_view_labels[100][1200];
extern char current_view_titles[100][512];
extern char current_view_artists[100][512];
extern unsigned int current_view_durations[100];
extern int current_view_is_current[100];
extern int current_view_is_autoplay[100];
extern char current_view_artwork_urls[100][1024];
extern unsigned int current_view_disc_numbers[100];
extern unsigned int current_view_track_numbers[100];
extern int current_view_container_rows[100];
extern char source_paths[107][512];
extern char source_titles[107][512];
extern unsigned int source_count;
extern int sidebar_drag_start_x;
extern Dimension sidebar_drag_start_width;
extern unsigned int sidebar_artwork_size;
extern int now_playing_source_visible;
extern unsigned int visible_to_model[100];
extern unsigned int visible_view_count;
extern char current_filter[256];
extern unsigned int current_view_count;
extern unsigned int current_render_limit;
extern int current_view_is_track_detail;
extern int current_view_collapsed;
extern int list_lazy_loading;
extern int browser_scroll_last_value;
extern int browser_scroll_redraw_from;
extern int browser_scroll_redraw_to;
extern int browser_scroll_redraw_pending;
extern XtIntervalId browser_resize_redraw_timer;
extern int volume_popup_visible;
extern int volume_icon_level;
extern int player_shuffle_enabled;
extern int player_repeat_mode;
extern int player_automix_enabled;
extern int player_autoplay_enabled;
extern int rendered_shuffle_state;
extern int rendered_repeat_mode;
extern int rendered_automix_state;
extern int rendered_autoplay_state;
extern int browser_list_selected_model;
extern int browser_list_last_click_model;
extern Time browser_list_last_click_time;
extern XtAppContext application_context;

const char *json_key(const char *json, const char *key);
int json_string(const char *json, const char *key, char *output, size_t size);
int json_number(const char *json, const char *key, double *output);
int json_boolean(const char *json, const char *key, int *output);
Pixmap make_button_pixmap_color(Widget widget, char **xpm, Pixel background);
Pixmap make_button_pixmap(Widget widget, char **xpm);
ButtonIconState *find_button_icon(Widget widget);
void render_button_icon(ButtonIconState *state);
void button_icon_event(Widget widget, XtPointer client_data, XEvent *event, Boolean *continue_dispatch);
void register_button_icon(Widget widget, char **xpm, Pixmap pixmap);
void set_button_icon(Widget widget, char **xpm);
const char *repeat_mode_name(int mode);
void damage_list_title_row(void);
void update_shuffle_button(void);
void update_repeat_button(void);
void update_automix_button(void);
void update_autoplay_button(void);
void set_status(const char *text);
void set_volume_icon(double volume);
XmFontList browser_list_font_list(void);
void draw_browser_text(Display *display, Drawable drawable, GC gc, const char *text, int x, int y, int width, unsigned char alignment);
void draw_browser_row_columns(Display *display, Drawable drawable, GC gc, unsigned int model, int y, int width);
void sync_browser_scheme_colors(void);
void browser_list_reflow(void);
void refresh_browser_list(void);
void repaint_browser_visible_area(void);
void browser_resize_redraw_timeout(XtPointer client_data, XtIntervalId *timer_id);
void schedule_browser_resize_redraw(void);
void draw_browser_list(XExposeEvent *expose);
int browser_visible_row_for_model(int model);
void redraw_browser_model_row(int model);
void set_browser_selection(int model);
int contains_case_insensitive(const char *text, const char *needle);
void apply_filter(const char *filter);
int populate_view(const char *response);
void load_view(const char *path);
void search_filter_changed(Widget widget, XtPointer client_data, XtPointer call_data);
void url_encode(const char *input, char *output, size_t size);
void search_activated(Widget widget, XtPointer client_data, XtPointer call_data);
void refresh_library(void);
void source_selected(Widget widget, XtPointer client_data, XtPointer call_data);
size_t utf8_sequence_length(unsigned char c);
int sidebar_text_width(const char *text);
void sidebar_display_title(const char *title, int max_pixels, char *display, size_t display_size);
int sidebar_available_label_width(void);
void refresh_source_labels(void);
unsigned int sidebar_artwork_size_for_width(Dimension width);
void resize_sidebar_artwork(Dimension width, int refresh_pixmap);
void sidebar_sizer_expose(Widget widget, XtPointer client_data, XtPointer call_data);
void sidebar_sizer_event(Widget widget, XtPointer client_data, XEvent *event, Boolean *continue_dispatch);
void select_current_source(void);
void populate_sources(void);
void view_model_selected(unsigned int index);
void view_model_opened(unsigned int index);
void browser_list_event(Widget widget, XtPointer client_data, XEvent *event, Boolean *continue_dispatch);
unsigned long rgb_component_pixel(unsigned char component, unsigned long mask);
Pixmap rgb_pixmap_from_bytes(Widget widget, const unsigned char *rgb, unsigned int width, unsigned int height);
Pixmap fetch_artwork_pixmap(Widget widget, unsigned int size, const char *url);
void update_artwork_widget(Widget widget, unsigned int size, Pixmap *stored_pixmap);
void update_artwork(void);
Pixmap load_album_grid_pixmap(const char *url, unsigned int index);
void queue_album_popup_track(Widget widget, XtPointer client_data, XtPointer call_data);
void show_album_tracks(unsigned int index, int cell_x, int cell_y);
void reflow_album_grid(void);
void draw_album_grid(XExposeEvent *expose);
void grid_artwork_timeout(XtPointer client_data, XtIntervalId *timer_id);
void schedule_grid_artwork(void);
void album_grid_scrolled(Widget widget, XtPointer client_data, XtPointer call_data);
void album_grid_event(Widget widget, XtPointer client_data, XEvent *event, Boolean *continue_dispatch);
void populate_album_grid(void);
void view_mode_selected(Widget widget, XtPointer client_data, XtPointer call_data);
void show_sidebar_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
void show_sidebar_artwork_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
void extend_lazy_views(void);
void redraw_browser_scroll_strip(int old_value, int new_value);
void browser_scroll_redraw_timeout(XtPointer client_data, XtIntervalId *timer_id);
void repaint_scrollbar_widget(Widget scrollbar);
void list_scrolled(Widget widget, XtPointer client_data, XtPointer call_data);
void repaint_position_scale(int percent);
void schedule_player_poll(unsigned long delay_ms);
void poll_player(XtPointer client_data, XtIntervalId *timer_id);
void player_action(Widget widget, XtPointer client_data, XtPointer call_data);
void volume_changed(Widget widget, XtPointer client_data, XtPointer call_data);
void position_changed(Widget widget, XtPointer client_data, XtPointer call_data);
void position_volume_popup(void);
void follow_volume_popup(Widget widget, XtPointer client_data, XEvent *event, Boolean *continue_dispatch);
void toggle_volume_popup(Widget widget, XtPointer client_data, XtPointer call_data);
void dismiss_volume_popup(Widget widget, XtPointer client_data, XEvent *event, Boolean *continue_dispatch);
Widget icon_button(Widget parent, const char *name, const char *action, char **xpm);
Widget popup_icon_button(Widget parent, const char *name, char **xpm);
void play_pause_action(Widget widget, XtPointer client_data, XtPointer call_data);
Widget play_pause_icon_button(Widget parent);
void shuffle_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
void repeat_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
void automix_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
void autoplay_toggled(Widget widget, XtPointer client_data, XtPointer call_data);
void shutdown_app(Widget widget, XtPointer client_data, XtPointer call_data);
int command_available(const char *program);
void show_fatal_startup_error(const char *message);
void authorize_music(Widget widget, XtPointer client_data, XtPointer call_data);
int main(int argc, char **argv);

#endif
