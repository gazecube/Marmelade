    first = expose->y <= LIST_TITLE_HEIGHT ? 0 :
        (unsigned int)(expose->y - LIST_TITLE_HEIGHT) / LIST_ROW_HEIGHT;
    last = (unsigned int)(expose->y + expose->height - LIST_TITLE_HEIGHT) / LIST_ROW_HEIGHT + 1;
    if (last > visible_view_count) last = visible_view_count;
    for (row = first; row < last; ++row) {
        unsigned int model = visible_to_model[row];
        int y = LIST_TITLE_HEIGHT + (int)row * LIST_ROW_HEIGHT;
        if ((int)model == browser_list_selected_model) {
            XSetForeground(display, gc, select);
            XFillRectangle(display, window, gc, 0, y, width, LIST_ROW_HEIGHT);
            XSetForeground(display, gc, foreground);
        }
        draw_browser_row_columns(display, window, gc, model, y, (int)width);
    }
    XFreeGC(display, gc);
}

static int browser_visible_row_for_model(int model)
{
    unsigned int row;
    if (model < 0) return -1;
    for (row = 0; row < visible_view_count; ++row)
        if ((int)visible_to_model[row] == model) return (int)row;
    return -1;
}

static void redraw_browser_model_row(int model)
{
    int row;
    XExposeEvent expose;
    Display *display;
    Window window;

    if (browser_list_canvas == NULL || !XtIsRealized(browser_list_canvas)) return;
    row = browser_visible_row_for_model(model);
    if (row < 0) return;
    display = XtDisplay(browser_list_canvas);
    window = XtWindow(browser_list_canvas);

    /* Clear without generating an Expose, then repaint this row synchronously.
       This keeps selection feedback immediate even if the click is about to
       perform a blocking bridge request to open a collection. */
    XClearArea(display, window, 0,
               LIST_TITLE_HEIGHT + row * LIST_ROW_HEIGHT,
               0, LIST_ROW_HEIGHT, False);
    memset(&expose, 0, sizeof(expose));
    expose.type = Expose;
    expose.display = display;
    expose.window = window;
    expose.x = 0;
    expose.y = LIST_TITLE_HEIGHT + row * LIST_ROW_HEIGHT;
    expose.width = (int)WidthOfScreen(XtScreen(browser_list_canvas));
    expose.height = LIST_ROW_HEIGHT;
    draw_browser_list(&expose);
    XFlush(display);
}

static void set_browser_selection(int model)
{
    int previous = browser_list_selected_model;
    if (previous == model) return;
    browser_list_selected_model = model;
    redraw_browser_model_row(previous);
    redraw_browser_model_row(model);
}

static int contains_case_insensitive(const char *text, const char *needle)
{
    size_t length = strlen(needle), i, j;
    if (length == 0) return 1;
    for (i = 0; text[i] != '\0'; ++i) {
        for (j = 0; j < length && text[i + j] != '\0' &&
             tolower((unsigned char)text[i + j]) ==
             tolower((unsigned char)needle[j]); ++j) {}
        if (j == length) return 1;
    }
    return 0;
}

static void apply_filter(const char *filter)
{
    unsigned int i;
    visible_view_count = 0;
    for (i = 0; i < current_view_count; ++i) {
        if (current_view_is_track_detail && current_view_collapsed && i > 0) continue;
        if (!contains_case_insensitive(current_view_labels[i], filter)) continue;
        if (visible_view_count >= current_render_limit) break;
        visible_to_model[visible_view_count++] = i;
    }
    refresh_browser_list();
}


