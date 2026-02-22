#pragma once
#include <Epub.h>
#include <I18n.h>

#include <functional>
#include <string>
#include <vector>

#include "../ActivityWithSubactivity.h"
#include "util/ButtonNavigator.h"

class EpubReaderMenuActivity final : public ActivityWithSubactivity {
 public:
  enum class MenuAction {
    SELECT_CHAPTER,
    ROTATE_SCREEN,
    BUTTON_MOD_SETTINGS,
    SWAP_CONTROLS,
    SWAP_LANDSCAPE_CONTROLS,
    GO_TO_PERCENT,
    GO_HOME,
    SYNC,
    DELETE_CACHE
  };

  explicit EpubReaderMenuActivity(GfxRenderer& renderer, MappedInputManager& mappedInput, const std::string& title,
                                  const int currentPage, const int totalPages, const int bookProgressPercent,
                                  const float bookProgressExact, const size_t totalBookBytes,
                                  const uint8_t currentOrientation, const std::function<void(uint8_t)>& onBack,
                                  const std::function<void(MenuAction)>& onAction)
      : ActivityWithSubactivity("EpubReaderMenu", renderer, mappedInput),
        title(title),
        pendingOrientation(currentOrientation),
        currentPage(currentPage),
        totalPages(totalPages),
        bookProgressPercent(bookProgressPercent),
        bookProgressExact(bookProgressExact),
        totalBookBytes(totalBookBytes),
        onBack(onBack),
        onAction(onAction) {}

  void onEnter() override;
  void onExit() override;
  void loop() override;
  void render(Activity::RenderLock&&) override;

 private:
  struct MenuItem {
    MenuAction action;
    StrId labelId;
  };

  // Fixed menu layout (order matters for up/down navigation).
  const std::vector<MenuItem> menuItems = {{MenuAction::SELECT_CHAPTER, StrId::STR_SELECT_CHAPTER},
                                           {MenuAction::ROTATE_SCREEN, StrId::STR_ORIENTATION},
                                           {MenuAction::BUTTON_MOD_SETTINGS, StrId::STR_BUTTON_MOD_SETTINGS},
                                           {MenuAction::SWAP_CONTROLS, StrId::STR_PORTRAIT_CONTROLS},
                                           {MenuAction::SWAP_LANDSCAPE_CONTROLS, StrId::STR_LANDSCAPE_CONTROLS},
                                           {MenuAction::GO_TO_PERCENT, StrId::STR_GO_TO_PERCENT},
                                           {MenuAction::GO_HOME, StrId::STR_GO_HOME_BUTTON},
                                           {MenuAction::SYNC, StrId::STR_SYNC_PROGRESS},
                                           {MenuAction::DELETE_CACHE, StrId::STR_DELETE_CACHE}};

  int selectedIndex = 0;

  ButtonNavigator buttonNavigator;
  std::string title = "Reader Menu";
  uint8_t pendingOrientation = 0;
  const std::vector<StrId> orientationLabels = {StrId::STR_PORTRAIT, StrId::STR_LANDSCAPE_CW, StrId::STR_INVERTED,
                                                StrId::STR_LANDSCAPE_CCW};
  const std::vector<const char*> buttonModLabels = {"Off", "Simple", "Full"};
  const std::vector<const char*> swapControlsLabels = {"Default", "Swapped"};
  int currentPage = 0;
  int totalPages = 0;
  int bookProgressPercent = 0;
  float bookProgressExact = 0.0f;
  size_t totalBookBytes = 0;

  const std::function<void(uint8_t)> onBack;
  const std::function<void(MenuAction)> onAction;
};
