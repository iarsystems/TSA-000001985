/* Make sure to enable C11 */
/* In MSVC make sure that _CRT_SECURE_NO_WARNINGS is defined */

#include <stdint.h>
#include <stdnoreturn.h> /* remove if it does not exist, if so fix the noreturn */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ********************************************************************** */
/*  defines                                                               */
/* ********************************************************************** */

#define VERSION "1.0"

/* Various ELF constants needed to dig out the lz77 entries */
#define EI_NIDENT       16
#define ELFCLASS32      1 /* 32-bit objects */
#define ELFCLASS64      2 /* 64-bit objects */
#define EI_MAG0         0 /* File identification */
#define EI_MAG1         1 /* File identification */
#define EI_MAG2         2 /* File identification */
#define EI_MAG3         3 /* File identification */
#define EI_CLASS        4
#define ELFMAG0         0x7f /* e_ident[EI_MAG0] */
#define ELFMAG1         'E'  /* e_ident[EI_MAG1] */
#define ELFMAG2         'L'  /* e_ident[EI_MAG2] */
#define ELFMAG3         'F'  /* e_ident[EI_MAG3] */

/* ********************************************************************** */
/*  types                                                                 */
/* ********************************************************************** */

/* for reading 32-bit ELF */

typedef uint32_t Elf32_Off;
typedef uint32_t Elf32_Word;
typedef int32_t  Elf32_Sword;
typedef uint16_t Elf32_Half;
typedef uint8_t  Elf32_Small;
typedef uint32_t Elf32_Addr;

typedef struct elf32_header
{
    unsigned char   e_ident[EI_NIDENT];
    Elf32_Half      e_type;
    Elf32_Half      e_machine;
    Elf32_Word      e_version;
    Elf32_Addr      e_entry;
    Elf32_Off       e_phoff;
    Elf32_Off       e_shoff;
    Elf32_Word      e_flags;
    Elf32_Half      e_ehsize;
    Elf32_Half      e_phentsize;
    Elf32_Half      e_phnum;
    Elf32_Half      e_shentsize;
    Elf32_Half      e_shnum;
    Elf32_Half      e_shstrndx;
} elf32_header;

typedef struct elf32_segment
{
    Elf32_Word	   p_type;
    Elf32_Off      p_offset;
    Elf32_Addr	   p_vaddr;
    Elf32_Addr	   p_paddr;
    Elf32_Word	   p_filesz;
    Elf32_Word	   p_memsz;
    Elf32_Word	   p_flags;
    Elf32_Word	   p_align;
} elf32_segment;

/* for reading 64-bit ELF */

typedef uint64_t Elf64_Addr;
typedef int64_t  Elf64_Off;
typedef uint16_t Elf64_Half;
typedef uint32_t Elf64_Word;
typedef int32_t  Elf64_Sword;
typedef uint64_t Elf64_Xword;
typedef int64_t  Elf64_Sxword;

typedef struct elf64_header
{
    unsigned char   e_ident[EI_NIDENT];
    Elf64_Half      e_type;
    Elf64_Half      e_machine;
    Elf64_Word      e_version;
    Elf64_Addr      e_entry;
    Elf64_Off       e_phoff;
    Elf64_Off       e_shoff;
    Elf64_Word      e_flags;
    Elf64_Half      e_ehsize;
    Elf64_Half      e_phentsize;
    Elf64_Half      e_phnum;
    Elf64_Half      e_shentsize;
    Elf64_Half      e_shnum;
    Elf64_Half      e_shstrndx;
} elf64_header;

typedef struct elf64_segment
{
    Elf64_Word	   p_type;
    Elf64_Word     p_flags;
    Elf64_Off      p_offset;
    Elf64_Addr	   p_vaddr;
    Elf64_Addr	   p_paddr;
    Elf64_Xword	   p_filesz;
    Elf64_Xword	   p_memsz;
    Elf64_Xword	   p_align;
} elf64_segment;

/* Stores the places that contain lz77 entries */
typedef struct Range
{
  uint64_t       start;
  uint64_t       size;
} Range;

/* The result of parsing the command line */
typedef struct ParseResults
{
  int          isVerbose;  /* verbose output */
  uint32_t     numRanges;  /* number of ranges */
  Range *      ranges;     /* ranges */
  const char * fileName;   /* the name of the ELF-file */
  FILE *       fp;         /* pointer to ELF-file */
  uint64_t     expected;   /* expected size of output */
  uint64_t     compressed; /* number of compressed bytes in the ranges */
} ParseResults;

typedef struct lz77
{
  uint8_t*   bytes;        /* lz77 entries */
  uint64_t   compressed;   /* length of entrioes */
  uint64_t   decompressed; /* expected size of decompressed */
  uint64_t   dest;         /* decompressed bytes */
} lz77;

