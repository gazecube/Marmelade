#include "app_internal.h"

XmFontList browser_list_font_list(void)
{
    XmFontList font_list = NULL;
    if (source_list_widget != NULL)
        XtVaGetValues(source_list_widget, XmNfontList, &font_list, NULL);
    return font_list;
}

void draw_browser_text(Display *display, Drawable drawable, GC gc,
                       const char *text, int x, int y, int width,
                       unsigned char alignment)
{
    XmFontList font_list = browser_list_font_list();
    XmString string;
    Dimension height;
    if (font_list == NULL || text == NULL || width <= 0) return;
    string = XmStringCreateLocalized((char *)text);
    height = XmStringHeight(font_list, string);
    XmStringDraw(display, drawable, font_list, string, gc,
                 (Position)x, (Position)(y + (LIST_ROW_HEIGHT - (int)height) / 2),
                 (Dimension)width, alignment, XmSTRING_DIRECTION_L_TO_R, NULL);
    XmStringFree(string);
}

void draw_browser_row_columns(Display *display, Drawable drawable, GC gc,
                              unsigned int model, int y, int width)
{
    char left[768];
    int duration_width = 52;
    int artist_width = 200;
    int gap = 12;
    int duration_x, artist_x, title_width;
    const char *kind;

    if (model >= current_view_count) return;
    kind = current_view_item_kinds[model];

    if (current_view_container_rows[model] || strcmp(kind, "disc") == 0) {
        draw_browser_text(display, drawable, gc, current_view_labels[model],
                          LIST_SIDE_PAD, y,
                          width > LIST_SIDE_PAD * 2 ? width - LIST_SIDE_PAD * 2 : width,
                          XmALIGNMENT_BEGINNING);
        return;
    }

    if (width < 420) artist_width = width / 3;
    if (artist_width < 96) artist_width = 96;

    duration_x = width - LIST_SIDE_PAD - duration_width;
    artist_x = duration_x - gap - artist_width;
    title_width = artist_x - gap - LIST_SIDE_PAD;
    if (title_width < 32) title_width = 32;

    if (strcmp(current_view_type, "queue") == 0) {
        snprintf(left, sizeof(left), "%s%s %s",
                 current_view_is_current[model] ? ">" : " ",
                 current_view_is_autoplay[model] ? "A" : " ",
                 current_view_titles[model]);
    } else if (current_view_track_numbers[model] > 0) {
        snprintf(left, sizeof(left), "%02u  %s",
                 current_view_track_numbers[model], current_view_titles[model]);
    } else if (strcmp(kind, "album") == 0 || strcmp(kind, "playlist") == 0) {
        snprintf(left, sizeof(left), "[+] %s", current_view_titles[model]);
    } else {
        snprintf(left, sizeof(left), "%s", current_view_titles[model]);
    }

    draw_browser_text(display, drawable, gc, left, LIST_SIDE_PAD, y,
                      title_width, XmALIGNMENT_BEGINNING);
    draw_browser_text(display, drawable, gc, current_view_artists[model],
                      artist_x, y, artist_width, XmALIGNMENT_BEGINNING);

    if (current_view_durations[model] > 0) {
        char duration[24];
        snprintf(duration, sizeof(duration), "%u:%02u",
                 current_view_durations[model] / 60,
                 current_view_durations[model] % 60);
        draw_browser_text(display, drawable, gc, duration, duration_x, y,
                          duration_width, XmALIGNMENT_END);
    }
}

void sync_browser_scheme_colors(void)
{
    Widget clip = NULL;
    Pixel background, foreground;

    if (source_list_widget == NULL || browser_list_canvas == NULL) return;
    XtVaGetValues(source_list_widget,
                  XmNbackground, &background,
                  XmNforeground, &foreground,
                  NULL);

    XtVaSetValues(browser_list_canvas,
                  XmNbackground, background,
                  XmNforeground, foreground,
                  NULL);
    if (browser_list_scroller != NULL) {
        XtVaSetValues(browser_list_scroller,
                      XmNbackground, background,
                      XmNforeground, foreground,
                      NULL);
        XtVaGetValues(browser_list_scroller, XmNclipWindow, &clip, NULL);
        if (clip != NULL)
            XtVaSetValues(clip,
                          XmNbackground, background,
                          XmNforeground, foreground,
                          NULL);
    }
    if (list_frame != NULL)
        XtVaSetValues(list_frame,
                      XmNbackground, background,
                      XmNforeground, foreground,
                      NULL);
}

