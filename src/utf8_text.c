#include <Xm/Xm.h>

#ifdef MARMELADE_USE_XFT
#include <X11/Xft/Xft.h>
#include <fontconfig/fontconfig.h>
#endif

#include <stdlib.h>
#include <string.h>

#ifdef MARMELADE_USE_XFT
static XftFont *primary_font;
static XftFont *cjk_font;
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

static FcChar32 utf8_codepoint(const unsigned char *text, unsigned int length)
{
    if (length == 1) return text[0];
    if (length == 2)
        return ((FcChar32)(text[0] & 0x1fu) << 6) |
               (FcChar32)(text[1] & 0x3fu);
    if (length == 3)
        return ((FcChar32)(text[0] & 0x0fu) << 12) |
               ((FcChar32)(text[1] & 0x3fu) << 6) |
               (FcChar32)(text[2] & 0x3fu);
    if (length == 4)
        return ((FcChar32)(text[0] & 0x07u) << 18) |
               ((FcChar32)(text[1] & 0x3fu) << 12) |
               ((FcChar32)(text[2] & 0x3fu) << 6) |
               (FcChar32)(text[3] & 0x3fu);
    return text[0];
}

static XftFont *open_fallback_font(Display *display, int screen)
{
    FcPattern *pattern;
    FcPattern *match;
    FcCharSet *charset;
    FcResult result;

    pattern = FcPatternCreate();
    charset = FcCharSetCreate();
    if (pattern == NULL || charset == NULL) {
        if (pattern != NULL) FcPatternDestroy(pattern);
        if (charset != NULL) FcCharSetDestroy(charset);
        return NULL;
    }

    FcCharSetAddChar(charset, 0x3042); /* Hiragana A: force a Japanese-capable match. */
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

static void ensure_fonts(Display *display)
{
    const char *font_name;
    int screen;

    if (display == NULL) return;
    screen = DefaultScreen(display);
    if (display == font_display && screen == font_screen && primary_font != NULL)
        return;

    if (primary_font != NULL && font_display != NULL)
        XftFontClose(font_display, primary_font);
    if (cjk_font != NULL && font_display != NULL)
        XftFontClose(font_display, cjk_font);

    primary_font = NULL;
    cjk_font = NULL;
    font_display = display;
    font_screen = screen;

    font_name = getenv("MARMELADE_XFT_FONT");
    if (font_name == NULL || font_name[0] == '\0') font_name = "sans-10";
    primary_font = XftFontOpenName(display, screen, font_name);
    if (primary_font == NULL)
        primary_font = XftFontOpenName(display, screen, "sans-10");
    cjk_font = open_fallback_font(display, screen);
}

static XftFont *font_for_codepoint(Display *display, FcChar32 codepoint)
{
    if (primary_font != NULL && XftCharExists(display, primary_font, codepoint))
        return primary_font;
    if (cjk_font != NULL && XftCharExists(display, cjk_font, codepoint))
        return cjk_font;
    return primary_font != NULL ? primary_font : cjk_font;
}

static int utf8_text_width(Display *display, const char *text)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int width = 0;

    while (*cursor != '\0') {
        const unsigned char *run = cursor;
        XftFont *font;
        unsigned int run_bytes = 0;

        {
            unsigned int n = utf8_length(*cursor);
            FcChar32 cp = utf8_codepoint(cursor, n);
            font = font_for_codepoint(display, cp);
        }
        if (font == NULL) break;

        while (*cursor != '\0') {
            unsigned int n = utf8_length(*cursor);
            FcChar32 cp = utf8_codepoint(cursor, n);
            if (font_for_codepoint(display, cp) != font) break;
            cursor += n;
            run_bytes += n;
        }

        if (run_bytes > 0) {
            XGlyphInfo extents;
            XftTextExtentsUtf8(display, font, run, (int)run_bytes, &extents);
            width += extents.xOff;
        }
    }

    return width;
}

static void draw_utf8_runs(Display *display, XftDraw *draw, XftColor *color,
                           const char *text, int x, int baseline)
{
    const unsigned char *cursor = (const unsigned char *)text;
    int draw_x = x;

    while (*cursor != '\0') {
        const unsigned char *run = cursor;
        XftFont *font;
        unsigned int run_bytes = 0;

        {
            unsigned int n = utf8_length(*cursor);
            FcChar32 cp = utf8_codepoint(cursor, n);
            font = font_for_codepoint(display, cp);
        }
        if (font == NULL) break;

        while (*cursor != '\0') {
            unsigned int n = utf8_length(*cursor);
            FcChar32 cp = utf8_codepoint(cursor, n);
            if (font_for_codepoint(display, cp) != font) break;
            cursor += n;
            run_bytes += n;
        }

        if (run_bytes > 0) {
            XGlyphInfo extents;
            XftDrawStringUtf8(draw, color, font, draw_x, baseline,
                              run, (int)run_bytes);
            XftTextExtentsUtf8(display, font, run, (int)run_bytes, &extents);
            draw_x += extents.xOff;
        }
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
    int ascent = 0;

    (void)font_list;
    (void)layout_direction;

    ensure_fonts(display);
    if ((primary_font == NULL && cjk_font == NULL) ||
        !XmStringGetLtoR(string, XmFONTLIST_DEFAULT_TAG, &text) || text == NULL) {
        XmStringDraw(display, drawable, font_list, string, gc, x, y, width,
                     alignment, layout_direction, clip);
        return;
    }

    if (!XGetGCValues(display, gc, GCForeground, &gc_values)) {
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

    text_width = utf8_text_width(display, text);
    draw_x = (int)x;
    if (alignment == XmALIGNMENT_CENTER)
        draw_x += ((int)width - text_width) / 2;
    else if (alignment == XmALIGNMENT_END)
        draw_x += (int)width - text_width;

    if (primary_font != NULL && primary_font->ascent > ascent)
        ascent = primary_font->ascent;
    if (cjk_font != NULL && cjk_font->ascent > ascent)
        ascent = cjk_font->ascent;

    local_clip.x = x;
    local_clip.y = y;
    local_clip.width = width;
    local_clip.height = (unsigned short)((primary_font != NULL ? primary_font->height : 0) >
                                         (cjk_font != NULL ? cjk_font->height : 0) ?
                                         (primary_font != NULL ? primary_font->height : 0) :
                                         (cjk_font != NULL ? cjk_font->height : 0));
    if (local_clip.height == 0) local_clip.height = 32;
    XftDrawSetClipRectangles(draw, 0, 0, clip != NULL ? clip : &local_clip, 1);

    draw_utf8_runs(display, draw, &color, text, draw_x, (int)y + ascent);

    XftDrawDestroy(draw);
    XtFree(text);
#else
    XmStringDraw(display, drawable, font_list, string, gc, x, y, width,
                 alignment, layout_direction, clip);
#endif
}