/* ********************************************************************** */
/*  forward declarations                                                  */
/* ********************************************************************** */

int check_lz77_sequence(lz77 const* lz77_info, char verbose);

/* ********************************************************************** */
/*  functions                                                             */
/* ********************************************************************** */

/* called when malloc fails, aborts */

/* remove noreturn (introduced in C11) if you get warnings/errors here
   out_of_memory guards against null pointers, so you might get
   warnings regarding possible use of null if you remove it
*/

noreturn
void out_of_memory(void)
{
  printf("out of memory");
  abort();
}

/* reads the ranges from the arguments to the program */
void GetRanges(char const * input, ParseResults * pr)
{
  /* count number of ranges */
  int i = 0, commas = 0;
  uint8_t c;
  while ((c = input[i]))
  {
    if (c == ',')
      ++commas;
    ++i;
  }
  /* allocate memory for the ranges */
  pr->numRanges = commas+1;
  pr->ranges = (Range *)malloc(sizeof(Range)*pr->numRanges);
  if (!pr->ranges) out_of_memory();

  /* read a range numRanges times */
  uint64_t compressed = 0;
  char * tmp;
  for (uint32_t j = 0 ; j < pr->numRanges ; ++j)
  {
    /* start address */
    uint64_t start = strtoull(input, &tmp, 16);
    /* check that we get : next */
    if (*tmp != ':')
    {
      char c = tmp ? *tmp : 0;
      printf("expected ':', got %c\n", c);
      abort();
    }
    ++tmp; /* skip : */
    /* size */
    uint64_t size = strtoull(tmp, &tmp, 16);
    if (size == 0)
    {
      printf("range size cannot be 0\n");
      abort();
    }
    /* check that we get , or \0 next */
    if ((*tmp != ',') && (*tmp != 0))
    {
      printf("unxpected token '%c'\n", *tmp);
      abort();
    }
    ++tmp;
    input = tmp;
    /* create entry */
    pr->ranges[j].start = start;
    pr->ranges[j].size  = size;

    compressed += size;
  }
  /* keep track of the total number of bytes */
  pr->compressed = compressed;
}

/* for debug and verification purposes */
void
print_range(Range const * rng, uint32_t numRanges)
{
  for (uint32_t i = 0 ; i < numRanges ; ++i)
  {
    printf("%llX+%llX", rng[i].start, rng[i].size);
    if (i+1 < numRanges)
      printf(",");
  }
}

/* parse the command line */
void ParseResult(int argc, char * argv[], ParseResults * pr)
{
  /* hardwired positions
     [0] is the name of the appliation
     [1] is the name of the file
     [2] is the ranges
     [3] is the expected value
     [4] (optional) is verbose
  */

  /* Get the ELF-file name, argv[1]*/
  pr->fileName = argv[1];
  pr->fp = fopen(argv[1], "rb");
  if (!pr->fp)
  {
    printf("Unable to open %s\n", argv[1]);
    abort();
  }

  /* Get the ranges, argv[2] */
  GetRanges(argv[2], pr);

  /* Get expected, argv[3] */
  char * endCheck;
  pr->expected = strtoull(argv[3], &endCheck, 16);
  if (*endCheck)
  {
    printf("unexpected expected-suffix %c\n", *endCheck);
    abort();
  }

  /* Check optional verbose, argv[4] */
  pr->isVerbose = 0;
  if (argc >= 5)
  {
    if (   strcmp(argv[4], "verbose") == 0
        || *argv[4] == 'v')
    {
      pr->isVerbose = 1;
    }
    else
    {
      printf("Unknown option %s\n", argv[4]);
      abort();
    }
  }
  if (argc >= 6)
  {
    printf("Too many parameters %d\n", argc);
    abort();
  }
  /* do verbose output if requested */
  if (pr->isVerbose)
  {
    printf("Parsing done.\n");
    printf("File     = \"%s\"\n", pr->fileName);
    printf("Range    = ");
    print_range(pr->ranges, pr->numRanges);
    printf("\n");
    printf("Expected = %llX\n", pr->expected);
    printf("Verbose  = true\n");
  }
}

/* true iff the Range fits inside the segment boundaries */
int
contained_in_seg(uint64_t segStart, uint64_t segSize, Range const * r)
{
  uint64_t addr = r->start;
  return (addr >= segStart) &&
    ((addr + r->size) <= (segStart + segSize));
}