void browser_list_reflow(void)
{
    Widget clip = NULL;
    Dimension width = 640, height = 360, content_height;
    if (browser_list_canvas == NULL || browser_list_scroller == NULL) return;
    XtVaGetValues(browser_list_scroller, XmNclipWindow, &clip, NULL);
    if (clip != NULL)
        XtVaGetValues(clip, XmNwidth, &width, XmNheight, &height, NULL);
    else
        XtVaGetValues(browser_list_scroller, XmNwidth, &width, XmNheight, &height, NULL);
    if (width < 80) width = 80;
    content_height = (Dimension)(LIST_TITLE_HEIGHT + visible_view_count * LIST_ROW_HEIGHT);
    if (content_height < height) content_height = height;
    XtVaSetValues(browser_list_canvas, XmNwidth, width, XmNheight, content_height, NULL);
}

void refresh_browser_list(void)
{
    sync_browser_scheme_colors();
    browser_list_reflow();
    if (browser_list_canvas != NULL && XtIsRealized(browser_list_canvas))
        XClearArea(XtDisplay(browser_list_canvas), XtWindow(browser_list_canvas),
                   0, 0, 0, 0, True);
}

void repaint_browser_visible_area(void)
{
    Widget clip = NULL, vertical = NULL;
    Dimension clip_width = 0, clip_height = 0;
    int value = 0;

    if (browser_list_canvas == NULL || browser_list_scroller == NULL ||
        !XtIsRealized(browser_list_canvas)) return;

    XtVaGetValues(browser_list_scroller,
                  XmNclipWindow, &clip,
                  XmNverticalScrollBar, &vertical,
                  NULL);
    if (clip == NULL) return;
    XtVaGetValues(clip, XmNwidth, &clip_width, XmNheight, &clip_height, NULL);
    if (vertical != NULL) XtVaGetValues(vertical, XmNvalue, &value, NULL);
    if (clip_width == 0 || clip_height == 0) return;

    XClearArea(XtDisplay(browser_list_canvas), XtWindow(browser_list_canvas),
               0, value, clip_width, clip_height, True);
    XFlush(XtDisplay(browser_list_canvas));
}

void browser_resize_redraw_timeout(XtPointer client_data, XtIntervalId *timer_id)
{
    (void)client_data; (void)timer_id;
    browser_resize_redraw_timer = (XtIntervalId)0;
    browser_list_reflow();
    repaint_browser_visible_area();
}

void schedule_browser_resize_redraw(void)
{
    if (browser_resize_redraw_timer != (XtIntervalId)0) return;
    browser_resize_redraw_timer = XtAppAddTimeOut(application_context, 0,
                                                   browser_resize_redraw_timeout, NULL);
}