static int populate_view(const char *response)
{
    char title[512], artist[512], line[1200], heading[512], kind[32], id[256];
    char item_kind[32], catalog_id[256], artwork_url[1024], container_kind[32];
    char container_id[256], container_catalog_id[256];
    const char *cursor;
    unsigned int count = 0;
    int is_current, is_autoplay;
    double duration, disc_number, disc_count, track_number;
    unsigned int previous_disc = 0;
    heading[0] = '\0';
    container_kind[0] = '\0'; container_id[0] = '\0'; container_catalog_id[0] = '\0';
    if (strstr(response, "\"source\":\"apple-music-web\"") == NULL)
        return 0;
    if (json_string(response, "viewTitle", heading, sizeof(heading)))
        snprintf(current_view_heading, sizeof(current_view_heading), "%s", heading);
    if (json_string(response, "viewType", kind, sizeof(kind)))
        snprintf(current_view_type, sizeof(current_view_type), "%s", kind);
    current_view_container_kind[0] = '\0';
    cursor = strstr(response, "\"items\"");
    if (cursor == NULL) return 0;
    current_view_count = 0;
    current_render_limit = 30;
    current_view_is_track_detail = strcmp(current_view_type, "tracks") == 0;
    current_view_collapsed = 0;
    if (current_view_is_track_detail && heading[0] != '\0') {
        json_string(response, "containerKind", container_kind, sizeof(container_kind));
        snprintf(current_view_container_kind, sizeof(current_view_container_kind), "%s",
                 container_kind);
        json_string(response, "containerId", container_id, sizeof(container_id));
        json_string(response, "containerCatalogId", container_catalog_id,
                    sizeof(container_catalog_id));
        snprintf(current_view_labels[count], sizeof(current_view_labels[count]),
                 "[-] %s", heading);
        snprintf(current_view_ids[count], sizeof(current_view_ids[count]), "%s", container_id);
        snprintf(current_view_catalog_ids[count], sizeof(current_view_catalog_ids[count]), "%s",
                 container_catalog_id[0] != '\0' ? container_catalog_id : container_id);
        snprintf(current_view_item_kinds[count], sizeof(current_view_item_kinds[count]),
                 "%s", container_kind[0] != '\0' ? container_kind : "container");
        current_view_disc_numbers[count] = 0;
        current_view_track_numbers[count] = 0;
        current_view_container_rows[count] = 1;
        current_view_artwork_urls[count][0] = '\0';
        snprintf(current_view_titles[count], sizeof(current_view_titles[count]), "%s", heading);
        current_view_artists[count][0] = '\0';
        current_view_durations[count] = 0;
        current_view_is_current[count] = 0;
        current_view_is_autoplay[count] = 0;
        count++;
    }
    while ((cursor = strstr(cursor, "\"id\"")) != NULL && count < 100) {
        title[0] = '\0'; artist[0] = '\0'; id[0] = '\0';
        item_kind[0] = '\0'; catalog_id[0] = '\0'; artwork_url[0] = '\0';
        duration = 0.0; disc_number = 0.0; disc_count = 0.0; track_number = 0.0;
        is_current = 0; is_autoplay = 0;
        if (!json_string(cursor, "id", id, sizeof(id))) break;
        json_string(cursor, "kind", item_kind, sizeof(item_kind));
        json_string(cursor, "catalogId", catalog_id, sizeof(catalog_id));
        json_string(cursor, "artworkURL", artwork_url, sizeof(artwork_url));
        if (!json_string(cursor, "title", title, sizeof(title))) break;
        json_string(cursor, "artist", artist, sizeof(artist));
        json_number(cursor, "duration", &duration);
        json_number(cursor, "discNumber", &disc_number);
        json_number(cursor, "discCount", &disc_count);
        json_number(cursor, "trackNumber", &track_number);
        if (strcmp(current_view_type, "queue") == 0) {
            json_boolean(cursor, "current", &is_current);
            json_boolean(cursor, "autoplay", &is_autoplay);
        }
        if ((strcmp(current_view_type, "tracks") == 0 ||
             strcmp(current_view_type, "songs") == 0) && disc_count > 1.0 &&
            disc_number > 0.0 &&
            (unsigned int)disc_number != previous_disc && count < 99) {
            snprintf(current_view_labels[count], sizeof(current_view_labels[count]),
                     "----- Disc %u -----", (unsigned int)disc_number);
            current_view_ids[count][0] = '\0';
            current_view_catalog_ids[count][0] = '\0';
            snprintf(current_view_item_kinds[count], sizeof(current_view_item_kinds[count]), "disc");
            current_view_disc_numbers[count] = (unsigned int)disc_number;
            current_view_track_numbers[count] = 0;
            current_view_container_rows[count] = 0;
            current_view_artwork_urls[count][0] = '\0';
            snprintf(current_view_titles[count], sizeof(current_view_titles[count]),
                     "Disc %u", (unsigned int)disc_number);
            current_view_artists[count][0] = '\0';
            current_view_durations[count] = 0;
            current_view_is_current[count] = 0;
            current_view_is_autoplay[count] = 0;
            count++;
            previous_disc = (unsigned int)disc_number;
        }
        if (strcmp(current_view_type, "queue") == 0)
            snprintf(line, sizeof(line), "%s%s %-32.32s  %-24.24s  %u:%02u",
                     is_current ? ">" : " ", is_autoplay ? "A" : " ", title, artist,
                     (unsigned int)duration / 60, (unsigned int)duration % 60);
        else if (strcmp(item_kind, "album") == 0 || strcmp(item_kind, "playlist") == 0)
            snprintf(line, sizeof(line), "[+] %-32.32s  %-24.24s", title, artist);
        else if (track_number > 0.0)
            snprintf(line, sizeof(line), "    %02u  %-32.32s  %-24.24s  %u:%02u",
                     (unsigned int)track_number, title, artist,
                     (unsigned int)duration / 60, (unsigned int)duration % 60);
        else
            snprintf(line, sizeof(line), "    %-32.32s  %-24.24s  %u:%02u",
                     title, artist, (unsigned int)duration / 60,
                     (unsigned int)duration % 60);
        snprintf(current_view_labels[count], sizeof(current_view_labels[count]), "%s", line);
        snprintf(current_view_titles[count], sizeof(current_view_titles[count]), "%s", title);
        snprintf(current_view_artists[count], sizeof(current_view_artists[count]), "%s", artist);
        current_view_durations[count] = duration > 0.0 ? (unsigned int)duration : 0;
        current_view_is_current[count] = is_current;
        current_view_is_autoplay[count] = is_autoplay;
        snprintf(current_view_ids[count], sizeof(current_view_ids[count]), "%s", id);
        snprintf(current_view_catalog_ids[count], sizeof(current_view_catalog_ids[count]),
                 "%s", catalog_id[0] != '\0' ? catalog_id : id);
        snprintf(current_view_item_kinds[count], sizeof(current_view_item_kinds[count]),
                 "%s", item_kind);
        current_view_disc_numbers[count] = (unsigned int)disc_number;
        current_view_track_numbers[count] = (unsigned int)track_number;
        current_view_container_rows[count] = 0;
        snprintf(current_view_artwork_urls[count], sizeof(current_view_artwork_urls[count]),
                 "%s", artwork_url);
        count++;
        cursor += 4;
    }
    current_view_count = count;
    if (grid_view_button != NULL)
        XtSetSensitive(grid_view_button,
                       strcmp(current_view_container_kind, "album") != 0);
    apply_filter(current_filter);
    return count > 0;
}

