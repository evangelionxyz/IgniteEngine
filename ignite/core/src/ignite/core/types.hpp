// Copyright (c) 2026 Evangelion Manuhutu

#pragma once
#ifndef IGN_CORE_TYPES_HPP
#define IGN_CORE_TYPES_HPP

#include <memory>

typedef unsigned char byte;
typedef unsigned short ushort;

typedef unsigned long long u64;
typedef unsigned int u32;
typedef unsigned short u16;
typedef unsigned char u8;

typedef long long i64;
typedef int i32;
typedef short i16;
typedef char i8;

typedef float f32;
typedef double f64;

template<typename T>
using Ref = std::shared_ptr<T>;

template<typename T>
using WeakRef = std::weak_ptr<T>;

template<typename T>
using Scope = std::unique_ptr<T>;

template<typename T, typename... Args>
static Ref<T> CreateRef(Args&&... args) { return std::make_shared<T>(std::forward<Args>(args)...); }

template<typename T, typename... Args>
static Scope<T> CreateScope(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

#endif
