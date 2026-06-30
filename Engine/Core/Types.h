#pragma once
// Companion macro header for Koi.Core.Types.
// Import Koi.Core.Types before using these macros.

#define KOI_FILENAME ::koi::ExtractFilename(__FILE__)
#define KOI_FILE     ::koi::ExtractFilenameNoExt(__FILE__)
