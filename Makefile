CFLAGS := -Og -g3 -Wall -Wextra -flto
ht2bmp: LDLIBS := -lz
ktx2raw: LDLIBS := -lktx
ktx2mtp64: LDLIBS := -lktx /usr/lib/liblz4.a

all: ht2bmp ktx2raw ktx2mtp64
