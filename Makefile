# ============================================================
# TABEL v3.0 - Makefile
# Standar: C89 + POSIX.1-2008
# Penulis: Chandra Lesmana
# ============================================================

# Compiler dan Flags
CC = cc
CFLAGS = -D_POSIX_C_SOURCE=200809L -D_XOPEN_SOURCE=600
LDFLAGS = -lm

# Target executable
TARGET = tabel

# Header utama
HEADER = tabel.h

# Source files di root
SRC_ROOT = tabel.c \
           kunci.c \
           tampilan.c \
           warna.c \
           bantuan.c \
           sel.c \
           berkas.c \
           jendela.c \
           form.c

# Source files di subdirektori berkas/
SRC_BERKAS = berkas/tbl.c \
             berkas/json.c \
             berkas/pdf.c \
             berkas/ods.c \
             berkas/xlsx.c \
             berkas/xls.c \
             berkas/html.c \
             berkas/xmlss.c

# Source files di subdirektori formula/
SRC_FORMULA = formula/teks.c \
              formula/waktu.c \
              formula/hitung.c

# Semua source files
SRCS = $(SRC_ROOT) $(SRC_BERKAS) $(SRC_FORMULA)

# Object files
OBJS = $(SRCS:.c=.o)

# ============================================================
# Aturan Build
# ============================================================

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(OBJS) -o $(TARGET) $(LDFLAGS)

# Kompilasi file .c di root
%.o: %.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

# Kompilasi file .c di subdirektori berkas/
berkas/%.o: berkas/%.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

# Kompilasi file .c di subdirektori formula/
formula/%.o: formula/%.c $(HEADER)
	$(CC) $(CFLAGS) -c $< -o $@

# ============================================================
# Clean
# ============================================================

clean:
	rm -f $(OBJS)

# ============================================================
# Dependencies
# ============================================================

tabel.o: tabel.c $(HEADER)
kunci.o: kunci.c $(HEADER)
tampilan.o: tampilan.c $(HEADER)
warna.o: warna.c $(HEADER)
bantuan.o: bantuan.c $(HEADER)
sel.o: sel.c $(HEADER)
berkas.o: berkas.c $(HEADER)
jendela.o: jendela.c $(HEADER)
form.o: form.c $(HEADER)

berkas/tbl.o: berkas/tbl.c $(HEADER)
berkas/json.o: berkas/json.c $(HEADER)
berkas/pdf.o: berkas/pdf.c $(HEADER)
berkas/ods.o: berkas/ods.c $(HEADER)
berkas/xlsx.o: berkas/xlsx.c $(HEADER)
berkas/xls.o: berkas/xls.c $(HEADER)
berkas/html.o: berkas/html.c $(HEADER)
berkas/xmlss.o: berkas/xmlss.c $(HEADER)

formula/teks.o: formula/teks.c $(HEADER)
formula/waktu.o: formula/waktu.c $(HEADER)
formula/hitung.o: formula/hitung.c $(HEADER)

.PHONY: all clean
