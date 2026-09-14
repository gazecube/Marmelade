#include "app_internal.h"

#define ARTWORK_REFRESH_DELAY_MS 500

static XtIntervalId artwork_refresh_timer = 0;

unsigned long rgb_component_pixel(unsigned char component, unsigned long mask)
{
    unsigned int shift = 0;
    unsigned long maximum;
    if (mask == 0) return 0;
    while (((mask >> shift) & 1UL) == 0UL) shift++;
    maximum = mask >> shift;
    return ((((unsigned long)component * maximum + 127UL) / 255UL) << shift) & mask;
}

Pixmap rgb_pixmap_from_bytes(Widget widget, const unsigned char *rgb,
                                    unsigned int width, unsigned int height)
{
    Display *display = XtDisplay(widget);
    Screen *screen = XtScreen(widget);
    Visual *visual = DefaultVisualOfScreen(screen);
    int depth = DefaultDepthOfScreen(screen);
    XImage *image;
    Pixmap pixmap;
    GC gc;
    unsigned int x, y;

    if (visual->class != TrueColor && visual->class != DirectColor)
        return XmUNSPECIFIED_PIXMAP;
    image = XCreateImage(display, visual, (unsigned int)depth, ZPixmap, 0,
                         NULL, width, height, 32, 0);
    if (image == NULL) return XmUNSPECIFIED_PIXMAP;
    image->data = calloc((size_t)image->bytes_per_line, height);
    if (image->data == NULL) {
        XDestroyImage(image);
        return XmUNSPECIFIED_PIXMAP;
    }
    for (y = 0; y < height; ++y) {
        for (x = 0; x < width; ++x) {
            size_t offset = ((size_t)y * width + x) * 3;
            unsigned long pixel =
                rgb_component_pixel(rgb[offset], visual->red_mask) |
                rgb_component_pixel(rgb[offset + 1], visual->green_mask) |
                rgb_component_pixel(rgb[offset + 2], visual->blue_mask);
            XPutPixel(image, (int)x, (int)y, pixel);
        }
    }
    pixmap = XCreatePixmap(display, RootWindowOfScreen(screen), width, height,
                           (unsigned int)depth);
    if (pixmap == None) {
        XDestroyImage(image);
        return XmUNSPECIFIED_PIXMAP;
    }
    gc = XCreateGC(display, pixmap, 0, NULL);
    if (gc == NULL) {
        XFreePixmap(display, pixmap);
        XDestroyImage(image);
        return XmUNSPECIFIED_PIXMAP;
    }
    XPutImage(display, pixmap, gc, image, 0, 0, 0, 0, width, height);
    XFreeGC(display, gc);
    XDestroyImage(image);
    return pixmap;
}

Pixmap fetch_artwork_pixmap(Widget widget, unsigned int size, const char *url)
{
    char encoded[3072], path[4096];
    unsigned char *rgb = NULL;
    size_t expected = (size_t)size * size * 3;
    size_t received = 0;
    Pixmap pixmap = XmUNSPECIFIED_PIXMAP;

    if (url != NULL && url[0] != '\0') {
        url_encode(url, encoded, sizeof(encoded));
        snprintf(path, sizeof(path), "/v1/player/artwork.rgb?size=%u&url=%s", size, encoded);
    } else {
        snprintf(path, sizeof(path), "/v1/player/artwork.rgb?size=%u", size);
    }

    if (bridge_client_request_bytes(&bridge, "GET", path, NULL,
                                    &rgb, &received) == 0 &&
        received == expected && rgb != NULL)
        pixmap = rgb_pixmap_from_bytes(widget, rgb, size, size);
    free(rgb);
    return pixmap;
}

void update_artwork_widget(Widget widget, unsigned int size, Pixmap *stored_pixmap)
{
    Pixmap pixmap, old_pixmap;
    XmString unavailable;

    pixmap = fetch_artwork_pixmap(widget, size, NULL);
    if (pixmap == XmUNSPECIFIED_PIXMAP || pixmap == None) goto unavailable;
    old_pixmap = *stored_pixmap;
    *stored_pixmap = pixmap;
    XtVaSetValues(widget, XmNlabelType, XmPIXMAP,
                  XmNlabelPixmap, *stored_pixmap, NULL);
    if (old_pixmap != XmUNSPECIFIED_PIXMAP && old_pixmap != None)
        XFreePixmap(XtDisplay(widget), old_pixmap);
    return;

unavailable:
    unavailable = XmStringCreateLocalized("No Artwork");
    XtVaSetValues(widget, XmNlabelType, XmSTRING,
                  XmNlabelString, unavailable, NULL);
    XmStringFree(unavailable);
}

static void refresh_artwork_after_change(XtPointer client_data, XtIntervalId *id)
{
    (void)client_data;
    (void)id;
    artwork_refresh_timer = 0;
    update_artwork_widget(artwork_label, sidebar_artwork_size, &artwork_pixmap);
    update_artwork_widget(main_artwork_label, 240, &main_artwork_pixmap);
}

void update_artwork(void)
{
    update_artwork_widget(artwork_label, sidebar_artwork_size, &artwork_pixmap);
    update_artwork_widget(main_artwork_label, 240, &main_artwork_pixmap);

    /* The player state can change a fraction before Apple updates the artwork source. */
    if (artwork_refresh_timer != 0)
        XtRemoveTimeOut(artwork_refresh_timer);
    artwork_refresh_timer = XtAppAddTimeOut(application_context,
                                             ARTWORK_REFRESH_DELAY_MS,
                                             refresh_artwork_after_change, NULL);
}

Pixmap load_album_grid_pixmap(const char *url, unsigned int index)
{
    (void)index;
    if (url == NULL || url[0] == '\0') return XmUNSPECIFIED_PIXMAP;
    return fetch_artwork_pixmap(album_grid, GRID_ARTWORK, url);
}

void schedule_grid_artwork(void);
