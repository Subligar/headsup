#pragma once

#include "fish_case.h"
#include "plate_image.h"

namespace headsup
{
    constexpr const char* kCaseFont  = "Georgia";
    constexpr const char* kCaseTitle = "Open Tackle Box";
    constexpr int kCaseTextMargin    = 2; // clear pixels kept around a text

    // The pictures painted once: Solid to Band. Empty for the rest.
    Image CaseImage(CasePicture picture);
    // A text GDI drew with `drawn` clear pixels around it, as white letters with its coverage as their alpha, cut to
    // kCaseTextMargin on every side and kept as tall as its line. Wine draws some glyphs past the width GDI measures,
    // so the text is drawn wide and cut after its last ink. Empty for a text with no ink.
    Image CaseTextImage(const Coverage& text, int drawn);
    // The font size for a text shown `shown` pixels tall: Georgia's line is about a quarter taller than its size. The
    // picture is stretched to its height either way: this only keeps it sharp.
    int CaseFontPixels(float shown);
}
