#ifndef STDDEF_H_
#define STDDEF_H_

#ifdef _WIN32
typedef unsigned long long size_t;
typedef long long ssize_t;
typedef long long intptr_t;
typedef unsigned long long uintptr_t;
#else
typedef unsigned long size_t;
typedef long ssize_t;
typedef long intptr_t;
typedef unsigned long uintptr_t;
#endif

#endif // STDDEF_H_