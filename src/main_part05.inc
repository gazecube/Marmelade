        }
        return;
    }
    {
        unsigned int row = (unsigned int)(event->xbutton.y - LIST_TITLE_HEIGHT) / LIST_ROW_HEIGHT;
        unsigned int model;
        unsigned long multi_click;
        if (row >= visible_view_count) return;
        model = visible_to_model[row];
        multi_click = XtGetMultiClickTime(XtDisplay(widget));
        if (browser_list_last_click_model == (int)model &&
            event->xbutton.time >= browser_list_last_click_time &&
            event->xbutton.time - browser_list_last_click_time <= multi_click) {
            browser_list_last_click_model = -1;
            browser_list_last_click_time = 0;
            view_model_opened(model);
        } else {
            browser_list_last_click_model = (int)model;
            browser_list_last_click_time = event->xbutton.time;
            view_model_selected(model);
        }
    }
}


static unsigned long rgb_component_pixel(unsigned char component, unsigned long mask)
{
    unsigned int shift = 0;
    unsigned long maximum;
    if (mask == 0) return 0;
    while (((mask >> shift) & 1UL) == 0UL) shift++;
    maximum = mask >> shift;
    return ((((unsigned long)component * maximum + 127UL) / 255UL) << shift) & mask;
}

static Pixmap rgb_pixmap_from_bytes(Widget widget, const unsigned char *rgb,
                                    unsigned int width, unsigned int height)
{
    Display *display = XtDisplay(widget);
    Screen *screen = XtScreen(widget);
    Visual *visual = DefaultVisualOfScreen(screen);
    int depth = DefaultDepthOfScreen(screen);
    XImage *image;
    Pixmap pixmap;
    GC gc;
    unsigned int x, y;

    if (visual->class != TrueColor && visual->class != DirectColor)
        return XmUNSPECIFIED_PIXMAP;
    image = XCreateImage(display, visual, (unsigned int)depth, ZPixmap, 0,
                         NULL, width, height, 32, 0);
    if (image == NULL) return XmUNSPECIFIED_PIXMAP;
    image->data = calloc((size_t)image->bytes_per_line, height);
    if (image->data == NULL) {
        XDestroyImage(image);
        return XmUNSPECIFIED_PIXMAP;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            size_t offset = ((size_t)y * width + x) * 3;
            unsigned long pixel =
                rgb_component_pixel(rgb[offset], visual->red_mask) |
                rgb_component_pixel(rgb[offset + 1], visual->green_mask) |
                rgb_component_pixel(rgb[offset + 2], visual->blue_mask);
            XPutPixel(image, (int)x, (int)y, pixel);
        }
    }
    pixmap = XCreatePixmap(display, RootWindowOfScreen(screen), width, height,
                           (unsigned int)depth);
    if (pixmap == None) {
        XDestroyImage(image);
        return XmUNSPECIFIED_PIXMAP;
    }
    gc = XCreateGC(display, pixmap, 0, NULL);
    if (gc == NULL) {
        XFreePixmap(display, pixmap);
        XDestroyImage(image);
        return XmUNSPECIFIED_PIXMAP;
    }
    XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, width, height);
    XFreeGC(display, gc);
    XDestroyImage(image);
    return pixmap;
}

static Pixmap fetch_artwork_pixmap(Widget widget, unsigned int size, const char *url)
{
    char encoded[3072], path[4096];
    unsigned char *rgb;
    size_t expected = (size_t)size * size * 3;
    size_t received = 0;
    Pixmap pixmap = XmUNSPECIFIED_PIXMAP;

    if (url != NULL && url[0] != '\0') {
        url_encode(url, encoded, sizeof(encoded));
        snprintf(path, sizeof(path), "/v1/player/artwork.rgb?size=%u&url=%s", size, encoded);
    } else {
        snprintf(path, sizeof(path), "/v1/player/artwork.rgb?size=%u", size);
    }
    rgb = malloc(expected);
    if (rgb == NULL) return XmUNSPECIFIED_PIXMAP;
    if (bridge_client_request_bytes(&bridge, "GET", path, NULL, rgb, expected,
                                    &received) == 0 && received == expected)
        pixmap = rgb_pixmap_from_bytes(widget, rgb, size, size);
    free(rgb);
    return pixmap;
}

static void update_artwork_widget(Widget widget, unsigned int size, Pixmap *stored_pixmap)
{
    Pixmap pixmap, old_pixmap;
    XmString unavailable;

    pixmap = fetch_artwork_pixmap(widget, size, NULL);
    if (pixmap == XmUNSPECIFIED_PIXMAP || pixmap == None) goto unavailable;
    old_pixmap = *stored_pixmap;
    *stored_pixmap = pixmap;
    XtVaSetValues(widget, XmNlabelType, XmPIXMAP,
                  XmNlabelPixmap, *stored_pixmap, NULL);
    if (old_pixmap != XmUNSPECIFIED_PIXMAP && old_pixmap != None)
        XFreePixmap(XtDisplay(widget), old_pixmap);
    return;

unavailable:
    unavailable = XmStringCreateLocalized("No Artwork");
    XtVaSetValues(widget, XmNlabelType, XmSTRING,
                  XmNlabelString, unavailable, NULL);
    XmStringFree(unavailable);
}

static void update_artwork(void)
{
    update_artwork_widget(artwork_label, sidebar_artwork_size, &artwork_pixmap);
    update_artwork_widget(main_artwork_label, 240, &main_artwork_pixmap);
}

static Pixmap load_album_grid_pixmap(const char *url, unsigned int index)
{
    (void)index;
    if (url == NULL || url[0] == '\0') return XmUNSPECIFIED_PIXMAP;
    return fetch_artwork_pixmap(album_grid, GRID_ARTWORK, url);
}

static void schedule_grid_artwork(void);

static void queue_album_popup_track(Widget widget, XtPointer client_data,
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

static void show_album_tracks(unsigned int index, int cell_x, int cell_y)
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

static void reflow_album_grid(void)
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

static void draw_album_grid(XExposeEvent *expose)
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

static void grid_artwork_timeout(XtPointer client_data, XtIntervalId *timer_id)
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
