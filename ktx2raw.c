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

   ktxTexture *texture;
   KTX_error_code result;
   ktx_uint8_t *image;
   result = ktxTexture_CreateFromNamedFile(in_file,
                                           KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
                                           &texture);
   ASSERT(result == 0);

   ktx_uint32_t w = texture->baseWidth;
   ktx_uint32_t h = texture->baseHeight;

   size_t buffer_sz = w * h * sizeof(uint32_t);
   uint8_t *buffer = malloc(w * h * buffer_sz);
   result = ktxTexture_LoadImageData(texture, buffer, buffer_sz);
   ASSERT(result == 0);

   {
      FILE *f = fopen(out_file, "wb");
      ASSERT(f != NULL);
      fwrite(buffer, 1, buffer_sz, f);
      fclose(f);
   }

   fprintf(stdout, "Dumped raw %dx%d image.\n", w, h);

   ktxTexture_Destroy(texture);
}
