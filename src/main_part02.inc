
static void update_repeat_button(void)
{
    char **xpm;
    if (browser_list_canvas == NULL || rendered_repeat_mode == player_repeat_mode) return;
    rendered_repeat_mode = player_repeat_mode;
    xpm = player_repeat_mode == 2 ? repeat_one_icon_xpm :
          (player_repeat_mode == 1 ? repeat_all_icon_xpm : repeat_off_icon_xpm);
    if (repeat_list_pixmap != XmUNSPECIFIED_PIXMAP && repeat_list_pixmap != None)
        XFreePixmap(XtDisplay(browser_list_canvas), repeat_list_pixmap);
    repeat_list_pixmap = make_button_pixmap(browser_list_canvas, xpm);
    damage_list_title_row();
}

static void update_automix_button(void)
{
    if (browser_list_canvas == NULL || rendered_automix_state == player_automix_enabled) return;
    rendered_automix_state = player_automix_enabled;
    if (automix_list_pixmap != XmUNSPECIFIED_PIXMAP && automix_list_pixmap != None)
        XFreePixmap(XtDisplay(browser_list_canvas), automix_list_pixmap);
    automix_list_pixmap = make_button_pixmap(browser_list_canvas,
        player_automix_enabled ? automix_on_icon_xpm : automix_off_icon_xpm);
    damage_list_title_row();
}

static void update_autoplay_button(void)
{
    if (browser_list_canvas == NULL || rendered_autoplay_state == player_autoplay_enabled) return;
    rendered_autoplay_state = player_autoplay_enabled;
    if (autoplay_list_pixmap != XmUNSPECIFIED_PIXMAP && autoplay_list_pixmap != None)
        XFreePixmap(XtDisplay(browser_list_canvas), autoplay_list_pixmap);
    autoplay_list_pixmap = make_button_pixmap(browser_list_canvas,
        player_autoplay_enabled ? autoplay_on_icon_xpm : autoplay_off_icon_xpm);
    damage_list_title_row();
}

static void set_status(const char *text)
{
    XmString value = XmStringCreateLocalized((char *)text);
    XtVaSetValues(status_label, XmNlabelString, value, NULL);
    XmStringFree(value);
}

static void set_volume_icon(double volume)
{
    int level;
    char **xpm;
    if (volume <= 0.001) { level = 0; xpm = muted_icon_xpm; }
    else if (volume <= 0.33) { level = 1; xpm = volume_low_icon_xpm; }
    else if (volume <= 0.66) { level = 2; xpm = volume_medium_icon_xpm; }
    else { level = 3; xpm = volume_icon_xpm; }
    if (volume_button != NULL && level != volume_icon_level) {
        volume_icon_level = level;
        set_button_icon(volume_button, xpm);
    }
}

static XmFontList browser_list_font_list(void)
{
    XmFontList font_list = NULL;
    if (source_list_widget != NULL)
        XtVaGetValues(source_list_widget, XmNfontList, &font_list, NULL);
    return font_list;
}

static void draw_browser_text(Display *display, Drawable drawable, GC gc,
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

static void draw_browser_row_columns(Display *display, Drawable drawable, GC gc,
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

    /* Structural rows keep their legacy single-string rendering. */
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

static void sync_browser_scheme_colors(void)
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

static void browser_list_reflow(void)
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

static void refresh_browser_list(void)
{
    sync_browser_scheme_colors();
    browser_list_reflow();
    if (browser_list_canvas != NULL && XtIsRealized(browser_list_canvas))
        XClearArea(XtDisplay(browser_list_canvas), XtWindow(browser_list_canvas),
                   0, 0, 0, 0, True);
}

static void repaint_browser_visible_area(void)
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

    /* Repaint only the viewport.  Resizing can move the title-row icons while
       leaving their previous pixels untouched; clearing the visible slice
       forces a fresh Expose without invalidating an arbitrarily tall lazy list. */
    XClearArea(XtDisplay(browser_list_canvas), XtWindow(browser_list_canvas),
               0, value, clip_width, clip_height, True);
    XFlush(XtDisplay(browser_list_canvas));
}

static void browser_resize_redraw_timeout(XtPointer client_data, XtIntervalId *timer_id)
{
    (void)client_data; (void)timer_id;
    browser_resize_redraw_timer = (XtIntervalId)0;
    browser_list_reflow();
    repaint_browser_visible_area();
}

static void schedule_browser_resize_redraw(void)
{
    if (browser_resize_redraw_timer != (XtIntervalId)0) return;
    browser_resize_redraw_timer = XtAppAddTimeOut(application_context, 0,
                                                   browser_resize_redraw_timeout, NULL);
}

static void draw_browser_list(XExposeEvent *expose)
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
