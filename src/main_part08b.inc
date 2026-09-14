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
