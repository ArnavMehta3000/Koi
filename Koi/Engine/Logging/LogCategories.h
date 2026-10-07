#pragma once
#include "Koi/Engine/Logging/Logger.h"

// Included by Logger.h. Engine, Editor and Game are the catch-alls the KOI_LOG_<LEVEL> macros pick by file path
KOI_LOG_CATEGORY_IN(Engine,   Engine)
KOI_LOG_CATEGORY_IN(Core,     Engine)
KOI_LOG_CATEGORY_IN(Platform, Engine)
KOI_LOG_CATEGORY_IN(Input,    Engine)
KOI_LOG_CATEGORY_IN(IO,       Engine)
KOI_LOG_CATEGORY_IN(Assets,   Engine)
KOI_LOG_CATEGORY_IN(ECS,      Engine)
KOI_LOG_CATEGORY_IN(Render,   Engine)
KOI_LOG_CATEGORY_IN(RHI,      Engine)
KOI_LOG_CATEGORY_IN(Shaders,  Engine)

KOI_LOG_CATEGORY_IN(Editor,   Editor)
KOI_LOG_CATEGORY_IN(Import,   Editor)
KOI_LOG_CATEGORY_IN(Cook,     Editor)

KOI_LOG_CATEGORY_IN(Game,     Game)
