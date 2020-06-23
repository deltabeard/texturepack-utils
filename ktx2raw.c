/**
 * Copyright (c) 2020 Mahyar Koshkouei
 * Dumps texture data from a KTX file. Used for texting.
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

#include <ktx.h>
#include <stdio.h>
#include <stdlib.h>

#define DIE()  do{fprintf(stderr, "Error on line %d\n", __LINE__); abort();}while(0)
#define ASSERT(x) if(!(x))DIE()

int main(int argc, char *argv[])
{
   const char *in_file;
   const char *out_file;

   if (argc != 3)
   {
      fprintf(stderr, "Usage: ktx2raw in_file out_file\n"
              "Dumps KTX file to raw data.\n");
      return EXIT_FAILURE;
   }

   in_file = argv[1];
   out_file = argv[2];
   uint8_t *tex;
   ktxTexture *ktex;
   size_t tex_sz;
   KTX_error_code kres;
   kres = ktxTexture_CreateFromNamedFile(in_file,
                                         KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                         &ktex);
   if (kres != 0)
   {
      fprintf(stderr, "Failed to open input file: %s", ktxErrorString(kres));
      return EXIT_FAILURE;
   }

   tex_sz = ktxTexture_GetDataSize(ktex);
   tex = ktxTexture_GetData(ktex);
   ASSERT(tex != NULL);

   {
      FILE *f = fopen(out_file, "wb");
      if(f == NULL)
      {
         fprintf(stderr, "Unable to create output file.\n");
         return EXIT_FAILURE;
      }

      fwrite(tex, 1, tex_sz, f);
      fclose(f);
   }

   ktxTexture_Destroy(ktex);
   return EXIT_SUCCESS;
}
