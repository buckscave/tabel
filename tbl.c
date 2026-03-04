/* ============================================================
 * TABEL v3.0
 * Berkas: tbl.c - Binary native format untuk TABEL
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA TBL
 * ============================================================ */

/* Magic number: "TBL3" dalam hex */
#define TBL_MAGIC           0x334C4254  /* "TBL3" little-endian */
#define TBL_VERSION         0x0003      /* Version 3.0 */
#define TBL_VERSION_MIN     0x0001      /* Minimum supported version */

/* Section types */
#define TBL_SECTION_CONFIG   0x01
#define TBL_SECTION_DATA     0x02
#define TBL_SECTION_LAYOUT   0x03
#define TBL_SECTION_ALIGN    0x04
#define TBL_SECTION_MERGE    0x05
#define TBL_SECTION_BORDER   0x06
#define TBL_SECTION_COLOR    0x07
#define TBL_SECTION_FORMAT   0x08
#define TBL_SECTION_FORMULA  0x09
#define TBL_SECTION_END      0xFF

/* Flags */
#define TBL_FLAG_HAS_FORMULA  0x0001
#define TBL_FLAG_HAS_BORDER   0x0002
#define TBL_FLAG_HAS_COLOR    0x0004
#define TBL_FLAG_HAS_FORMAT   0x0008
#define TBL_FLAG_HAS_MERGE    0x0010

/* ============================================================
 * STRUKTUR TBL
 * ============================================================ */

/* File header (16 bytes) */
struct tbl_header {
    unsigned int magic;           /* TBL_MAGIC */
    unsigned short version;       /* TBL_VERSION */
    unsigned short flags;         /* Flags */
    unsigned int reserved;        /* Reserved for future use */
    unsigned int checksum;        /* CRC32 of data */
};

/* Section header (8 bytes) */
struct tbl_section {
    unsigned char type;           /* Section type */
    unsigned char flags;          /* Section flags */
    unsigned short count;         /* Number of items */
    unsigned int size;            /* Section size in bytes */
};

/* Config section data */
struct tbl_config {
    int cols;
    int rows;
    int active_x;
    int active_y;
    int view_col;
    int view_row;
    int offset_x;
    int offset_y;
    char csv_delim[4];
    char reserved[28];            /* Padding to 64 bytes */
};

/* Cell data */
struct tbl_cell {
    int row;
    int col;
    unsigned short len;           /* String length */
    /* Followed by string data (UTF-8) */
};

/* Layout data */
struct tbl_layout {
    int index;
    int value;
};

/* Alignment data */
struct tbl_align {
    int row;
    int col;
    unsigned char align;          /* 0=left, 1=center, 2=right */
};

/* Merge data */
struct tbl_merge {
    int anchor_x;
    int anchor_y;
    int width;
    int height;
};

/* Border data */
struct tbl_border {
    int row;
    int col;
    unsigned char top;
    unsigned char bottom;
    unsigned char left;
    unsigned char right;
    unsigned char color;          /* warna_glyph index */
};

/* Color data */
struct tbl_color {
    int row;
    int col;
    unsigned char color;          /* warna_glyph index */
};

/* Format data */
struct tbl_format {
    int row;
    int col;
    unsigned char format;         /* jenis_format_sel */
};

/* Formula data */
struct tbl_formula {
    int row;
    int col;
    unsigned short len;           /* Formula length */
    /* Followed by formula string */
};

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static char tbl_error_msg[512] = "";

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

/* Set error message */
static void tbl_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(tbl_error_msg, sizeof(tbl_error_msg), fmt, args);
    va_end(args);
}

/* Get error message */
const char *tbl_get_error(void) {
    return tbl_error_msg;
}

/* CRC32 table */
static unsigned int tbl_crc32_table[256];
static int tbl_crc32_init = 0;

static void tbl_init_crc32(void) {
    unsigned int i, j, crc;
    
    if (tbl_crc32_init) return;
    
    for (i = 0; i < 256; i++) {
        crc = i;
        for (j = 0; j < 8; j++) {
            if (crc & 1)
                crc = (crc >> 1) ^ 0xEDB88320;
            else
                crc >>= 1;
        }
        tbl_crc32_table[i] = crc;
    }
    tbl_crc32_init = 1;
}

