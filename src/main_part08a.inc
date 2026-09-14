
    main_window = XtVaCreateManagedWidget("mainWindow", xmMainWindowWidgetClass, shell, NULL);
    menu_bar = XmCreateMenuBar(main_window, "menuBar", NULL, 0);
    file_menu = XmCreatePulldownMenu(menu_bar, "fileMenu", NULL, 0);
    label = XmStringCreateLocalized("File");
    file_cascade = XtVaCreateManagedWidget("File", xmCascadeButtonWidgetClass, menu_bar,
                                            XmNlabelString, label,
                                            XmNsubMenuId, file_menu, NULL);
    (void)file_cascade;
    XmStringFree(label);
    XtAddCallback(XtVaCreateManagedWidget("Show Apple Music Login...",
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
