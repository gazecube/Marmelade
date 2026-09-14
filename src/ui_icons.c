#include "app_internal.h"
#include "icons.h"

Pixmap make_button_pixmap_color(Widget widget, char **xpm, Pixel background)
{
    Display *display = XtDisplay(widget);
    Screen *screen = XtScreen(widget);
    Pixmap source = None, mask = None, result;
    GC gc;
    XGCValues values;
    XpmAttributes attributes;

    memset(&attributes, 0, sizeof(attributes));
    attributes.valuemask = XpmSize;
    if (XpmCreatePixmapFromData(display, RootWindowOfScreen(screen), xpm,
                                &source, &mask, &attributes) != XpmSuccess)
        return XmUNSPECIFIED_PIXMAP;

    result = XCreatePixmap(display, RootWindowOfScreen(screen),
                           attributes.width, attributes.height,
                           DefaultDepthOfScreen(screen));
    values.foreground = background;
    gc = XCreateGC(display, result, GCForeground, &values);
    XFillRectangle(display, result, gc, 0, 0, attributes.width, attributes.height);
    if (mask != None) {
        XSetClipMask(display, gc, mask);
        XSetClipOrigin(display, gc, 0, 0);
    }
    XCopyArea(display, source, result, gc, 0, 0,
              attributes.width, attributes.height, 0, 0);
    XFreeGC(display, gc);
    XFreePixmap(display, source);
    if (mask != None) XFreePixmap(display, mask);
    return result;
}

Pixmap make_button_pixmap(Widget widget, char **xpm)
{
    Pixel background;
    XtVaGetValues(widget, XmNbackground, &background, NULL);
    return make_button_pixmap_color(widget, xpm, background);
}

ButtonIconState *find_button_icon(Widget widget)
{
    unsigned int i;
    for (i = 0; i < button_icon_count; ++i)
        if (button_icons[i].widget == widget) return &button_icons[i];
    return NULL;
}

void render_button_icon(ButtonIconState *state)
{
    Pixel background;
    Pixmap replacement;
    XtVaGetValues(state->widget,
                  state->active ? XmNarmColor : XmNbackground, &background, NULL);
    replacement = make_button_pixmap_color(state->widget, state->xpm, background);
    if (replacement == XmUNSPECIFIED_PIXMAP) return;
    XtVaSetValues(state->widget, XmNlabelPixmap, replacement, NULL);
    if (state->pixmap != XmUNSPECIFIED_PIXMAP && state->pixmap != None)
        XFreePixmap(XtDisplay(state->widget), state->pixmap);
    state->pixmap = replacement;
}

void button_icon_event(Widget widget, XtPointer client_data,
                       XEvent *event, Boolean *continue_dispatch)
{
    ButtonIconState *state = (ButtonIconState *)client_data;
    (void)widget; (void)continue_dispatch;
    state->active = event->type == EnterNotify || event->type == ButtonPress;
    if (event->type == LeaveNotify) state->active = 0;
    render_button_icon(state);
}

void register_button_icon(Widget widget, char **xpm, Pixmap pixmap)
{
    ButtonIconState *state;
    if (button_icon_count >= 8) return;
    state = &button_icons[button_icon_count++];
    state->widget = widget; state->xpm = xpm; state->pixmap = pixmap; state->active = 0;
    XtAddEventHandler(widget, EnterWindowMask | LeaveWindowMask | ButtonPressMask,
                      False, button_icon_event, state);
}

void set_button_icon(Widget widget, char **xpm)
{
    ButtonIconState *state = find_button_icon(widget);
    if (state == NULL || state->xpm == xpm) return;
    state->xpm = xpm;
    render_button_icon(state);
}

const char *repeat_mode_name(int mode)
{
    return mode == 2 ? "one" : (mode == 1 ? "all" : "off");
}

void damage_list_title_row(void)
{
    if (browser_list_canvas != NULL && XtIsRealized(browser_list_canvas))
        XClearArea(XtDisplay(browser_list_canvas), XtWindow(browser_list_canvas),
                   0, 0, 0, LIST_TITLE_HEIGHT, True);
}

void update_shuffle_button(void)
{
    if (browser_list_canvas == NULL || rendered_shuffle_state == player_shuffle_enabled) return;
    rendered_shuffle_state = player_shuffle_enabled;
    if (shuffle_list_pixmap != XmUNSPECIFIED_PIXMAP && shuffle_list_pixmap != None)
        XFreePixmap(XtDisplay(browser_list_canvas), shuffle_list_pixmap);
    shuffle_list_pixmap = make_button_pixmap(browser_list_canvas,
        player_shuffle_enabled ? shuffle_on_icon_xpm : shuffle_off_icon_xpm);
    damage_list_title_row();
}

void update_repeat_button(void)
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

void update_automix_button(void)
{
    if (browser_list_canvas == NULL || rendered_automix_state == player_automix_enabled) return;
    rendered_automix_state = player_automix_enabled;
    if (automix_list_pixmap != XmUNSPECIFIED_PIXMAP && automix_list_pixmap != None)
        XFreePixmap(XtDisplay(browser_list_canvas), automix_list_pixmap);
    automix_list_pixmap = make_button_pixmap(browser_list_canvas,
        player_automix_enabled ? automix_on_icon_xpm : automix_off_icon_xpm);
    damage_list_title_row();
}

void update_autoplay_button(void)
{
    if (browser_list_canvas == NULL || rendered_autoplay_state == player_autoplay_enabled) return;
    rendered_autoplay_state = player_autoplay_enabled;
    if (autoplay_list_pixmap != XmUNSPECIFIED_PIXMAP && autoplay_list_pixmap != None)
        XFreePixmap(XtDisplay(browser_list_canvas), autoplay_list_pixmap);
    autoplay_list_pixmap = make_button_pixmap(browser_list_canvas,
        player_autoplay_enabled ? autoplay_on_icon_xpm : autoplay_off_icon_xpm);
    damage_list_title_row();
}

void set_status(const char *text)
{
    XmString value = XmStringCreateLocalized((char *)text);
    XtVaSetValues(status_label, XmNlabelString, value, NULL);
    XmStringFree(value);
}

void set_volume_icon(double volume)
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
