
static void schedule_grid_artwork(void)
{
    if (!album_grid_timer_pending) {
        album_grid_timer_pending = 1;
        XtAppAddTimeOut(application_context, 25, grid_artwork_timeout, NULL);
    }
}

static void album_grid_scrolled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    (void)widget; (void)client_data; (void)call_data;
    schedule_grid_artwork();
}

static void album_grid_event(Widget widget, XtPointer client_data,
                             XEvent *event, Boolean *continue_dispatch)
{
    (void)widget; (void)client_data; (void)continue_dispatch;
    if (event->type == ConfigureNotify) reflow_album_grid();
    else if (event->type == Expose) {
        draw_album_grid(&event->xexpose);
        schedule_grid_artwork();
    } else if (event->type == ButtonPress) {
        unsigned int column, row, index;
        if (event->xbutton.x < album_grid_origin_x ||
            event->xbutton.y < GRID_MARGIN) return;
        column = (unsigned int)(event->xbutton.x - album_grid_origin_x) / GRID_CELL;
        row = (unsigned int)(event->xbutton.y - GRID_MARGIN) / GRID_CELL;
        if (column >= album_grid_columns) return;
        index = row * album_grid_columns + column;
        if (index >= current_view_count) return;
        show_album_tracks(index,
                          album_grid_origin_x + (int)column * GRID_CELL +
                              (GRID_CELL - GRID_ARTWORK) / 2,
                          GRID_MARGIN + (int)row * GRID_CELL +
                              (GRID_CELL - GRID_ARTWORK) / 2);
    }
}

static void populate_album_grid(void)
{
    unsigned int i;
    for (i = 0; i < album_grid_pixmap_count; ++i)
        if (album_grid_pixmaps[i] != XmUNSPECIFIED_PIXMAP && album_grid_pixmaps[i] != None)
            XFreePixmap(XtDisplay(album_grid), album_grid_pixmaps[i]);
    for (i = 0; i < 100; ++i) album_grid_pixmaps[i] = XmUNSPECIFIED_PIXMAP;
    album_grid_pixmap_count = current_view_count;
    reflow_album_grid();
    if (XtIsRealized(album_grid)) XClearArea(XtDisplay(album_grid), XtWindow(album_grid),
                                              0, 0, 0, 0, True);
    schedule_grid_artwork();
}

static void view_mode_selected(Widget widget, XtPointer client_data, XtPointer call_data)
{
    int mode = (int)(long)client_data;
    (void)widget; (void)call_data;
    if (mode == 1 && strcmp(current_view_container_kind, "album") == 0) {
        set_status("Grid view is unavailable inside an album");
        return;
    }
    browser_mode = mode;
    if (album_tracks_popup != NULL) XtPopdown(album_tracks_popup);
    XtUnmanageChild(browser_widget);
    XtUnmanageChild(album_grid_scroller);
    XtUnmanageChild(main_artwork_label);
    if (mode == 1) {
        XtManageChild(album_grid_scroller);
        snprintf(current_view_path, sizeof(current_view_path), "%s", "/v1/library/albums");
        load_view(current_view_path);
        populate_album_grid();
    } else if (mode == 2) {
        XtManageChild(main_artwork_label);
        select_current_source();
    } else {
        XtManageChild(browser_widget);
    }
}

static void show_sidebar_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    Boolean shown = XmToggleButtonGetState(widget);
    (void)client_data; (void)call_data;
    if (shown) {
        XtManageChild(sidebar_frame);
        XtManageChild(sidebar_sizer);
        XtVaSetValues(right_pane_widget, XmNleftAttachment, XmATTACH_WIDGET,
                      XmNleftWidget, sidebar_sizer, NULL);
        refresh_source_labels();
    } else {
        XtUnmanageChild(sidebar_sizer);
        XtUnmanageChild(sidebar_frame);
        XtVaSetValues(right_pane_widget, XmNleftAttachment, XmATTACH_FORM, NULL);
    }
}

static void show_sidebar_artwork_toggled(Widget widget, XtPointer client_data,
                                        XtPointer call_data)
{
    Boolean shown = XmToggleButtonGetState(widget);
    (void)client_data; (void)call_data;
    if (shown) {
        XtManageChild(artwork_label);
        XtVaSetValues(search_widget, XmNbottomAttachment, XmATTACH_WIDGET,
                      XmNbottomWidget, artwork_label, NULL);
    } else {
        XtUnmanageChild(artwork_label);
        XtVaSetValues(search_widget, XmNbottomAttachment, XmATTACH_FORM, NULL);
    }
}

