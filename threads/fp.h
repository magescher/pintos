#ifndef THREADS_FP_H
#define THREADS_FP_H

#include <lib/limits.h>
#define F (1 << 14)

static inline int i2f     (int x) { return x*F; }
static inline int f2i     (int x) { return x/F; }
static inline int f2i_rnd (int x) { return (x<0)?(x-F/2)/F:(x+F/2)/F; }
static inline int fp_add  (int x, int y) { return x+y; }
static inline int fp_addi (int x, int n) { return x+i2f(n); }
static inline int fp_sub  (int x, int y) { return x-y; }
static inline int fp_subi (int x, int n) { return x-i2f(n); }
static inline int fp_mul  (int x, int y) { return ((int64_t)x)*y/F; }
static inline int fp_muli (int x, int n) { return x*n; }
static inline int fp_div  (int x, int y) { return ((int64_t)x)*F/y; }
static inline int fp_divi (int x, int n) { return x/n; }

#endif /* THREADS_FP_H */
