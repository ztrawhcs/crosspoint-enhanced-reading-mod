// From
// https://github.com/vroland/epdiy/blob/c61e9e923ce2418150d54f88cea5d196cdc40c54/src/epd_internals.h

#pragma once
#include <cstdint>

/// Font data stored PER GLYPH
typedef struct {
  uint8_t width;        ///< Bitmap dimensions in pixels
  uint8_t height;       ///< Bitmap dimensions in pixels
  uint16_t advanceX;    ///< Distance to advance cursor (x axis)
  int16_t left;         ///< X dist from cursor pos to UL corner
  int16_t top;          ///< Y dist from cursor pos to UL corner
  uint16_t dataLength;  ///< Size of the font data.
  uint32_t dataOffset;  ///< Pointer into EpdFont->bitmap (or within-group offset for compressed fonts)
} EpdGlyph;

/// Compressed font group: a DEFLATE-compressed block of glyph bitmaps
typedef struct {
  uint32_t compressedOffset;  ///< Byte offset into compressed data array
  uint32_t compressedSize;    ///< Compressed DEFLATE stream size
  uint32_t uncompressedSize;  ///< Decompressed size
  uint16_t glyphCount;        ///< Number of glyphs in this group
  uint16_t firstGlyphIndex;   ///< First glyph index in the global glyph array
} EpdFontGroup;

/// Glyph interval structure
typedef struct {
  uint32_t first;   ///< The first unicode code point of the interval
  uint32_t last;    ///< The last unicode code point of the interval
  uint32_t offset;  ///< Index of the first code point into the glyph array
} EpdUnicodeInterval;

/// Kerning class entry: maps a codepoint to a kerning class index
typedef struct {
  uint32_t codepoint;  ///< Unicode code point
  uint16_t classId;    ///< Kerning class index
} EpdKernClassEntry;

/// Ligature pair: two codepoints that combine into a replacement glyph
typedef struct {
  uint32_t pair;          ///< Two 16-bit codepoints packed into uint32_t (first << 16 | second)
  uint16_t replacement;   ///< Replacement codepoint
} EpdLigaturePair;

/// Data stored for FONT AS A WHOLE
typedef struct {
  const uint8_t* bitmap;                ///< Glyph bitmaps, concatenated
  const EpdGlyph* glyph;                ///< Glyph array
  const EpdUnicodeInterval* intervals;  ///< Valid unicode intervals for this font
  uint32_t intervalCount;               ///< Number of unicode intervals.
  uint8_t advanceY;                     ///< Newline distance (y axis)
  int ascender;                         ///< Maximal height of a glyph above the base line
  int descender;                        ///< Maximal height of a glyph below the base line
  bool is2Bit;
  const EpdFontGroup* groups;           ///< NULL for uncompressed fonts
  uint16_t groupCount;                  ///< 0 for uncompressed fonts
  const void* kernPairTable;            ///< Reserved (legacy pair kerning), always nullptr
  const EpdKernClassEntry* kernLeftClasses;   ///< Left kerning classes (nullptr if no kerning)
  const EpdKernClassEntry* kernRightClasses;  ///< Right kerning classes (nullptr if no kerning)
  const int8_t* kernMatrix;             ///< Kerning matrix (leftClassMax * rightClassMax)
  uint16_t kernLeftClassCount;          ///< Number of left class entries
  uint16_t kernRightClassCount;         ///< Number of right class entries
  uint16_t kernLeftClassMax;            ///< Number of distinct left classes
  uint16_t kernRightClassMax;           ///< Number of distinct right classes
  const EpdLigaturePair* ligaturePairs; ///< Ligature pair table (nullptr if none)
  uint16_t ligatureCount;               ///< Number of ligature pairs
} EpdFontData;