static void extend_lazy_views(void)
{
    Widget vertical = NULL;
    int value = 0, slider_size = 0;
    if (browser_mode != 0 || current_render_limit >= current_view_count || list_lazy_loading)
        return;
    XtVaGetValues(browser_list_scroller, XmNverticalScrollBar, &vertical, NULL);
    if (vertical == NULL) return;
    XtVaGetValues(vertical, XmNvalue, &value, XmNsliderSize, &slider_size, NULL);
    if ((unsigned int)(value + slider_size + LIST_ROW_HEIGHT * 5) >=
        LIST_TITLE_HEIGHT + visible_view_count * LIST_ROW_HEIGHT) {
        list_lazy_loading = 1;
        current_render_limit += 30;
        if (current_render_limit > current_view_count)
            current_render_limit = current_view_count;
        apply_filter(current_filter);
        list_lazy_loading = 0;
    }
}

static void redraw_browser_scroll_strip(int old_value, int new_value)
{
    Widget clip = NULL;
    Dimension clip_width = 0, clip_height = 0;
    XExposeEvent expose;
    int delta, y, height;

    if (browser_list_canvas == NULL || browser_list_scroller == NULL ||
        !XtIsRealized(browser_list_canvas) || old_value < 0 || old_value == new_value)
        return;

    XtVaGetValues(browser_list_scroller, XmNclipWindow, &clip, NULL);
    if (clip == NULL) return;
    XtVaGetValues(clip, XmNwidth, &clip_width, XmNheight, &clip_height, NULL);
    if (clip_width == 0 || clip_height == 0) return;

    delta = new_value - old_value;
    if (delta > 0) {
        height = delta < (int)clip_height ? delta : (int)clip_height;
        y = new_value + (int)clip_height - height;
    } else {
        height = -delta < (int)clip_height ? -delta : (int)clip_height;
        y = new_value;
    }

    memset(&expose, 0, sizeof(expose));
    expose.type = Expose;
    expose.display = XtDisplay(browser_list_canvas);
    expose.window = XtWindow(browser_list_canvas);
    expose.x = 0;
    expose.y = y;
    expose.width = (int)clip_width;
    expose.height = height;
    draw_browser_list(&expose);
}

static void browser_scroll_redraw_timeout(XtPointer client_data, XtIntervalId *timer_id)
{
    int from, to;
    (void)client_data; (void)timer_id;
    browser_scroll_redraw_pending = 0;
    from = browser_scroll_redraw_from;
    to = browser_scroll_redraw_to;
    browser_scroll_redraw_from = to;
    redraw_browser_scroll_strip(from, to);
}

static void repaint_scrollbar_widget(Widget scrollbar)
{
    if (scrollbar == NULL || !XtIsRealized(scrollbar)) return;

    /* The SGI/MaXX scrollbar trough may use a patterned scheme background.
       Motif moves the thumb during drag without always damaging the pixels the
       old thumb uncovered, so explicitly invalidate the scrollbar window and
       let the widget redraw its own scheme-native trough, arrows, and thumb. */
    XClearArea(XtDisplay(scrollbar), XtWindow(scrollbar),
               0, 0, 0, 0, True);
    XmUpdateDisplay(scrollbar);
}

static void list_scrolled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XmScrollBarCallbackStruct *scroll = (XmScrollBarCallbackStruct *)call_data;
    int value = 0;
    (void)client_data;

    if (scroll != NULL)
        value = scroll->value;
    else
        XtVaGetValues(widget, XmNvalue, &value, NULL);

    if (browser_scroll_last_value < 0) browser_scroll_last_value = value;
    if (!browser_scroll_redraw_pending) {
        browser_scroll_redraw_from = browser_scroll_last_value;
        browser_scroll_redraw_pending = 1;
        XtAppAddTimeOut(application_context, 0, browser_scroll_redraw_timeout, NULL);
    }
    browser_scroll_redraw_to = value;
    browser_scroll_last_value = value;

    /* Keep the scrollbar itself visually correct while the thumb moves.
       This is separate from repainting the browser contents. */
    repaint_scrollbar_widget(widget);
    extend_lazy_views();
}