static unsigned int tbl_calc_crc32(const unsigned char *buf, size_t len) {
    unsigned int crc = 0xFFFFFFFF;
    size_t i;
    
    if (!tbl_crc32_init) tbl_init_crc32();
    
    for (i = 0; i < len; i++) {
        crc = tbl_crc32_table[(crc ^ buf[i]) & 0xFF] ^ (crc >> 8);
    }
    
    return crc ^ 0xFFFFFFFF;
}

/* Read helpers */
static int read_byte(FILE *fp, unsigned char *v) {
    return fread(v, 1, 1, fp) == 1 ? 0 : -1;
}

static int read_u16(FILE *fp, unsigned short *v) {
    unsigned char buf[2];
    if (fread(buf, 1, 2, fp) != 2) return -1;
    *v = buf[0] | (buf[1] << 8);
    return 0;
}

static int read_u32(FILE *fp, unsigned int *v) {
    unsigned char buf[4];
    if (fread(buf, 1, 4, fp) != 4) return -1;
    *v = buf[0] | (buf[1] << 8) | (buf[2] << 16) | (buf[3] << 24);
    return 0;
}

static int read_i32(FILE *fp, int *v) {
    return read_u32(fp, (unsigned int *)v);
}

/* Write helpers */
static int write_byte(FILE *fp, unsigned char v) {
    return fputc(v, fp) == v ? 0 : -1;
}

static int write_u16(FILE *fp, unsigned short v) {
    unsigned char buf[2];
    buf[0] = v & 0xFF;
    buf[1] = (v >> 8) & 0xFF;
    return fwrite(buf, 1, 2, fp) == 2 ? 0 : -1;
}

static int write_u32(FILE *fp, unsigned int v) {
    unsigned char buf[4];
    buf[0] = v & 0xFF;
    buf[1] = (v >> 8) & 0xFF;
    buf[2] = (v >> 16) & 0xFF;
    buf[3] = (v >> 24) & 0xFF;
    return fwrite(buf, 1, 4, fp) == 4 ? 0 : -1;
}

static int write_i32(FILE *fp, int v) {
    return write_u32(fp, (unsigned int)v);
}

/* ============================================================
 * WRITE TBL FILE
 * ============================================================ */

/* Write header */
static int tbl_write_header(FILE *fp, unsigned short flags) {
    struct tbl_header hdr;
    
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = TBL_MAGIC;
    hdr.version = TBL_VERSION;
    hdr.flags = flags;
    hdr.reserved = 0;
    hdr.checksum = 0;  /* Will be calculated later */
    
    /* Write header */
    if (write_u32(fp, hdr.magic) != 0) return -1;
    if (write_u16(fp, hdr.version) != 0) return -1;
    if (write_u16(fp, hdr.flags) != 0) return -1;
    if (write_u32(fp, hdr.reserved) != 0) return -1;
    if (write_u32(fp, hdr.checksum) != 0) return -1;
    
    return 0;
}

/* Write section header */
static int tbl_write_section_header(FILE *fp, unsigned char type, 
                                     unsigned short count, unsigned int size) {
    if (write_byte(fp, type) != 0) return -1;
    if (write_byte(fp, 0) != 0) return -1;  /* flags */
    if (write_u16(fp, count) != 0) return -1;
    if (write_u32(fp, size) != 0) return -1;
    return 0;
}

