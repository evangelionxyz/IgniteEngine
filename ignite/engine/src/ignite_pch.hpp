// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGNITE_PCH_HPP
#define IGNITE_PCH_HPP

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>
#include <string>
#include <string_view>
#include <vector>
#include <span>
#include <queue>
#include <unordered_map>
#include <unordered_set>
#include <map>
#include <set>
#include <mutex>
#include <atomic>
#include <condition_variable>
#include <chrono>
#include <cmath>
#include <cctype>
#include <array>
#include <filesystem>
#include <ostream>
#include <fstream>
#include <format>
#include <concepts>
#include <ranges>
#include <type_traits>
#include <cstdio>
#include <iomanip>
#include <thread>
#include <sstream>
#include <random>
#include <limits>
#include <typeinfo>

#ifdef PLATFORM_WINDOWS
#include <Windows.h>
#include <ShObjIdl.h>
#include <commdlg.h>
#include <objbase.h> // for CoCreateGuid
#elif PLATFORM_LINUX
#include <unistd.h>
#endif

#endif
