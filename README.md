# Fast Base64 stream encoder/decoder

This is an implementation of a base64 stream encoding/decoding library in C89
with SIMD (AVX2, NEON, AArch64/NEON, SSSE3) acceleration. It also contains
wrapper functions to encode/decode simple length-delimited strings. This
library aims to be:

- FAST;
- easy to use;
- elegant.

On x86, the library does runtime feature detection. The first time it's called,
the library will determine the appropriate encoding/decoding routines for the
machine. It then remembers them for the lifetime of the program. If your
processor supports AVX2 or SSSE3 instructions, the library will pick an
optimized codec that lets it encode/decode 12 or 24 bytes at a time, which
gives a speedup of four or more times compared to the "plain" bytewise codec.

NEON support is hardcoded to on or off at compile time, because portable
runtime feature detection is unavailable on ARM.

Even if your processor does not support SIMD instructions, this is a very fast
library. The fallback routine can process 32 or 64 bits of input in one round,
depending on your processor's word width, which still makes it significantly
faster than naive bytewise implementations. On some 64-bit machines, the 64-bit
routines even outperform the SSSE3 ones.

To the author's knowledge, at the time of original release, this was the only
Base64 library to offer SIMD acceleration. The author wrote
[an article](http://www.alfredklomp.com/programming/sse-base64) explaining one
possible SIMD approach to encoding/decoding Base64. The article can help figure
out what the code is doing, and why.

The original AVX2, NEON and Aarch64/NEON codecs were generously contributed by
[Inkymail](https://github.com/inkymail/base64), who, in their fork, also
implemented some additional features. Their work is slowly being backported
into this project.

Notable features:

- Really fast on x86 and ARM systems by using SIMD vector processing;
- Really fast on other 32 or 64-bit platforms through optimized routines;
- Reads/writes blocks of streaming data;
- Does not dynamically allocate memory;
- Valid C89 that compiles with pedantic options on;
- Re-entrant and threadsafe;
- Unit tested;
- Uses Duff's Device.

## Building

The `lib` directory contains the code for the actual library.
Typing `make` in the toplevel directory will build `lib/libbase64.o` and `bin/base64`.
The first is a single, self-contained object file that you can link into your own project.
The second is a standalone test binary that works similarly to the `base64` system utility.

The matching header file needed to use this library is in `include/libbase64.h`.

To compile just the "plain" library without SIMD codecs, type:

```sh
make lib/libbase64.o
```

Optional SIMD codecs can be included by specifying the `AVX2_CFLAGS`, `NEON32_CFLAGS`, `NEON64_CFLAGS` and/or `SSSE3_CFLAGS` environment variables.
A typical build invocation on x86 looks like this:

```sh
AVX2_CFLAGS=-mavx2 SSSE3_CFLAGS=-mssse3 make lib/libbase64.o
```

### AVX2

To build and include the AVX2 codec, set the `AVX2_CFLAGS` environment variable to a value that will turn on AVX2 support in your compiler, typically `-mavx2`.
Example:

```sh
AVX2_CFLAGS=-mavx2 make
```

The codec will only be used if runtime feature detection shows that the target machine supports AVX2.

### SSSE3

To build and include the SSSE3 codec, set the `SSSE3_CFLAGS` environment variable to a value that will turn on SSSE3 support in your compiler, typically `-mssse3`.
Example:

```sh
SSSE3_CFLAGS=-mssse3 make
```

The codec will only be used if runtime feature detection shows that the target machine supports SSSE3.

### NEON

This library includes two NEON codecs: one for regular 32-bit ARM and one for the 64-bit AArch64 with NEON, which has double the amount of SIMD registers and can do full 64-byte table lookups.
These codecs encode in 48-byte chunks and decode in massive 64-byte chunks, so they had to be augmented with an uint32/64 codec to stay fast on smaller inputs!

Use LLVM/Clang for compiling the NEON codecs.
The code generation of at least GCC 4.6 (the version shipped with Raspbian and used for testing) contains a bug when compiling `vstq4_u8()`, and the generated assembly code is of low quality.
NEON intrinsics are a known weak area of GCC.
Clang does a better job.

NEON support can unfortunately not be portably detected at runtime from userland (the `mrc` instruction is privileged), so the default value for using the NEON codec is determined at compile-time.
But you can do your own runtime detection.
You can include the NEON codec and make it the default, then do a runtime check if the CPU has NEON support, and if not, force a downgrade to non-NEON with `BASE64_FORCE_PLAIN`.

These are your options:

1. Don't include NEON support;
2. build NEON support and make it the default, but build all other code without NEON flags so that you can override the default at runtime with `BASE64_FORCE_PLAIN`;
3. build everything with NEON support and make it the default;
4. build everything with NEON support, but don't make it the default (which makes no sense).

For option 1, simply don't specify any NEON-specific compiler flags at all, like so:

```sh
CC=clang CFLAGS="-march=armv6" make
```

For option 2, keep your `CFLAGS` plain, but set the `NEON32_CFLAGS` environment variable to a value that will build NEON support.
The line below, for instance, will build all the code at ARMv6 level, except for the NEON codec, which is built at ARMv7.
It will also make the NEON codec the default.
For ARMv6 platforms, override that default at runtime with the `BASE64_FORCE_PLAIN` flag.
No ARMv7/NEON code will then be touched.

```sh
CC=clang CFLAGS="-march=armv6" NEON32_CFLAGS="-march=armv7 -mfpu=neon" make
```

For option 3, put everything in your `CFLAGS` and use a stub, but non-empty, `NEON32_CFLAGS`.
This example works for the Raspberry Pi 2, which has NEON support:

```sh
CC=clang CFLAGS="-march=armv7 -mtune=cortex-a7" NEON32_CFLAGS=" " make
```

To build and include the NEON64 codec, use `CFLAGS` as usual to define the platform and set `NEON64_CFLAGS` to a nonempty stub.
(The AArch64 target has mandatory NEON64 support.)
Example:

```sh
CC=clang CFLAGS="--target=aarch64-linux-gnu -march=armv8-a" NEON64_CFLAGS=" " make
```

## API reference

Strings are represented as a pointer and a length; they are not
zero-terminated. This was a conscious design decision. In the decoding step,
relying on zero-termination would make no sense since the output could contain
legitimate zero bytes. In the encoding step, returning the length saves the
overhead of calling `strlen()` on the output. If you insist on the trailing
zero, you can easily add it yourself at the given offset.

### Flags

Some API calls take a `flags` argument.
That argument can be used to force the use of a specific codec, even if that codec is a no-op in the current build.
Mainly there for testing purposes, this is also useful on ARM where the only way to do runtime NEON detection is to ask the OS if it's available.
The following constants can be used:

- `BASE64_FORCE_AVX2`
- `BASE64_FORCE_NEON32`
- `BASE64_FORCE_NEON64`
- `BASE64_FORCE_PLAIN`
- `BASE64_FORCE_SSSE3`

Set `flags` to `0` for the default behavior, which is runtime feature detection on x86, a compile-time fixed codec on ARM, and the plain codec on other platforms.

### Encoding

#### base64_encode

```c
void base64_encode
    ( const char  *src
    , size_t       srclen
    , char        *out
    , size_t      *outlen
    , int          flags
    ) ;
```

Wrapper function to encode a plain string of given length.
Output is written to `out` without trailing zero.
Output length in bytes is written to `outlen`.
The buffer in `out` has been allocated by the caller and is at least 4/3 the size of the input.

#### base64_stream_encode_init

```c
void base64_stream_encode_init
    ( struct base64_state  *state
    , int                   flags
    ) ;
```

Call this before calling `base64_stream_encode()` to init the state.

#### base64_stream_encode

```c
void base64_stream_encode
    ( struct base64_state  *state
    , const char           *src
    , size_t                srclen
    , char                 *out
    , size_t               *outlen
    ) ;
```

Encodes the block of data of given length at `src`, into the buffer at `out`.
Caller is responsible for allocating a large enough out-buffer; it must be at least 4/3 the size of the in-buffer, but take some margin.
Places the number of new bytes written into `outlen` (which is set to zero when the function starts).
Does not zero-terminate or finalize the output.

#### base64_stream_encode_final

```c
void base64_stream_encode_final
    ( struct base64_state  *state
    , char                 *out
    , size_t               *outlen
    ) ;
```

Finalizes the output begun by previous calls to `base64_stream_encode()`.
Adds the required end-of-stream markers if appropriate.
`outlen` is modified and will contain the number of new bytes written at `out` (which will quite often be zero).

### Decoding

#### base64_decode

```c
int base64_decode
    ( const char  *src
    , size_t       srclen
    , char        *out
    , size_t      *outlen
    , int          flags
    ) ;
```

Wrapper function to decode a plain string of given length.
Output is written to `out` without trailing zero. Output length in bytes is written to `outlen`.
The buffer in `out` has been allocated by the caller and is at least 3/4 the size of the input.

#### base64_stream_decode_init

```c
void base64_stream_decode_init
    ( struct base64_state  *state
    , int                   flags
    ) ;
```

Call this before calling `base64_stream_decode()` to init the state.

#### base64_stream_decode

```c
int base64_stream_decode
    ( struct base64_state  *state
    , const char           *src
    , size_t                srclen
    , char                 *out
    , size_t               *outlen
    ) ;
```

Decodes the block of data of given length at `src`, into the buffer at `out`.
Caller is responsible for allocating a large enough out-buffer; it must be at least 3/4 the size of the in-buffer, but take some margin.
Places the number of new bytes written into `outlen` (which is set to zero when the function starts).
Does not zero-terminate the output.
Returns 1 if all is well, and 0 if a decoding error was found, such as an invalid character.
Returns -1 if the chosen codec is not included in the current build.
Used by the test harness to check whether a codec is available for testing.

## Examples

A simple example of encoding a static string to base64 and printing the output
to stdout:

```c
#include <stdio.h>	/* fwrite */
#include "libbase64.h"

int main ()
{
	char src[] = "hello world";
	char out[20];
	size_t srclen = sizeof(src) - 1;
	size_t outlen;

	base64_encode(src, srclen, out, &outlen, 0);

	fwrite(out, outlen, 1, stdout);

	return 0;
}
```

A simple example (no error checking, etc) of stream encoding standard input to
standard output:

```c
#include <stdio.h>
#include "libbase64.h"

int main ()
{
	size_t nread, nout;
	char buf[12000], out[16000];
	struct base64_state state;

	base64_stream_encode_init(&state, 0);
	while ((nread = fread(buf, 1, sizeof(buf), stdin)) > 0) {
		base64_stream_encode(&state, buf, nread, out, &nout);
		if (nout) fwrite(out, nout, 1, stdout);
		if (feof(stdin)) break;
	}
	base64_stream_encode_final(&state, out, &nout);
	if (nout) fwrite(out, nout, 1, stdout);

	return 0;
}
```

Also see `bin/base64.c` for a simple re-implementation of the `base64` utility.
A file or standard input is fed through the encoder/decoder, and the output is
written to standard output.

## Tests

See `tests/` for a small test suite. Testing is automated with [Travis CI](https://travis-ci.org/aklomp/base64):

[![Build Status](https://travis-ci.org/aklomp/base64.png?branch=master)](https://travis-ci.org/aklomp/base64)

## Benchmarks

Benchmarks can be run with the built-in benchmark program as follows:

```sh
make -C test benchmark <buildflags> && test/benchmark
```

It will run an encoding and decoding benchmark for all of the compiled-in codecs.

The table below contains some results on machines of mine. All numbers are in MB/sec, rounded to the nearest integer.

| Processor             | Plain enc | Plain dec | SSSE3 enc | SSSE3 dec | AVX2 enc | AVX2 dec | NEON enc | NEON dec |
|-----------------------|----------:|----------:|----------:|----------:|---------:|---------:|---------:|---------:|
| i5-4590S @ 3.0 GHz    | 1719      | 1643      | 2422      | 3033      | 3647     | 4879     | -        | -        |
| Xeon X5570 @ 2.93 GHz | 1037      | 1047      | 1558      | 2173      | -        | -        | -        | -        |
| Pentium4 @ 3.4 GHz    | 528       | 448       | -         | -         | -        | -        | -        | -        |
| Atom N270             | 124       | 122       | 288       | 350       | -        | -        | -        | -        |
| AMD E-450             | 360       | 333       | 294       | 332       | -        | -        | -        | -        |
| Raspberry PI B+       | 46        | 40        | -         | -         | -        | -        | -        | -        |
| Raspberry PI 2 B      | 39        | 35        | -         | -         | -        | -        | 7        | 27       |

## License

This repository is licensed under the
[BSD 2-clause License](http://opensource.org/licenses/BSD-2-Clause). See the
LICENSE file.