/* returns the elf32_segment that contains the range */ 
static
elf32_segment const *
find_segment32(elf32_segment const * segs, uint32_t numSegs,
               Range const * r)
{
  /* check each segment */
  for (unsigned int i = 0; i < numSegs; ++i)
  {
    elf32_segment const* s = &segs[i];
    if (contained_in_seg(s->p_paddr, s->p_filesz, r))
      return s;
  }
  printf("The range 0x%llX+0x%llX was not found in any segment\n",
         r->start, r->size);
  abort();
}

/* returns the elf64_segment that contains the range */

static
elf64_segment const *
find_segment64(elf64_segment const * segs, uint64_t numSegs,
               Range const * r)
{
  /* check each segment */
  for (unsigned int i = 0; i < numSegs; ++i)
  {
    elf64_segment const* s = &segs[i];
    if (contained_in_seg(s->p_paddr, s->p_filesz, r))
      return s;
  }
  printf("The range 0x%llX+0x%llX was not found in any segment\n",
         r->start, r->size);
  abort();
}

/* fill the lz77 struct */
static
void
fill_lz77_object(lz77 * l, ParseResults const * pr)
{
  l->bytes = (uint8_t *)malloc(pr->compressed);
  if (!l->bytes) out_of_memory();
  l->compressed   = pr->compressed;
  l->decompressed = pr->expected;
  l->dest = 0;
}

/* read bytes into range */
static
void
read_range_bytes(FILE * fp, uint32_t offset, uint32_t size, uint8_t * bytes)
{
  /* reposition file */
  fseek(fp, offset, SEEK_SET);
  /* read */
  int read = (int)fread(bytes, 1, size, fp);
  /* check read bytes */
  if (read != size)
  {
    printf("Could not read %lX bytes from file, only read %X", size, read);
    abort();
  }
}

/* a non-zero return means a problem was found */
static
int Check32File(ParseResults const * pr)
{
  /* reset file */
  FILE * fp = pr->fp;
  fseek(fp, 0, SEEK_SET);

  elf32_header hdr;

  /* read header */
  fread(&hdr, 1, sizeof(elf32_header), fp);

  /* read segments */
  uint32_t numSegs = hdr.e_phnum;
  elf32_segment * segs = (elf32_segment*)malloc(sizeof(elf32_segment) * numSegs);
  if (!segs) out_of_memory();
  fseek(fp, hdr.e_phoff, SEEK_SET);
  fread(segs, sizeof(elf32_segment), numSegs, fp);

  /* create the lz77 object */
  lz77 l;
  fill_lz77_object(&l, pr);

  /* fill lz77 bytes */
  uint32_t src = 0;
  for (uint32_t i = 0 ; i < pr->numRanges ; ++i)
  {
    Range const * r = &pr->ranges[i];
    elf32_segment const * seg = find_segment32(segs, numSegs, r);
    uint32_t file_offset = ((uint32_t)r->start - seg->p_vaddr) + seg->p_offset;
    uint32_t size = (uint32_t)r->size;
    read_range_bytes(fp, file_offset, size, &l.bytes[src]);
    src += size;
  }
  if (src != l.compressed)
  {
    printf("read %lX bytes, expected %llX bytes\n", src, l.compressed);
    abort();
  }
  /* check the lz77 sequence */
  int ok = check_lz77_sequence(&l, pr->isVerbose);

  /* free allocated memory */
  free (segs);
  free (l.bytes);

  return ok;
}

/* a non-zero return means a problem was found */
static
int Check64File(ParseResults const * pr)
{
  /* reset file */
  FILE * fp = pr->fp;
  fseek(fp, 0, SEEK_SET);

  elf64_header hdr;

  /* read header */
  fread(&hdr, 1, sizeof(elf64_header), fp);

  /* read segments */
  uint64_t numSegs = hdr.e_phnum;
  elf64_segment * segs = (elf64_segment*)malloc(sizeof(elf64_segment) * numSegs);
  if (!segs) out_of_memory();
  fseek(fp, (uint32_t)hdr.e_phoff, SEEK_SET);
  fread(segs, sizeof(elf64_segment), numSegs, fp);

  /* create the lz77 object */
  lz77 l;
  fill_lz77_object(&l, pr);

  /* fill lz77 bytes */
  uint32_t src = 0;
  for (uint32_t i = 0 ; i < pr->numRanges ; ++i)
  {
    Range const* r = &pr->ranges[i];
    elf64_segment const * seg = find_segment64(segs, numSegs, r);
    uint32_t file_offset = (uint32_t)((r->start - seg->p_vaddr) + seg->p_offset);
    uint32_t size = (uint32_t)r->size;
    read_range_bytes(fp, file_offset, size, &l.bytes[src]);
    src += size;
  }
  if (src != l.compressed)
  {
    printf("read %lX compressed bytes, expected %llX\n", src, l.compressed);
  }

  /* check the lz77 sequence */
  int ok = check_lz77_sequence(&l, pr->isVerbose);

  /* free allocated memory */
  free (segs);
  free (l.bytes);

  return ok;
}

