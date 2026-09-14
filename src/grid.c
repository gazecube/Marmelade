#include "app_internal.h"

void queue_album_popup_track(Widget widget, XtPointer client_data,
                             XtPointer call_data)
{
    XmListCallbackStruct *selection = (XmListCallbackStruct *)call_data;
    unsigned int index;
    char body[640], response[2048];
    (void)widget; (void)client_data;
    if (selection->item_position < 1) return;
    index = (unsigned int)selection->item_position - 1;
    if (index >= album_popup_count) return;
    snprintf(body, sizeof(body), "{\"kind\":\"%s\",\"id\":\"%s\"}",
             album_popup_kinds[index], album_popup_catalog_ids[index]);
    if (bridge_client_request(&bridge, "POST", "/v1/player/queue", body,
                              response, sizeof(response)) == 0)
        set_status("Starting selected album track...");
    else
        set_status("Could not play the selected album track");
}

void show_album_tracks(unsigned int index, int cell_x, int cell_y)
{
    char path[640], response[65536], title[512] = "Album";
    char track[512], artist[512], id[256], kind[32], catalog_id[256], line[1200];
    const char *cursor;
    Position root_x, root_y;
    XmString value;
    double track_number, duration;
    if (index >= current_view_count) return;
    snprintf(path, sizeof(path), "/v1/library/albums/%s", current_view_ids[index]);
    if (bridge_client_request(&bridge, "GET", path, NULL,
                              response, sizeof(response)) != 0) {
        set_status("Could not load the selected album");
        return;
    }
    json_string(response, "viewTitle", title, sizeof(title));
    value = XmStringCreateLocalized(title);
    XtVaSetValues(album_tracks_title, XmNlabelString, value, NULL);
    XmStringFree(value);
    XmListDeleteAllItems(album_tracks_list);
    album_popup_count = 0;
    cursor = strstr(response, "\"items\"");
    while (cursor && (cursor = strstr(cursor, "\"id\"")) != NULL &&
           album_popup_count < 100) {
        track[0] = '\0'; artist[0] = '\0'; id[0] = '\0'; kind[0] = '\0';
        catalog_id[0] = '\0'; track_number = 0.0; duration = 0.0;
        if (!json_string(cursor, "id", id, sizeof(id)) ||
            !json_string(cursor, "title", track, sizeof(track))) break;
        json_string(cursor, "artist", artist, sizeof(artist));
        json_string(cursor, "kind", kind, sizeof(kind));
        json_string(cursor, "catalogId", catalog_id, sizeof(catalog_id));
        json_number(cursor, "trackNumber", &track_number);
        json_number(cursor, "duration", &duration);
        snprintf(line, sizeof(line), "%02u  %-36.36s  %-24.24s  %u:%02u",
                 (unsigned int)track_number, track, artist,
                 (unsigned int)duration / 60, (unsigned int)duration % 60);
        value = XmStringCreateLocalized(line);
        XmListAddItemUnselected(album_tracks_list, value, 0);
        XmStringFree(value);
        snprintf(album_popup_catalog_ids[album_popup_count],
                 sizeof(album_popup_catalog_ids[0]), "%s",
                 catalog_id[0] != '\0' ? catalog_id : id);
        snprintf(album_popup_kinds[album_popup_count],
                 sizeof(album_popup_kinds[0]), "%s", kind[0] != '\0' ? kind : "song");
        album_popup_count++;
        cursor += 4;
    }
    XtTranslateCoords(album_grid, (Position)cell_x,
                      (Position)(cell_y + GRID_ARTWORK + GRID_MARGIN), &root_x, &root_y);
    XtVaSetValues(album_tracks_popup, XmNx, root_x, XmNy, root_y, NULL);
    XtPopup(album_tracks_popup, XtGrabNone);
}

void reflow_album_grid(void)
{
    Widget clip = NULL;
    Dimension width = GRID_CELL + GRID_MARGIN * 2;
    unsigned int rows;
    unsigned int columns;
    unsigned int used_width;

    XtVaGetValues(album_grid_scroller, XmNclipWindow, &clip, NULL);
    if (clip != NULL)
        XtVaGetValues(clip, XmNwidth, &width, NULL);
    else
        XtVaGetValues(album_grid_scroller, XmNwidth, &width, NULL);

    columns = width > GRID_MARGIN * 2 ?
              (unsigned int)(width - GRID_MARGIN * 2) / GRID_CELL : 1;
    if (columns < 1) columns = 1;
    if (current_view_count > 0 && columns > current_view_count)
        columns = current_view_count;

    album_grid_columns = columns;
    used_width = album_grid_columns * GRID_CELL;
    album_grid_origin_x = width > used_width ?
                          ((int)width - (int)used_width) / 2 : GRID_MARGIN;
    if (album_grid_origin_x < GRID_MARGIN) album_grid_origin_x = GRID_MARGIN;

    rows = current_view_count > 0 ?
           (current_view_count + album_grid_columns - 1) / album_grid_columns : 1;
    XtVaSetValues(album_grid, XmNwidth, width,
                  XmNheight, rows * GRID_CELL + GRID_MARGIN * 2, NULL);
}

