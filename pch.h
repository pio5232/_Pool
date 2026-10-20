#pragma once

#include <Windows.h>

#include <deque>
#include <map>
#include <unordered_map>
#include <queue>
#include <array>
#include <vector>
#include <memory>
#include "CrashDump.h"
#include "SRWLockGuard.h"
#include "Profiler.h"
#include "PerformanceDataInfo.h"
#include "Memory.h"
#include "MemorySystem.h"
extern jh::MemorySystem* g_pMemSystem;
#include "MySTLAllocator.h"
#include "MySTL.h"