/* Write config section */
static int tbl_write_config(FILE *fp, const struct buffer_tabel *buf) {
    struct tbl_config cfg;
    
    memset(&cfg, 0, sizeof(cfg));
    cfg.cols = buf->cfg.kolom;
    cfg.rows = buf->cfg.baris;
    cfg.active_x = buf->cfg.aktif_x;
    cfg.active_y = buf->cfg.aktif_y;
    cfg.view_col = buf->cfg.lihat_kolom;
    cfg.view_row = buf->cfg.lihat_baris;
    cfg.offset_x = buf->cfg.offset_x;
    cfg.offset_y = buf->cfg.offset_y;
    memcpy(cfg.csv_delim, buf->cfg.csv_delim, sizeof(cfg.csv_delim));
    
    /* Write section header */
    if (tbl_write_section_header(fp, TBL_SECTION_CONFIG, 1, sizeof(cfg)) != 0) {
        return -1;
    }
    
    /* Write config data */
    if (write_i32(fp, cfg.cols) != 0) return -1;
    if (write_i32(fp, cfg.rows) != 0) return -1;
    if (write_i32(fp, cfg.active_x) != 0) return -1;
    if (write_i32(fp, cfg.active_y) != 0) return -1;
    if (write_i32(fp, cfg.view_col) != 0) return -1;
    if (write_i32(fp, cfg.view_row) != 0) return -1;
    if (write_i32(fp, cfg.offset_x) != 0) return -1;
    if (write_i32(fp, cfg.offset_y) != 0) return -1;
    if (fwrite(cfg.csv_delim, 1, sizeof(cfg.csv_delim), fp) != sizeof(cfg.csv_delim)) {
        return -1;
    }
    if (fwrite(cfg.reserved, 1, sizeof(cfg.reserved), fp) != sizeof(cfg.reserved)) {
        return -1;
    }
    
    return 0;
}

/* Write data section */
static int tbl_write_data(FILE *fp, const struct buffer_tabel *buf) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    /* Count non-empty cells */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->isi[r][c][0] != '\0') {
                count++;
            }
        }
    }
    
    /* Remember position for size */
    section_start = ftell(fp);
    
    /* Write placeholder section header */
    if (tbl_write_section_header(fp, TBL_SECTION_DATA, count, 0) != 0) {
        return -1;
    }
    
    /* Write each non-empty cell */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            const char *str = buf->isi[r][c];
            if (str[0] != '\0') {
                size_t len = strlen(str);
                
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_u16(fp, (unsigned short)len) != 0) return -1;
                if (fwrite(str, 1, len, fp) != len) return -1;
            }
        }
    }
    
    /* Update section size */
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);  /* Minus header */
    
    /* Seek back and update size */
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write layout section (column widths and row heights) */
static int tbl_write_layout(FILE *fp, const struct buffer_tabel *buf) {
    int i;
    int count = buf->cfg.kolom + buf->cfg.baris;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    section_start = ftell(fp);
    
    /* Write placeholder section header */
    if (tbl_write_section_header(fp, TBL_SECTION_LAYOUT, count, 0) != 0) {
        return -1;
    }
    
    /* Write column widths */
    for (i = 0; i < buf->cfg.kolom; i++) {
        if (write_i32(fp, i) != 0) return -1;           /* index */
        if (write_i32(fp, buf->lebar_kolom[i]) != 0) return -1;  /* value */
    }
    
    /* Write row heights (with offset) */
    for (i = 0; i < buf->cfg.baris; i++) {
        if (write_i32(fp, i + buf->cfg.kolom) != 0) return -1;  /* index with offset */
        if (write_i32(fp, buf->tinggi_baris[i]) != 0) return -1;
    }
    
    /* Update section size */
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write alignment section */
static int tbl_write_align(FILE *fp, const struct buffer_tabel *buf) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    /* Count cells with non-default alignment */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->perataan_sel[r][c] != RATA_KIRI) {
                count++;
            }
        }
    }
    
    if (count == 0) return 0;  /* Skip if all default */
    
    section_start = ftell(fp);
    
    if (tbl_write_section_header(fp, TBL_SECTION_ALIGN, count, 0) != 0) {
        return -1;
    }
    
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->perataan_sel[r][c] != RATA_KIRI) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)buf->perataan_sel[r][c]) != 0) return -1;
                /* Padding to 4 bytes */
                if (write_byte(fp, 0) != 0) return -1;
                if (write_byte(fp, 0) != 0) return -1;
                if (write_byte(fp, 0) != 0) return -1;
            }
        }
    }
    
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write merge section */
static int tbl_write_merge(FILE *fp, const struct buffer_tabel *buf) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    /* Count merged cells */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->tergabung[r][c].adalah_anchor) {
                count++;
            }
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    
    if (tbl_write_section_header(fp, TBL_SECTION_MERGE, count, 0) != 0) {
        return -1;
    }
    
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->tergabung[r][c].adalah_anchor) {
                if (write_i32(fp, c) != 0) return -1;  /* anchor_x */
                if (write_i32(fp, r) != 0) return -1;  /* anchor_y */
                if (write_i32(fp, buf->tergabung[r][c].lebar) != 0) return -1;
                if (write_i32(fp, buf->tergabung[r][c].tinggi) != 0) return -1;
            }
        }
    }
    
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write border section */
static int tbl_write_border(FILE *fp, const struct buffer_tabel *buf) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (!buf->border_sel) return 0;
    
    /* Count cells with borders */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            struct info_border *b = &buf->border_sel[r][c];
            if (b->top || b->bottom || b->left || b->right) {
                count++;
            }
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    
    if (tbl_write_section_header(fp, TBL_SECTION_BORDER, count, 0) != 0) {
        return -1;
    }
    
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            struct info_border *b = &buf->border_sel[r][c];
            if (b->top || b->bottom || b->left || b->right) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->top) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->bottom) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->left) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->right) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->warna) != 0) return -1;
                if (write_byte(fp, 0) != 0) return -1;  /* padding */
                if (write_byte(fp, 0) != 0) return -1;
            }
        }
    }
    
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write color section */
static int tbl_write_color(FILE *fp, const struct buffer_tabel *buf) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (!buf->warna_teks_sel) return 0;
    
    /* Count cells with non-default color */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->warna_teks_sel[r][c] != WARNA_DEFAULT) {
                count++;
            }
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    
    if (tbl_write_section_header(fp, TBL_SECTION_COLOR, count, 0) != 0) {
        return -1;
    }
    
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->warna_teks_sel[r][c] != WARNA_DEFAULT) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)buf->warna_teks_sel[r][c]) != 0) return -1;
                if (write_byte(fp, 0) != 0) return -1;  /* padding */
                if (write_byte(fp, 0) != 0) return -1;
                if (write_byte(fp, 0) != 0) return -1;
            }
        }
    }
    
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write format section */
static int tbl_write_format(FILE *fp, const struct buffer_tabel *buf) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (!buf->format_sel) return 0;
    
    /* Count cells with non-default format */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->format_sel[r][c] != FORMAT_UMUM) {
                count++;
            }
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    
    if (tbl_write_section_header(fp, TBL_SECTION_FORMAT, count, 0) != 0) {
        return -1;
    }
    
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->format_sel[r][c] != FORMAT_UMUM) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)buf->format_sel[r][c]) != 0) return -1;
                if (write_byte(fp, 0) != 0) return -1;  /* padding */
                if (write_byte(fp, 0) != 0) return -1;
                if (write_byte(fp, 0) != 0) return -1;
            }
        }
    }
    
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write end section */
static int tbl_write_end(FILE *fp) {
    return tbl_write_section_header(fp, TBL_SECTION_END, 0, 0);
}

