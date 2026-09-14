#include <Xm/Xm.h>

#ifdef MARMELADE_USE_XFT
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef MARMELADE_USE_XFT
static XftFont *fallback_font;
static Display *font_display;
static int font_screen = -1;

static unsigned int utf8_length(unsigned char c)
{
    if ((c & 0x80u) == 0) return 1;
    if ((c & 0xe0u) == 0xc0u) return 2;
    if ((c & 0xf0u) == 0xe0u) return 3;
    if ((c & 0xf8u) == 0xf0u) return 4;
    return 1;
}

static XftFont *open_fallback_font(Display *display, int screen)
{
    const char *override = getenv("MARMELADE_XFT_FONT");
    FcPattern *pattern;
    FcPattern *match;
    FcCharSet *charset;
    FcResult result;

    if (override != NULL && override[0] != '\0') {
        XftFont *font = XftFontOpenName(display, screen, override);
        if (font != NULL) return font;
    }

    pattern = FcPatternCreate();
    charset = FcCharSetCreate();
    if (pattern == NULL || charset == NULL) {
        if (pattern != NULL) FcPatternDestroy(pattern);
        if (charset != NULL) FcCharSetDestroy(charset);
        return NULL;
    }

    /* Ask Fontconfig for a font that can actually render Japanese. */
    FcCharSetAddChar(charset, 0x3042);
    FcPatternAddString(pattern, FC_FAMILY, (const FcChar8 *)"sans");
    FcPatternAddCharSet(pattern, FC_CHARSET, charset);
    FcPatternAddDouble(pattern, FC_SIZE, 10.0);
    FcConfigSubstitute(NULL, pattern, FcMatchPattern);
    FcDefaultSubstitute(pattern);
    match = FcFontMatch(NULL, pattern, &result);
    FcCharSetDestroy(charset);
    FcPatternDestroy(pattern);
    if (match == NULL) return NULL;

    return XftFontOpenPattern(display, match);
}

static void ensure_fallback_font(Display *display)
{
    int screen;

    if (display == NULL) return;
    screen = DefaultScreen(display);
    if (display == font_display && screen == font_screen && fallback_font != NULL)
        return;

    if (fallback_font != NULL && font_display != NULL)
        XftFontClose(font_display, fallback_font);

    fallback_font = NULL;
    font_display = display;
    font_screen = screen;
    fallback_font = open_fallback_font(display, screen);
}

static int motif_run_width(XmFontList font_list,
                           const unsigned char *text, size_t length)
{
    char *copy;
    XmString string;
    Dimension width;

    if (font_list == NULL || length == 0) return 0;
    copy = malloc(length + 1);
    if (copy == NULL) return 0;
    memcpy(copy, text, length);
    copy[length] = '\0';
    string = XmStringCreateLocalized(copy);
    free(copy);
    if (string == NULL) return 0;
    width = XmStringWidth(font_list, string);
    XmStringFree(string);
    return (int)width;
}

static int fallback_run_width(Display *display,
                              const unsigned char *text, size_t length)
{
    XGlyphInfo extents;
    if (fallback_font == NULL || length == 0) return 0;
    XftTextExtentsUtf8(display, fallback_font, text, (int)length, &extents);
    return extents.xOff;
}

static int mixed_text_width(Display *display, XmFontList font_list,
                            const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int width = 0;

    while (*cursor != '\0') {
        const unsigned char *run = cursor;
        size_t run_bytes = 0;
        int ascii = *cursor < 0x80u;

        while (*cursor != '\0' && ((*cursor < 0x80u) != 0) == ascii) {
            unsigned int n = ascii ? 1 : utf8_length(*cursor);
            cursor += n;
            run_bytes += n;
        }

        if (ascii)
            width += motif_run_width(font_list, run, run_bytes);
        else
            width += fallback_run_width(display, run, run_bytes);
    }
    return width;
}

static void draw_motif_run(Display *display, Window drawable,
                           XmFontList font_list, GC gc,
                           const unsigned char *text, size_t length,
                           int x, int y, int width,
                           unsigned char layout_direction,
                           XRectangle *clip)
{
    char *copy;
    XmString string;

    if (length == 0) return;
    copy = malloc(length + 1);
    if (copy == NULL) return;
    memcpy(copy, text, length);
    copy[length] = '\0';
    string = XmStringCreateLocalized(copy);
    free(copy);
    if (string == NULL) return;
    XmStringDraw(display, drawable, font_list, string, gc,
                 (Position)x, (Position)y, (Dimension)width,
                 XmALIGNMENT_BEGINNING, layout_direction, clip);
    XmStringFree(string);
}

