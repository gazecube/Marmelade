#include "app_internal.h"
#include "icons.h"

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
        "If you're seeing this, the bridge either hasn't loaded for some reason, or",
        "the list view hasn't repopulated. If it's the latter, switch to another",
        "tab to repopulate the list with live data."
    };

    XtSetLanguageProc(NULL, NULL, NULL);
    shell = XtVaAppInitialize(&app, "MotifAppleMusic", NULL, 0, &argc, argv,
                              NULL, XmNtitle, "Marmelade", NULL);
    application_context = app;
    XtVaSetValues(shell, XmNwidth, 940, XmNheight, 620, NULL);
    XtAddCallback(shell, XmNdestroyCallback, shutdown_app, NULL);
    {
        Atom wm_delete = XmInternAtom(XtDisplay(shell), "WM_DELETE_WINDOW", False);
        XmAddWMProtocolCallback(shell, wm_delete, shutdown_app, NULL);
    }

    main_window = XtVaCreateManagedWidget("mainWindow", xmMainWindowWidgetClass, shell, NULL);
    menu_bar = XmCreateMenuBar(main_window, "menuBar", NULL, 0);
    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    label = XmStringCreateLocalized("File");
    file_cascade = XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar,
                                            XmNlabelString, label,
                                            XmNsubMenuId, file_menu, NULL);
    (void)file_cascade;
    XmStringFree(label);
    XtAddCallback(XtVaCreateManagedWidget("Log In",
                                           xmPushButtonWidgetClass, file_menu, NULL),
                  XmNactivateCallback, authorize_music, NULL);
    XtVaCreateManagedWidget("separator", xmSeparatorGadgetClass, file_menu, NULL);
    XtAddCallback(XtVaCreateManagedWidget("Quit", xmPushButtonWidgetClass, file_menu, NULL),
                  XmNactivateCallback, shutdown_app, NULL);
    view_menu = XmCreatePulldownMenu(menu_bar, "viewMenu", NULL, 0);
    label = XmStringCreateLocalized("View");
    view_cascade = XtVaCreateManagedWidget("View", xmCascadeButtonWidgetClass, menu_bar,
                                            XmNlabelString, label,
                                            XmNsubMenuId, view_menu, NULL);
    (void)view_cascade;
    XmStringFree(label);
    toggle = XtVaCreateManagedWidget("Show Sidebar", xmToggleButtonWidgetClass, view_menu,
                                     XmNset, True, NULL);
    XtAddCallback(toggle, XmNvalueChangedCallback, show_sidebar_toggled, NULL);
    toggle = XtVaCreateManagedWidget("Show Sidebar Artwork", xmToggleButtonWidgetClass,
                                     view_menu, XmNset, True, NULL);
    XtAddCallback(toggle, XmNvalueChangedCallback, show_sidebar_artwork_toggled, NULL);
    XtVaCreateManagedWidget("viewSeparator", xmSeparatorGadgetClass, view_menu, NULL);
    XtAddCallback(XtVaCreateManagedWidget("List", xmPushButtonWidgetClass, view_menu, NULL),
                  XmNactivateCallback, view_mode_selected, (XtPointer)(long)0);
    grid_view_button = XtVaCreateManagedWidget("Grid", xmPushButtonWidgetClass,
                                               view_menu, NULL);
    XtAddCallback(grid_view_button, XmNactivateCallback,
                  view_mode_selected, (XtPointer)(long)1);
    XtAddCallback(XtVaCreateManagedWidget("Now Playing Artwork", xmPushButtonWidgetClass,
                                          view_menu, NULL),
                  XmNactivateCallback, view_mode_selected, (XtPointer)(long)2);
    XtManageChild(menu_bar);

    form = XtVaCreateManagedWidget("workArea", xmFormWidgetClass, main_window, NULL);
    content = XtVaCreateManagedWidget("content", xmFormWidgetClass, form,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment, XmATTACH_FORM,
                                      XmNbottomAttachment, XmATTACH_FORM, NULL);
    sidebar_frame = XtVaCreateManagedWidget("sidebarFrame", xmFrameWidgetClass, content,
                                      XmNwidth, 184,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNbottomAttachment, XmATTACH_FORM,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNshadowType, XmSHADOW_ETCHED_IN, NULL);
    sources = XtVaCreateManagedWidget("sources", xmFormWidgetClass, sidebar_frame,
                                      XmNwidth, 180, NULL);
    sidebar_sizer = XtVaCreateManagedWidget("sidebarSizer", xmDrawingAreaWidgetClass, content,
                                            XmNwidth, 6,
                                            XmNborderWidth, 0,
                                            XmNtopAttachment, XmATTACH_FORM,
                                            XmNbottomAttachment, XmATTACH_FORM,
                                            XmNleftAttachment, XmATTACH_WIDGET,
                                            XmNleftWidget, sidebar_frame, NULL);
    XtAddEventHandler(sidebar_sizer, ButtonPressMask | Button1MotionMask | ButtonReleaseMask, False,
                      sidebar_sizer_event, NULL);
    XtAddCallback(sidebar_sizer, XmNexposeCallback, sidebar_sizer_expose, NULL);
    library_heading = XtVaCreateManagedWidget("LIBRARY", xmLabelWidgetClass, sources,
                                               XmNtopAttachment, XmATTACH_FORM,
                                               XmNleftAttachment, XmATTACH_FORM,
                                               XmNrightAttachment, XmATTACH_FORM, NULL);
    artwork_label = XtVaCreateManagedWidget("No Artwork", xmLabelWidgetClass, sources,
                                            XmNwidth, sidebar_artwork_size, XmNheight, sidebar_artwork_size,
                                            XmNleftAttachment, XmATTACH_FORM,
                                            XmNrightAttachment, XmATTACH_FORM,
                                            XmNbottomAttachment, XmATTACH_FORM, NULL);
    search = XtVaCreateManagedWidget("Search", xmTextFieldWidgetClass, sources,
                                     XmNcolumns, 19,
                                     XmNleftAttachment, XmATTACH_FORM,
                                     XmNrightAttachment, XmATTACH_FORM,
                                     XmNbottomAttachment, XmATTACH_WIDGET,
                                     XmNbottomWidget, artwork_label, NULL);
    source_list_widget = XtVaCreateManagedWidget("sourceList", xmListWidgetClass, sources,
                                                 XmNvisibleItemCount, 14,
                                                 XmNlistSizePolicy, XmCONSTANT,
                                                 XmNtopAttachment, XmATTACH_WIDGET,
                                                 XmNtopWidget, library_heading,
                                                 XmNbottomAttachment, XmATTACH_WIDGET,
                                                 XmNbottomWidget, search,
                                                 XmNleftAttachment, XmATTACH_FORM,
                                                 XmNrightAttachment, XmATTACH_FORM, NULL);
    XtAddCallback(source_list_widget, XmNbrowseSelectionCallback, source_selected, NULL);
    XtAddCallback(search, XmNvalueChangedCallback, search_filter_changed, NULL);
    XtAddCallback(search, XmNactivateCallback, search_activated, NULL);

    right_pane = XtVaCreateManagedWidget("rightPane", xmFormWidgetClass, content,
                                         XmNtopAttachment, XmATTACH_FORM,
                                         XmNbottomAttachment, XmATTACH_FORM,
                                         XmNleftAttachment, XmATTACH_WIDGET,
                                         XmNleftWidget, sidebar_sizer,
                                         XmNrightAttachment, XmATTACH_FORM, NULL);
    right_pane_widget = right_pane;
    browser = XtVaCreateManagedWidget("browser", xmFormWidgetClass, right_pane,
                                      XmNtopAttachment, XmATTACH_FORM,
                                      XmNleftAttachment, XmATTACH_FORM,
                                      XmNrightAttachment, XmATTACH_FORM,
                                      XmNbottomAttachment, XmATTACH_POSITION,
                                      XmNbottomPosition, 78, NULL);
    browser_widget = browser;
    search_widget = search;
    main_artwork_label = XtVaCreateWidget("No Artwork", xmLabelWidgetClass, right_pane,
                                          XmNalignment, XmALIGNMENT_CENTER,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_POSITION,
                                          XmNbottomPosition, 78, NULL);
    album_grid_scroller = XtVaCreateWidget("gridScroller", xmScrolledWindowWidgetClass,
                                  right_pane,
                                  XmNscrollingPolicy, XmAUTOMATIC,
                                  XmNtopAttachment, XmATTACH_FORM,
                                  XmNleftAttachment, XmATTACH_FORM,
                                  XmNrightAttachment, XmATTACH_FORM,
                                  XmNbottomAttachment, XmATTACH_POSITION,
                                  XmNbottomPosition, 78, NULL);
    album_grid = XtVaCreateManagedWidget("albumGrid", xmDrawingAreaWidgetClass,
                                  album_grid_scroller,
                                  XmNresizePolicy, XmRESIZE_NONE,
                                  XmNwidth, GRID_CELL,
                                  XmNheight, GRID_CELL, NULL);
    XmScrolledWindowSetAreas(album_grid_scroller, NULL, NULL, album_grid);
    XtAddEventHandler(album_grid, ExposureMask | ButtonPressMask | StructureNotifyMask,
                      False, album_grid_event, NULL);
    XtAddEventHandler(album_grid_scroller, StructureNotifyMask,
                      False, album_grid_event, NULL);
    {
        Widget popup_form;
        album_tracks_popup = XtVaCreatePopupShell("albumTracksPopup",
                                                   overrideShellWidgetClass, shell,
                                                   XmNallowShellResize, True,
                                                   XmNwidth, 520, XmNheight, 260, NULL);
        popup_form = XtVaCreateManagedWidget("albumTracksForm", xmFormWidgetClass,
                                             album_tracks_popup, NULL);
        album_tracks_title = XtVaCreateManagedWidget("Album", xmLabelWidgetClass,
                                              popup_form,
                                              XmNtopAttachment, XmATTACH_FORM,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM, NULL);
        album_tracks_list = XtVaCreateManagedWidget("albumTracks", xmListWidgetClass,
                                              popup_form,
                                              XmNselectionPolicy, XmBROWSE_SELECT,
                                              XmNvisibleItemCount, 10,
                                              XmNtopAttachment, XmATTACH_WIDGET,
                                              XmNtopWidget, album_tracks_title,
                                              XmNleftAttachment, XmATTACH_FORM,
                                              XmNrightAttachment, XmATTACH_FORM,
                                              XmNbottomAttachment, XmATTACH_FORM, NULL);
        XtAddCallback(album_tracks_list, XmNdefaultActionCallback,
                      queue_album_popup_track, NULL);
    }
    list_frame = XtVaCreateManagedWidget("listFrame", xmFrameWidgetClass,
                                          browser,
                                          XmNtopAttachment, XmATTACH_FORM,
                                          XmNleftAttachment, XmATTACH_FORM,
                                          XmNrightAttachment, XmATTACH_FORM,
                                          XmNbottomAttachment, XmATTACH_FORM,
                                          XmNmarginWidth, 0,
                                          XmNmarginHeight, 0,
                                          NULL);
    browser_list_scroller = XtVaCreateManagedWidget("browserListScroller",
                                          xmScrolledWindowWidgetClass, list_frame,
                                          XmNscrollingPolicy, XmAUTOMATIC,
                                          XmNshadowThickness, 0,
                                          NULL);
    browser_list_canvas = XtVaCreateManagedWidget("browserList",
                                          xmDrawingAreaWidgetClass,
                                          browser_list_scroller,
                                          XmNresizePolicy, XmRESIZE_NONE,
                                          XmNwidth, 640,
                                          XmNheight, LIST_TITLE_HEIGHT + 18 * LIST_ROW_HEIGHT,
                                          XmNmarginWidth, 0,
                                          XmNmarginHeight, 0,
                                          NULL);
    XmScrolledWindowSetAreas(browser_list_scroller, NULL, NULL, browser_list_canvas);
    sync_browser_scheme_colors();
    XtAddEventHandler(browser_list_canvas,
                      ExposureMask | ButtonPressMask | StructureNotifyMask,
                      False, browser_list_event, NULL);
    XtAddEventHandler(browser_list_scroller, StructureNotifyMask,
                      False, browser_list_event, NULL);

    position_scale = XtVaCreateManagedWidget("position", xmScaleWidgetClass, right_pane,
                                             XmNminimum, 0, XmNmaximum, 100,
                                             XmNvalue, 0, XmNorientation, XmHORIZONTAL,
                                             XmNshowValue, False,
                                             XmNtopAttachment, XmATTACH_POSITION,
                                             XmNtopPosition, 79,
                                             XmNbottomAttachment, XmATTACH_POSITION,
                                             XmNbottomPosition, 86,
                                             XmNleftAttachment, XmATTACH_FORM,
                                             XmNleftOffset, 6,
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
    XtAddEventHandler(shell, StructureNotifyMask | FocusChangeMask, False, follow_volume_popup, NULL);
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
