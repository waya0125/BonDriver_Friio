// SPDX-License-Identifier: MIT
#pragma once
#include <stdexcept>
using traceable_error = std::runtime_error;
template <typename... Args>
inline void err(const char*, Args...) {}
