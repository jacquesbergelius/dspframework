/*
 * timer.cpp -- Timer object
 *
 * Measures execution time using high-resolution Windows timer
 *
  * Written by Jarkko Vuori 2013
 */
#include "timer.h"

// Initialize the resolution of the timer
LARGE_INTEGER Timer::m_freq = (QueryPerformanceFrequency(&Timer::m_freq), Timer::m_freq);
  
// Calculate the overhead of the timer
LONGLONG Timer::m_overhead = Timer::GetOverhead();

DWORD_PTR Timer::omask = NULL;

HANDLE Timer::hThread = GetCurrentThread();