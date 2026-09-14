#include "app_internal.h"

void view_model_selected(unsigned int index)
{
    const char *plural;
    if (index >= current_view_count) return;
    set_browser_selection((int)index);
    if (current_view_container_rows[index]) {
        current_view_collapsed = !current_view_collapsed;
        current_view_labels[index][1] = current_view_collapsed ? '+' : '-';
        apply_filter(current_filter);
        return;
    }
    if (strcmp(current_view_item_kinds[index], "album") != 0 &&
        strcmp(current_view_item_kinds[index], "playlist") != 0 &&
        strcmp(current_view_item_kinds[index], "artist") != 0) return;
    plural = strcmp(current_view_item_kinds[index], "artist") == 0 ? "artists" :
             (strcmp(current_view_item_kinds[index], "album") == 0 ? "albums" : "playlists");
    snprintf(current_view_path, sizeof(current_view_path), "/v1/library/%s/%s",
             plural, current_view_ids[index]);
    load_view(current_view_path);
}

void view_model_opened(unsigned int index)
{
    char body[640], response[2048];
    if (index >= current_view_count) return;
    if (strcmp(current_view_type, "albums") == 0 ||
        strcmp(current_view_type, "artists") == 0 ||
        strcmp(current_view_type, "playlists") == 0) {
        snprintf(current_view_path, sizeof(current_view_path), "/v1/library/%s/%s",
                 current_view_type, current_view_ids[index]);
        load_view(current_view_path);
    } else if (strcmp(current_view_item_kinds[index], "song") == 0 ||
               strcmp(current_view_item_kinds[index], "album") == 0 ||
               strcmp(current_view_item_kinds[index], "playlist") == 0 ||
               strcmp(current_view_item_kinds[index], "station") == 0) {
        snprintf(body, sizeof(body), "{\"kind\":\"%s\",\"id\":\"%s\"}",
                 current_view_item_kinds[index], current_view_catalog_ids[index]);
        if (bridge_client_request(&bridge, "POST", "/v1/player/queue", body,
                                  response, sizeof(response)) == 0) {
            set_status("Starting selected Apple Music item...");
            snprintf(current_view_path, sizeof(current_view_path), "%s", "/v1/player/queue");
            load_view(current_view_path);
        } else
            set_status("Could not play the selected Apple Music item");
    }
}

void browser_list_event(Widget widget, XtPointer client_data,
                        XEvent *event, Boolean *continue_dispatch)
{
    Dimension width = 0;
    int queue_view;
    (void)client_data; (void)continue_dispatch;

    if (event->type == ConfigureNotify) {
        if (widget == browser_list_scroller || widget == browser_list_canvas)
            schedule_browser_resize_redraw();
        return;
    }
    if (widget != browser_list_canvas) return;
    if (event->type == Expose) {
        draw_browser_list(&event->xexpose);
        return;
    }
    if (event->type != ButtonPress || event->xbutton.button != Button1) return;

    queue_view = browser_mode == 0 && strcmp(current_view_path, "/v1/player/queue") == 0;
    if (event->xbutton.y < LIST_TITLE_HEIGHT) {
        if (queue_view) {
            int autoplay_x, automix_x, repeat_x, shuffle_x;
            XtVaGetValues(browser_list_canvas, XmNwidth, &width, NULL);
            autoplay_x = (int)width - LIST_SIDE_PAD - LIST_SMALL_ICON_SIZE;
            automix_x = autoplay_x - LIST_ICON_GAP - LIST_SMALL_ICON_SIZE;
            repeat_x = automix_x - LIST_ICON_GAP - LIST_ICON_SIZE;
            shuffle_x = repeat_x - LIST_ICON_GAP - LIST_ICON_SIZE;
            if (event->xbutton.x >= autoplay_x &&
                event->xbutton.x < autoplay_x + LIST_SMALL_ICON_SIZE) {
                autoplay_toggled(widget, NULL, NULL);
            } else if (event->xbutton.x >= automix_x &&
                       event->xbutton.x < automix_x + LIST_SMALL_ICON_SIZE) {
                automix_toggled(widget, NULL, NULL);
            } else if (event->xbutton.x >= repeat_x &&
                       event->xbutton.x < repeat_x + LIST_ICON_SIZE) {
                repeat_toggled(widget, NULL, NULL);
            } else if (event->xbutton.x >= shuffle_x &&
                       event->xbutton.x < shuffle_x + LIST_ICON_SIZE) {
                shuffle_toggled(widget, NULL, NULL);
            }
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
