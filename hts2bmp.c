/**
 * Copyright (c) 2020 Mahyar Koshkouei
 * Dump HTC and HTS texture packs to the current folder.
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

#define _DEFAULT_SOURCE

#include <ctype.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <zlib.h>

#define GL_TEXFMT_GZ    0x80000000

struct mapping_s
{
   uint64_t offset;
   uint64_t crc;
};

void write_bmp(const uint8_t *bmp, size_t bmp_sz, uint64_t crc, size_t w,
               size_t h)
{
   static unsigned char argb_bmp[] =
   {
      0x42, 0x4d, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x8a, 0x00,
      0x00, 0x00, 0x7c, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01, 0x00,
      0x00, 0x00, 0x01, 0x00, 0x20, 0x00, 0x03, 0x00, 0x00, 0x00, 0x04, 0x00,
      0x00, 0x00, 0x13, 0x0b, 0x00, 0x00, 0x13, 0x0b, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x00, 0x00, 0xff,
      0x00, 0x00, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0x42, 0x47,
      0x52, 0x73, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x00, 0x00, 0x00, 0x00
   };
   FILE *f;
   char out[32];
   snprintf(out, sizeof(out), "%016lX.bmp", crc);

   size_t size = h * (w * 4) + sizeof(argb_bmp);
   argb_bmp[2] = size >>  0;
   argb_bmp[3] = size >>  8;
   argb_bmp[4] = size >> 16;
   argb_bmp[5] = size >> 24;

   argb_bmp[18] = w >>  0;
   argb_bmp[19] = w >>  8;
   argb_bmp[20] = w >> 16;
   argb_bmp[21] = w >> 24;

   argb_bmp[22] = -h >>  0;
   argb_bmp[23] = -h >>  8;
   argb_bmp[24] = -h >> 16;
   argb_bmp[25] = -h >> 24;

   f = fopen(out, "wb");
   fwrite(argb_bmp, 1, sizeof(argb_bmp), f);
   fwrite(bmp, 1, bmp_sz, f);
   fclose(f);
}

int compare_offset(const void *in1, const void *in2)
{
   const struct mapping_s *m1 = in1;
   const struct mapping_s *m2 = in2;
   int64_t rem;
   bool overflow = __builtin_sub_overflow(m1->offset, m2->offset, &rem);

   if (overflow || rem < 0)
      return -1;
   else if (rem > 0)
      return 1;

   return 0;
}

int dump_hts(const char *hts_filename)
{
   gzFile gzfp = gzopen(hts_filename, "rb");
   uint64_t keymap_off;
   struct mapping_s *map = NULL;
   size_t map_nmemb = 0;
   size_t map_sz = 1024;
   uint8_t *texture_buf = NULL;
   size_t texture_buf_sz = 1 * 1024 * 1024;
   clock_t start_time = clock();

   if (gzfp == NULL)
   {
      fprintf(stderr, "gzip was unable to open the input file.\n");
      return EXIT_FAILURE;
   }

   map = calloc(map_sz, sizeof(*map));
   texture_buf = malloc(texture_buf_sz);

   if (map == NULL || texture_buf == NULL)
   {
      fprintf(stderr, "Unable to allocate memory.\n");
      return EXIT_FAILURE;
   }

   /* Skip reading config. */
   gzseek(gzfp, 4, SEEK_CUR);

   gzread(gzfp, &keymap_off, 8);
   gzseek(gzfp, keymap_off, SEEK_CUR);

   fprintf(stdout, "Reading key mappings\n");
   fflush(stdout);

   while (gzeof(gzfp) != 1)
   {
      gzread(gzfp, &map[map_nmemb].offset, sizeof(map[map_nmemb].offset));
      gzread(gzfp, &map[map_nmemb].crc, sizeof(map[map_nmemb].crc));

      map_nmemb++;

      if (map_nmemb >= map_sz)
      {
         map_sz <<= 1;
         map = reallocarray(map, map_sz, sizeof(*map));

         if (map == NULL)
            goto allocerr;
      }
   }

   /* Sort by offset so that we can sequentially read the file later. */
   qsort(map, map_nmemb, sizeof(*map), compare_offset);

   fprintf(stdout, "Dumping %lu textures\n", map_nmemb);

   for (uint64_t i = 0; i < map_nmemb; i++)
   {
      int32_t w, h;
      uint32_t fmt;
      const uint8_t fmt_len = 5;
      size_t dst_len;
      int32_t tex_sz;

      gzseek(gzfp, map[i].offset, SEEK_SET);
      gzread(gzfp, &w, 4);
      gzread(gzfp, &h, 4);
      gzread(gzfp, &fmt, 4);
      gzseek(gzfp, fmt_len, SEEK_CUR);
      gzread(gzfp, &tex_sz, 4);
      dst_len = (size_t)h * (size_t)w * sizeof(uint32_t);

      if (dst_len > texture_buf_sz)
      {
         while (dst_len > texture_buf_sz)
            texture_buf_sz <<= 1;

         texture_buf = realloc(texture_buf, texture_buf_sz);

         if (texture_buf == NULL)
         {
            free(map);
            goto allocerr;
         }
      }

      gzread(gzfp, texture_buf, tex_sz);

      if ((size_t)tex_sz > w * h * sizeof(uint32_t))
      {
         fprintf(stderr, "Texture format at %lu not supported.\n",
                 map[i].offset);
         continue;
      }

      /* If texture is zlib compressed, uncompress it. */
      if (fmt & GL_TEXFMT_GZ)
      {
         uint8_t *dst = malloc(dst_len);

         if (dst == NULL)
         {
            free(map);
            goto allocerr;
         }

         if (uncompress(dst, &dst_len, texture_buf, tex_sz) != Z_OK)
            fprintf(stderr, "zlib failure for texture at %lu\n", map[i].offset);
         else
            memcpy(texture_buf, dst, dst_len);

         tex_sz = dst_len;
         free(dst);
      }

      write_bmp(texture_buf, tex_sz, map[i].crc, w, h);

      /* Update progress every 200 ms. */
      if ((clock() - start_time) / (CLOCKS_PER_SEC / 1000) >= 200)
      {
         start_time = clock();
         fprintf(stdout, "%8lu\r", i);
         fflush(stdout);
      }
   }

   fprintf(stdout, "\nCompleted\n");
   free(texture_buf);
   free(map);
   gzclose(gzfp);
   return EXIT_SUCCESS;

