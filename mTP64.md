# mTP64

This format aims to reduce the size and decoding time of texture packs when used
with GLideNHQ. Advantages of this format over HTC/HTS/RICE texture packs include:

- Compressed textures in ETC1 format, supported natively by OpenGLES, OpenGL and
  Vulkan.
  Most, if not all systems supported by GLideN may copy textures directly to
  VRAM without any processing.
- Compressed textures means less RAM and file system space is required.
- Textures may be further compressed with LZ4 for even smaller file size whilst
  maintaining very fast decompression.
- Textures are sorted by CRC checksum, which improves texture look-up speed due
  to the use of binary searching.
- No duplicate textures required for different formats.
- Use of variable types with specific widths throughout.
- Only dependant on LZ4 decoder. Does not require zlib, or libpng for decoding.

## File Structure

The information presented within this section is pseudocode representing the
structure of the mTP64 file.

```
uint8_t magic[10]
uint8_t version
uint8_t tp_version[3]
char rom_target[20]
char pack_name[32]
char pack_author[32]
uint32_t pack_size
uint32_t n_textures
uint32_t n_mappings
uint32_t first_texture_offset
uint8_t dictionary_size
uint8_t dictionary_data[dictionary_size]
uint8_t unused[3]

for each n_mappings
	uint32_t crc
	uint32_t texture_offset
end

for each n_textures
	uint8_t data_format
	uint32_t data_size
	uint16_t tex_width
	uint16_t tex_height
	uint8_t data[data_size]
	uint8_t padding[0-7]
end
```

### magic

A unique set of bytes used to identify the file. These bytes must be to:

`uint8_t magic[10] = { 0xAB, 'm', 'T', 'P', '@', 0xBB, 0x0D, 0x0A, 0x1A, 0x0A }`

The rationale for these values are the same as those for the KTX and PNG
formats, in that they reduce the probability of incorrect file type detection
and bad transfer errors.

### version

Version information of the texture pack file format. This specification is for
version 1.

### tp_version

Version information of the texture pack itself in major, minor and patch order
whereby major version is stored within the first byte `tp_version[0]`.

### rom_target

The ROM header string that this texture pack was designed for. This should only
be used as weak sign of compatibility with the loaded game. This string *may
not* be null terminated if the length of the ROM header is 20 characters.

### pack_name

The name of this texture pack. This string *may not* be null terminated if the
length of pack_name is 32 characters.

### pack_author

The name of the author(s) of the texture pack. This string *may not* be null
terminated if the length of pack_author is 32 characters.

### pack_size

Size of this texture pack in bytes, divided by eight. The emulator guarantees
not to read further than this offset. Where `texture pack size = pack_size * 8`.

If `pack_size = 12058624` then the file size of the texture pack must be
96468992 Bytes. The maximum file size supported by this format is therefore
34359738368 Bytes (32 GiB).

### n_textures

Number of textures within this texture pack.

### first_texture_offset

Offset where the first texture pack is located within the texture
pack file. This value should be the address of the texture within the texture
pack minus the address of the first byte of the texture pack.

### dictionary_size

The size of the dictionary in bytes, divided by 1024. This must be used when
decompressing LZ4 textures. Set to 0 if no dictionary is used.

So a value of 4 gives a dictionary size of 4096 bytes.

### dictionary_data

The dictionary data for LZ4 to use when decompressing. Only available when
dictionary_size is not zero.

### unused

These three bytes are reserved for future use and must not be used.

### for each n_mappings

A hash map of sorted CRC values to the offset of the corresponding texture
within the texture pack. Multiple CRC entries may exist for the same texture.

#### crc

A 32-bit CRC value.

#### texture_offset

The offset of the texture entry within the file in bytes, divided by eight.
Whereby a value of 1 is equal to 8 bytes.

### for each n_textures

An entry for each texture within the texture pack.

#### data_format

The format of the data.

0: ETC1 RGB8
1: RAW RGBA8888

If the most significant bit (bit 7) is set, then the data is also compressed
with LZ4. Decompress the data with LZ4 in order to obtain the data in the format
described above. Recommended settings for LZ4 include the use of high
compression `--best` and favouring decompression speed `--favor-decSpeed`. If
using a dictionary improves decompression speed or file size, then set the
values of dictionary_size and dictionary_data.

#### data_size

The size of texture data in bytes.

#### tex_width

The width of the texture in pixels.

#### tex_height

The height of the texture in pixels.

#### data

The raw texture data.

#### padding

Unused bytes for 8-byte alignment of texture entries.

## License

Copyright (C) 2020 Mahyar Koshkouei
This work is licensed under the Creative Commons Attribution-ShareAlike 4.0
International (CC BY-SA 4.0) License. To view a copy of this license, visit
https://creativecommons.org/licenses/by-sa/4.0/
