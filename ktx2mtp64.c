/**
 * Copyright (c) 2020 Mahyar Koshkouei
 * Create mTP64 texture packs from KTX (ETC1/RGBA8) texture files.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES WITH
 * REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT,
 * INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM
 * LOSS OF USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
 * OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <assert.h>
#include <ktx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LZ4F_STATIC_LINKING_ONLY 1
#include <lz4frame.h>
#include <lz4hc.h>

#define XXH_INLINE_ALL
#include "xxhash.h"

#define CRC32_STR_LEN      8
#define GL_ETC1_RGB8_OES   0x8D64
#define GL_RGBA8_EXT       0x8058
#define DATA_LZ4_COMPRESSED 0x80

/**
 * Used for rare system errors, such as ENOMEM, which make continuing difficult.
 */
void fatal_error(int line);
#define ASSERT(x) do{if(!(x)){fatal_error(__LINE__);}}while(0)

struct textures_s
{
   uint32_t crc;
   enum data_type_e
   {
      TYPE_ETC1 = 0,
      TYPE_RGBA8888
   } type;
   uint64_t data_sz;
   char *filename;
};

struct map_s
{
   uint32_t crc;
   uint32_t offset;
} __attribute__((packed));

struct texture_header_s
{
   uint8_t data_format;
   uint32_t data_size;
   uint16_t tex_width;
   uint16_t tex_height;
} __attribute__((packed));

struct mtp64_header_s
{
   uint8_t magic[10];
   uint8_t version;
   uint8_t tp_version[3];
   char rom_target[20];
   char pack_name[32];
   char pack_author[32];
   uint32_t pack_size;
   uint32_t n_textures;
   uint32_t n_mappings;
   uint32_t first_texture_offset;
   uint8_t dictionary_size;
} __attribute__((packed));

#define MTP64_HEADER_INIT {   \
      .magic = { 0xAB, 'm', 'T', 'P', '@', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A }, \
      .version = 1, .tp_version = { 0, 1, 0 }, .rom_target = { 0 },        \
      .pack_name = { 0 }, .pack_author = { 0 }, .pack_size = 0             \
   }

void fatal_error(int line)
{
   char buf[128];
   snprintf(buf, sizeof(buf), "Fatal error on line %d", line);
   perror(buf);
   abort();
}

int compare_crc(const void *in1, const void *in2)
{
   const struct textures_s *tex1 = in1;
   const struct textures_s *tex2 = in2;
   int64_t diff = tex1->crc - tex2->crc;

   if (diff < 0)
      return -1;
   else if (diff > 0)
      return 1;

   return 0;
}

int compare_size(const void *in1, const void *in2)
{
   const struct textures_s *tex1 = in1;
   const struct textures_s *tex2 = in2;
   int64_t diff = tex1->data_sz - tex2->data_sz;

   if (diff < 0)
      return -1;
   else if (diff > 0)
      return 1;

   return 0;
}

struct textures_s *add_textures(char **filenames, uint_fast32_t *entries)
{
   struct textures_s *textures = NULL;
   uint_fast32_t alloc_nmemb = 1024;
   size_t etc1_tally = 0;
   size_t rgba8_tally = 0;

   *entries = 0;
   textures = malloc(alloc_nmemb * sizeof(*textures));
   ASSERT(textures != NULL);

