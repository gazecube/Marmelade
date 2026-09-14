#include "app_internal.h"
#include "icons.h"

void poll_player(XtPointer client_data, XtIntervalId *timer_id);

void repaint_position_scale(int percent)
{
    if (position_scale == NULL || !XtIsRealized(position_scale)) return;
    if (percent == prior_position_percent) return;
    prior_position_percent = percent;
    XmScaleSetValue(position_scale, percent);
    XClearArea(XtDisplay(position_scale), XtWindow(position_scale),
               0, 0, 0, 0, True);
    XmUpdateDisplay(position_scale);
}

void schedule_player_poll(unsigned long delay_ms)
{
    if (player_poll_timer != (XtIntervalId)0) return;
    player_poll_timer = XtAppAddTimeOut(application_context, delay_ms,
                                        poll_player, NULL);
}

void poll_player(XtPointer client_data, XtIntervalId *timer_id)
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
    player_duration = duration;
    if (duration > 0.0) percent = (int)((position / duration) * 100.0);
    if (percent < 0) percent = 0;
    if (percent > 100) percent = 100;
    repaint_position_scale(percent);
    player_is_playing = strcmp(playback, "playing") == 0;
    if (play_pause_button != NULL)
        set_button_icon(play_pause_button,
                        player_is_playing ? pause_icon_xpm : play_icon_xpm);
    set_volume_icon(player_volume);
    update_shuffle_button();
    update_repeat_button();
    update_automix_button();
    update_autoplay_button();
    if (title[0] != '\0' && artist[0] != '\0')
        snprintf(message, sizeof(message), "%s: %s - %s", playback, title, artist);
    else if (title[0] != '\0')
        snprintf(message, sizeof(message), "%s: %s", playback, title);
    else
        snprintf(message, sizeof(message), "Apple Music: %s", playback);
    set_status(message);
    snprintf(artwork_key, sizeof(artwork_key), "%s|%s|%s", track_id, title, artist);
    if (title[0] != '\0' && strcmp(artwork_key, prior_artwork_key) != 0) {
        snprintf(prior_artwork_key, sizeof(prior_artwork_key), "%s", artwork_key);
        update_artwork();
    }
    extend_lazy_views();
    if (player_poll_count++ % 10 == 0) refresh_library();

schedule:
    schedule_player_poll(1000);
}

void player_action(Widget widget, XtPointer client_data, XtPointer call_data)
{
    const char *action = (const char *)client_data;
    char path[128], response[2048];
    (void)widget; (void)call_data;
    snprintf(path, sizeof(path), "/v1/player/%s", action);
    if (bridge_client_request(&bridge, "POST", path, "{}", response, sizeof(response)) == 0) {
        char message[128];
        snprintf(message, sizeof(message), "Player: %s", action);
        set_status(message);
    } else
        set_status("Bridge request failed");
}

void volume_changed(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XmScaleCallbackStruct *scale = (XmScaleCallbackStruct *)call_data;
    char body[64], response[1024];
    (void)widget; (void)client_data;
    set_volume_icon(scale->value / 100.0);
    snprintf(body, sizeof(body), "{\"value\":%.2f}", scale->value / 100.0);
    if (bridge_client_request(&bridge, "POST", "/v1/player/volume", body,
                              response, sizeof(response)) != 0)
        set_status("Volume update failed");
}

void position_changed(Widget widget, XtPointer client_data, XtPointer call_data)
{
    XmScaleCallbackStruct *scale = (XmScaleCallbackStruct *)call_data;
    char body[96], response[1024];
    (void)widget; (void)client_data;
    if (player_duration <= 0.0) return;
    snprintf(body, sizeof(body), "{\"time\":%.3f}",
             player_duration * ((double)scale->value / 100.0));
    bridge_client_request(&bridge, "POST", "/v1/player/seek", body,
                          response, sizeof(response));
}

void position_volume_popup(void)
{
    Position root_x, root_y;
    Dimension button_width, button_height, popup_height;

    if (volume_popup == NULL || volume_button == NULL ||
        !XtIsRealized(volume_button)) return;
    XtVaGetValues(volume_button,
                  XmNwidth, &button_width,
                  XmNheight, &button_height,
                  NULL);
    XtVaGetValues(volume_popup, XmNheight, &popup_height, NULL);
    XtTranslateCoords(volume_button, 0, 0, &root_x, &root_y);
    XtVaSetValues(volume_popup,
                  XmNx, root_x + (Position)button_width + 4,
                  XmNy, root_y - ((Position)popup_height - (Position)button_height) / 2,
                  NULL);
}

