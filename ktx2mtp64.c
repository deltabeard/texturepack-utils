#include <assert.h>
#include <ktx.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define LZ4F_STATIC_LINKING_ONLY 1
#include <lz4frame.h>
#include <lz4hc.h>

#define CRC32_STR_LEN      8
#define GL_ETC1_RGB8_OES   0x8D64
#define GL_RGBA8_EXT       0x8058
#define DATA_LZ4_COMPRESSED 0x80

struct textures_s
{
   uint32_t crc;
   enum data_type_e {
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

int compare_crc(const void *in1, const void *in2)
{
   const struct textures_s *tex1 = in1;
   const struct textures_s *tex2 = in2;
   int64_t diff = tex1->crc - tex2->crc;

   if(diff < 0)
      return -1;
   else if(diff > 0)
      return 1;

   return 0;
}

int compare_size(const void *in1, const void *in2)
{
   const struct textures_s *tex1 = in1;
   const struct textures_s *tex2 = in2;
   int64_t diff = tex1->data_sz - tex2->data_sz;

   if(diff < 0)
      return -1;
   else if(diff > 0)
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
   textures = calloc(alloc_nmemb, sizeof(*textures));

   if (textures == NULL)
   {
      fprintf(stderr, "allocation failure\n");
      goto err;
   }

   for (char **filename = filenames; *filename != NULL; filename++)
   {
      ktxTexture *tex;
      KTX_error_code ktx_res;

      /* Is the file name correct? */
      {
         char *dot = strrchr(*filename, '.');
         size_t len;
         char crcstr[CRC32_STR_LEN + 1];

         if (dot == NULL)
         {
            fprintf(stderr, "could not determine file extension\n");
            goto err;
         }

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
            if(once)
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

         if (textures[*entries].crc == UINT32_MAX)
         {
            fprintf(stderr, "overflow in strtol\n");
            goto err;
         }
      }

      uint32_t glInternalformat = 0;
      FILE *f = fopen(*filename, "rb");
      fseek(f, 28, SEEK_SET);
      fread(&glInternalformat, sizeof(glInternalformat), 1, f);
      fclose(f);

      /* Can we open it? */
      ktx_res = ktxTexture_CreateFromNamedFile(*filename,
                                 KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT, &tex);

      if (ktx_res != KTX_SUCCESS)
      {
         fprintf(stderr, "libktx returned error %d when opening file %s\n",
                 ktx_res, *filename);
         goto err;
      }

      switch(glInternalformat)
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
            fprintf(stderr, "unsupported texture format %x in %s\n",
                    glInternalformat, *filename);
            ktxTexture_Destroy(tex);
            goto err;
      }

      textures[*entries].data_sz = ktxTexture_GetDataSizeUncompressed(tex);
      if (textures[*entries].data_sz > UINT32_MAX)
      {
         fprintf(stderr, "input file %s larger than 4 GiB\n", *filename);
         ktxTexture_Destroy(tex);
         goto err;
      }

      ktxTexture_Destroy(tex);
      (*entries)++;

      if (*entries >= alloc_nmemb)
      {
         alloc_nmemb <<= 2;
         textures = realloc(textures, alloc_nmemb * sizeof(*textures));

         if (textures == NULL)
         {
            fprintf(stderr, "unable to realloc\n");
            goto err;
         }
      }
   }

