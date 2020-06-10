/**
 * Copyright (C) 2020 Mahyar Koshkouei
 * This work is licensed under the Creative Commons Attribution-ShareAlike 4.0
 * International (CC BY-SA 4.0) License. To view a copy of this license, visit
 * https://creativecommons.org/licenses/by-sa/4.0/
 */

/**
 * The following is a structure written in pseudocode defining the mTP64
 * texture pack format used within mini64.
 * mini64 uses a modified version of GLideNHQ to support this file type.
 *
 * This texture pack format aims to reduce the file size of previous texture
 * pack types, such as .HTC and .HTS by using WEBP image compression, instead of
 * zlib or no compression. This will however increase loading times drastically,
 * so it is expected that a large texture buffer is available for commonly used
 * textures to reside in their decoded format.
 *
 * In order to reduce performance issues during gameplay, the emulator may
 * decode and preload textures in RAM on startup. Larger textures take longer to
 * decode, and as such the textures are sorted in largest to smallest order. The
 * emulator can traverse the array of textures until the sizes of the encoded
 * textures are small enough that loading them in realtime need not pose
 * significant performance problems during gameplay.
 *
 * mTP64 stores images in WEBP format. This may be lossless or lossy. The author
 * of the texture pack should balance file size and image quality. It is
 * recommended that lossy WEBP format be used, as even 95% quality lossy WEBP
 * produces negligable or no difference in image quality to lossless WEBP whilst
 * being smaller in size.
 *
 * There should be no duplicate textures within mTP64 texture packs. As such,
 * multiple mappings may point to the same texture.
 *
 * mTP64 has a limited file size of 4GiB. It expected that mTP64 texture packs
 * are far smaller than 4GiB due to the use of WEBP encoding. For instance, a
 * 22 GiB HTS texture pack may be encoded to a 600MiB mTP64 texture pack with
 * little or no different in image quality.
 *
 * All values are in little endian order.
 */

#if 0
#include <stdint.h>

struct tex_pack_s {
	/* "mTP@" */
	char magic[4];

	/* Version information of the texture file format.
	 * This specification is for version 1. */
	uint8_t version;

	/* Version information of the texture pack itself. */
	uint8_t tp_ver_major;
	uint8_t tp_ver_minor;
	uint8_t tp_ver_patch;

	/* Name of the game texture pack was designed for. This should not be
	 * used as a check for compatability between games.
	 * The array is filled with NULL characters if the string is shorter
	 * than the array size. If the string is 32 characters, then it will
	 * *not* be NULL terminated. */
	/* The ROM header string that this texture pack was designed for. This
	 * should only be used as weak sign of compatability with the loaded
	 * game. This string *may not* be null terminated if the length of the
	 * string is 20 characters. */
	char rom_name[20];

	/* The name of this texture pack.
	 * This string *may not* be null terminated if the length of the string
	 * is 32 characters. */
	char pack_name[32];

	/* The author(s) of this texture pack.
	 * This string *may not* be null terminated if the length of the string
	 * is 32 characters. */
	char author[32];

	/* Size of this texture pack in bytes. The emulator guarantees not to
	 * read further than this offset. */
	uint32_t pack_size;

	/* Number of textures within this texture pack. */
	uint32_t n_textures;

	/* Number of mappings within this texture pack. This may exceed the
	 * number of textures as multiple CRC checksums may point to the same
	 * texture. The number of mappings must always be larger than or equal
	 * to the number of textures n_textures. */
	uint32_t n_mappings;

	/* Offset where the first texture pack is located within the texture
	 * pack file. This value should be (&textures - &magic). */
	uint32_t first_tex_offset;

	/* The values of this 4 byte array are undefined and may be used in a
	 * future version of this format. */
	char unused[4];

	/* A map of the texture CRC value and the offset where the texture is
	 * stored within the texture pack. The map is sorted by CRC (lowest to
	 * highest) to enable binary search. */
	struct {
		uint32_t crc;
		uint32_t offset;
	} map[n_mappings];

	/* An array of structures holding texture data encoded in WEBP format.
	 * The location of the texture corresponds to the offset found within
	 * the map.
	 * This array is sorted by largest texture size first.
	 */
	struct {
		char meta_magic[4] = "RIFF";
		uint32_t img_len;
		char img_magic[4] = "WEBP";
		char enc_magic[4] = "VP8 "; // or "VP8L" for lossless
		uint8_t webp_data[img_len];
	} textures[n_textures];
};
#endif
