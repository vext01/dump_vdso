/*
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
*/

/*
 * Small program to dump the VDSO page to file on Linux systems.
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/auxv.h>
#include <sys/types.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <elf.h>

/* Prototypes */
void usage(char *);
size_t get_vdso_length(char *, int);

void usage(char *name)
{
    fprintf(stderr, "usage: %s <output-file>\n", name);
    exit(EXIT_FAILURE);
}

/*
 * Computes the length of the VDSO shared object.
 *
 * Linux provides no interface to do this, so we have to figure it out
 * ourselves. We do this by taking the maximum end address of the contents of
 * the VDSO.
 */
size_t get_vdso_length(char *vdso_start, int bits)
{
    int phdr_tab_max,
        sec_tab_max,
        num_phdrs,
        num_shdrs,
        phentsize,
        shentsize,
        phoff,
        shoff,
        i;
    off_t offset,
          max_offset = 0;

#define CODE do {							\
    /* Look at the program headers for the segments */			\
    num_phdrs = hdr->e_phnum;						\
    phoff = hdr->e_phoff;						\
    phentsize = hdr->e_phentsize;					\
    for( i = 0; i < num_phdrs; i++ )					\
    {									\
        phdr = (void *)((char *)vdso_start + phoff + i * phentsize);	\
        offset = phdr->p_offset + phdr->p_filesz;			\
        if(max_offset < offset)						\
           max_offset = offset;						\
    }									\
									\
    /* Look at the sections */						\
    num_shdrs = hdr->e_shnum;						\
    shentsize = hdr->e_shentsize;					\
    shoff = hdr->e_shoff;						\
    for( i = 0; i < num_shdrs; i++ )					\
    {									\
        shdr = (void *)((char *)vdso_start + shoff + i * shentsize);	\
        offset = shdr->sh_offset + shdr->sh_size;			\
        if(max_offset < offset)						\
           max_offset = offset;						\
    }									\
									\
    /* The section table */						\
    sec_tab_max = shoff + num_shdrs * shentsize;			\
    if(max_offset < sec_tab_max)					\
       max_offset = sec_tab_max;					\
									\
    /* Program header table */						\
    phdr_tab_max = phoff + num_phdrs * phentsize;			\
    if(max_offset < phdr_tab_max)					\
       max_offset = phdr_tab_max;					\
} while(0)

    if(bits == 32)
    {
        Elf32_Ehdr *hdr = (void *)vdso_start;
        Elf32_Phdr *phdr;
        Elf32_Shdr *shdr;
        CODE;
    }
    else if(bits == 64)
    {
        Elf64_Ehdr *hdr = (void *)vdso_start;
        Elf64_Phdr *phdr;
        Elf64_Shdr *shdr;
        CODE;
    }
    return max_offset;
}

int main(int argc, char **argv)
{
    Elf32_Ehdr *hdr;
    void *vdso_start;
    int bits,
        ret = EXIT_SUCCESS;
    FILE *fh = NULL;
    size_t n_wrote,
           vdso_len;

    if(argc < 2)
    {
        usage(argv[0]);
    }

    /* Get the start virtual address of the VDSO */
    hdr = vdso_start = (void *)getauxval(AT_SYSINFO_EHDR);
    if(memcmp(hdr->e_ident, ELFMAG, SELFMAG))
    {
        fprintf(stderr, "elf magic bad");
        ret = EXIT_FAILURE;
        goto clean;
    }

    if(!(bits = hdr->e_ident[EI_CLASS] * 32))
    {
        ret = EXIT_FAILURE;
        goto clean;
    }

    vdso_len = get_vdso_length(vdso_start, bits);

    /* Now it's just a matter of putting the VDSO to disk. */
    fh = fopen(argv[1], "w");
    if(!fh)
    {
        perror("fopen");
        ret = EXIT_FAILURE;
        goto clean;
    }

    n_wrote = fwrite(vdso_start, 1, vdso_len, fh);
    if(n_wrote != vdso_len)
    {
        fprintf(stderr, "fwrite failed\n");
        ret = EXIT_FAILURE;
    }

clean:
    if(fh)
    {
        fclose(fh);
    }

    return ret;
}