   textures = realloc(textures, *entries * sizeof(*textures));
   if(textures == NULL)
   {
      fprintf(stderr, "unable to realloc to number of entries\n");
      goto err;
   }

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

int main(int argc, char *argv[])
{
   char **filenames;
   struct textures_s *textures;
   uint_fast32_t entries;
   struct
   {
      unsigned char dump_textures : 1;
      unsigned char use_dictionary : 1;
      char *mtp64_out;
      char *dictionary_file;
   } options = { 0 };

   if (argc < 3)
   {
      fprintf(stderr, "Usage: ktx2mtp64 [OPTIONS] [out_file.mtp64] in_file...\n"
              "Creates an mTP64 texture pack from input KTX files.\n"
              "\n"
              "Options:\n"
              "  --dump-textures   Dumps each raw texture data into the current "
              "folder\n"
              "                    This may be used to create a dictionary before "
              "compressing the texture data with LZ4.\n"
              "  --dictionary DICT Use DICT as Dictionary for LZ4 compression and decompression.\n\n"
              "Input files must be either KTX files in ETC1 or RGBA8888 format, or textures \n"
              "previously dumped with the --dump-textures option.\n"
              "The filenames of each input file must be a 32-bit CRC hash, in addition to \n"
              "their file extension.\n");
      return EXIT_FAILURE;
   }

   for (unsigned i = 1; i < 4 && i < argc; i++)
   {
      const char *const args[] = { "--dump-textures", "--dictionary" };

      if (strncmp(args[0], argv[i], strlen(args[0])) == 0)
         options.dump_textures = 1;
      else if (strncmp(args[1], argv[i], strlen(args[1])) == 0)
      {
         options.use_dictionary = 1;
         options.dictionary_file = argv[++i];
      }
   }

   if (options.dump_textures && options.use_dictionary)
   {
      fprintf(stderr, "You may not dump textures and use a dictionary for LZ4 compression, \n"
              "as no compression takes place when dumping textures.\n");
      return EXIT_FAILURE;
   }

   if (options.use_dictionary)
   {
      puts("Currently unsupported.");
      return EXIT_FAILURE;
   }

   {
      char **first_file = argv + 1;

      if (options.dump_textures)
         first_file++;

      if (options.use_dictionary)
         first_file += 2;

      filenames = first_file;
   }

   if (options.dump_textures == 0)
   {
      options.mtp64_out = *filenames;
      filenames++;
   }

   textures = add_textures(filenames, &entries);
   if(textures == NULL)
   {
      fprintf(stderr, "A failure occured.\n");
      return EXIT_FAILURE;
   }

   struct textures_s *crc_sorted = textures;
   struct textures_s *size_sorted = malloc(entries * sizeof(*textures));
   if(size_sorted == NULL)
   {
      fprintf(stderr, "alloc error\n");
      return EXIT_FAILURE;
   }

   memcpy(size_sorted, crc_sorted, entries * sizeof(*textures));
   /* Sort texture data from smallest size to largest. */
   qsort(size_sorted, entries, sizeof(*textures), compare_size);

   /* Write mTP64 file. */
   const uint8_t magic[10] = {
      0xAB, 'm', 'T', 'P', '@', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A
   };
   const uint8_t version = 1;
   const uint8_t tp_version[3] = { 0, 0, 1 };
   const char rom_target[20] = "STUB";
   const char pack_name[32] = "STUB";
   uint32_t pack_size = 0;
   const uint32_t n_textures = entries;
   const uint32_t n_mappings = entries;
   uint32_t first_texture_offset = 0;
   uint8_t dictionary_size = 0; /* Dictionary size divided by 1024. */
   size_t fdic_sz = 0; /* Actual dictionary size. */
   uint8_t *dictionary = NULL;
   const uint8_t unused[3] = { 0 };
   FILE *f_out;

   if(options.use_dictionary)
   {
      FILE *fdic = fopen(options.dictionary_file, "rb");

      fseek(fdic, 0, SEEK_END);
      fdic_sz = ftell(fdic);
      if(fdic_sz % 1024 != 0)
      {
         fprintf(stderr, "Dictionary file size is not a multiple of 1024\n");
         free(crc_sorted);
         free(size_sorted);
         fclose(fdic);
         return EXIT_FAILURE;
      }

      dictionary_size = fdic_sz / 1024;
      dictionary = malloc(fdic_sz);
      assert(dictionary != NULL);
      rewind(fdic);
      fread(dictionary, 1, fdic_sz, fdic);
      fclose(fdic);
   }

   if (options.dump_textures == 0)
   {
      f_out = fopen(options.mtp64_out, "wb");

      if (f_out == NULL)
      {
         fprintf(stderr, "Unable to open output file\n");
         free(size_sorted);
         free(crc_sorted);
         return EXIT_FAILURE;
      }

#define fw(p) fwrite(p, 1, sizeof(p), f_out)
#define fwv(v) fwrite(&v, 1, sizeof(v), f_out)
      fw(magic);
      fwv(version);
      fw(tp_version);
      fw(rom_target);
      fw(pack_name);
      fwv(pack_size);
      fwv(n_textures);
      fwv(n_mappings);
      fwv(first_texture_offset);
      fwv(dictionary_size);
      if(dictionary_size != 0)
         fwrite(dictionary, 1, fdic_sz, f_out);

      fw(unused);
   }

   for(size_t i = 0; i < entries; i++)
   {
      uint32_t offset = 0;

      if(options.dump_textures == 0)
         fwv(crc_sorted[i].crc);

      for (size_t j = 0; j < entries; j++)
      {
         if (crc_sorted[i].crc == size_sorted[j].crc)
         {
            uint8_t *tex;
            FILE *f_dmp;
            char dump_name[8 + 1 + 4 + 1] = "0A0B0C0D.ETC1";
            ktxTexture *ktex;
            KTX_error_code kret;

            if (options.dump_textures == 0)
            {
               fwv(offset);
               goto found;
            }

            kret = ktxTexture_CreateFromNamedFile(crc_sorted[i].filename,
                                                  KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                                  &ktex);
            assert(kret == KTX_SUCCESS);
            tex = ktxTexture_GetData(ktex);

            snprintf(dump_name, sizeof(dump_name), "%08X.%s",
                     crc_sorted[i].crc,
                     crc_sorted[i].type == TYPE_ETC1 ? "ETC1" : "RGB8");
            f_dmp = fopen(dump_name, "wb");
            fwrite(tex, 1, crc_sorted[i].data_sz, f_dmp);
            fclose(f_dmp);

            ktxTexture_Destroy(ktex);

            goto found;
         }

         offset += size_sorted[j].data_sz;
      }

      fprintf(stderr, "key did not exist\n");
      abort();

found:
      continue;
   }

   fprintf(stdout, "Writing texture data\n");
   fflush(stdout);

   /* Prepare LZ4 compression dictionary. */
   LZ4F_CDict *cdict = NULL;
   if(dictionary_size != 0)
      cdict = LZ4F_createCDict(dictionary, fdic_sz);

   for (size_t i = 0; i < entries; i++)
   {
      uint8_t *tex;
      ktxTexture *ktex;
      KTX_error_code kret;

      kret = ktxTexture_CreateFromNamedFile(size_sorted[i].filename,
                                            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                            &ktex);
      assert(kret == KTX_SUCCESS);
      tex = ktxTexture_GetData(ktex);

      if (options.dump_textures)
      {
         FILE *f_dmp;
         char dump_name[8 + 1 + 4 + 1] = "0A0B0C0D.ETC1";
         snprintf(dump_name, sizeof(dump_name), "%08X.%s",
                  size_sorted[i].crc,
                  size_sorted[i].type == TYPE_ETC1 ? "ETC1" : "RGB8");

         f_dmp = fopen(dump_name, "wb");
         fwrite(tex, 1, size_sorted[i].data_sz, f_dmp);
         fclose(f_dmp);
         ktxTexture_Destroy(ktex);
         continue;
      }

      assert(size_sorted[i].data_sz == ktxTexture_GetDataSize(ktex));

      /* Compress texture with LZ4. */
      {
         LZ4F_cctx *cctxPtr;
         LZ4F_preferences_t lz4pref = LZ4F_INIT_PREFERENCES;
         LZ4F_errorCode_t lz4err;
         size_t lz4sz_max;
         size_t lz4sz;
         char *lz4tex;
         struct texture_header_s hdr;

         lz4pref.compressionLevel = LZ4HC_CLEVEL_DEFAULT;

         lz4sz_max = LZ4F_compressFrameBound(size_sorted[i].data_sz, &lz4pref);
         lz4tex = malloc(lz4sz_max);

         lz4err = LZ4F_createCompressionContext(&cctxPtr, LZ4F_VERSION);
         if(LZ4F_isError(lz4err))
         {
            fprintf(stderr, "error compressing with LZ4: %s\n",
                    LZ4F_getErrorName(lz4err));
            abort();
         }

         lz4sz = LZ4F_compressFrame_usingCDict(cctxPtr, lz4tex, lz4sz_max, tex,
                                       size_sorted[i].data_sz, cdict, &lz4pref);

         hdr.data_format = size_sorted[i].type | DATA_LZ4_COMPRESSED;
         hdr.data_size = lz4sz;
         hdr.tex_height = ktex->baseHeight;
         hdr.tex_height = ktex->baseWidth;
         fwv(hdr);

         fwrite(lz4tex, 1, lz4sz, f_out);

         /* Add padding for 8-byte alignment. */
#define ALIGNMENT 8
         uint8_t mod = (hdr.data_size + sizeof(hdr)) % ALIGNMENT;
         const uint8_t padding[ALIGNMENT] = { 0 };
         if(mod != 0)
            fwrite(padding, 1, ALIGNMENT - mod, f_out);
#undef ALIGNMENT

         free(lz4tex);
      }

      ktxTexture_Destroy(ktex);
   }

   if (options.dump_textures == 0)
      fclose(f_out);

#undef fw
#undef fwv

   // Allocate hashmap given the number of inputs.
   // Read each file, storing CRC and texture data. Check for duplicates.
   // Create dictionary on all texture data
   // Compress each texture with LZ4
   // Sort textures from smallest to largest in size
   // Sort CRC map
   // Resolve texture data pointers
   // Save output texture pack
   // Output compression ratio, etc.

   free(size_sorted);
   free(crc_sorted);

   if(dictionary != NULL)
   {
      free(dictionary);
      LZ4F_freeCDict(cdict);
   }

   return EXIT_SUCCESS;
}