static void draw_mixed_runs(Display *display, Window drawable,
                            XmFontList font_list, GC gc,
                            XftDraw *draw, XftColor *color,
                            const char *text, int x, int y,
                            int motif_height,
                            unsigned char layout_direction,
                            XRectangle *clip)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int draw_x = x;

    while (*cursor != '\0') {
        const unsigned char *run = cursor;
        size_t run_bytes = 0;
        int ascii = *cursor < 0x80u;
        int run_width;

        while (*cursor != '\0' && ((*cursor < 0x80u) != 0) == ascii) {
            unsigned int n = ascii ? 1 : utf8_length(*cursor);
            cursor += n;
            run_bytes += n;
        }

        if (ascii) {
            run_width = motif_run_width(font_list, run, run_bytes);
            draw_motif_run(display, drawable, font_list, gc,
                           run, run_bytes, draw_x, y, run_width,
                           layout_direction, clip);
        } else {
            int baseline;
            run_width = fallback_run_width(display, run, run_bytes);
            baseline = y + (motif_height - fallback_font->height) / 2 +
                       fallback_font->ascent;
            XftDrawStringUtf8(draw, color, fallback_font,
                              draw_x, baseline, run, (int)run_bytes);
        }
        draw_x += run_width;
    }
}
#endif

void marmelade_utf8_string_draw(Display *display, Window drawable,
                                XmFontList font_list, XmString string,
                                GC gc, Position x, Position y,
                                Dimension width, unsigned char alignment,
                                unsigned char layout_direction,
                                XRectangle *clip)
{
#ifdef MARMELADE_USE_XFT
    char *text = NULL;
    XGCValues gc_values;
    XColor xcolor;
    XRenderColor render_color;
    XftColor color;
    XftDraw *draw;
    XRectangle local_clip;
    int text_width;
    int draw_x;
    int motif_height;
    int has_non_ascii = 0;
    const unsigned char *scan;

    if (!XmStringGetLtoR(string, XmFONTLIST_DEFAULT_TAG, &text) || text == NULL) {
        XmStringDraw(display, drawable, font_list, string, gc, x, y, width,
                     alignment, layout_direction, clip);
        return;
    }

    for (scan = (const unsigned char *)text; *scan != '\0'; ++scan) {
        if (*scan >= 0x80u) {
            has_non_ascii = 1;
            break;
        }
    }

    /* Keep normal Motif text completely native. Xft only fills Unicode gaps. */
    if (!has_non_ascii) {
        XtFree(text);
        XmStringDraw(display, drawable, font_list, string, gc, x, y, width,
                     alignment, layout_direction, clip);
        return;
    }

    ensure_fallback_font(display);
    if (fallback_font == NULL ||
        !XGetGCValues(display, gc, GCForeground, &gc_values)) {
        XtFree(text);
        XmStringDraw(display, drawable, font_list, string, gc, x, y, width,
                     alignment, layout_direction, clip);
        return;
    }

    xcolor.pixel = gc_values.foreground;
    XQueryColor(display, DefaultColormap(display, DefaultScreen(display)), &xcolor);
    render_color.red = xcolor.red;
    render_color.green = xcolor.green;
    render_color.blue = xcolor.blue;
    render_color.alpha = 0xffff;
    color.pixel = gc_values.foreground;
    color.color = render_color;

    draw = XftDrawCreate(display, drawable,
                         DefaultVisual(display, DefaultScreen(display)),
                         DefaultColormap(display, DefaultScreen(display)));
    if (draw == NULL) {
        XtFree(text);
        XmStringDraw(display, drawable, font_list, string, gc, x, y, width,
                     alignment, layout_direction, clip);
        return;
    }

    text_width = mixed_text_width(display, font_list, text);
    draw_x = (int)x;
    if (alignment == XmALIGNMENT_CENTER)
        draw_x += ((int)width - text_width) / 2;
    else if (alignment == XmALIGNMENT_END)
        draw_x += (int)width - text_width;

    motif_height = (int)XmStringHeight(font_list, string);
    if (motif_height <= 0) motif_height = fallback_font->height;

    local_clip.x = x;
    local_clip.y = y;
    local_clip.width = width;
    local_clip.height = (unsigned short)(motif_height > fallback_font->height ?
                                         motif_height : fallback_font->height);
    XftDrawSetClipRectangles(draw, 0, 0, clip != NULL ? clip : &local_clip, 1);

    draw_mixed_runs(display, drawable, font_list, gc, draw, &color,
                    text, draw_x, (int)y, motif_height,
                    layout_direction, clip);

    XftDrawDestroy(draw);
    XtFree(text);
#else
    XmStringDraw(display, drawable, font_list, string, gc, x, y, width,
                 alignment, layout_direction, clip);
#endif
}
