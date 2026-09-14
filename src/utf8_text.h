#ifndef MARMELADE_UTF8_TEXT_H
#define MARMELADE_UTF8_TEXT_H

#include <Xm/Xm.h>

void marmelade_utf8_string_draw(Display *display, Window drawable,
                                XmFontList font_list, XmString string,
                                GC gc, Position x, Position y,
                                Dimension width, unsigned char alignment,
                                unsigned char layout_direction,
                                XRectangle *clip);

/* Custom-drawn Motif text goes through Xft when available. */
#define XmStringDraw marmelade_utf8_string_draw

#endif
