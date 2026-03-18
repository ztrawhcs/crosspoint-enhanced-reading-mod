// --- HIGHLIGHT MODE ---
#include "HighlightStore.h"

#include <GfxRenderer.h>
#include <Logging.h>

#include <algorithm>

#include "StringUtils.h"

namespace HighlightStore {

bool ensureDir() { return Storage.ensureDirectoryExists(HIGHLIGHT_DIR); }

// --- Step 6: Text extraction helpers ---

// Internal: collect pointers to all PageLine elements (text lines only)
static std::vector<const PageLine*> getTextLines(const Page& page) {
  std::vector<const PageLine*> lines;
  for (const auto& el : page.elements) {
    if (el->getTag() == TAG_PageLine) {
      lines.push_back(static_cast<const PageLine*>(el.get()));
    }
  }
  return lines;
}

int countTextLines(const Page& page) {
  int count = 0;
  for (const auto& el : page.elements) {
    if (el->getTag() == TAG_PageLine) count++;
  }
  return count;
}

std::string getLineText(const Page& page, int textLineIndex) {
  auto lines = getTextLines(page);
  if (textLineIndex < 0 || textLineIndex >= static_cast<int>(lines.size())) return "";

  const auto& block = lines[textLineIndex]->getBlock();
  const auto& words = block->getWords();
  std::string result;
  for (auto it = words.begin(); it != words.end(); ++it) {
    if (it != words.begin()) result += ' ';
    result += *it;
  }
  return result;
}

bool getLineGeometry(const Page& page, int fontId, int textLineIndex, int16_t& outY, int16_t& outHeight) {
  auto lines = getTextLines(page);
  if (textLineIndex < 0 || textLineIndex >= static_cast<int>(lines.size())) return false;

  outY = lines[textLineIndex]->yPos;
  // Use the block's word list to determine if this is a real text line
  // Height is approximated from font line height — caller must provide fontId
  // We'll set a reasonable height based on line spacing between consecutive lines
  if (textLineIndex + 1 < static_cast<int>(lines.size())) {
    outHeight = lines[textLineIndex + 1]->yPos - outY;
  } else {
    // Last line: estimate from earlier spacing or use a default
    if (textLineIndex > 0) {
      outHeight = outY - lines[textLineIndex - 1]->yPos;
    } else {
      outHeight = 20;  // fallback for single-line pages
    }
  }
  return true;
}

bool getLineXExtent(const Page& page, int fontId, int textLineIndex, int16_t& outXStart, int16_t& outXEnd) {
  auto lines = getTextLines(page);
  if (textLineIndex < 0 || textLineIndex >= static_cast<int>(lines.size())) return false;

  const auto& pl = lines[textLineIndex];
  const auto& block = pl->getBlock();
  const auto& words = block->getWords();
  const auto& xpositions = block->getWordXPositions();

  if (words.empty()) return false;

  // X start is the first word's position (relative to block) + page element's xPos
  outXStart = pl->xPos + *xpositions.begin();

  // X end is the last word's position + its rendered width
  auto lastWordIt = words.end();
  --lastWordIt;
  auto lastXposIt = xpositions.end();
  --lastXposIt;
  // We can't call renderer.getTextWidth here without a renderer reference,
  // but we can approximate the end from the last word's x position + a rough estimate.
  // For correct rendering, the caller should compute the end X using the renderer.
  // We store the last word x position + xPos as a minimum.
  outXEnd = pl->xPos + *lastXposIt;  // caller adjusts by adding last word width

  return true;
}

int findFirstSentenceStart(const std::string& lineText) {
  // The start of the line is always a valid sentence start for the first selected line
  // But if user adjusts, we look for sentence boundaries
  // A sentence start is: position 0, or the first non-space char after ". " or "! " or "? "
  if (lineText.empty()) return 0;

  for (size_t i = 0; i + 1 < lineText.size(); i++) {
    char c = lineText[i];
    if ((c == '.' || c == '!' || c == '?') && i + 1 < lineText.size() && lineText[i + 1] == ' ') {
      // Found sentence boundary — the sentence starts after the space
      size_t start = i + 2;
      while (start < lineText.size() && lineText[start] == ' ') start++;
      if (start < lineText.size()) return static_cast<int>(start);
    }
  }
  return 0;  // whole line is one sentence
}

int findLastSentenceEnd(const std::string& lineText) {
  if (lineText.empty()) return -1;

  int lastEnd = -1;
  for (size_t i = 0; i < lineText.size(); i++) {
    char c = lineText[i];
    if (c == '.' || c == '!' || c == '?') {
      lastEnd = static_cast<int>(i + 1);  // one past the punctuation
    }
  }
  return lastEnd;
}

std::string extractText(const Page& page, int startLine, int startChar, int endLine, int endChar) {
  auto lines = getTextLines(page);
  int lineCount = static_cast<int>(lines.size());

  if (startLine < 0) startLine = 0;
  if (endLine >= lineCount) endLine = lineCount - 1;
  if (startLine > endLine) return "";

  std::string result;

  for (int i = startLine; i <= endLine; i++) {
    std::string lineText;
    const auto& block = lines[i]->getBlock();
    const auto& words = block->getWords();
    for (auto it = words.begin(); it != words.end(); ++it) {
      if (it != words.begin()) lineText += ' ';
      lineText += *it;
    }

    if (i == startLine && i == endLine) {
      // Single line selection
      int start = std::max(0, startChar);
      int end =
          (endChar == -1) ? static_cast<int>(lineText.size()) : std::min(endChar, static_cast<int>(lineText.size()));
      if (start < end) {
        result += lineText.substr(start, end - start);
      }
    } else if (i == startLine) {
      int start = std::max(0, startChar);
      if (start < static_cast<int>(lineText.size())) {
        result += lineText.substr(start);
      }
    } else if (i == endLine) {
      int end =
          (endChar == -1) ? static_cast<int>(lineText.size()) : std::min(endChar, static_cast<int>(lineText.size()));
      if (end > 0) {
        if (!result.empty()) result += ' ';
        result += lineText.substr(0, end);
      }
    } else {
      // Middle lines: include entirely
      if (!result.empty()) result += ' ';
      result += lineText;
    }
  }

  return result;
}

// Build the per-book highlights file path: /highlights/<sanitized-title>.txt
static std::string bookFilePath(const std::string& title) {
  std::string sanitized = StringUtils::sanitizeFilename(title, 40);
  return std::string(HIGHLIGHT_DIR) + "/HIGHLIGHTS - " + sanitized + ".txt";
}

// Delimiter between highlight entries in the per-book file
// Old format: "=== HIGHLIGHT ===" with separate "Ref: N.S-E" line
// New format: "=== HIGHLIGHT [N.S-E] ===" with ref embedded in delimiter
static constexpr const char* HIGHLIGHT_DELIM_OLD = "=== HIGHLIGHT ===";
static constexpr const char* HIGHLIGHT_DELIM_PREFIX = "=== HIGHLIGHT [";

bool saveHighlight(const std::string& title, const std::string& author, int spineIndex, const std::string& chapterName,
                   int startPage, int endPage, int totalPages, float progressPercent,
                   const std::string& highlightedText) {
  if (!ensureDir()) {
    LOG_ERR("HLS", "Failed to create highlights directory");
    return false;
  }

  std::string filePath = bookFilePath(title);

  // Chapter display: use TOC name if available, else "Chapter N"
  String chapterDisplay;
  if (!chapterName.empty()) {
    chapterDisplay = chapterName.c_str();
  } else {
    chapterDisplay = "Chapter ";
    chapterDisplay += String(spineIndex);
  }

  // Build the new highlight block — human-readable format
  // Internal ref is embedded in the delimiter line for parser use (not visually prominent)
  String newBlock;
  newBlock += "=== HIGHLIGHT [";
  newBlock += String(spineIndex);
  newBlock += ".";
  newBlock += String(startPage);
  newBlock += "-";
  newBlock += String(endPage);
  newBlock += "] ===\n";
  newBlock += chapterDisplay;
  newBlock += " | Page ";
  newBlock += String(startPage + 1);
  if (endPage != startPage) {
    newBlock += "-";
    newBlock += String(endPage + 1);
  }
  newBlock += " / ";
  newBlock += String(totalPages);
  newBlock += " | ";
  newBlock += String(static_cast<int>(progressPercent + 0.5f));
  newBlock += "%\n";
  newBlock += "\n";
  newBlock += highlightedText.c_str();
  newBlock += "\n\n";

  // Read existing file (if any) and append; otherwise write book header + first highlight
  String existing = Storage.readFile(filePath.c_str());
  String content;

  if (existing.length() == 0) {
    // First highlight for this book — write header
    content += "Book: ";
    content += title.c_str();
    content += "\n";
    content += "Author: ";
    content += author.c_str();
    content += "\n\n";
  } else {
    content = existing;
  }

  content += newBlock;

  if (!Storage.writeFile(filePath.c_str(), content)) {
    LOG_ERR("HLS", "Failed to write highlight to %s", filePath.c_str());
    return false;
  }

  LOG_DBG("HLS", "Highlight appended to %s (%d bytes total)", filePath.c_str(), content.length());
  return true;
}

std::vector<SavedHighlight> loadHighlightsForPage(const std::string& title, int spineIndex, int page) {
  std::vector<SavedHighlight> results;

  std::string filePath = bookFilePath(title);
  String raw = Storage.readFile(filePath.c_str());
  if (raw.length() == 0) {
    return results;
  }

  std::string content(raw.c_str());
  const std::string delimPrefix = std::string(HIGHLIGHT_DELIM_PREFIX);
  const std::string delimOld = std::string(HIGHLIGHT_DELIM_OLD) + "\n";

  // Split content on highlight delimiter lines (supports old and new format)
  size_t searchPos = 0;
  while (true) {
    // Find next highlight block — try new format first, then old
    size_t delimPos = content.find(delimPrefix, searchPos);
    size_t delimOldPos = content.find(delimOld, searchPos);
    bool isNewFormat;

    if (delimPos == std::string::npos && delimOldPos == std::string::npos) break;
    if (delimPos == std::string::npos) {
      delimPos = delimOldPos;
      isNewFormat = false;
    } else if (delimOldPos == std::string::npos) {
      isNewFormat = true;
    } else {
      isNewFormat = (delimPos <= delimOldPos);
      if (!isNewFormat) delimPos = delimOldPos;
    }

    // Find end of delimiter line
    size_t delimLineEnd = content.find('\n', delimPos);
    if (delimLineEnd == std::string::npos) break;
    size_t blockStart = delimLineEnd + 1;

    // Find the next delimiter (either format) for block boundary
    size_t nextNew = content.find(delimPrefix, blockStart);
    size_t nextOld = content.find(delimOld, blockStart);
    size_t nextDelim = std::string::npos;
    if (nextNew != std::string::npos) nextDelim = nextNew;
    if (nextOld != std::string::npos && nextOld < nextDelim) nextDelim = nextOld;

    std::string block = (nextDelim == std::string::npos) ? content.substr(blockStart)
                                                         : content.substr(blockStart, nextDelim - blockStart);

    searchPos = blockStart;

    int parsedSpine = -1;
    int parsedStartPage = -1;
    int parsedEndPage = -1;
    std::string parsedText;
    bool inText = false;

    // New format: parse ref from delimiter line "=== HIGHLIGHT [N.S-E] ==="
    if (isNewFormat) {
      size_t bracketStart = content.find('[', delimPos);
      size_t bracketEnd = content.find(']', delimPos);
      if (bracketStart != std::string::npos && bracketEnd != std::string::npos && bracketEnd > bracketStart) {
        std::string ref = content.substr(bracketStart + 1, bracketEnd - bracketStart - 1);
        size_t dotPos = ref.find('.');
        if (dotPos != std::string::npos) {
          parsedSpine = atoi(ref.substr(0, dotPos).c_str());
          size_t dashPos = ref.find('-', dotPos + 1);
          if (dashPos != std::string::npos) {
            parsedStartPage = atoi(ref.substr(dotPos + 1, dashPos - dotPos - 1).c_str());
            parsedEndPage = atoi(ref.substr(dashPos + 1).c_str());
          } else {
            parsedStartPage = atoi(ref.substr(dotPos + 1).c_str());
            parsedEndPage = parsedStartPage;
          }
        }
      }
    }

    size_t pos = 0;
    int headerLinesRead = 0;
    while (pos < block.size()) {
      size_t eol = block.find('\n', pos);
      if (eol == std::string::npos) eol = block.size();
      std::string line = block.substr(pos, eol - pos);
      pos = eol + 1;

      // Old format: "Ref: N.S-E" as a separate line
      if (!isNewFormat && line.rfind("Ref: ", 0) == 0) {
        size_t dotPos = line.find('.', 5);
        if (dotPos != std::string::npos) {
          parsedSpine = atoi(line.substr(5, dotPos - 5).c_str());
          size_t dashPos = line.find('-', dotPos + 1);
          if (dashPos != std::string::npos) {
            parsedStartPage = atoi(line.substr(dotPos + 1, dashPos - dotPos - 1).c_str());
            parsedEndPage = atoi(line.substr(dashPos + 1).c_str());
          } else {
            parsedStartPage = atoi(line.substr(dotPos + 1).c_str());
            parsedEndPage = parsedStartPage;
          }
        }
        headerLinesRead++;
        continue;
      }

      // The display line (chapter | page | progress) — skip for parsing
      if (!inText && line.find(" | Page ") != std::string::npos) {
        headerLinesRead++;
        continue;
      }

      // Blank line after header signals start of text
      if (!inText && line.empty() && headerLinesRead >= 0) {
        inText = true;
        continue;
      }

      if (inText) {
        if (!parsedText.empty()) parsedText += '\n';
        parsedText += line;
      }
    }

    // Match on spine index only — don't filter by page number. Font size or layout
    // changes reflow text to different pages, so the saved page numbers may be stale.
    // The caller uses findHighlightBounds() to check if the text actually appears on
    // the current page, so returning extra candidates here is safe.
    if (parsedSpine == spineIndex) {
      // Trim trailing whitespace/newlines from text
      while (!parsedText.empty() && (parsedText.back() == '\n' || parsedText.back() == ' ')) {
        parsedText.pop_back();
      }
      if (!parsedText.empty()) {
        SavedHighlight hl;
        hl.spineIndex = parsedSpine;
        hl.startPage = parsedStartPage;
        hl.endPage = parsedEndPage;
        hl.text = parsedText;
        results.push_back(hl);
      }
    }
  }

  return results;
}

bool findHighlightBounds(const Page& page, const std::string& text, HighlightPageRole role, int& outStartLine,
                         int& outStartChar, int& outEndLine, int& outEndChar) {
  auto lines = getTextLines(page);
  if (lines.empty() || text.empty()) return false;
  const int lineCount = static_cast<int>(lines.size());

  // MIDDLE: entire page is highlighted
  if (role == HighlightPageRole::MIDDLE) {
    outStartLine = 0;
    outStartChar = 0;
    outEndLine = lineCount - 1;
    outEndChar = -1;
    return true;
  }

  // Extract first and last words from saved text
  auto firstNonSpace = text.find_first_not_of(" \t\r\n");
  if (firstNonSpace == std::string::npos) return false;
  size_t firstWordEnd = text.find(' ', firstNonSpace);
  std::string firstWord = (firstWordEnd == std::string::npos)
                              ? text.substr(firstNonSpace)
                              : text.substr(firstNonSpace, firstWordEnd - firstNonSpace);

  auto lastNonSpace = text.find_last_not_of(" \t\r\n");
  size_t lastWordStart = text.rfind(' ', lastNonSpace);
  std::string lastWord = (lastWordStart == std::string::npos)
                             ? text.substr(0, lastNonSpace + 1)
                             : text.substr(lastWordStart + 1, lastNonSpace - lastWordStart);

  // Find start: FULL or START roles match first word; END role starts at page beginning
  int startLine = 0, startChar = 0;
  if (role == HighlightPageRole::FULL || role == HighlightPageRole::START) {
    startLine = -1;
    for (int i = 0; i < lineCount; i++) {
      const auto& words = lines[i]->getBlock()->getWords();
      int charOff = 0;
      for (const auto& w : words) {
        if (w == firstWord) {
          startLine = i;
          startChar = charOff;
          break;
        }
        charOff += static_cast<int>(w.size()) + 1;
      }
      if (startLine >= 0) break;
    }
    if (startLine < 0) return false;
  }

  // Find end: FULL or END roles match last word; START role ends at page end
  int endLine = lineCount - 1, endChar = -1;
  if (role == HighlightPageRole::FULL || role == HighlightPageRole::END) {
    endLine = -1;
    for (int i = lineCount - 1; i >= startLine; i--) {
      const auto& words = lines[i]->getBlock()->getWords();
      int charOff = 0, lastMatchEnd = -1;
      for (const auto& w : words) {
        if (w == lastWord) lastMatchEnd = charOff + static_cast<int>(w.size());
        charOff += static_cast<int>(w.size()) + 1;
      }
      if (lastMatchEnd >= 0) {
        endLine = i;
        endChar = lastMatchEnd;
        break;
      }
    }
    if (endLine < 0) {
      endLine = startLine;
      endChar = -1;
    }
  }

  outStartLine = startLine;
  outStartChar = startChar;
  outEndLine = endLine;
  outEndChar = endChar;
  return true;
}

bool deleteHighlight(const std::string& title, int spineIndex, int startPage, int endPage) {
  std::string filePath = bookFilePath(title);
  String raw = Storage.readFile(filePath.c_str());
  if (raw.length() == 0) return false;

  std::string content(raw.c_str());

  // Build ref patterns for both old and new format
  char refNewBuf[64];
  snprintf(refNewBuf, sizeof(refNewBuf), "[%d.%d-%d]", spineIndex, startPage, endPage);
  const std::string refNew(refNewBuf);
  char refOldBuf[64];
  snprintf(refOldBuf, sizeof(refOldBuf), "Ref: %d.%d-%d\n", spineIndex, startPage, endPage);
  const std::string refOld(refOldBuf);

  // Scan for highlight blocks and remove the matching one
  // Look for either delimiter format
  size_t searchPos = 0;
  while (true) {
    size_t newPos = content.find(HIGHLIGHT_DELIM_PREFIX, searchPos);
    size_t oldPos = content.find(std::string(HIGHLIGHT_DELIM_OLD) + "\n", searchPos);
    size_t delimPos;
    if (newPos == std::string::npos && oldPos == std::string::npos) break;
    if (newPos == std::string::npos)
      delimPos = oldPos;
    else if (oldPos == std::string::npos)
      delimPos = newPos;
    else
      delimPos = std::min(newPos, oldPos);

    // Find end of this block (next delimiter of either type)
    size_t delimLineEnd = content.find('\n', delimPos);
    if (delimLineEnd == std::string::npos) break;
    size_t blockContentStart = delimLineEnd + 1;

    size_t nextNew = content.find(HIGHLIGHT_DELIM_PREFIX, blockContentStart);
    size_t nextOld = content.find(std::string(HIGHLIGHT_DELIM_OLD) + "\n", blockContentStart);
    size_t blockEnd = std::string::npos;
    if (nextNew != std::string::npos) blockEnd = nextNew;
    if (nextOld != std::string::npos && nextOld < blockEnd) blockEnd = nextOld;
    if (blockEnd == std::string::npos) blockEnd = content.size();

    // Check if this block's delimiter line or content contains our ref
    std::string delimLine = content.substr(delimPos, delimLineEnd - delimPos);
    std::string blockContent = content.substr(blockContentStart, blockEnd - blockContentStart);
    if (delimLine.find(refNew) != std::string::npos || blockContent.find(refOld) != std::string::npos) {
      std::string newContent = content.substr(0, delimPos) + content.substr(blockEnd);
      if (!Storage.writeFile(filePath.c_str(), String(newContent.c_str()))) {
        LOG_ERR("HLS", "Failed to write highlights file after delete");
        return false;
      }
      LOG_DBG("HLS", "Deleted highlight %d.%d-%d from %s", spineIndex, startPage, endPage, filePath.c_str());
      return true;
    }

    searchPos = blockContentStart;
  }

  LOG_ERR("HLS", "deleteHighlight: no match for %d.%d-%d", spineIndex, startPage, endPage);
  return false;
}

}  // namespace HighlightStore
// --- HIGHLIGHT MODE ---