static int window_is_inside_volume_popup(Display *display, Window window)
{
    Window popup_window;

    if (volume_popup == NULL || !XtIsRealized(volume_popup) || window == None)
        return 0;
    popup_window = XtWindow(volume_popup);
    while (window != None && window != PointerRoot) {
        Window root, parent, *children = NULL;
        unsigned int child_count = 0;

        if (window == popup_window) return 1;
        if (!XQueryTree(display, window, &root, &parent, &children, &child_count))
            break;
        if (children != NULL) XFree(children);
        if (parent == window) break;
        window = parent;
    }
    return 0;
}

void follow_volume_popup(Widget widget, XtPointer client_data,
                         XEvent *event, Boolean *continue_dispatch)
{
    (void)widget; (void)client_data; (void)continue_dispatch;
    if (!volume_popup_visible || event == NULL) return;
    if (event->type == ConfigureNotify) {
        position_volume_popup();
    } else if (event->type == FocusOut) {
        Window focus = None;
        int revert_to;
        XGetInputFocus(XtDisplay(widget), &focus, &revert_to);
        if (window_is_inside_volume_popup(XtDisplay(widget), focus)) return;
        XtPopdown(volume_popup);
        volume_popup_visible = 0;
    } else if (event->type == UnmapNotify) {
        XtPopdown(volume_popup);
        volume_popup_visible = 0;
    }
}

void toggle_volume_popup(Widget widget, XtPointer client_data, XtPointer call_data)
{
    (void)widget; (void)client_data; (void)call_data;
    if (volume_popup_visible) {
        XtPopdown(volume_popup);
        volume_popup_visible = 0;
        return;
    }
    position_volume_popup();
    XtPopup(volume_popup, XtGrabNone);
    volume_popup_visible = 1;
}

void dismiss_volume_popup(Widget widget, XtPointer client_data,
                          XEvent *event, Boolean *continue_dispatch)
{
    (void)widget; (void)client_data; (void)event; (void)continue_dispatch;
    if (!volume_popup_visible) return;
    XtPopdown(volume_popup);
    volume_popup_visible = 0;
}

Widget icon_button(Widget parent, const char *name, const char *action,
                   char **xpm)
{
    Widget result = XtVaCreateManagedWidget(name, xmPushButtonWidgetClass, parent, NULL);
    Pixmap pixmap = make_button_pixmap(result, xpm);
    if (pixmap != XmUNSPECIFIED_PIXMAP)
        XtVaSetValues(result,
                      XmNlabelType, XmPIXMAP,
                      XmNlabelPixmap, pixmap,
                      XmNwidth, 31,
                      XmNheight, 31,
                      NULL);
    register_button_icon(result, xpm, pixmap);
    XtAddCallback(result, XmNactivateCallback, player_action, (XtPointer)action);
    XtAddEventHandler(result, ButtonPressMask, False, dismiss_volume_popup, NULL);
    return result;
}

Widget popup_icon_button(Widget parent, const char *name, char **xpm)
{
    Widget result = XtVaCreateManagedWidget(name, xmPushButtonWidgetClass, parent, NULL);
    Pixmap pixmap = make_button_pixmap(result, xpm);
    if (pixmap != XmUNSPECIFIED_PIXMAP)
        XtVaSetValues(result,
                      XmNlabelType, XmPIXMAP,
                      XmNlabelPixmap, pixmap,
                      XmNwidth, 31,
                      XmNheight, 31,
                      NULL);
    register_button_icon(result, xpm, pixmap);
    return result;
}

void play_pause_action(Widget widget, XtPointer client_data, XtPointer call_data)
{
    (void)client_data;
    player_action(widget, (XtPointer)(player_is_playing ? "pause" : "play"), call_data);
}

Widget play_pause_icon_button(Widget parent)
{
    Widget result = XtVaCreateManagedWidget("playPause", xmPushButtonWidgetClass,
                                             parent, NULL);
    {
        Pixmap pixmap = make_button_pixmap(result, play_icon_xpm);
        if (pixmap != XmUNSPECIFIED_PIXMAP)
        XtVaSetValues(result, XmNlabelType, XmPIXMAP,
                      XmNlabelPixmap, pixmap,
                      XmNwidth, 31, XmNheight, 31, NULL);
        register_button_icon(result, play_icon_xpm, pixmap);
    }
    XtAddCallback(result, XmNactivateCallback, play_pause_action, NULL);
    XtAddEventHandler(result, ButtonPressMask, False, dismiss_volume_popup, NULL);
    return result;
}

