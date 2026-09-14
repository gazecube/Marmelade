#include "app_internal.h"

#define PLAYLIST_RETRY_DELAY_MS 750
#define PLAYLIST_RETRY_LIMIT 10

static XtIntervalId playlist_retry_timer = 0;
static unsigned int playlist_retry_attempts = 0;
static Widget sidebar_source_scroller = NULL;
static Widget sidebar_source_canvas = NULL;
static int sidebar_selected_index = -1;

size_t utf8_sequence_length(unsigned char c)
{
    if ((c & 0x80u) == 0) return 1;
    if ((c & 0xe0u) == 0xc0u) return 2;
    if ((c & 0xf0u) == 0xe0u) return 3;
    if ((c & 0xf8u) == 0xf0u) return 4;
    return 1;
}

int sidebar_text_width(const char *text)
{
    XmFontList font_list = NULL;
    XmString value;
    Dimension width = 0;
    if (source_list_widget == NULL || text == NULL) return 0;
    XtVaGetValues(source_list_widget, XmNfontList, &font_list, NULL);
    if (font_list == NULL) return 0;
    value = XmStringCreateLocalized((char *)text);
    width = XmStringWidth(font_list, value);
    XmStringFree(value);
    return (int)width;
}

void sidebar_display_title(const char *title, int max_pixels,
                           char *display, size_t display_size)
{
    const unsigned char *p = (const unsigned char *)title;
    size_t boundaries[512], bytes = 0, count = 0, i;
    if (display_size == 0) return;
    display[0] = '\0';
    if (title == NULL) return;
    if (max_pixels <= 0 || sidebar_text_width(title) <= max_pixels) {
        snprintf(display, display_size, "%s", title);
        return;
    }
    while (p[bytes] != '\0' && count < sizeof(boundaries) / sizeof(boundaries[0])) {
        size_t n = utf8_sequence_length(p[bytes]);
        size_t j;
        int valid = 1;
        for (j = 1; j < n; ++j) {
            if (p[bytes + j] == '\0' || (p[bytes + j] & 0xc0u) != 0x80u) {
                valid = 0;
                break;
            }
        }
        bytes += valid ? n : 1;
        boundaries[count++] = bytes;
    }
    for (i = count; i > 0; --i) {
        size_t keep = boundaries[i - 1];
        if (keep + 4 > display_size) continue;
        memcpy(display, title, keep);
        memcpy(display + keep, "...", 4);
        if (sidebar_text_width(display) <= max_pixels) return;
    }
    snprintf(display, display_size, "...");
}

int sidebar_available_label_width(void)
{
    Dimension width = 0;
    if (sidebar_source_canvas != NULL)
        XtVaGetValues(sidebar_source_canvas, XmNwidth, &width, NULL);
    else if (source_list_widget != NULL)
        XtVaGetValues(source_list_widget, XmNwidth, &width, NULL);
    else
        return 140;
    return width > 16 ? (int)width - 16 : 1;
}

static void sidebar_list_reflow(void)
{
    Widget clip = NULL;
    Dimension width = 140, height = 120, content_height;

    if (sidebar_source_scroller == NULL || sidebar_source_canvas == NULL) return;
    XtVaGetValues(sidebar_source_scroller, XmNclipWindow, &clip, NULL);
    if (clip != NULL)
        XtVaGetValues(clip, XmNwidth, &width, XmNheight, &height, NULL);
    else
        XtVaGetValues(sidebar_source_scroller, XmNwidth, &width, XmNheight, &height, NULL);

    if (width < 48) width = 48;
    content_height = (Dimension)(source_count * LIST_ROW_HEIGHT);
    if (content_height < height) content_height = height;
    if (content_height < LIST_ROW_HEIGHT) content_height = LIST_ROW_HEIGHT;
    XtVaSetValues(sidebar_source_canvas,
                  XmNwidth, width,
                  XmNheight, content_height,
                  NULL);
}

static void redraw_sidebar_row(int row)
{
    if (row < 0 || (unsigned int)row >= source_count ||
        sidebar_source_canvas == NULL || !XtIsRealized(sidebar_source_canvas)) return;
    XClearArea(XtDisplay(sidebar_source_canvas), XtWindow(sidebar_source_canvas),
               0, row * LIST_ROW_HEIGHT, 0, LIST_ROW_HEIGHT, True);
}

