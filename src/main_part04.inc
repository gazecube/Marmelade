
static size_t utf8_sequence_length(unsigned char c)
{
    if ((c & 0x80u) == 0) return 1;
    if ((c & 0xe0u) == 0xc0u) return 2;
    if ((c & 0xf0u) == 0xe0u) return 3;
    if ((c & 0xf8u) == 0xf0u) return 4;
    return 1;
}

static int sidebar_text_width(const char *text)
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

static void sidebar_display_title(const char *title, int max_pixels,
                                  char *display, size_t display_size)
{
    const unsigned char *p = (const unsigned char *)title;
    size_t boundaries[512];
    size_t bytes = 0, count = 0, i;

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

static int sidebar_available_label_width(void)
{
    Dimension width = 0;
    if (source_list_widget == NULL) return 140;
    XtVaGetValues(source_list_widget, XmNwidth, &width, NULL);
    return width > 24 ? (int)width - 24 : 1;
}

static void select_current_source(void);

static void refresh_source_labels(void)
{
    unsigned int i;
    int max_pixels;
    XmString item;
    char display[512];

    if (source_list_widget == NULL) return;
    max_pixels = sidebar_available_label_width();
    XmListDeleteAllItems(source_list_widget);
    for (i = 0; i < source_count; ++i) {
        if (source_paths[i][0] == '\0')
            snprintf(display, sizeof(display), "%s", source_titles[i]);
        else
            sidebar_display_title(source_titles[i], max_pixels, display, sizeof(display));
        item = XmStringCreateLocalized(display);
        XmListAddItemUnselected(source_list_widget, item, 0);
        XmStringFree(item);
    }
    select_current_source();
}

static unsigned int sidebar_artwork_size_for_width(Dimension width)
{
    int size = (int)width - 8;
    if (size < 96) size = 96;
    if (size > 240) size = 240;
    return (unsigned int)size;
}

static void resize_sidebar_artwork(Dimension width, int refresh_pixmap)
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


static void sidebar_sizer_expose(Widget widget, XtPointer client_data, XtPointer call_data)
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
    XtVaGetValues(widget,
                  XmNwidth, &width, XmNheight, &height,
                  XmNtopShadowColor, &top,
                  XmNbottomShadowColor, &bottom,
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

static void sidebar_sizer_event(Widget widget, XtPointer client_data,
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

static void select_current_source(void)
{
    unsigned int i;
    const char *wanted = (browser_mode == 2 ||
                          strcmp(current_view_path, "/v1/player/queue") == 0)
                             ? "@now-playing" : current_view_path;
    if (source_list_widget == NULL) return;
    for (i = 0; i < source_count; ++i) {
        if (strcmp(source_paths[i], wanted) == 0) {
            XmListSelectPos(source_list_widget, (int)i + 1, False);
            return;
        }
    }
}

static void populate_sources(void)
{
    static const char *names[] = {
        "Now Playing", "Listen Now", "Recently Played", "Albums",
        "Artists", "Songs", "Radio"
    };
    static const char *paths[] = {
        "@now-playing", "/v1/listen-now", "/v1/library/recent",
        "/v1/library/albums", "/v1/library/artists",
        "/v1/library/songs", "/v1/radio"
    };
    char response[65536], title[512], id[256], path[512];
    const char *cursor;
    unsigned int i, first = now_playing_source_visible ? 0 : 1;
    XmListDeleteAllItems(source_list_widget);
    source_count = 0;
    for (i = first; i < 7; ++i) {
        snprintf(source_titles[source_count], sizeof(source_titles[0]), "%s", names[i]);
        snprintf(source_paths[source_count], sizeof(source_paths[0]), "%s", paths[i]);
        source_count++;
    }
    snprintf(source_titles[source_count], sizeof(source_titles[0]), "PLAYLISTS");
    source_paths[source_count][0] = '\0';
    source_count++;
    if (bridge_client_request(&bridge, "GET", "/v1/library/playlists", NULL,
                              response, sizeof(response)) != 0) {
        refresh_source_labels();
        return;
    }
    cursor = strstr(response, "\"items\"");
    while (cursor && (cursor = strstr(cursor, "\"id\"")) != NULL && source_count < 107) {
        if (!json_string(cursor, "id", id, sizeof(id)) ||
            !json_string(cursor, "title", title, sizeof(title))) break;
        snprintf(path, sizeof(path), "/v1/library/playlists/%s", id);
        snprintf(source_titles[source_count], sizeof(source_titles[0]), "%s", title);
        snprintf(source_paths[source_count], sizeof(source_paths[0]), "%s", path);
        source_count++;
        cursor += 4;
    }
    refresh_source_labels();
}

static void view_model_selected(unsigned int index)
{
    const char *plural;
    if (index >= current_view_count) return;
    set_browser_selection((int)index);
    if (current_view_container_rows[index]) {
        current_view_collapsed = !current_view_collapsed;
        current_view_labels[index][1] = current_view_collapsed ? '+' : '-';
        apply_filter(current_filter);
        return;
    }
    if (strcmp(current_view_item_kinds[index], "album") != 0 &&
        strcmp(current_view_item_kinds[index], "playlist") != 0 &&
        strcmp(current_view_item_kinds[index], "artist") != 0) return;
    plural = strcmp(current_view_item_kinds[index], "artist") == 0 ? "artists" :
             (strcmp(current_view_item_kinds[index], "album") == 0 ? "albums" : "playlists");
    snprintf(current_view_path, sizeof(current_view_path), "/v1/library/%s/%s",
             plural, current_view_ids[index]);
    load_view(current_view_path);
}

static void view_model_opened(unsigned int index)
{
    char body[640], response[2048];
    if (index >= current_view_count) return;
    if (strcmp(current_view_type, "albums") == 0 ||
        strcmp(current_view_type, "artists") == 0 ||
        strcmp(current_view_type, "playlists") == 0) {
        snprintf(current_view_path, sizeof(current_view_path), "/v1/library/%s/%s",
                 current_view_type, current_view_ids[index]);
        load_view(current_view_path);
    } else if (strcmp(current_view_item_kinds[index], "song") == 0 ||
               strcmp(current_view_item_kinds[index], "album") == 0 ||
               strcmp(current_view_item_kinds[index], "playlist") == 0 ||
               strcmp(current_view_item_kinds[index], "station") == 0) {
        snprintf(body, sizeof(body), "{\"kind\":\"%s\",\"id\":\"%s\"}",
                 current_view_item_kinds[index], current_view_catalog_ids[index]);
        if (bridge_client_request(&bridge, "POST", "/v1/player/queue", body,
                                  response, sizeof(response)) == 0) {
            set_status("Starting selected Apple Music item...");
            snprintf(current_view_path, sizeof(current_view_path), "%s", "/v1/player/queue");
            load_view(current_view_path);
        } else
            set_status("Could not play the selected Apple Music item");
    }
}


static void browser_list_event(Widget widget, XtPointer client_data,
                               XEvent *event, Boolean *continue_dispatch)
{
    Dimension width = 0;
    int queue_view;
    (void)client_data; (void)continue_dispatch;

    if (event->type == ConfigureNotify) {
        if (widget == browser_list_scroller || widget == browser_list_canvas)
            schedule_browser_resize_redraw();
        return;
    }
    if (widget != browser_list_canvas) return;
    if (event->type == Expose) {
        draw_browser_list(&event->xexpose);
        return;
    }
    if (event->type != ButtonPress || event->xbutton.button != Button1) return;

    queue_view = browser_mode == 0 && strcmp(current_view_path, "/v1/player/queue") == 0;
    if (event->xbutton.y < LIST_TITLE_HEIGHT) {
        if (queue_view) {
            int autoplay_x, automix_x, repeat_x, shuffle_x;
            XtVaGetValues(browser_list_canvas, XmNwidth, &width, NULL);
            autoplay_x = (int)width - LIST_SIDE_PAD - LIST_SMALL_ICON_SIZE;
            automix_x = autoplay_x - LIST_ICON_GAP - LIST_SMALL_ICON_SIZE;
            repeat_x = automix_x - LIST_ICON_GAP - LIST_ICON_SIZE;
            shuffle_x = repeat_x - LIST_ICON_GAP - LIST_ICON_SIZE;
            if (event->xbutton.x >= autoplay_x &&
                event->xbutton.x < autoplay_x + LIST_SMALL_ICON_SIZE) {
                autoplay_toggled(widget, NULL, NULL);
            } else if (event->xbutton.x >= automix_x &&
                       event->xbutton.x < automix_x + LIST_SMALL_ICON_SIZE) {
                automix_toggled(widget, NULL, NULL);
            } else if (event->xbutton.x >= repeat_x &&
                       event->xbutton.x < repeat_x + LIST_ICON_SIZE) {
                repeat_toggled(widget, NULL, NULL);
            } else if (event->xbutton.x >= shuffle_x &&
                       event->xbutton.x < shuffle_x + LIST_ICON_SIZE) {
                shuffle_toggled(widget, NULL, NULL);
            }
