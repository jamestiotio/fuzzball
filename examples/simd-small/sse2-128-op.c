#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <errno.h>
#include <sys/mman.h>

/* Sample compilation commands: */

/* gcc-9 -m64 -Wall -g sse2-128-op.c -o sse2-128-op */

/* gcc-9 -I/usr/include/x86_64-linux-gnu -m32 -msse2 -Wall -g sse2-128-op.c -o sse2-128-op-32 */

/* Sample usage:
   ./sse2-128-op 0x67 0x00020003000400050006000700080009 \
                      0x001000200040008000f000ff8000ffff
*/

typedef unsigned char uchar_x16 __attribute__ ((vector_size (16)));

/* The memory region used for generated code. code_buf points within
   code_area, and is page-aligned. */
unsigned char code_area[8192];
unsigned char *code_buf;

int parse_hex_digit(char c) {
    if (c >= '0' && c <= '9') {
	return c - '0';
    } else if (c >= 'a' && c <= 'f') {
	return c - 'a' + 10;
    } else if (c >= 'A' && c <= 'F') {
	return c - 'A' + 10;
    } else {
	fprintf(stderr, "Illegal character '%c' in hex\n", c);
	exit(1);
    }
}

uchar_x16 parse_128(const char *s) {
    uchar_x16 r;
    int pos, i;
    if (s[0] == '0' && s[1] == 'x')
	s += 2;
    i = 0;
    for (pos = 15; pos >= 0; pos--) {
	unsigned char byte;
	if (!s[i]) {
	    fprintf(stderr, "Too few hex digits\n");
	    exit(1);
	}
	while (s[i] == ' ' || s[i] == '_')
	    i++;
	byte = parse_hex_digit(s[i]) << 4;
	byte |= parse_hex_digit(s[i + 1]);
	r[pos] = byte;
	i += 2;
    }
    if (s[i] != '\0') {
	fprintf(stderr, "Too many hex digits\n");
	exit(1);
    }
    return r;
}

void print_128(uchar_x16 x) {
    int pos;
    for (pos = 15; pos >= 0; pos--) {
	printf("%02x", x[pos]);
    }
}

int main(int argc, char **argv) {
    long opcode;
    uchar_x16 arg1, arg2, result;
    unsigned char *cp;
    int res;
    if (argc != 4) {
	fprintf(stderr, "Usage: sse2-128-op <opcode> <arg1hex> <arg2hex>\n");
	return 1;
    }

    opcode = strtol(argv[1], 0, 0);
    arg1 = parse_128(argv[2]);
    arg2 = parse_128(argv[3]);

    if (opcode < 0 || opcode > 0xff) {
	fprintf(stderr, "Opcode 0x%lx out of range\n", opcode);
	exit(1);
    }

    /* Set up a page of memory that is both writable and executable to
       hold the generated code, as an exception to W xor X. */
    code_buf = (unsigned char *)((4095 + (unsigned long)code_area) & ~0xfff);
    res = mprotect(code_buf, 4096, PROT_READ|PROT_WRITE|PROT_EXEC);
    if (res != 0) {
        fprintf(stderr, "mprotect() failed: %s\n", strerror(errno));
        return 1;
    }

    cp = code_buf;
    *cp++ = 0x66; /* 0x66 prefix generally means 128-bit SSE2 */
    *cp++ = 0x0f; /* 0x0f is first byte of a two-byte opcode */
    *cp++ = opcode;
    *cp++ = 0xca; /* mod/rm: dest = %xmm1, src = %xmm2 */
    *cp++ = 0xc3; /* return */

    asm ("movdqa %[a1],%%xmm1; movdqa %[a2],%%xmm2; "
	 "call *%[code]; movdqa %%xmm1, %[res]"
	 : [res] "=x" (result)
	 : [a1] "x" (arg1), [a2] "x" (arg2), [code] "r" (code_buf)
	 : "xmm1", "xmm2");

    printf("0x");
    print_128(result);
    printf("\n");
    return 0;
}
