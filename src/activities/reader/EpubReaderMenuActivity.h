#pragma once
#include <Epub.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <freertos/task.h>

#include <functional>
#include <string>
#include <vector>

#include “../ActivityWithSubactivity.h”
#include “util/ButtonNavigator.h”

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
const uint8_t currentOrientation, const std::function<void(uint8_t)>& onBack,
const std::function<void(MenuAction)>& onAction)
: ActivityWithSubactivity(“EpubReaderMenu”, renderer, mappedInput),
title(title),
pendingOrientation(currentOrientation),
currentPage(currentPage),
totalPages(totalPages),
bookProgressPercent(bookProgressPercent),
onBack(onBack),
onAction(onAction) {}

void onEnter() override;
void onExit() override;
void loop() override;

private:
struct MenuItem {
MenuAction action;
std::string label;
};

const std::vector<MenuItem> menuItems = {{MenuAction::SELECT_CHAPTER, “Go to Chapter”},
{MenuAction::ROTATE_SCREEN, “Reading Orientation”},
{MenuAction::BUTTON_MOD_SETTINGS, “Button Mods”},
{MenuAction::SWAP_CONTROLS, “Portrait Controls”},
{MenuAction::SWAP_LANDSCAPE_CONTROLS, “Landscape Controls”},
{MenuAction::GO_TO_PERCENT, “Go to %”},
{MenuAction::GO_HOME, “Go Home”},
{MenuAction::SYNC, “Sync Progress”},
{MenuAction::DELETE_CACHE, “Delete Book Cache”}};

int selectedIndex = 0;
bool updateRequired = false;
TaskHandle_t displayTaskHandle = nullptr;
SemaphoreHandle_t renderingMutex = nullptr;
ButtonNavigator buttonNavigator;
std::string title = “Reader Menu”;
uint8_t pendingOrientation = 0;
const std::vector<const char*> orientationLabels = {“Portrait”, “Landscape CW”, “Inverted”, “Landscape CCW”};
const std::vector<const char*> buttonModLabels = {“Off”, “Simple”, “Full”};
const std::vector<const char*> swapControlsLabels = {“Front: Format / Side: Page”, “Front: Page / Side: Format”};
int currentPage = 0;
int totalPages = 0;
int bookProgressPercent = 0;

const std::function<void(uint8_t)> onBack;
const std::function<void(MenuAction)> onAction;

static void taskTrampoline(void* param);
[[noreturn]] void displayTaskLoop();
void renderScreen();
};