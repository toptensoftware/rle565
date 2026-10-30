#pragma once

#include <windows.h>

// Module handle of this DLL
extern HMODULE g_module;

// Count of live COM objects and server locks, for DllCanUnloadNow
void ModuleAddRef();
void ModuleRelease();
