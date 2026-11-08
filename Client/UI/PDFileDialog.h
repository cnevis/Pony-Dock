#pragma once

#include <windows.h>

#include <string>

bool openFileDialog(HWND owner, char const* title, std::string* result);
bool saveFileDialog(HWND owner, char const* title, char const* suggestedName, std::string* result);
