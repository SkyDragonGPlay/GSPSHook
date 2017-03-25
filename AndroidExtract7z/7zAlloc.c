/* 7zAlloc.c -- Allocation functions
2015-11-09 : Igor Pavlov : Public domain */

#include "Precomp.h"

#include "7zAlloc.h"

//#define _SZ_ALLOC_DEBUG 1
/* use _SZ_ALLOC_DEBUG to debug alloc/free operations */

#ifdef _SZ_ALLOC_DEBUG

#ifdef _WIN32
#include <windows.h>
#endif

#if defined(ANDROID)
#include <android/log.h>
#define  LOG_TAG "7zAlloc"
#define  LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define  LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define  LOGD printf
#define  LOGE printf
#endif

#include <stdio.h>
int g_allocCount = 0;
int g_allocCountTemp = 0;

#endif

void *SzAlloc(void *p, size_t size)
{
  UNUSED_VAR(p);
  if (size == 0)
    return 0;
  #ifdef _SZ_ALLOC_DEBUG
  LOGD("\nAlloc %10u bytes; count = %10d", (unsigned)size, g_allocCount);
  g_allocCount++;
  #endif
  return malloc(size);
}

void SzFree(void *p, void *address)
{
  UNUSED_VAR(p);
  #ifdef _SZ_ALLOC_DEBUG
  if (address != 0)
  {
    g_allocCount--;
    LOGD("\nFree; count = %10d", g_allocCount);
  }
  #endif
  free(address);
}

void *SzAllocTemp(void *p, size_t size)
{
  UNUSED_VAR(p);
  if (size == 0)
    return 0;
  #ifdef _SZ_ALLOC_DEBUG
  LOGD("\nAlloc_temp %10u bytes;  count = %10d", (unsigned)size, g_allocCountTemp);
  g_allocCountTemp++;
  #ifdef _WIN32
  return HeapAlloc(GetProcessHeap(), 0, size);
  #endif
  #endif
  return malloc(size);
}

void SzFreeTemp(void *p, void *address)
{
  UNUSED_VAR(p);
  #ifdef _SZ_ALLOC_DEBUG
  if (address != 0)
  {
    g_allocCountTemp--;
    LOGD("\nFree_temp; count = %10d", g_allocCountTemp);
  }
  #ifdef _WIN32
  HeapFree(GetProcessHeap(), 0, address);
  return;
  #endif
  #endif
  free(address);
}