static void load_view(const char *path)
{
    char response[65536];
    unsigned int prior_limit = current_render_limit;
    int same_view = strcmp(path, populated_view_path) == 0;
    if (bridge_client_request(&bridge, "GET", path, NULL,
                              response, sizeof(response)) == 0) {
        if (!same_view) browser_list_selected_model = -1;
        populate_view(response);
        if (same_view && prior_limit > current_render_limit) {
            current_render_limit = prior_limit;
            if (current_render_limit > current_view_count)
                current_render_limit = current_view_count;
            apply_filter(current_filter);
        }
        snprintf(populated_view_path, sizeof(populated_view_path), "%s", path);
    }
}

static void search_filter_changed(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char *value = XmTextFieldGetString(widget);
    (void)client_data; (void)call_data;
    snprintf(current_filter, sizeof(current_filter), "%s", value != NULL ? value : "");
    if (value != NULL) XtFree(value);
    apply_filter(current_filter);
}

static void url_encode(const char *input, char *output, size_t size)
{
    static const char hex[] = "0123456789ABCDEF";
    size_t used = 0;
    while (*input != '\0' && used + 1 < size) {
        unsigned char c = (unsigned char)*input++;
        if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
            output[used++] = (char)c;
        else if (used + 3 < size) {
            output[used++] = '%'; output[used++] = hex[c >> 4]; output[used++] = hex[c & 15];
        } else break;
    }
    output[used] = '\0';
}

static void search_activated(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char *value = XmTextFieldGetString(widget);
    char encoded[768];
    (void)client_data; (void)call_data;
    if (value == NULL || value[0] == '\0') {
        if (value != NULL) XtFree(value);
        return;
    }
    url_encode(value, encoded, sizeof(encoded));
    snprintf(current_view_path, sizeof(current_view_path), "/v1/search?q=%s", encoded);
    XtFree(value);
    browser_mode = 0;
    XtUnmanageChild(album_grid_scroller);
    XtUnmanageChild(main_artwork_label);
    XtManageChild(browser_widget);
    load_view(current_view_path);
}

static void refresh_library(void)
{
    load_view(current_view_path);
}

static void source_selected(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XmListCallbackStruct *selection = (XmListCallbackStruct *)call_data;
    const char *path;
    (void)widget; (void)client_data;
    if (selection->item_position < 1 ||
        (unsigned int)selection->item_position > source_count) return;
    path = source_paths[selection->item_position - 1];
    if (path[0] == '\0') return;
    if (strcmp(path, "@now-playing") == 0) {
        browser_mode = 0;
        if (album_tracks_popup != NULL) XtPopdown(album_tracks_popup);
        snprintf(current_view_path, sizeof(current_view_path), "%s", "/v1/player/queue");
        XtUnmanageChild(album_grid_scroller);
        XtUnmanageChild(main_artwork_label);
        XtManageChild(browser_widget);
        load_view(current_view_path);
            return;
    }
    snprintf(current_view_path, sizeof(current_view_path), "%s", path);
    browser_mode = 0;
    XtUnmanageChild(album_grid_scroller);
    XtUnmanageChild(main_artwork_label);
    XtManageChild(browser_widget);
    load_view(current_view_path);
}
