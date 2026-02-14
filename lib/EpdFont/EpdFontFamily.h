#pragma once
#include <cstdint>

#include "EpdFont.h"

class EpdFontFamily {
 public:
  enum Style : uint8_t { REGULAR = 0, BOLD = 1, ITALIC = 2, BOLD_ITALIC = 3, UNDERLINE = 4 };

  static bool globalForceBold;

  explicit EpdFontFamily(const EpdFont* regular, const EpdFont* bold = nullptr, const EpdFont* italic = nullptr,
                         const EpdFont* boldItalic = nullptr);

  const EpdFont* getFont(Style style) const;
  const EpdFontData* getData(Style style) const;
  const EpdGlyph* getGlyph(uint32_t cp, Style style) const;
  bool hasPrintableChars(const char* string, Style style) const;
  void getTextDimensions(const char* string, int* w, int* h, Style style) const;

 private:
  const EpdFont* regular;
  const EpdFont* bold;
  const EpdFont* italic;
  const EpdFont* boldItalic;
};