/* Main save function */
int simpan_tbl(const struct buffer_tabel *buf, const char *fn) {
    FILE *fp;
    unsigned short flags = 0;
    long header_pos;
    long data_start;
    long data_end;
    unsigned char *data_buf;
    size_t data_size;
    unsigned int checksum;
    
    if (!buf || !fn) return -1;
    
    fp = fopen(fn, "wb");
    if (!fp) {
        tbl_set_error("Cannot create file: %s", fn);
        return -1;
    }
    
    /* Determine flags */
    if (buf->border_sel) flags |= TBL_FLAG_HAS_BORDER;
    if (buf->warna_teks_sel) flags |= TBL_FLAG_HAS_COLOR;
    if (buf->format_sel) flags |= TBL_FLAG_HAS_FORMAT;
    
    /* Check for merged cells */
    {
        int r, c;
        for (r = 0; r < buf->cfg.baris; r++) {
            for (c = 0; c < buf->cfg.kolom; c++) {
                if (buf->tergabung[r][c].adalah_anchor) {
                    flags |= TBL_FLAG_HAS_MERGE;
                    break;
                }
            }
            if (flags & TBL_FLAG_HAS_MERGE) break;
        }
    }
    
    /* Write header (with placeholder checksum) */
    header_pos = ftell(fp);
    if (tbl_write_header(fp, flags) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Write all sections */
    if (tbl_write_config(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_data(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_layout(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_align(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_merge(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_border(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_color(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_format(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (tbl_write_end(fp) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Calculate checksum */
    data_start = header_pos + sizeof(struct tbl_header);
    data_end = ftell(fp);
    data_size = data_end - data_start;
    
    data_buf = malloc(data_size);
    if (data_buf) {
        fseek(fp, data_start, SEEK_SET);
        fread(data_buf, 1, data_size, fp);
        checksum = tbl_calc_crc32(data_buf, data_size);
        free(data_buf);
        
        /* Write checksum to header */
        fseek(fp, header_pos + 12, SEEK_SET);  /* checksum offset */
        write_u32(fp, checksum);
    }
    
    if (fsync_berkas(fp) != 0) {
        fclose(fp);
        return -1;
    }
    
    fclose(fp);
    return 0;
}

/* ============================================================
 * READ TBL FILE
 * ============================================================ */

/* Read header */
static int tbl_read_header(FILE *fp, struct tbl_header *hdr) {
    if (read_u32(fp, &hdr->magic) != 0) return -1;
    if (read_u16(fp, &hdr->version) != 0) return -1;
    if (read_u16(fp, &hdr->flags) != 0) return -1;
    if (read_u32(fp, &hdr->reserved) != 0) return -1;
    if (read_u32(fp, &hdr->checksum) != 0) return -1;
    
    /* Validate */
    if (hdr->magic != TBL_MAGIC) {
        tbl_set_error("Invalid TBL file: wrong magic number");
        return -1;
    }
    
    if (hdr->version < TBL_VERSION_MIN || hdr->version > TBL_VERSION) {
        tbl_set_error("Unsupported TBL version: %d.%d", 
                      hdr->version >> 8, hdr->version & 0xFF);
        return -1;
    }
    
    return 0;
}

/* Read section header */
static int tbl_read_section(FILE *fp, struct tbl_section *sec) {
    if (read_byte(fp, &sec->type) != 0) return -1;
    if (read_byte(fp, &sec->flags) != 0) return -1;
    if (read_u16(fp, &sec->count) != 0) return -1;
    if (read_u32(fp, &sec->size) != 0) return -1;
    return 0;
}

/* Read config section */
static int tbl_read_config(FILE *fp, struct buffer_tabel *buf, unsigned int size) {
    int cols, rows;
    
    if (read_i32(fp, &cols) != 0) return -1;
    if (read_i32(fp, &rows) != 0) return -1;
    
    buf->cfg.kolom = cols;
    buf->cfg.baris = rows;
    
    if (read_i32(fp, &buf->cfg.aktif_x) != 0) return -1;
    if (read_i32(fp, &buf->cfg.aktif_y) != 0) return -1;
    if (read_i32(fp, &buf->cfg.lihat_kolom) != 0) return -1;
    if (read_i32(fp, &buf->cfg.lihat_baris) != 0) return -1;
    if (read_i32(fp, &buf->cfg.offset_x) != 0) return -1;
    if (read_i32(fp, &buf->cfg.offset_y) != 0) return -1;
    
    if (fread(buf->cfg.csv_delim, 1, 4, fp) != 4) return -1;
    
    /* Skip remaining bytes */
    {
        unsigned int read_size = 4 * 8 + 4;  /* 8 ints + 4 byte delimiter */
        if (size > read_size) {
            fseek(fp, size - read_size, SEEK_CUR);
        }
    }
    
    return 0;
}

/* Read data section */
static int tbl_read_data(FILE *fp, struct buffer_tabel *buf, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned short len;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_u16(fp, &len) != 0) return -1;
        
        if (row >= 0 && row < buf->cfg.baris && 
            col >= 0 && col < buf->cfg.kolom) {
            if (len >= MAKS_TEKS) len = MAKS_TEKS - 1;
            if (fread(buf->isi[row][col], 1, len, fp) != len) return -1;
            buf->isi[row][col][len] = '\0';
        } else {
            /* Skip invalid cell */
            fseek(fp, len, SEEK_CUR);
        }
    }
    
    return 0;
}

/* Read layout section */
static int tbl_read_layout(FILE *fp, struct buffer_tabel *buf, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int index, value;
        
        if (read_i32(fp, &index) != 0) return -1;
        if (read_i32(fp, &value) != 0) return -1;
        
        if (index >= 0 && index < buf->cfg.kolom) {
            buf->lebar_kolom[index] = value;
        } else if (index >= buf->cfg.kolom && index < buf->cfg.kolom + buf->cfg.baris) {
            buf->tinggi_baris[index - buf->cfg.kolom] = value;
        }
    }
    
    return 0;
}

/* Read align section */
static int tbl_read_align(FILE *fp, struct buffer_tabel *buf, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char align;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &align) != 0) return -1;
        
        /* Skip padding */
        fseek(fp, 3, SEEK_CUR);
        
        if (row >= 0 && row < buf->cfg.baris && 
            col >= 0 && col < buf->cfg.kolom) {
            buf->perataan_sel[row][col] = (perataan_teks)align;
        }
    }
    
    return 0;
}

/* Read merge section */
static int tbl_read_merge(FILE *fp, struct buffer_tabel *buf, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int anchor_x, anchor_y, width, height;
        
        if (read_i32(fp, &anchor_x) != 0) return -1;
        if (read_i32(fp, &anchor_y) != 0) return -1;
        if (read_i32(fp, &width) != 0) return -1;
        if (read_i32(fp, &height) != 0) return -1;
        
        if (anchor_x >= 0 && anchor_x < buf->cfg.kolom &&
            anchor_y >= 0 && anchor_y < buf->cfg.baris &&
            width > 0 && height > 0) {
            atur_blok_gabung_buf(buf, anchor_x, anchor_y, width, height);
        }
    }
    
    return 0;
}

/* Read border section */
static int tbl_read_border(FILE *fp, struct buffer_tabel *buf, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char top, bottom, left, right, color;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &top) != 0) return -1;
        if (read_byte(fp, &bottom) != 0) return -1;
        if (read_byte(fp, &left) != 0) return -1;
        if (read_byte(fp, &right) != 0) return -1;
        if (read_byte(fp, &color) != 0) return -1;
        
        /* Skip padding */
        fseek(fp, 2, SEEK_CUR);
        
        if (row >= 0 && row < buf->cfg.baris && 
            col >= 0 && col < buf->cfg.kolom) {
            if (buf->border_sel) {
                buf->border_sel[row][col].top = top;
                buf->border_sel[row][col].bottom = bottom;
                buf->border_sel[row][col].left = left;
                buf->border_sel[row][col].right = right;
                buf->border_sel[row][col].warna = (warna_glyph)color;
            }
        }
    }
    
    return 0;
}

/* Read color section */
static int tbl_read_color(FILE *fp, struct buffer_tabel *buf, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char color;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &color) != 0) return -1;
        
        /* Skip padding */
        fseek(fp, 3, SEEK_CUR);
        
        if (row >= 0 && row < buf->cfg.baris && 
            col >= 0 && col < buf->cfg.kolom) {
            if (buf->warna_teks_sel) {
                buf->warna_teks_sel[row][col] = (warna_glyph)color;
            }
        }
    }
    
    return 0;
}

