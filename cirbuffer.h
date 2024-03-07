#pragma once
#include <windows.h>

using namespace std;

#define FS          44100	// Fs


class CircularBuffer {
public:
	CircularBuffer(size_t capacity): capacity_(capacity) {
		data_ = new INT16[capacity+7];	// additional reserve for 128-bit load at circular buffer boundary
		beg_ = data_; end_ = &data_[capacity-1];
		memset(data_, 0, (capacity+7)*sizeof(INT16));
		wr_ = (INT16 *)end_;
	}

	~CircularBuffer() {
		delete [] data_;
	}

	size_t capacity() const { return capacity_; }

	/* write a new sample to the delay line */
	inline void write(INT16 data) {
		if (--wr_ < beg_) {
			wr_ = (INT16 *)end_;
			memcpy((INT16 *)end_+1, beg_, 7*sizeof(INT16));	// ensure that 128-bit read work in wrapround case
		}
	
		*wr_ = data;
	}

	/* read last sample from the delay line */
	inline INT16 read() {
		INT16 *p = wr_-1;

		return (p >= beg_ ? *p : *(INT16 *)end_);
	}

	/* read nth last sample from the delay line */
	inline INT16 readpos(int n) {
		INT16 *p = wr_ + n;

		while (p > end_)
			p -= capacity_;

		return (*p);
	}

protected:
	/* get pointer to the latest inserted sample, previous samples are located after this sample */
	inline INT16 *getPtr() {
		return (wr_);
	}

	/* check the circular buffer ending boundary */
	inline void ptrCheck(INT16* &ptr) {
		if (ptr > end_) ptr = (INT16 *)beg_;
	}

	/* check the circular buffer ending boundary */
	inline void ptrCheck(__m128i* &ptr) {
		if ((INT16 *)ptr > end_) ptr = (__m128i *)((INT16 *)ptr - capacity_);
	}

	/* convert 32-bit integer sample to 16-bit integer sample with saturation */
	inline INT16 saturate(INT32 x) {
		return (x > 32767) ? 32767 : ((x < -32768) ? -32768 : x);
	}

	/* convert floating point sample to 16-bit integer sample with saturation and rounding */
	inline INT16 saturate(double x) {
		if (x >= 0.0)
			if (x > 32767.0)  return (32767);
			else              return ((INT16)(x+0.5));
		else
			if (x < -32768.0) return (-32768);
			else			  return ((INT16)(x-0.5));
	}

	/* convert floating point sample to 16-bit integer sample with saturation and rounding */
	inline INT16 saturate(float x) {
		if (x >= 0.0f)
			if (x > 32767.0f)  return (32767);
			else              return ((INT16)(x+0.5f));
		else
			if (x < -32768.0f) return (-32768);
			else			  return ((INT16)(x-0.5f));
	}

	/* multiply to Q15 numbers */
	inline INT32 mpy(INT16 x, INT16 c) {
		return ((INT32)x * c) >> 15;
	}

private:
	size_t       capacity_;
	INT16       *data_;
	const INT16 *beg_, *end_;
	INT16       *wr_;
};