void draw_album_grid(XExposeEvent *expose)
{
    Display *display = XtDisplay(album_grid);
    Window window = XtWindow(album_grid);
    Pixel foreground, background;
    XGCValues values;
    GC gc;
    unsigned int i;
    XtVaGetValues(album_grid, XmNforeground, &foreground,
                  XmNbackground, &background, NULL);
    values.foreground = foreground; values.background = background;
    gc = XCreateGC(display, window, GCForeground | GCBackground, &values);
    for (i = 0; i < current_view_count; ++i) {
        int column = (int)(i % album_grid_columns);
        int row = (int)(i / album_grid_columns);
        int cell_x = album_grid_origin_x + column * GRID_CELL;
        int x = cell_x + (GRID_CELL - GRID_ARTWORK) / 2;
        int y = GRID_MARGIN + row * GRID_CELL +
                (GRID_CELL - GRID_ARTWORK) / 2;
        if (cell_x + GRID_CELL < expose->x ||
            cell_x > expose->x + expose->width ||
            GRID_MARGIN + row * GRID_CELL + GRID_CELL < expose->y ||
            GRID_MARGIN + row * GRID_CELL > expose->y + expose->height) continue;
        if (album_grid_pixmaps[i] != XmUNSPECIFIED_PIXMAP &&
            album_grid_pixmaps[i] != None)
            XCopyArea(display, album_grid_pixmaps[i], window, gc,
                      0, 0, GRID_ARTWORK, GRID_ARTWORK, x, y);
        else
            XDrawRectangle(display, window, gc, x, y,
                           GRID_ARTWORK - 1, GRID_ARTWORK - 1);
    }
    XFreeGC(display, gc);
}

void grid_artwork_timeout(XtPointer client_data, XtIntervalId *timer_id)
{
    Widget vertical = NULL;
    int value = 0, slider_size = 400;
    unsigned int first_row, last_row, first, last, i;
    (void)client_data; (void)timer_id;
    album_grid_timer_pending = 0;
    if (browser_mode != 1) return;
    XtVaGetValues(album_grid_scroller, XmNverticalScrollBar, &vertical, NULL);
    if (vertical != NULL)
        XtVaGetValues(vertical, XmNvalue, &value,
                      XmNsliderSize, &slider_size, NULL);
    first_row = (unsigned int)(value / GRID_CELL);
    last_row = (unsigned int)((value + slider_size) / GRID_CELL + 1);
    first = first_row * album_grid_columns;
    last = (last_row + 1) * album_grid_columns;
    if (last > current_view_count) last = current_view_count;
    for (i = first; i < last; ++i) {
        if (album_grid_pixmaps[i] == XmUNSPECIFIED_PIXMAP &&
            current_view_artwork_urls[i][0] != '\0') {
            album_grid_pixmaps[i] = load_album_grid_pixmap(current_view_artwork_urls[i], i);
            {
                int column = (int)(i % album_grid_columns);
                int row = (int)(i / album_grid_columns);
                XClearArea(XtDisplay(album_grid), XtWindow(album_grid),
                           album_grid_origin_x + column * GRID_CELL,
                           GRID_MARGIN + row * GRID_CELL,
                           GRID_CELL, GRID_CELL, True);
            }
            schedule_grid_artwork();
            return;
        }
    }
}

void schedule_grid_artwork(void)
{
    if (!album_grid_timer_pending) {
        album_grid_timer_pending = 1;
        XtAppAddTimeOut(application_context, 25, grid_artwork_timeout, NULL);
    }
}

void album_grid_scrolled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    (void)widget; (void)client_data; (void)call_data;
    schedule_grid_artwork();
}

void album_grid_event(Widget widget, XtPointer client_data,
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

void populate_album_grid(void)
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

void view_mode_selected(Widget widget, XtPointer client_data, XtPointer call_data)
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

void show_sidebar_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
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

void show_sidebar_artwork_toggled(Widget widget, XtPointer client_data,
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

void extend_lazy_views(void)
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

void redraw_browser_scroll_strip(int old_value, int new_value)
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

void browser_scroll_redraw_timeout(XtPointer client_data, XtIntervalId *timer_id)
{
    int from, to;
    (void)client_data; (void)timer_id;
    browser_scroll_redraw_pending = 0;
    from = browser_scroll_redraw_from;
    to = browser_scroll_redraw_to;
    browser_scroll_redraw_from = to;
    redraw_browser_scroll_strip(from, to);
}

void repaint_scrollbar_widget(Widget scrollbar)
{
    if (scrollbar == NULL || !XtIsRealized(scrollbar)) return;
    XClearArea(XtDisplay(scrollbar), XtWindow(scrollbar),
               0, 0, 0, 0, True);
    XmUpdateDisplay(scrollbar);
}

void list_scrolled(Widget widget, XtPointer client_data, XtPointer call_data)
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
    repaint_scrollbar_widget(widget);
    extend_lazy_views();
}