void draw_browser_list(XExposeEvent *expose)
{
    Display *display;
    Window window;
    Screen *screen;
    Colormap colormap;
    Pixel foreground, background, derived_foreground, top, bottom, select;
    XGCValues values;
    GC gc;
    Dimension width = 0;
    unsigned int first, last, row;
    int queue_view;

    if (browser_list_canvas == NULL || !XtIsRealized(browser_list_canvas)) return;
    display = XtDisplay(browser_list_canvas);
    window = XtWindow(browser_list_canvas);
    screen = XtScreen(browser_list_canvas);
    colormap = DefaultColormapOfScreen(screen);
    XtVaGetValues(browser_list_canvas,
                  XmNforeground, &foreground,
                  XmNbackground, &background,
                  XmNwidth, &width,
                  NULL);
    XmGetColors(screen, colormap, background, &derived_foreground, &top, &bottom, &select);
    values.foreground = foreground;
    values.background = background;
    gc = XCreateGC(display, window, GCForeground | GCBackground, &values);

    queue_view = browser_mode == 0 && strcmp(current_view_path, "/v1/player/queue") == 0;
    if (expose->y < LIST_TITLE_HEIGHT) {
        XmFontList font_list = browser_list_font_list();
        XmString title = XmStringCreateLocalized(current_view_heading);
        Dimension title_height = font_list != NULL ? XmStringHeight(font_list, title) : 14;
        int title_y = (LIST_TITLE_HEIGHT - (int)title_height) / 2;
        if (queue_view && font_list != NULL) {
            XmStringDraw(display, window, font_list, title, gc,
                         0, (Position)title_y, width,
                         XmALIGNMENT_CENTER, XmSTRING_DIRECTION_L_TO_R, NULL);
            update_shuffle_button();
            update_repeat_button();
            update_automix_button();
            update_autoplay_button();
            if (shuffle_list_pixmap != XmUNSPECIFIED_PIXMAP && shuffle_list_pixmap != None &&
                repeat_list_pixmap != XmUNSPECIFIED_PIXMAP && repeat_list_pixmap != None &&
                automix_list_pixmap != XmUNSPECIFIED_PIXMAP && automix_list_pixmap != None &&
                autoplay_list_pixmap != XmUNSPECIFIED_PIXMAP && autoplay_list_pixmap != None) {
                int autoplay_x = (int)width - LIST_SIDE_PAD - LIST_SMALL_ICON_SIZE;
                int automix_x = autoplay_x - LIST_ICON_GAP - LIST_SMALL_ICON_SIZE;
                int repeat_x = automix_x - LIST_ICON_GAP - LIST_ICON_SIZE;
                int shuffle_x = repeat_x - LIST_ICON_GAP - LIST_ICON_SIZE;
                int icon_y = (LIST_TITLE_HEIGHT - LIST_ICON_SIZE) / 2;
                int small_icon_y = (LIST_TITLE_HEIGHT - LIST_SMALL_ICON_SIZE) / 2;
                XCopyArea(display, shuffle_list_pixmap, window, gc, 0, 0,
                          LIST_ICON_SIZE, LIST_ICON_SIZE, shuffle_x, icon_y);
                XCopyArea(display, repeat_list_pixmap, window, gc, 0, 0,
                          LIST_ICON_SIZE, LIST_ICON_SIZE, repeat_x, icon_y);
                XCopyArea(display, automix_list_pixmap, window, gc, 0, 0,
                          LIST_SMALL_ICON_SIZE, LIST_SMALL_ICON_SIZE, automix_x, small_icon_y);
                XCopyArea(display, autoplay_list_pixmap, window, gc, 0, 0,
                          LIST_SMALL_ICON_SIZE, LIST_SMALL_ICON_SIZE, autoplay_x, small_icon_y);
            }
        } else if (font_list != NULL) {
            XmStringDraw(display, window, font_list, title, gc,
                         LIST_SIDE_PAD, (Position)title_y,
                         width > LIST_SIDE_PAD * 2 ? width - LIST_SIDE_PAD * 2 : width,
                         XmALIGNMENT_BEGINNING, XmSTRING_DIRECTION_L_TO_R, NULL);
        }
        XmStringFree(title);
    }

    if (expose->y + expose->height <= LIST_TITLE_HEIGHT || visible_view_count == 0) {
        XFreeGC(display, gc);
        return;
    }
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

int browser_visible_row_for_model(int model)
{
    unsigned int row;
    if (model < 0) return -1;
    for (row = 0; row < visible_view_count; ++row)
        if ((int)visible_to_model[row] == model) return (int)row;
    return -1;
}

void redraw_browser_model_row(int model)
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

void set_browser_selection(int model)
{
    int previous = browser_list_selected_model;
    if (previous == model) return;
    browser_list_selected_model = model;
    redraw_browser_model_row(previous);
    redraw_browser_model_row(model);
}

int contains_case_insensitive(const char *text, const char *needle)
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

void apply_filter(const char *filter)
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

int populate_view(const char *response)
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
        if (strcmp(current_view_type, "artists") == 0 || strcmp(item_kind, "artist") == 0)
            current_view_artists[count][0] = '\0';
        else
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

void load_view(const char *path)
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

void search_filter_changed(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char *value = XmTextFieldGetString(widget);
    (void)client_data; (void)call_data;
    snprintf(current_filter, sizeof(current_filter), "%s", value != NULL ? value : "");
    if (value != NULL) XtFree(value);
    apply_filter(current_filter);
}

void url_encode(const char *input, char *output, size_t size)
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

void search_activated(Widget widget, XtPointer client_data, XtPointer call_data)
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

void refresh_library(void)
{
    load_view(current_view_path);
}

void source_selected(Widget widget, XtPointer client_data, XtPointer call_data)
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
