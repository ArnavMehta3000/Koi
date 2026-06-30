#pragma once
// Companion macro header for Koi.Memory.MemoryTracker.
// Import Koi.Memory.MemoryTracker before using this macro.

#define KOI_MEM_TAG_SCOPE(tag) ::koi::MemTagScope _koi_mem_tag_scope_##__LINE__{tag}