allocerr:
   fprintf(stderr, "Unable to reallocate memory.\n");
   gzclose(gzfp);
   return EXIT_FAILURE;
}

int dump_htc(const char *htc_filename)
{
   gzFile gzfp;
   uint8_t *texture_buf = NULL;
   size_t texture_buf_sz = 1 * 1024 * 1024;
   size_t file_size;
   clock_t start_time = clock();

   {
      FILE *f = fopen(htc_filename, "rb");

      if (f == NULL)
      {
         fprintf(stderr, "Unable to open input file.\n");
         return EXIT_FAILURE;
      }

      fseek(f, 0, SEEK_END);
      file_size = ftell(f);
      fclose(f);
   }

   gzfp = gzopen(htc_filename, "rb");

   if (gzfp == NULL)
   {
      fprintf(stderr, "gzip was unable to open the input file.\n");
      return EXIT_FAILURE;
   }

   texture_buf = malloc(texture_buf_sz);

   if (texture_buf == NULL)
      goto allocerr;

   /* Skip reading config. */
   gzseek(gzfp, 4, SEEK_CUR);

   while (gzeof(gzfp) != 1)
   {
      int32_t w, h;
      uint32_t fmt;
      uint64_t crc;
      const uint8_t fmt_len = 5;
      int32_t tex_sz;
      size_t dst_len;

      gzread(gzfp, &crc, 8);
      gzread(gzfp, &w, 4);
      gzread(gzfp, &h, 4);
      gzread(gzfp, &fmt, 4);
      gzseek(gzfp, fmt_len, SEEK_CUR);
      gzread(gzfp, &tex_sz, 4);

      dst_len = (size_t)w * (size_t)h * sizeof(uint32_t);

      while (dst_len > texture_buf_sz)
      {
         texture_buf_sz <<= 1;
         texture_buf = realloc(texture_buf, texture_buf_sz);

         if (texture_buf == NULL)
            goto allocerr;
      }

      gzread(gzfp, texture_buf, tex_sz);

      if ((size_t)tex_sz > (size_t)w * (size_t)h * sizeof(uint32_t))
      {
         fprintf(stderr, "Texture format at %lu not supported.\n",
                 gztell(gzfp));
         continue;
      }

      /* If texture is zlib compressed, uncompress it. */
      if (fmt & GL_TEXFMT_GZ)
      {
         uint8_t *dst = malloc(dst_len);

         if (dst == NULL)
            goto allocerr;

         if (uncompress(dst, &dst_len, texture_buf, tex_sz) != Z_OK)
            fprintf(stderr, "zlib failure for texture at %lu\n", gztell(gzfp));
         else
            memcpy(texture_buf, dst, dst_len);

         tex_sz = dst_len;
         free(dst);
      }

      write_bmp(texture_buf, tex_sz, crc, w, h);

      /* Update progress every 200 ms. */
      if ((clock() - start_time) / (CLOCKS_PER_SEC / 1000) >= 200)
      {
         start_time = clock();
         fprintf(stdout, "%4.2f\r", ((float)gzoffset(gzfp) / file_size) * 100.0f);
         fflush(stdout);
      }
   }

   fprintf(stdout, "\nCompleted\n");
   free(texture_buf);
   gzclose(gzfp);
   return EXIT_SUCCESS;

allocerr:
   fprintf(stderr, "Unable to reallocate memory.\n");
   gzclose(gzfp);
   return EXIT_FAILURE;
}

int main(int argc, char *argv[])
{
   const char *filename;
   int (*dump_func)(const char *);

   if (argc != 2)
   {
      fprintf(stderr, "Usage: hts2bmp in_file\n"
              "Dumps the contents of a HTS or HTC texture pack \"in_file\" to "
              "the current folder\n");
      return EXIT_FAILURE;
   }

   filename = argv[1];
   /* Get file type from file extension. */
   {
      const char *ext = strrchr(filename, '.');
      char extchk[3];

      if (ext == NULL)
      {
         fprintf(stderr,
                 "Could not find dot in filename to deduce file type.\n");
         exit(EXIT_FAILURE);
      }

      ext++;

      if (*ext == '\0')
      {
         fprintf(stderr, "A file extension could not be found in filename.\n");
         exit(EXIT_FAILURE);
      }

      if (strlen(ext) != 3)
         goto incompatible;

      extchk[0] = tolower(*(ext++));
      extchk[1] = tolower(*(ext++));
      extchk[2] = tolower(*(ext));

      if (strncmp(extchk, "hts", 3) == 0)
         dump_func = dump_hts;
      else if (strncmp(extchk, "htc", 3) == 0)
         dump_func = dump_htc;
      else
         goto incompatible;
   }

   return dump_func(filename);

incompatible:
   fprintf(stderr, "File extension not hts or htc.");
   exit(EXIT_FAILURE);
}
