CFLAGS := -Og -g3 -Wall -Wextra
ht2bmp: LDLIBS := -lz
ktx2raw: LDLIBS := -lktx

all: ht2bmp ktx2raw