static void draw_sidebar_list(XExposeEvent *expose)
{
    Display *display;
    Window window;
    Screen *screen;
    Colormap colormap;
    XmFontList font_list = NULL;
    Pixel foreground, background, derived_foreground, top, bottom, select;
    XGCValues values;
    GC gc;
    Dimension width = 0;
    unsigned int first, last, row;
    char display_title[512];

    if (sidebar_source_canvas == NULL || source_list_widget == NULL ||
        !XtIsRealized(sidebar_source_canvas)) return;

    display = XtDisplay(sidebar_source_canvas);
    window = XtWindow(sidebar_source_canvas);
    screen = XtScreen(sidebar_source_canvas);
    colormap = DefaultColormapOfScreen(screen);
    XtVaGetValues(source_list_widget,
                  XmNfontList, &font_list,
                  XmNforeground, &foreground,
                  XmNbackground, &background,
                  NULL);
    XtVaGetValues(sidebar_source_canvas, XmNwidth, &width, NULL);
    if (font_list == NULL) return;

    XmGetColors(screen, colormap, background,
                &derived_foreground, &top, &bottom, &select);
    values.foreground = foreground;
    values.background = background;
    gc = XCreateGC(display, window, GCForeground | GCBackground, &values);

    first = expose->y > 0 ? (unsigned int)expose->y / LIST_ROW_HEIGHT : 0;
    last = (unsigned int)(expose->y + expose->height) / LIST_ROW_HEIGHT + 1;
    if (last > source_count) last = source_count;

    for (row = first; row < last; ++row) {
        XmString string;
        Dimension text_height;
        int y = (int)row * LIST_ROW_HEIGHT;
        int text_width = width > LIST_SIDE_PAD * 2 ?
                         (int)width - LIST_SIDE_PAD * 2 : (int)width;

        if ((int)row == sidebar_selected_index) {
            XSetForeground(display, gc, select);
            XFillRectangle(display, window, gc, 0, y, width, LIST_ROW_HEIGHT);
            XSetForeground(display, gc, foreground);
        }

        sidebar_display_title(source_titles[row], text_width,
                              display_title, sizeof(display_title));
        string = XmStringCreateLocalized(display_title);
        text_height = XmStringHeight(font_list, string);
        XmStringDraw(display, window, font_list, string, gc,
                     LIST_SIDE_PAD,
                     (Position)(y + (LIST_ROW_HEIGHT - (int)text_height) / 2),
                     (Dimension)text_width,
                     XmALIGNMENT_BEGINNING,
                     XmSTRING_DIRECTION_L_TO_R, NULL);
        XmStringFree(string);
    }

    XFreeGC(display, gc);
}

static void sidebar_list_scrolled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    (void)client_data;
    (void)call_data;
    repaint_scrollbar_widget(widget);
}

static void sidebar_list_event(Widget widget, XtPointer client_data,
                               XEvent *event, Boolean *continue_dispatch)
{
    (void)client_data;
    (void)continue_dispatch;

    if (event->type == ConfigureNotify) {
        if (widget == sidebar_source_scroller || widget == sidebar_source_canvas) {
            sidebar_list_reflow();
            if (sidebar_source_canvas != NULL && XtIsRealized(sidebar_source_canvas))
                XClearArea(XtDisplay(sidebar_source_canvas), XtWindow(sidebar_source_canvas),
                           0, 0, 0, 0, True);
        }
        return;
    }

    if (widget != sidebar_source_canvas) return;
    if (event->type == Expose) {
        draw_sidebar_list(&event->xexpose);
        return;
    }
    if (event->type == ButtonPress && event->xbutton.button == Button1) {
        unsigned int row = (unsigned int)event->xbutton.y / LIST_ROW_HEIGHT;
        if (row >= source_count) return;
        XmListSelectPos(source_list_widget, (int)row + 1, True);
    }
}