/* Read format section */
static int tbl_read_format(FILE *fp, struct buffer_tabel *buf, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char format;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &format) != 0) return -1;
        
        /* Skip padding */
        fseek(fp, 3, SEEK_CUR);
        
        if (row >= 0 && row < buf->cfg.baris && 
            col >= 0 && col < buf->cfg.kolom) {
            if (buf->format_sel) {
                buf->format_sel[row][col] = (jenis_format_sel)format;
            }
        }
    }
    
    return 0;
}

/* Main load function */
int buka_tbl(struct buffer_tabel **pbuf, const char *fn) {
    FILE *fp;
    struct tbl_header hdr;
    struct tbl_section sec;
    struct buffer_tabel *buf;
    int ret = -1;
    
    if (!pbuf || !fn) return -1;
    
    fp = fopen(fn, "rb");
    if (!fp) {
        tbl_set_error("Cannot open file: %s", fn);
        return -1;
    }
    
    /* Read header */
    if (tbl_read_header(fp, &hdr) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Create temporary buffer with minimal size */
    buf = buat_buffer(1, 1);
    if (!buf) {
        fclose(fp);
        return -1;
    }
    
    /* Read sections */
    while (tbl_read_section(fp, &sec) == 0) {
        if (sec.type == TBL_SECTION_END) {
            ret = 0;
            break;
        }
        
        switch (sec.type) {
            case TBL_SECTION_CONFIG: {
                int cols, rows;
                
                if (read_i32(fp, &cols) != 0 || read_i32(fp, &rows) != 0) {
                    tbl_set_error("Failed to read config");
                    goto cleanup;
                }
                
                /* Resize buffer */
                bebas_buffer(buf);
                buf = buat_buffer(rows, cols);
                if (!buf) {
                    tbl_set_error("Failed to create buffer");
                    goto cleanup;
                }
                
                /* Re-read config into proper buffer */
                buf->cfg.kolom = cols;
                buf->cfg.baris = rows;
                
                if (read_i32(fp, &buf->cfg.aktif_x) != 0 ||
                    read_i32(fp, &buf->cfg.aktif_y) != 0 ||
                    read_i32(fp, &buf->cfg.lihat_kolom) != 0 ||
                    read_i32(fp, &buf->cfg.lihat_baris) != 0 ||
                    read_i32(fp, &buf->cfg.offset_x) != 0 ||
                    read_i32(fp, &buf->cfg.offset_y) != 0) {
                    tbl_set_error("Failed to read config");
                    goto cleanup;
                }
                
                if (fread(buf->cfg.csv_delim, 1, 4, fp) != 4) {
                    tbl_set_error("Failed to read config");
                    goto cleanup;
                }
                
                /* Skip remaining */
                {
                    unsigned int read_size = 4 * 8 + 4;
                    if (sec.size > read_size) {
                        fseek(fp, sec.size - read_size, SEEK_CUR);
                    }
                }
                break;
            }
            
            case TBL_SECTION_DATA:
                if (tbl_read_data(fp, buf, sec.count) != 0) {
                    tbl_set_error("Failed to read data section");
                    goto cleanup;
                }
                break;
                
            case TBL_SECTION_LAYOUT:
                if (tbl_read_layout(fp, buf, sec.count) != 0) {
                    tbl_set_error("Failed to read layout section");
                    goto cleanup;
                }
                break;
                
            case TBL_SECTION_ALIGN:
                if (tbl_read_align(fp, buf, sec.count) != 0) {
                    tbl_set_error("Failed to read align section");
                    goto cleanup;
                }
                break;
                
            case TBL_SECTION_MERGE:
                if (tbl_read_merge(fp, buf, sec.count) != 0) {
                    tbl_set_error("Failed to read merge section");
                    goto cleanup;
                }
                break;
                
            case TBL_SECTION_BORDER:
                /* Ensure border array exists */
                if (!buf->border_sel && buf->cfg.baris > 0 && buf->cfg.kolom > 0) {
                    int r;
                    buf->border_sel = calloc(buf->cfg.baris, sizeof(struct info_border *));
                    if (buf->border_sel) {
                        for (r = 0; r < buf->cfg.baris; r++) {
                            buf->border_sel[r] = calloc(buf->cfg.kolom, sizeof(struct info_border));
                        }
                    }
                }
                if (tbl_read_border(fp, buf, sec.count) != 0) {
                    tbl_set_error("Failed to read border section");
                    goto cleanup;
                }
                break;
                
            case TBL_SECTION_COLOR:
                /* Ensure color array exists */
                if (!buf->warna_teks_sel && buf->cfg.baris > 0 && buf->cfg.kolom > 0) {
                    int r;
                    buf->warna_teks_sel = calloc(buf->cfg.baris, sizeof(warna_glyph *));
                    if (buf->warna_teks_sel) {
                        for (r = 0; r < buf->cfg.baris; r++) {
                            buf->warna_teks_sel[r] = calloc(buf->cfg.kolom, sizeof(warna_glyph));
                        }
                    }
                }
                if (tbl_read_color(fp, buf, sec.count) != 0) {
                    tbl_set_error("Failed to read color section");
                    goto cleanup;
                }
                break;
                
            case TBL_SECTION_FORMAT:
                /* Ensure format array exists */
                if (!buf->format_sel && buf->cfg.baris > 0 && buf->cfg.kolom > 0) {
                    int r;
                    buf->format_sel = calloc(buf->cfg.baris, sizeof(jenis_format_sel *));
                    if (buf->format_sel) {
                        for (r = 0; r < buf->cfg.baris; r++) {
                            buf->format_sel[r] = calloc(buf->cfg.kolom, sizeof(jenis_format_sel));
                        }
                    }
                }
                if (tbl_read_format(fp, buf, sec.count) != 0) {
                    tbl_set_error("Failed to read format section");
                    goto cleanup;
                }
                break;
                
            default:
                /* Unknown section, skip it */
                fseek(fp, sec.size, SEEK_CUR);
                break;
        }
    }
    
    if (ret == 0) {
        bebas_buffer(*pbuf);
        *pbuf = buf;
        
        /* Store file path */
        {
            char cwd[PANJANG_PATH_MAKS];
            if (getcwd(cwd, sizeof(cwd))) {
                snprintf(buf->cfg.path_file, sizeof(buf->cfg.path_file), 
                         "%s/%s", cwd, fn);
            } else {
                strncpy(buf->cfg.path_file, fn, PANJANG_PATH_MAKS - 1);
            }
        }
    }
    
cleanup:
    if (ret != 0) {
        bebas_buffer(buf);
    }
    fclose(fp);
    return ret;
}

/* ============================================================
 * VALIDASI DAN INFO
 * ============================================================ */

/* Check if file is valid TBL */
int tbl_apakah_valid(const char *fn) {
    FILE *fp;
    unsigned int magic;
    
    if (!fn) return 0;
    
    fp = fopen(fn, "rb");
    if (!fp) return 0;
    
    if (read_u32(fp, &magic) != 0) {
        fclose(fp);
        return 0;
    }
    
    fclose(fp);
    
    return (magic == TBL_MAGIC);
}

/* Get TBL file info */
int tbl_info(const char *fn, int *version, int *rows, int *cols, unsigned short *flags) {
    FILE *fp;
    struct tbl_header hdr;
    struct tbl_section sec;
    int found_config = 0;
    
    if (!fn) return -1;
    
    fp = fopen(fn, "rb");
    if (!fp) return -1;
    
    if (tbl_read_header(fp, &hdr) != 0) {
        fclose(fp);
        return -1;
    }
    
    if (version) *version = hdr.version;
    if (flags) *flags = hdr.flags;
    
    /* Find config section */
    while (tbl_read_section(fp, &sec) == 0) {
        if (sec.type == TBL_SECTION_END) break;
        
        if (sec.type == TBL_SECTION_CONFIG) {
            int c, r;
            if (read_i32(fp, &c) == 0 && read_i32(fp, &r) == 0) {
                if (cols) *cols = c;
                if (rows) *rows = r;
                found_config = 1;
            }
            break;
        }
        
        fseek(fp, sec.size, SEEK_CUR);
    }
    
    fclose(fp);
    return found_config ? 0 : -1;
}

/* ============================================================
 * KOMPATIBILITAS - HAPUS FUNGSI .META
 * ============================================================ */

/*
 * Fungsi simpan_layout dan muat_layout yang sebelumnya 
 * menggunakan file .meta sekarang dialihkan ke format TBL.
 * File .meta sudah tidak digunakan lagi.
 */

/* Stub untuk kompatibilitas - tidak melakukan apa-apa */
int simpan_layout_compat(const struct buffer_tabel *buf, const char *fn) {
    /* Tidak digunakan lagi - gunakan simpan_tbl */
    (void)buf;
    (void)fn;
    return 0;
}

int muat_layout_compat(struct buffer_tabel *buf, const char *fn) {
    /* Tidak digunakan lagi - gunakan buka_tbl */
    (void)buf;
    (void)fn;
    return 0;
}