int CheckFile(ParseResults const * pr)
{
  /* Read the 16 bytes */
  char ident[EI_NIDENT];
  fread(&ident[0], 1, EI_NIDENT, pr->fp);

  /* Check that this is an ELF file */
  if (   (ident[EI_MAG0] != ELFMAG0)
      || (ident[EI_MAG1] != ELFMAG1)
      || (ident[EI_MAG2] != ELFMAG2)
      || (ident[EI_MAG3] != ELFMAG3))
  {
    printf("The file is not an ELF file.\n");
    abort();
  }

  /* Check if this is 32 or 64 */
  switch(ident[EI_CLASS])
  {
  case ELFCLASS32:
    return Check32File(pr);
  case ELFCLASS64:
    return Check64File(pr);
  default:
    {
      printf ("The file is ELF, but neither ELF-32 nor ELF-64\n");
      abort();
    }
  }
}

void signon()
{
  printf("  Ilz77Check v" VERSION "\n");
  printf("  usage: ilz77check file range[,range[...]] expected [v|verbose]\n"
         "         file      The path to the ELF-file\n"
         "                   use \"first second\" if needed\n"
         "         range     Addresses to the lz77 entries,\n"
         "                   these have the format address:size\n"
         "                   example: 4000:12 (address 0x4000, size 0x12)\n"
         "         expected  The number of decompressed bytes.\n"
         "         verbose   Output more information\n");
  printf("  All numbers (range data & expected) are always hex (no 0x).\n");
  printf("  The ranges and expected sizes can be found in the map file.\n");
}

int check_lz77_sequence(lz77 const * lz77_info, char verbose)
{
  int ok = 1;
  uint32_t dest = 0;
  uint32_t len  = (uint32_t)lz77_info->compressed;
  uint8_t const * src = lz77_info->bytes;
  uint8_t const * org = src;
  uint8_t const * end = org + len;
  while (src < end)
  {
    uint32_t current_src = (uint32_t)(src - org);
    uint32_t current_dest = dest;
    uint8_t instr = *src++;
    int d = instr & 0x3;
    if (d == 0)
    {
      uint8_t byte = *src++;
      if (byte < 3)
      {
        if (verbose || ok)
        {
          uint64_t diff = src - org;
          printf("Error at offset %lX, read %d = overflow\n",
                 (uint32_t)diff, (int)byte);
        }
        ok = 0;
      }
      d = byte + 3;
    }
    int l0 = instr >> 4;
    if (l0 == 15)
      l0 += *src++;
    if (d > 1)
    {
      src += d - 1;
      dest += d - 1;
    }
    uint32_t reference_offset = 0;
    if (l0 != 0)
    {
      uint8_t ol = *src++;
      uint8_t oh = (instr & 0xc) >> 2;
      if (oh == 0x3)
        oh = *src++;
      reference_offset = ol + (oh << 8);
      if (reference_offset > (src - org))
      {
        if (verbose || ok)
        {
          uint64_t diff = src - org;
          printf("Error at offset %lX, offset %lX outside buffer\n",
                 (uint32_t)diff, reference_offset);
        }
        ok = 0;
      }
      dest += l0 + 2;
    }
    if (verbose)
    {
      uint32_t matched = l0 ? l0 + 2 : 0;
      uint32_t literal = d ? d - 1 : 0;
      printf("offset %lX, dest %lX, %d literal bytes, %ld matched, %lX offset,"
             " wrote %ld expected %ld\n",
             current_src, current_dest, d - 1, matched, reference_offset,
             matched + literal, dest - current_dest);
    }
  }
  uint32_t expected = (uint32_t)lz77_info->decompressed;
  if (dest != expected)
  {
    printf("Wrote 0x%lX bytes, expected 0x%lX.\n", dest, expected);
    ok = 0;
  }

  if (!ok)
  {
    printf("The sequence has at least one problem\n");
  }
  else if (verbose)
  {
    printf("The sequence does not have a problem\n");
  }

  if (ok)
    return 0; /* 0 = exit code, everythying ok */
  else
    return 1; /* 1 = exit code, not zero = something is wrong */
}

int main(int argc, char* argv[])
{
  /* possibly print signon */
  if (argc < 4)
  {
    signon();
    exit(0);
  }

  /* parse the command line */
  ParseResults pr;
  ParseResult(argc, argv, &pr);

  /* check the file */
  int ok = CheckFile(&pr);

  /* free allocated memory */
  free(pr.ranges);

  return ok;
}
