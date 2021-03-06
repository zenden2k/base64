#include <stddef.h>
#ifdef __AVX2__
#include <immintrin.h>
#endif

#include "../include/libbase64.h"
#include "codecs.h"

BASE64_ENC_FUNCTION(avx2)
{
#ifdef __AVX2__
	#include "enc/head.c"
	#include "enc/avx2.c"
	#include "enc/tail.c"
#else
	BASE64_ENC_STUB
#endif
}

BASE64_DEC_FUNCTION(avx2)
{
#ifdef __AVX2__
	#include "dec/head.c"
	#include "dec/avx2.c"
	#include "dec/tail.c"
#else
	BASE64_DEC_STUB
#endif
}
