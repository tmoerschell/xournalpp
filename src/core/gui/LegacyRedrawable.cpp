#include "LegacyRedrawable.h"

#include "model/Element.h"  // for Element
#include "util/Range.h"     // for Range

void LegacyRedrawable::repaintElement(Element* e) const {
    repaintArea(e->getX(), e->getY(), e->getElementWidth() + e->getX(), e->getElementHeight() + e->getY());
}

void LegacyRedrawable::repaintRect(double x, double y, double width, double height) const {
    repaintArea(x, y, x + width, y + height);
}

void LegacyRedrawable::rerenderRange(const Range& r) {
    const auto width = r.getWidth();
    const auto height = r.getHeight();
    if (width > 0.0 || height > 0.0) {
        rerenderRect(r.getX(), r.getY(), r.getWidth(), r.getHeight());
    }
}

void LegacyRedrawable::rerenderElement(const Element* e) {
    rerenderRect(e->getX(), e->getY(), e->getElementWidth(), e->getElementHeight());
}
