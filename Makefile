CFLAGS := -Og -g3 -Wall -Wextra -flto
LZ4LIB := /usr/lib/liblz4.a

ht2bmp: LDLIBS := -lz
ktx2raw: LDLIBS := -lktx
ktx2mtp64: LDLIBS := -lktx $(LZ4LIB)

all: ht2bmp ktx2raw ktx2mtp64
