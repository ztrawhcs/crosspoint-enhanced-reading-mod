#include "EpdFontFamily.h"

bool EpdFontFamily::globalForceBold = false;

EpdFontFamily::EpdFontFamily(const EpdFont* regular, const EpdFont* bold, const EpdFont* italic,
                             const EpdFont* boldItalic)
    : regular(regular), bold(bold), italic(italic), boldItalic(boldItalic) {}

const EpdFont* EpdFontFamily::getFont(const Style style) const {
  const bool hasBold = globalForceBold || (style & BOLD) != 0;
  const bool hasItalic = (style & ITALIC) != 0;

  if (hasBold && hasItalic && boldItalic) return boldItalic;
  if (hasBold && bold) return bold;
  if (hasItalic && italic) return italic;
  return regular;
}

const EpdFontData* EpdFontFamily::getData(const Style style) const { return getFont(style)->data; }

const EpdGlyph* EpdFontFamily::getGlyph(const uint32_t cp, const Style style) const {
  return getFont(style)->getGlyph(cp);
}

bool EpdFontFamily::hasPrintableChars(const char* string, const Style style) const {
  return getFont(style)->hasPrintableChars(string);
}

void EpdFontFamily::getTextDimensions(const char* string, int* w, int* h, const Style style) const {
  getFont(style)->getTextDimensions(string, w, h);
}
