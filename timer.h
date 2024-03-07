#pragma once
#include <windows.h>
#include <iostream>
#include <vector>
#include <algorithm>
 
class Timer {
public:
	// start timing
	inline void Start() {
		omask = SetThreadAffinityMask(hThread, 0x1); // use only single core
		QueryPerformanceCounter(&m_start);
	}
 
	// stop timing
	inline void Stop() {
		QueryPerformanceCounter(&m_stop);
		SetThreadAffinityMask(hThread, omask);

		m_results.push_back(m_stop.QuadPart - m_start.QuadPart);
	}

	// Returns elapsed time in milliseconds (ms)
	double Elapsed() {
		std::vector <LONGLONG>::iterator Iter1;
		LONGLONG t = 0;
		int n = 0;

		std::sort(m_results.begin(), m_results.end());
#if 0
		for (Iter1 = m_results.begin() ; Iter1 != m_results.end() ; Iter1++)
			std::cout << *Iter1 << " ";
		std::cout << std::endl;
#endif
		for (std::vector<LONGLONG>::size_type i = m_results.size()/3; i < 2*m_results.size()/3; i++) {
			t += m_results[i] - m_overhead;
			n++;
		}

		return (((double)t/(double)n)) * 1000.0 / m_freq.QuadPart;	// convert to ms
	}

private:
 	LARGE_INTEGER m_start, m_stop;
	std::vector <LONGLONG> m_results;
	static LARGE_INTEGER m_freq;
	static LONGLONG m_overhead;
	static DWORD_PTR omask;
	static HANDLE    hThread;

	// Returns the overhead of the timer in ticks
	static LONGLONG GetOverhead() {
		Timer t;

		t.Start();
		t.Stop();
		return t.m_stop.QuadPart - t.m_start.QuadPart;
	}
 };