static void ensure_sidebar_custom_list(void)
{
    Widget parent, top_widget = NULL, bottom_widget = NULL;
    Widget clip = NULL, vertical = NULL;
    Pixel background = 0, foreground = 0;

    if (sidebar_source_canvas != NULL || source_list_widget == NULL) return;

    parent = XtParent(source_list_widget);
    XtVaGetValues(source_list_widget,
                  XmNtopWidget, &top_widget,
                  XmNbottomWidget, &bottom_widget,
                  XmNbackground, &background,
                  XmNforeground, &foreground,
                  NULL);

    /* Keep the XmList around as a hidden font/style and callback proxy. */
    XtUnmanageChild(source_list_widget);

    sidebar_source_scroller = XtVaCreateManagedWidget(
        "sourceListScroller", xmScrolledWindowWidgetClass, parent,
        XmNscrollingPolicy, XmAUTOMATIC,
        XmNshadowThickness, 0,
        XmNbackground, background,
        XmNforeground, foreground,
        XmNtopAttachment, XmATTACH_WIDGET,
        XmNtopWidget, top_widget,
        XmNbottomAttachment, XmATTACH_WIDGET,
        XmNbottomWidget, bottom_widget,
        XmNleftAttachment, XmATTACH_FORM,
        XmNrightAttachment, XmATTACH_FORM,
        NULL);

    sidebar_source_canvas = XtVaCreateManagedWidget(
        "sourceListCanvas", xmDrawingAreaWidgetClass, sidebar_source_scroller,
        XmNresizePolicy, XmRESIZE_NONE,
        XmNwidth, 140,
        XmNheight, LIST_ROW_HEIGHT,
        XmNbackground, background,
        XmNforeground, foreground,
        NULL);

    XmScrolledWindowSetAreas(sidebar_source_scroller, NULL, NULL, sidebar_source_canvas);
    XtAddEventHandler(sidebar_source_canvas,
                      ExposureMask | ButtonPressMask | StructureNotifyMask,
                      False, sidebar_list_event, NULL);
    XtAddEventHandler(sidebar_source_scroller, StructureNotifyMask,
                      False, sidebar_list_event, NULL);
    XtAddEventHandler(sidebar_source_canvas, ButtonPressMask,
                      False, dismiss_volume_popup, NULL);

    XtVaGetValues(sidebar_source_scroller,
                  XmNclipWindow, &clip,
                  XmNverticalScrollBar, &vertical,
                  NULL);
    if (clip != NULL)
        XtVaSetValues(clip,
                      XmNbackground, background,
                      XmNforeground, foreground,
                      NULL);
    if (vertical != NULL) {
        XtAddCallback(vertical, XmNvalueChangedCallback, sidebar_list_scrolled, NULL);
        XtAddCallback(vertical, XmNdragCallback, sidebar_list_scrolled, NULL);
    }

    sidebar_list_reflow();
}

void select_current_source(void);

/* Rebuild labels after width changes, preserving the current selection. */
void refresh_source_labels(void)
{
    unsigned int i;
    int max_pixels;
    XmString item;
    char display[512];

    if (source_list_widget == NULL) return;
    max_pixels = sidebar_available_label_width();
    XmListDeleteAllItems(source_list_widget);
    for (i = 0; i < source_count; ++i) {
        sidebar_display_title(source_titles[i], max_pixels, display, sizeof(display));
        item = XmStringCreateLocalized(display);
        XmListAddItemUnselected(source_list_widget, item, 0);
        XmStringFree(item);
    }

    sidebar_list_reflow();
    select_current_source();
    if (sidebar_source_canvas != NULL && XtIsRealized(sidebar_source_canvas))
        XClearArea(XtDisplay(sidebar_source_canvas), XtWindow(sidebar_source_canvas),
                   0, 0, 0, 0, True);
}

unsigned int sidebar_artwork_size_for_width(Dimension width)
{
    int size = (int)width - 8;
    if (size < 96) size = 96;
    if (size > 240) size = 240;
    return (unsigned int)size;
}

void resize_sidebar_artwork(Dimension width, int refresh_pixmap)
{
    unsigned int size = sidebar_artwork_size_for_width(width);
    if (artwork_label == NULL) return;
    XtVaSetValues(artwork_label, XmNwidth, size, XmNheight, size, NULL);
    if (size != sidebar_artwork_size) {
        sidebar_artwork_size = size;
        if (refresh_pixmap && now_playing_source_visible)
            update_artwork_widget(artwork_label, size, &artwork_pixmap);
    }
}

void sidebar_sizer_expose(Widget widget, XtPointer client_data, XtPointer call_data)
{
    Display *dpy = XtDisplay(widget);
    Window win = XtWindow(widget);
    Dimension width = 0, height = 0;
    Pixel top = 0, bottom = 0, bg = 0;
    GC gc;
    XGCValues gcv;
    int x;
    (void)client_data; (void)call_data;
    if (!XtIsRealized(widget)) return;
    XtVaGetValues(widget, XmNwidth, &width, XmNheight, &height,
                  XmNtopShadowColor, &top, XmNbottomShadowColor, &bottom,
                  XmNbackground, &bg, NULL);
    XClearWindow(dpy, win);
    x = (int)width / 2 - 1;
    gcv.foreground = bottom;
    gc = XCreateGC(dpy, win, GCForeground, &gcv);
    XDrawLine(dpy, win, gc, x, 1, x, (int)height - 2);
    XFreeGC(dpy, gc);
    gcv.foreground = top;
    gc = XCreateGC(dpy, win, GCForeground, &gcv);
    XDrawLine(dpy, win, gc, x + 1, 1, x + 1, (int)height - 2);
    XFreeGC(dpy, gc);
    (void)bg;
}