   for (char **filename = filenames; *filename != NULL; filename++)
   {
      ktxTexture *tex;
      KTX_error_code ktx_res;
      char *dot;
      size_t len;
      char crcstr[CRC32_STR_LEN + 1];

      /* Is the file name correct? */
      dot = strrchr(*filename, '.');
      if (dot == NULL)
      {
         fprintf(stderr, "could not determine file extension\n");
         goto err;
      }

      /* Get the last 8 characters of the filename. */
      len = dot - *filename;

      if (len < CRC32_STR_LEN)
      {
         fprintf(stderr, "filename %s not a valid 32-bit CRC hash\n",
                 *filename);
         goto err;
      }

      if (len > CRC32_STR_LEN)
      {
         static uint8_t once = 1;

         if (once)
         {
            once = 0;
            fprintf(stderr, "CRC file names longer than %d characters will "
                    "be truncated\n", CRC32_STR_LEN);
         }
      }

      textures[*entries].filename = *filename;

      strncpy(crcstr, dot - CRC32_STR_LEN, CRC32_STR_LEN);
      crcstr[CRC32_STR_LEN] = '\0';

      textures[*entries].crc = strtol(crcstr, NULL, 16);
      ASSERT(textures[*entries].crc != UINT32_MAX);

      /* We have to obtain the format of the texture like this because libktx
       * does not seem to expose access to it. */
      {
         uint32_t glInternalformat;
         FILE *f = fopen(*filename, "rb");
         ASSERT(f != NULL);
         fseek(f, 28, SEEK_SET);
         fread(&glInternalformat, sizeof(glInternalformat), 1, f);
         fclose(f);

         switch (glInternalformat)
         {
         case GL_ETC1_RGB8_OES:
            textures[*entries].type = TYPE_ETC1;
            etc1_tally++;
            break;

         case GL_RGBA8_EXT:
            textures[*entries].type = TYPE_RGBA8888;
            rgba8_tally++;
            break;

         default:
            fprintf(stderr, "Unsupported texture format %x in %s\n"
                  "Format must be either %x or %x\n",
                    glInternalformat, *filename,
                    GL_ETC1_RGB8_OES, GL_RGBA8_EXT);
            goto err;
         }
      }

      /* Can we open it? */
      ktx_res = ktxTexture_CreateFromNamedFile(*filename,
                KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &tex);

      if (ktx_res != KTX_SUCCESS)
      {
         fprintf(stderr, "libktx returned error %d when opening file %s\n",
                 ktx_res, *filename);
         goto err;
      }

      textures[*entries].data_sz = ktxTexture_GetDataSizeUncompressed(tex);

      if (textures[*entries].data_sz > UINT32_MAX)
      {
         fprintf(stderr, "Input file %s is larger than 4 GiB\n", *filename);
         ktxTexture_Destroy(tex);
         goto err;
      }

      ktxTexture_Destroy(tex);
      (*entries)++;

      if (*entries >= alloc_nmemb)
      {
         alloc_nmemb <<= 2;
         textures = realloc(textures, alloc_nmemb * sizeof(*textures));
         ASSERT(textures);
      }

      if(*entries % 128 == 0)
      {
         putc('.', stdout);
         fflush(stdout);
      }
   }

   textures = realloc(textures, *entries * sizeof(*textures));
   ASSERT(textures != NULL);

   putc('\n', stdout);
   fprintf(stdout, "Successfully processed %lu ETC1 and %lu RGBA8888 textures\n",
           etc1_tally, rgba8_tally);

   /* Sort by CRC value. */
   qsort(textures, *entries, sizeof(*textures), compare_crc);
   fprintf(stdout, "Successfully sorted %lu CRC hashes\n",
           *entries);
   fflush(stdout);
   return textures;

err:
   free(textures);
   return NULL;
}

void print_help(void)
{
const char *const help_str = "Usage: ktx2mtp64 [OPTION...] [FILE...]\n"
         "Available options:\n"
         "  -help      \tPrints this help text\n"
         "  -out       \tSet output mtp64 texture pack file\n"
         "  -dump      \tDump raw texture data within the current folder\n"
         "  -dictionary\tUse a dictionary when compressing with LZ4\n"
         "\n"
         "'-dump' and '-out' may not be used at the same time. '-dump' "
         "can be used to create a dictionary before creating the mTP64 texture "
         "pack.\n"
         "A dictionary file generated by `zstd` may be specified using the "
         "option '-dictionary'. The size of the dictionary file must a "
         "multiple of 1024.\n"
         "Texture files must be in the KTX (Khronos TeXture) file format, "
         "encoded in either ETC1 or RGBA8888 only.\n"
         "Texture files must be named after the CRC hash that they are mapped "
         "to, in the format 'AABBCCDD.KTX'.\n"
         "\n"
         "Example workflow:\n"
         "  # Create a new folder to dump textures in\n"
         "  mkdir mtp64_work; cd mtp64_work\n"
         "  # Convert KTX texture files to RAW textures, and dump them\n"
         "  ktx2mtp64 -dump ~/textures/*.ktx\n"
         "  # Create a dictionary file based on the dumped textures\n"
         "  zstd --maxdict=32768 --train ./* -o dic_mtp64\n"
         "  # Create an mtp64 texture pack, using the dictionary for improved "
         "compression\n"
         "  ktx2mtp64 -out pack.mtp64 -dictionary dic_mtp64 ~/textures/*.ktx\n"
         "\n"
         "\n"
         "Copyright (c) 2020 Mahyar Koshkouei\n"
         "https://github.com/deltabeard/texturepack-utils\n\n";

   fprintf(stdout, "%s", help_str);
}

