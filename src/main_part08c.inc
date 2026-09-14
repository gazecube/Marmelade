                                             XmNrightAttachment, XmATTACH_FORM,
                                             XmNrightOffset, 6, NULL);
    XtAddCallback(position_scale, XmNvalueChangedCallback, position_changed, NULL);
    XtAddCallback(position_scale, XmNdragCallback, position_changed, NULL);
    transport = XtVaCreateManagedWidget("transport", xmRowColumnWidgetClass, right_pane,
                                        XmNorientation, XmHORIZONTAL,
                                        XmNpacking, XmPACK_TIGHT,
                                        XmNwidth, 140,
                                        XmNtopAttachment, XmATTACH_POSITION,
                                        XmNtopPosition, 87,
                                        XmNbottomAttachment, XmATTACH_POSITION,
                                        XmNbottomPosition, 94,
                                        XmNleftAttachment, XmATTACH_POSITION,
                                        XmNleftPosition, 50,
                                        XmNleftOffset, -70, NULL);
    icon_button(transport, "previous", "previous", next_icon_xpm);
    play_pause_button = play_pause_icon_button(transport);
    icon_button(transport, "next", "next", previous_icon_xpm);
    volume_button = popup_icon_button(transport, "volume", volume_icon_xpm);
    XtAddCallback(volume_button, XmNactivateCallback, toggle_volume_popup, NULL);

    volume_popup = XtVaCreatePopupShell("volumePopup", overrideShellWidgetClass,
                                         shell,
                                         XmNallowShellResize, True,
                                         XmNwidth, 190,
                                         XmNheight, 30,
                                         NULL);
    volume_frame = XtVaCreateManagedWidget("volumeFrame", xmFrameWidgetClass,
                                           volume_popup,
                                           XmNmarginWidth, 3,
                                           XmNmarginHeight, 3, NULL);
    volume = XtVaCreateManagedWidget("volume", xmScaleWidgetClass, volume_frame,
                                     XmNminimum, 0, XmNmaximum, 100,
                                     XmNvalue, 60,
                                     XmNorientation, XmHORIZONTAL,
                                     XmNprocessingDirection, XmMAX_ON_RIGHT,
                                     XmNshowValue, False,
                                     XmNwidth, 180,
                                     XmNheight, 20,
                                     NULL);
    XtAddCallback(volume, XmNvalueChangedCallback, volume_changed, NULL);
    XtAddCallback(volume, XmNdragCallback, volume_changed, NULL);
    status_label = XtVaCreateManagedWidget("Bridge not started", xmLabelWidgetClass,
                                           right_pane,
                                           XmNtopAttachment, XmATTACH_POSITION,
                                           XmNtopPosition, 95,
                                           XmNleftAttachment, XmATTACH_FORM,
                                           XmNrightAttachment, XmATTACH_FORM,
                                           XmNbottomAttachment, XmATTACH_FORM, NULL);
    XtAddEventHandler(form, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(content, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(sources, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(browser, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(source_list_widget, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(search, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(browser_list_canvas, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(position_scale, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XtAddEventHandler(transport, ButtonPressMask, False, dismiss_volume_popup, NULL);
    XmMainWindowSetAreas(main_window, menu_bar, NULL, NULL, NULL, form);

    bridge_client_init(&bridge);
    bridge_start_result = bridge_client_start(&bridge);
    if (bridge_start_result == BRIDGE_START_NODE_MISSING) {
        const char *node = getenv("MOTIF_APPLE_MUSIC_NODE");
        char message[768];
        if (node == NULL || node[0] == '\0') node = "node";
        snprintf(message, sizeof(message),
                 "Node.js is required to run the Apple Music bridge, but '%s' could not be executed. Install Node.js 20 or newer, or set MOTIF_APPLE_MUSIC_NODE to a valid Node.js executable.",
                 node);
        show_fatal_startup_error(message);
        return EXIT_FAILURE;
    }
    if (bridge_start_result == BRIDGE_START_INCOMPATIBLE)
        set_status("Incompatible bridge already uses port 17876 - stop the old bridge and restart");
    else if (bridge_start_result == BRIDGE_START_OK &&
             bridge_client_wait_ready(&bridge, 5000) == 0)
        set_status("Bridge ready - demo library active");
    else
        set_status("Bridge unavailable - set MOTIF_APPLE_MUSIC_BRIDGE_URL or check bridge startup");

    XtRealizeWidget(shell);
    {
        Widget vertical = NULL, list_vertical = NULL;
        XtVaGetValues(album_grid_scroller, XmNverticalScrollBar, &vertical, NULL);
        if (vertical != NULL) {
            XtAddCallback(vertical, XmNvalueChangedCallback, album_grid_scrolled, NULL);
            XtAddCallback(vertical, XmNdragCallback, album_grid_scrolled, NULL);
        }
        XtVaGetValues(browser_list_scroller, XmNverticalScrollBar,
                      &list_vertical, NULL);
        if (list_vertical != NULL) {
            XtVaGetValues(list_vertical, XmNvalue, &browser_scroll_last_value, NULL);
            XtAddCallback(list_vertical, XmNvalueChangedCallback, list_scrolled, NULL);
            XtAddCallback(list_vertical, XmNdragCallback, list_scrolled, NULL);
        }
    }
    now_playing_source_visible = 0;
    populate_sources();
    XmListSelectPos(source_list_widget, 1, True);
    schedule_player_poll(100);
    XtAppMainLoop(app);
    return 0;
}