/* Resize geometry live; the expensive artwork refresh waits for release. */
void sidebar_sizer_event(Widget widget, XtPointer client_data,
                         XEvent *event, Boolean *continue_dispatch)
{
    Dimension width;
    int next_width;
    (void)widget; (void)client_data; (void)continue_dispatch;
    if (event->type == ButtonPress && event->xbutton.button == Button1) {
        sidebar_drag_start_x = event->xbutton.x_root;
        XtVaGetValues(sidebar_frame, XmNwidth, &sidebar_drag_start_width, NULL);
        return;
    }
    if (event->type == ButtonRelease && event->xbutton.button == Button1) {
        XtVaGetValues(sidebar_frame, XmNwidth, &width, NULL);
        resize_sidebar_artwork(width, 1);
        refresh_source_labels();
        return;
    }
    if (event->type != MotionNotify || !(event->xmotion.state & Button1Mask)) return;
    next_width = (int)sidebar_drag_start_width +
                 (event->xmotion.x_root - sidebar_drag_start_x);
    if (next_width < 112) next_width = 112;
    if (next_width > 420) next_width = 420;
    width = (Dimension)next_width;
    XtVaSetValues(sidebar_frame, XmNwidth, width, NULL);
    resize_sidebar_artwork(width, 0);
    XmUpdateDisplay(sidebar_frame);
    refresh_source_labels();
}

void select_current_source(void)
{
    unsigned int i;
    int previous = sidebar_selected_index;
    const char *wanted = (browser_mode == 2 ||
                          strcmp(current_view_path, "/v1/player/queue") == 0)
                             ? "@now-playing" : current_view_path;

    sidebar_selected_index = -1;
    if (source_list_widget == NULL) return;
    for (i = 0; i < source_count; ++i) {
        if (strcmp(source_paths[i], wanted) == 0) {
            sidebar_selected_index = (int)i;
            XmListSelectPos(source_list_widget, (int)i + 1, False);
            break;
        }
    }
    if (previous != sidebar_selected_index) {
        redraw_sidebar_row(previous);
        redraw_sidebar_row(sidebar_selected_index);
    }
}

void populate_sources(void);

static void retry_playlists(XtPointer client_data, XtIntervalId *id)
{
    (void)client_data;
    (void)id;
    playlist_retry_timer = 0;
    populate_sources();
}

static void schedule_playlist_retry(void)
{
    if (playlist_retry_timer != 0 || playlist_retry_attempts >= PLAYLIST_RETRY_LIMIT)
        return;
    playlist_retry_attempts++;
    playlist_retry_timer = XtAppAddTimeOut(application_context,
                                            PLAYLIST_RETRY_DELAY_MS,
                                            retry_playlists, NULL);
}

/* Apple owns the normal source list; Marmelade only adds Now Playing and playlists. */
void populate_sources(void)
{
    char response[65536], title[512], id[256], path[512];
    const char *cursor;
    unsigned int playlist_count = 0;

    ensure_sidebar_custom_list();
    source_count = 0;

    if (now_playing_source_visible) {
        snprintf(source_titles[source_count], sizeof(source_titles[0]), "Now Playing");
        snprintf(source_paths[source_count], sizeof(source_paths[0]), "@now-playing");
        source_count++;
    }

    if (bridge_client_request(&bridge, "GET", "/v1/navigation/sources", NULL,
                              response, sizeof(response)) == 0) {
        cursor = strstr(response, "\"items\"");
        while (cursor && (cursor = strstr(cursor, "\"title\"")) != NULL &&
               source_count < 107) {
            if (!json_string(cursor, "title", title, sizeof(title)) ||
                !json_string(cursor, "path", path, sizeof(path))) break;
            snprintf(source_titles[source_count], sizeof(source_titles[0]), "%s", title);
            snprintf(source_paths[source_count], sizeof(source_paths[0]), "%s", path);
            source_count++;
            cursor += 7;
        }
    }

    if (bridge_client_request(&bridge, "GET", "/v1/library/playlists", NULL,
                              response, sizeof(response)) == 0) {
        cursor = strstr(response, "\"items\"");
        while (cursor && (cursor = strstr(cursor, "\"id\"")) != NULL && source_count < 107) {
            if (!json_string(cursor, "id", id, sizeof(id)) ||
                !json_string(cursor, "title", title, sizeof(title))) break;
            snprintf(path, sizeof(path), "/v1/library/playlists/%s", id);
            snprintf(source_titles[source_count], sizeof(source_titles[0]), "%s", title);
            snprintf(source_paths[source_count], sizeof(source_paths[0]), "%s", path);
            source_count++;
            playlist_count++;
            cursor += 4;
        }
    }

    refresh_source_labels();

    if (playlist_count == 0) {
        schedule_playlist_retry();
    } else {
        playlist_retry_attempts = 0;
        if (playlist_retry_timer != 0) {
            XtRemoveTimeOut(playlist_retry_timer);
            playlist_retry_timer = 0;
        }
    }
}
