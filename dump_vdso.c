// Copyright (c) 2018 King's College London
// created by the Software Development Team <http://soft-dev.org/> and
// Davin McCall.
//
// The Universal Permissive License (UPL), Version 1.0
//
// Subject to the condition set forth below, permission is hereby granted to any
// person obtaining a copy of this software, associated documentation and/or
// data (collectively the "Software"), free of charge and under any and all
// copyright rights in the Software, and any and all patent rights owned or
// freely licensable by each licensor hereunder covering either (i) the
// unmodified Software as contributed to or provided by such licensor, or (ii)
// the Larger Works (as defined below), to deal in both
//
// (a) the Software, and
// (b) any piece of software and/or hardware listed in the lrgrwrks.txt file
// if one is included with the Software (each a "Larger Work" to which the Software
// is contributed by such licensors),
//
// without restriction, including without limitation the rights to copy, create
// derivative works of, display, perform, and distribute the Software and make,
// use, sell, offer for sale, import, export, have made, and have sold the
// Software and the Larger Work(s), and to sublicense the foregoing rights on
// either these or other terms.
//
// This license is subject to the following condition: The above copyright
// notice and either this complete permission notice or at a minimum a reference
// to the UPL must be included in all copies or substantial portions of the
// Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

/*
 * Small program to dump the VDSO page to file on Linux systems.
 *
 * Assumes x86-64.
 */

#define _GNU_SOURCE

#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <err.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>

#define ELF_MAGIC 0x464C457F

// Protos.
void usage(void);
long get_vdso_length(unsigned long);

void
usage(void)
{
    fprintf(stderr, "usage: dump_vdso <output-file>\n");
    exit(EXIT_FAILURE);
}

/*
 * Computes the length of the VDSO shared object.
 *
 * Linux provides no interface to do this, so we have to figure it out
 * ourselves. We do this by taking the maximum end address of the contents of
 * the VDSO.
 */
long
get_vdso_length(unsigned long vdso_start) {
    Elf64_Ehdr *hdr = (Elf64_Ehdr *) vdso_start;
    long max_offset = 0;

    // Look at the program headers for the segments.
    int num_phdrs =  hdr->e_phnum;
    int phoff = hdr->e_phoff;
    int phentsize = hdr->e_phentsize;
    for (int i = 0; i < num_phdrs; i++) {
        Elf64_Phdr *phdr =
            (Elf64_Phdr *) (vdso_start + phoff + i * phentsize);
        long offset = phdr->p_offset + phdr->p_filesz;
        if (max_offset < offset) {
            max_offset = offset;
        }
    }

    // Look at the sections.
    int num_shdrs = hdr->e_shnum;
    int shentsize = hdr->e_shentsize;
    int shoff = hdr->e_shoff;
    for (int i = 0; i < num_shdrs; i++) {
        Elf64_Shdr *shdr =
            (Elf64_Shdr *) (vdso_start + shoff + i * shentsize);
        long offset = shdr->sh_offset + shdr->sh_size;
        if (max_offset < offset) {
            max_offset = offset;
        }
    }

    // The section table.
    int sec_tab_max = shoff + num_shdrs * shentsize;
    if (max_offset < sec_tab_max) {
        max_offset = sec_tab_max;
    }

    // Program header table.
    int phdr_tab_max = phoff + num_phdrs * phentsize;
    if (max_offset < phdr_tab_max) {
        max_offset = sec_tab_max;
    }

    return max_offset;
}

#if defined(__linux__) && defined(__x86_64__)
int main(int argc, char **argv)
{
    int ret = EXIT_SUCCESS;
    FILE *fh = NULL;

    if (argc != 2) {
        usage();
    }

    // Get the start virtual address of the VDSO.
    unsigned long vdso_start = getauxval(AT_SYSINFO_EHDR);
    uint32_t *elf_magic = (uint32_t *) vdso_start;
    if (*elf_magic != 0x464C457F) {
        fprintf(stderr, "elf magic bad");
        ret = EXIT_FAILURE;
        goto clean;
    }

    long vdso_len = get_vdso_length(vdso_start);

    // Now it's just a matter of putting the VDSO to disk.
    fh = fopen(argv[1], "wb");
    if (fh == NULL) {
        fprintf(stderr, "fopen: %s\n", strerror(errno));
        ret = EXIT_FAILURE;
        goto clean;
    }

    int n_wrote = fwrite((void *) vdso_start, 1, vdso_len, fh);
    if (n_wrote != vdso_len) {
        fprintf(stderr, "fwrite failed\n");
        ret = EXIT_FAILURE;
        goto clean;
    }

clean:
    if (fh) {
        fclose(fh);
    }

    return ret;
}
#else
int
main(int argc, char **argv)
{
    (void) argc;
    (void) argv;

    fprintf(stderr, "This is a Linux/X86_64 utility\n");
    exit(EXIT_FAILURE);
}
#endif
