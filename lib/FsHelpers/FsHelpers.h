#pragma once
#include <string>
#include <algorithm>

class FsHelpers {
 public:
  static std::string normalisePath(const std::string& path);

  static bool hasExtension(const std::string& path, const std::string& ext) {
    if (path.size() < ext.size()) return false;
    std::string suffix = path.substr(path.size() - ext.size());
    std::transform(suffix.begin(), suffix.end(), suffix.begin(), ::tolower);
    std::string lowerExt = ext;
    std::transform(lowerExt.begin(), lowerExt.end(), lowerExt.begin(), ::tolower);
    return suffix == lowerExt;
  }

  static bool hasEpubExtension(const std::string& path) { return hasExtension(path, ".epub"); }
  static bool hasXtcExtension(const std::string& path) {
    return hasExtension(path, ".xtc") || hasExtension(path, ".xtch");
  }
  static bool hasTxtExtension(const std::string& path) { return hasExtension(path, ".txt"); }
  static bool hasMarkdownExtension(const std::string& path) { return hasExtension(path, ".md"); }
};