static void poll_player(XtPointer client_data, XtIntervalId *timer_id);


static void repaint_position_scale(int percent)
{
    if (position_scale == NULL || !XtIsRealized(position_scale)) return;
    if (percent == prior_position_percent) return;

    /* Music time advances this Scale programmatically once per poll.  MaXX's
       patterned SGI Scale trough is not always damaged when the slider moves,
       which leaves the old slider pixels behind.  Use the public Scale setter,
       then invalidate the tiny Scale window and service its expose immediately
       before any later bridge work can block the Xt event loop. */
    prior_position_percent = percent;
    XmScaleSetValue(position_scale, percent);
    XClearArea(XtDisplay(position_scale), XtWindow(position_scale),
               0, 0, 0, 0, True);
    XmUpdateDisplay(position_scale);
}

static void schedule_player_poll(unsigned long delay_ms)
{
    if (player_poll_timer != (XtIntervalId)0) return;
    player_poll_timer = XtAppAddTimeOut(application_context, delay_ms,
                                        poll_player, NULL);
}

static void poll_player(XtPointer client_data, XtIntervalId *timer_id)
{
    char status[1024], player[4096], mode[96], playback[96] = "unavailable";
    char title[512] = "", artist[512] = "", message[1152];
    char track_id[256] = "", artwork_key[1024];
    static char prior_artwork_key[1024];
    double position = 0.0, duration = 0.0, player_volume = 0.6;
    int logged_in = 0, percent = 0;
    (void)client_data; (void)timer_id;
    player_poll_timer = (XtIntervalId)0;

    if (bridge_client_request(&bridge, "GET", "/v1/status", NULL,
                              status, sizeof(status)) != 0) {
        set_status("Apple Music bridge unavailable");
        goto schedule;
    }
    if (json_string(status, "mode", mode, sizeof(mode)) &&
        strcmp(mode, "ready") != 0) {
        if (strcmp(mode, "needs_login") == 0)
            set_status("Sign into Apple Music: File -> Show Apple Music Login...");
        else if (strcmp(mode, "starting_browser") == 0)
            set_status("Starting Apple Music browser engine...");
        else
            set_status(mode);
        goto schedule;
    }
    if (bridge_client_request(&bridge, "GET", "/v1/player/state", NULL,
                              player, sizeof(player)) != 0) {
        set_status("Could not read Apple Music player state");
        goto schedule;
    }
    json_boolean(player, "loggedIn", &logged_in);
    if (!logged_in) {
        set_status("Sign into Apple Music: File -> Show Apple Music Login...");
        goto schedule;
    }
    json_string(player, "playback", playback, sizeof(playback));
    json_string(player, "title", title, sizeof(title));
    json_string(player, "artist", artist, sizeof(artist));
    json_string(player, "id", track_id, sizeof(track_id));
    json_number(player, "position", &position);
    json_number(player, "duration", &duration);
    json_number(player, "volume", &player_volume);
    json_boolean(player, "shuffleEnabled", &player_shuffle_enabled);
    json_boolean(player, "automixEnabled", &player_automix_enabled);
    json_boolean(player, "autoplayEnabled", &player_autoplay_enabled);
    {
        char repeat_mode[32] = "off";
        if (json_string(player, "repeatMode", repeat_mode, sizeof(repeat_mode))) {
            if (strcmp(repeat_mode, "one") == 0) player_repeat_mode = 2;
            else if (strcmp(repeat_mode, "all") == 0) player_repeat_mode = 1;
            else player_repeat_mode = 0;
        }
    }
    {
        int has_current_item = track_id[0] != '\0' || title[0] != '\0';
        if (has_current_item != now_playing_source_visible) {
            now_playing_source_visible = has_current_item;
            if (!has_current_item &&
                (browser_mode == 2 || strcmp(current_view_path, "/v1/player/queue") == 0)) {
                browser_mode = 0;
                snprintf(current_view_path, sizeof(current_view_path), "%s", "/v1/listen-now");
                XtUnmanageChild(album_grid_scroller);
                XtUnmanageChild(main_artwork_label);
                XtManageChild(browser_widget);
                load_view(current_view_path);
            }
            populate_sources();
        }
    }
