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

static void player_action(Widget widget, XtPointer client_data, XtPointer call_data)
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

static void volume_changed(Widget widget, XtPointer client_data, XtPointer call_data)
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

static void position_changed(Widget widget, XtPointer client_data, XtPointer call_data)
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

static void toggle_volume_popup(Widget widget, XtPointer client_data, XtPointer call_data)
{
    Position root_x, root_y;
    Dimension button_width, button_height, popup_height;
    (void)client_data; (void)call_data;
    if (volume_popup_visible) {
        XtPopdown(volume_popup);
        volume_popup_visible = 0;
        return;
    }
    XtVaGetValues(widget, XmNwidth, &button_width, XmNheight, &button_height, NULL);
    XtVaGetValues(volume_popup, XmNheight, &popup_height, NULL);
    XtTranslateCoords(widget, 0, 0, &root_x, &root_y);
    XtVaSetValues(volume_popup,
                  XmNx, root_x + (Position)button_width + 4,
                  XmNy, root_y - ((Position)popup_height - (Position)button_height) / 2,
                  NULL);
    XtPopup(volume_popup, XtGrabNone);
    volume_popup_visible = 1;
}

static void dismiss_volume_popup(Widget widget, XtPointer client_data,
                                 XEvent *event, Boolean *continue_dispatch)
{
    (void)widget; (void)client_data; (void)event; (void)continue_dispatch;
    if (!volume_popup_visible) return;
    XtPopdown(volume_popup);
    volume_popup_visible = 0;
}

static Widget icon_button(Widget parent, const char *name, const char *action,
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

static Widget popup_icon_button(Widget parent, const char *name, char **xpm)
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

static void play_pause_action(Widget widget, XtPointer client_data, XtPointer call_data)
{
    (void)client_data;
    player_action(widget, (XtPointer)(player_is_playing ? "pause" : "play"), call_data);
}

static Widget play_pause_icon_button(Widget parent)
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

static void shuffle_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
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

static void repeat_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
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


static void automix_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
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

static void autoplay_toggled(Widget widget, XtPointer client_data, XtPointer call_data)
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



static void shutdown_app(Widget widget, XtPointer client_data, XtPointer call_data)
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


static int command_available(const char *program)
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

static void show_fatal_startup_error(const char *message)
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

static void authorize_music(Widget widget, XtPointer client_data, XtPointer call_data)
{
    char response[512];
    (void)widget; (void)client_data; (void)call_data;
    if (bridge_client_request(&bridge, "POST", "/v1/browser/show", "{}",
                              response, sizeof(response)) != 0)
        set_status("Could not show the Apple Music login window");
    else
        set_status("Sign into Apple Music in the browser window");
}

int main(int argc, char **argv)
{
    XtAppContext app;
    Widget shell, main_window, form, menu_bar, file_menu, file_cascade;
    Widget view_menu, view_cascade, toggle;
    Widget content, sources, right_pane, browser, transport, search, volume;
    Widget library_heading;
    Widget volume_frame;
    XmString label;
    unsigned int i;
    int bridge_start_result;
    static const char *demo_tracks[] = {
        "Cats on Mars                 SEATBELTS                 2:44",
        "Demo library is active       Motif Apple Music         --:--",
        "Configure MusicKit to load   Your Apple Music library  --:--",
        "Search and playback arrive   In the next milestone     --:--"
    };

    XtSetLanguageProc(NULL, NULL, NULL);
    shell = XtVaAppInitialize(&app, "MotifAppleMusic", NULL, 0, &argc, argv,
                              NULL, XmNtitle, "Apple Music", NULL);
    application_context = app;
    XtVaSetValues(shell, XmNwidth, 940, XmNheight, 620, NULL);
    XtAddCallback(shell, XmNdestroyCallback, shutdown_app, NULL);
    {
        Atom wm_delete = XmInternAtom(XtDisplay(shell), "WM_DELETE_WINDOW", False);
        XmAddWMProtocolCallback(shell, wm_delete, shutdown_app, NULL);
    }