void shuffle_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char body[64], response[2048];
    (void)widget; (void)client_data; (void)call_data;
    snprintf(body, sizeof(body), "{\"enabled\":%s}",
             player_shuffle_enabled ? "false" : "true");
    if (bridge_client_request(&bridge, "POST", "/v1/player/shuffle", body,
                              response, sizeof(response)) == 0) {
        player_shuffle_enabled = !player_shuffle_enabled;
        update_shuffle_button();
        set_status(player_shuffle_enabled ? "Shuffle enabled" : "Shuffle disabled");
    } else
        set_status("Could not update shuffle state");
}

void repeat_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char body[64], response[2048];
    int next_mode = (player_repeat_mode + 1) % 3;
    (void)widget; (void)client_data; (void)call_data;
    snprintf(body, sizeof(body), "{\"mode\":\"%s\"}", repeat_mode_name(next_mode));
    if (bridge_client_request(&bridge, "POST", "/v1/player/repeat", body,
                              response, sizeof(response)) == 0) {
        player_repeat_mode = next_mode;
        update_repeat_button();
        if (next_mode == 2) set_status("Repeat one enabled");
        else if (next_mode == 1) set_status("Repeat all enabled");
        else set_status("Repeat disabled");
    } else
        set_status("Could not update repeat state");
}

void automix_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char body[64], response[2048];
    (void)widget; (void)client_data; (void)call_data;
    snprintf(body, sizeof(body), "{\"enabled\":%s}",
             player_automix_enabled ? "false" : "true");
    if (bridge_client_request(&bridge, "POST", "/v1/player/automix", body,
                              response, sizeof(response)) == 0) {
        player_automix_enabled = !player_automix_enabled;
        update_automix_button();
        set_status(player_automix_enabled ? "AutoMix enabled" : "AutoMix disabled");
    } else
        set_status("Could not update AutoMix state");
}

void autoplay_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char body[64], response[2048];
    (void)widget; (void)client_data; (void)call_data;
    snprintf(body, sizeof(body), "{\"enabled\":%s}",
             player_autoplay_enabled ? "false" : "true");
    if (bridge_client_request(&bridge, "POST", "/v1/player/autoplay", body,
                              response, sizeof(response)) == 0) {
        player_autoplay_enabled = !player_autoplay_enabled;
        update_autoplay_button();
        set_status(player_autoplay_enabled ? "Autoplay enabled" : "Autoplay disabled");
    } else
        set_status("Could not update Autoplay state");
}

void shutdown_app(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char response[256];
    (void)widget; (void)client_data; (void)call_data;
    if (shutting_down) return;
    shutting_down = 1;
    bridge_client_request(&bridge, "POST", "/v1/shutdown", "{}",
                          response, sizeof(response));
    bridge_client_stop(&bridge);
    exit(0);
}

int command_available(const char *program)
{
    const char *path = getenv("PATH");
    const char *start, *end;
    char candidate[1024];

    if (program == NULL || program[0] == '\0') return 0;
    if (strchr(program, '/') != NULL) return access(program, X_OK) == 0;
    if (path == NULL || path[0] == '\0') path = "/usr/local/bin:/usr/bin:/bin";
    start = path;
    for (;;) {
        size_t n;
        end = strchr(start, ':');
        n = end != NULL ? (size_t)(end - start) : strlen(start);
        if (n && n + strlen(program) + 2 <= sizeof(candidate)) {
            memcpy(candidate, start, n);
            candidate[n] = '/';
            strcpy(candidate + n + 1, program);
            if (access(candidate, X_OK) == 0) return 1;
        }
        if (end == NULL) break;
        start = end + 1;
    }
    return 0;
}

void show_fatal_startup_error(const char *message)
{
    pid_t pid;

    fprintf(stderr, "motif-apple-music: fatal: %s\n", message);
    fflush(stderr);

    if (getenv("DISPLAY") == NULL || getenv("DISPLAY")[0] == '\0')
        return;

    pid = fork();
    if (pid != 0) return;

    if (command_available("zenity"))
        execlp("zenity", "zenity", "--error", "--title=Motif Apple Music", "--text", message, (char *)NULL);
    if (command_available("kdialog"))
        execlp("kdialog", "kdialog", "--title", "Motif Apple Music", "--error", message, (char *)NULL);
    if (command_available("xmessage"))
        execlp("xmessage", "xmessage", "-center", "-title", "Motif Apple Music", message, (char *)NULL);
    _exit(127);
}

void authorize_music(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char response[512];
    (void)widget; (void)client_data; (void)call_data;
    if (bridge_client_request(&bridge, "POST", "/v1/browser/show", "{}",
                              response, sizeof(response)) != 0)
        set_status("Could not show the Apple Music login window");
    else
        set_status("Sign into Apple Music in the browser window");
}