int main(int argc, char *argv[])
{
   char **filenames;
   struct textures_s *textures;
   uint_fast32_t entries;
   struct
   {
      unsigned char dump_textures;
      unsigned char show_help;
      char *mtp64_out;
      char *dictionary_file;
   } options = { 0 };

   if(argc < 2)
   {
      fprintf(stderr, "A command must be specified.\n"
         "Try 'ktx2mtp64 -help' for more information.\n");
      return EXIT_FAILURE;
   }

   /* Process arguments. */
   for (char **arg = (argv + 1); ; arg++)
   {
      struct optlist_s {
            const char *name;
            const enum { NONE, REQUIRED } param;
            union {
               void **valp;
               unsigned char *valc;
            };
      };
      struct optlist_s opts[] = {
         { "out",       REQUIRED, { .valp = (void**)&options.mtp64_out } },
         { "dump",      NONE,     { .valc = &options.dump_textures     } },
         { "help",      NONE,     { .valc = &options.show_help         } },
         { "dictionary",REQUIRED, { .valp = (void**)&options.dictionary_file } }
      };
      uint8_t valid_option = 0;

      /* Is this a command or a filename? */
      if(**arg != '-')
      {
         filenames = arg;
         break;
      }

      for (unsigned i = 0; i < sizeof(opts)/sizeof(*opts); i++)
      {
         if(strcmp(opts[i].name, (*arg) + 1) == 0)
         {
            valid_option = 1;

            if(opts[i].param == REQUIRED)
            {
               arg++;
               if(**arg == '-')
               {
                  fprintf(stderr, "The option '%s' expects a parameter.\n",
                          opts[i].name);
                  return EXIT_FAILURE;
               }

               *opts[i].valp = *arg;
            }
            else
            {
               *opts[i].valc = 1;
            }
         }
      }

      if (valid_option == 0)
      {
         fprintf(stderr, "Unrecognised option '%s'\n"
                 "Try 'ktx2mtp64 -help' for more information.\n", *arg);
         return EXIT_FAILURE;
      }

      if(options.show_help)
      {
         print_help();
         return EXIT_SUCCESS;
      }
   }

   if(options.mtp64_out == NULL && options.dump_textures == 0)
   {
      fprintf(stderr, "No output file was specified.\n");
      return EXIT_FAILURE;
   }

   if(filenames == NULL)
   {
      fprintf(stderr, "No file names were specified.\n");
      return EXIT_FAILURE;
   }

   if (options.dump_textures && options.dictionary_file != NULL)
   {
      fprintf(stderr, "You may not dump textures and use a dictionary for LZ4 compression, \n"
              "as no compression takes place when dumping textures.\n");
      return EXIT_FAILURE;
   }

   if (options.dump_textures && options.mtp64_out != NULL)
   {
      fprintf(stderr, "When dump is enabled, textures are dumped in the "
         "current folder and no mTP64 texture pack is created. Therefore, "
         "using dump and out arguments is not allowed.\n");
      return EXIT_FAILURE;
   }

   textures = add_textures(filenames, &entries);
   if (textures == NULL)
   {
      fprintf(stderr, "Unable to compile list of textures.\n");
      return EXIT_FAILURE;
   }

   if (options.dump_textures)
   {
      fprintf(stdout, "Dumping textures\n");

      for (size_t i = 0; i < entries; i++)
      {
         FILE *f_dmp;
         ktxTexture *ktex;
         KTX_error_code kret;
         uint8_t *tex;
         char dump_name[8 + 1 + 4 + 1]; /* Example: "0A0B0C0D.ETC1" */

         kret = ktxTexture_CreateFromNamedFile(textures[i].filename,
                                               KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                               &ktex);

         if (kret != KTX_SUCCESS)
         {
            fprintf(stdout, "%d: libktx was unable to create file %s: %s\n",
                    __LINE__, textures[i].filename, ktxErrorString(kret));
            abort();
         }

         tex = ktxTexture_GetData(ktex);
         snprintf(dump_name, sizeof(dump_name), "%08X.%s",
                  textures[i].crc,
                  textures[i].type == TYPE_ETC1 ? "ETC1" : "RGB8");

         f_dmp = fopen(dump_name, "wb");
         ASSERT(f_dmp != NULL);

         fwrite(tex, 1, textures[i].data_sz, f_dmp);
         fclose(f_dmp);
         ktxTexture_Destroy(ktex);

         if (i % (128 * sizeof(intptr_t)) == 0)
         {
            putc('.', stdout);
            fflush(stdout);
         }
      }

      fprintf(stdout, "\n%lu textures dumped.\n", entries);
      free(textures);
      return EXIT_SUCCESS;
   }

#if 0
   struct textures_s *crc_sorted = textures;

   /* Sort texture entries by size, smallest to largest size. */
   /* FIXME: do this after compression. */
   struct textures_s *size_sorted = malloc(entries * sizeof(*textures));
   ASSERT(size_sorted != NULL);
   memcpy(size_sorted, crc_sorted, entries * sizeof(*textures));
   qsort(size_sorted, entries, sizeof(*textures), compare_size);
#endif

   size_t fdic_sz = 0; /* Actual dictionary size. */
   uint8_t *dictionary = NULL;
   LZ4F_CDict *cdict = NULL;
   struct mtp64_header_s mtp64_hdr = MTP64_HEADER_INIT;
   const size_t map_sz = entries * sizeof(struct map_s);
   struct map_s *map = malloc(map_sz);

   ASSERT(map != NULL);

   for (size_t i = 0; i < entries; i++)
      map[i].crc = textures[i].crc;

   mtp64_hdr.n_mappings = entries;

   if (options.dictionary_file != NULL)
   {
      FILE *fdic = fopen(options.dictionary_file, "rb");
      ASSERT(fdic != NULL);

      fseek(fdic, 0, SEEK_END);
      fdic_sz = ftell(fdic);

      if (fdic_sz % 1024 != 0)
      {
         fprintf(stderr, "Dictionary file size is not a multiple of 1024\n");
         free(textures);
         free(map);
         fclose(fdic);
         return EXIT_FAILURE;
      }

      mtp64_hdr.dictionary_size = fdic_sz / 1024;
      dictionary = malloc(fdic_sz);
      ASSERT(dictionary != NULL);

      rewind(fdic);
      fread(dictionary, 1, fdic_sz, fdic);
      fclose(fdic);

      cdict = LZ4F_createCDict(dictionary, fdic_sz);
      puts("Dictionary was initialised and will be embedded within the texture "
           "pack");
   }

   FILE *f_out = fopen(options.mtp64_out, "wb");
   FILE *f_dupes = NULL;
   ASSERT(f_out);

#define fw(p) fwrite(p, 1, sizeof(p), f_out)
#define fwv(v) fwrite(&v, 1, sizeof(v), f_out)

   fwv(mtp64_hdr);
   if(mtp64_hdr.dictionary_size != 0)
      fwrite(dictionary, 1, fdic_sz, f_out);

   {
      uint8_t unused[4] = { 0, 0, 0, 0 };
      fw(unused);
   }

   fwrite(map, 1, map_sz, f_out);
   mtp64_hdr.first_texture_offset = ftell(f_out);

   puts("Writing texture data");

   struct tex_hash_list_s {
      uint64_t hash;
      char *filename;
   };
   struct tex_hash_list_s *tex_hash_list =
         malloc(entries * sizeof(struct tex_hash_list_s));
   mtp64_hdr.n_textures = 0;

   for(struct textures_s *tex = textures; tex < textures + entries; tex++)
   {
      uint8_t data_format = tex->type;
      size_t data_size;
      uint64_t data_hash;
      uint8_t *data_tex;
      ktxTexture *ktex;
      KTX_error_code kret;

      kret = ktxTexture_CreateFromNamedFile(tex->filename,
                                            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                            &ktex);

      if (kret != KTX_SUCCESS)
      {
         fprintf(stdout, "Error at %d: libktx was unable to open file %s: %s\n",
                 __LINE__, tex->filename, ktxErrorString(kret));
         abort();
      }

      data_size = ktxTexture_GetDataSize(ktex);
      data_tex = ktxTexture_GetData(ktex);

      data_hash = XXH64(data_tex, data_size, 0xDEADBEEF);

      /* Check for duplicates. */
      for (size_t d = 0; d < mtp64_hdr.n_textures; d++)
      {
         if (tex_hash_list[d].hash != data_hash)
            continue;

         if (f_dupes == NULL)
         {
            f_dupes = fopen("duplicates.txt", "wb");
            ASSERT(f_dupes != NULL);
         }

         fprintf(f_dupes, "\"%s\" \"%s\"\n",
                 tex_hash_list[d].filename, tex->filename);
         goto duplicate;
      }

      tex_hash_list[mtp64_hdr.n_textures].hash = data_hash;
      tex_hash_list[mtp64_hdr.n_textures].filename = tex->filename;

      for(size_t i = 0; i < mtp64_hdr.n_textures; i++)
      {
         if(map[i].crc == tex->crc)
         {
            long offset = ftell(f_out);
            assert(offset % 8 == 0);
            map[i].offset = (unsigned long)offset / 8;
            break;
         }
      }

      mtp64_hdr.n_textures++;

      /* Compress texture with LZ4. */
      {
         LZ4F_cctx *cctxPtr;
         LZ4F_preferences_t lz4pref = LZ4F_INIT_PREFERENCES;
         LZ4F_errorCode_t lz4err;
         size_t lz4sz_max;
         size_t lz4sz;
         char *lz4tex;
         struct texture_header_s tex_hdr;

         lz4pref.compressionLevel = LZ4HC_CLEVEL_DEFAULT;

         lz4sz_max = LZ4F_compressFrameBound(data_size, &lz4pref);
         lz4tex = malloc(lz4sz_max);
         ASSERT(lz4tex != NULL);

         lz4err = LZ4F_createCompressionContext(&cctxPtr, LZ4F_VERSION);

         if (LZ4F_isError(lz4err))
         {
            fprintf(stderr, "Error compressing texture with LZ4: %s\n",
                    LZ4F_getErrorName(lz4err));
            abort();
         }

         lz4sz = LZ4F_compressFrame_usingCDict(cctxPtr, lz4tex, lz4sz_max,
                                               data_tex, data_size, cdict,
                                               &lz4pref);

         tex_hdr.data_format = data_format;
         tex_hdr.data_size = lz4sz;
         tex_hdr.tex_width = ktex->baseWidth;
         tex_hdr.tex_height = ktex->baseHeight;
         fwv(tex_hdr);
         fwrite(lz4tex, 1, lz4sz, f_out);

         free(lz4tex);
         LZ4F_freeCompressionContext(cctxPtr);

         /* Add any required padding. */
         {
            const unsigned required_padding = 8;
            const uint8_t padding[7] = { 0 };
            long offset = ftell(f_out);
            uint8_t add_pad = offset % required_padding;

            if(add_pad != 0)
            {
               add_pad = required_padding - add_pad;
               fwrite(padding, 1, add_pad, f_out);
            }
         }
      }

duplicate:
      ktxTexture_Destroy(ktex);
      if((intptr_t)tex % (128 * sizeof(intptr_t)) == 0)
      {
         putc('.', stdout);
         fflush(stdout);
      }

      continue;
   }

   putc('\n', stdout);

   /* Rewrite header and hash map information. */
   fseek(f_out, 0, SEEK_SET);
   fwv(mtp64_hdr);
   fwrite(dictionary, 1, fdic_sz, f_out);
   {
      uint8_t unused[4] = { 0, 0, 0, 0 };
      fw(unused);
   }
   fwrite(map, 1, map_sz, f_out);
   fclose(f_out);

   if(f_dupes != NULL)
   {
      puts("duplicates.txt file created");
      fclose(f_dupes);
   }

   fprintf(stdout, "Wrote %u CRC entries and %u textures (%u duplicates) to %s\n",
           mtp64_hdr.n_mappings, mtp64_hdr.n_textures,
           (mtp64_hdr.n_mappings - mtp64_hdr.n_textures), options.mtp64_out);

#undef fw
#undef fwv

   free(textures);
   free(tex_hash_list);
   free(map);

   if (dictionary != NULL)
   {
      LZ4F_freeCDict(cdict);
      free(dictionary);
   }

   return EXIT_SUCCESS;
}
