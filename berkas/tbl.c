/* ============================================================
 * TABEL v3.0
 * Berkas: tbl.c - Binary native format untuk TABEL
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * PERBAIKAN v3.0:
 * - Dukungan warna depan dan latar (foreground/background)
 * - Dukungan sticky (freeze pane) pada config
 * - Dukungan font style per sel
 * - Perbaikan tbl_read_border yang broken (variabel tidak dibaca)
 * - Multi-sheet support
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA TBL
 * ============================================================ */

/* Magic number: "TBL3" dalam hex */
#define TBL_MAGIC           0x334C4254  /* "TBL3" little-endian */
#define TBL_VERSION         0x0004      /* Version 4 - added style & sticky */
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
#define TBL_SECTION_STYLE    0x0A
#define TBL_SECTION_SHEET    0x0B  /* Multi-sheet info */
#define TBL_SECTION_END      0xFF

/* Flags */
#define TBL_FLAG_HAS_FORMULA  0x0001
#define TBL_FLAG_HAS_BORDER   0x0002
#define TBL_FLAG_HAS_COLOR    0x0004
#define TBL_FLAG_HAS_FORMAT   0x0008
#define TBL_FLAG_HAS_MERGE    0x0010
#define TBL_FLAG_HAS_STYLE    0x0020
#define TBL_FLAG_HAS_STICKY   0x0040
#define TBL_FLAG_MULTI_SHEET  0x0080

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

/* Config section data - v4 layout */
struct tbl_config_v4 {
    int cols;
    int rows;
    int active_x;
    int active_y;
    int view_col;
    int view_row;
    int offset_x;
    int offset_y;
    char csv_delim[4];
    int sticky_baris;             /* Freeze pane - sticky rows */
    int sticky_kolom;             /* Freeze pane - sticky cols */
    int active_sheet;             /* Active sheet index */
    int num_sheets;               /* Total number of sheets */
    char reserved[16];            /* Padding */
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

/* Border data dengan warna depan dan latar */
struct tbl_border {
    int row;
    int col;
    unsigned char top;
    unsigned char bottom;
    unsigned char left;
    unsigned char right;
    unsigned char warna_depan;    /* Warna depan border */
    unsigned char warna_latar;    /* Warna latar border */
};

/* Color data dengan kombinasi depan dan latar */
struct tbl_color {
    int row;
    int col;
    unsigned char warna_depan;    /* Warna teks (foreground) */
    unsigned char warna_latar;    /* Warna latar (background) */
};

/* Format data */
struct tbl_format {
    int row;
    int col;
    unsigned char format;         /* jenis_format_sel */
};

/* Style data (font style per sel) */
struct tbl_style {
    int row;
    int col;
    unsigned char style;          /* font_style bitmask */
};

/* Formula data */
struct tbl_formula {
    int row;
    int col;
    unsigned short len;           /* Formula length */
    /* Followed by formula string */
};

/* Sheet info */
struct tbl_sheet_info {
    int rows;
    int cols;
    int sticky_baris;
    int sticky_kolom;
    char nama[NAMA_LEMBAR_MAKS];
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
    if (write_u32(fp, TBL_MAGIC) != 0) return -1;
    if (write_u16(fp, TBL_VERSION) != 0) return -1;
    if (write_u16(fp, flags) != 0) return -1;
    if (write_u32(fp, 0) != 0) return -1;  /* reserved */
    if (write_u32(fp, 0) != 0) return -1;  /* checksum placeholder */
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

/* Write config section - v4 format dengan sticky dan sheet info */
static int tbl_write_config(FILE *fp, const struct buffer_tabel *buf) {
    long section_start;
    long section_end;
    unsigned int section_size;
    
    section_start = ftell(fp);
    
    /* Write placeholder section header */
    if (tbl_write_section_header(fp, TBL_SECTION_CONFIG, 1, 0) != 0) {
        return -1;
    }
    
    /* Write config data */
    if (write_i32(fp, buf->cfg.kolom) != 0) return -1;
    if (write_i32(fp, buf->cfg.baris) != 0) return -1;
    if (write_i32(fp, buf->cfg.aktif_x) != 0) return -1;
    if (write_i32(fp, buf->cfg.aktif_y) != 0) return -1;
    if (write_i32(fp, buf->cfg.lihat_kolom) != 0) return -1;
    if (write_i32(fp, buf->cfg.lihat_baris) != 0) return -1;
    if (write_i32(fp, buf->cfg.offset_x) != 0) return -1;
    if (write_i32(fp, buf->cfg.offset_y) != 0) return -1;
    if (fwrite(buf->cfg.csv_delim, 1, 4, fp) != 4) return -1;
    /* Sticky (freeze pane) */
    if (write_i32(fp, buf->cfg.sticky_baris) != 0) return -1;
    if (write_i32(fp, buf->cfg.sticky_kolom) != 0) return -1;
    /* Sheet info */
    if (write_i32(fp, buf->lembar_aktif) != 0) return -1;
    if (write_i32(fp, buf->jumlah_lembar) != 0) return -1;
    /* Reserved padding */
    {
        char reserved[16];
        memset(reserved, 0, sizeof(reserved));
        if (fwrite(reserved, 1, sizeof(reserved), fp) != sizeof(reserved)) return -1;
    }
    
    /* Update section size */
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write data section for a specific sheet */
static int tbl_write_data_sheet(FILE *fp, const struct buffer_tabel *buf, int sheet_idx) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    struct lembar_tabel *lem;
    
    if (sheet_idx < 0 || sheet_idx >= buf->jumlah_lembar) return -1;
    lem = buf->lembar[sheet_idx];
    
    /* Count non-empty cells */
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->isi[r][c][0] != '\0') {
                count++;
            }
        }
    }
    
    section_start = ftell(fp);
    
    if (tbl_write_section_header(fp, TBL_SECTION_DATA, count, 0) != 0) return -1;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            const char *str = lem->isi[r][c];
            if (str[0] != '\0') {
                size_t len = strlen(str);
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_u16(fp, (unsigned short)len) != 0) return -1;
                if (fwrite(str, 1, len, fp) != len) return -1;
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

/* Write layout section for a specific sheet */
static int tbl_write_layout_sheet(FILE *fp, const struct lembar_tabel *lem) {
    int i;
    int count = lem->kolom + lem->baris;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    section_start = ftell(fp);
    
    if (tbl_write_section_header(fp, TBL_SECTION_LAYOUT, count, 0) != 0) return -1;
    
    /* Write column widths */
    for (i = 0; i < lem->kolom; i++) {
        if (write_i32(fp, i) != 0) return -1;
        if (write_i32(fp, lem->lebar_kolom[i]) != 0) return -1;
    }
    
    /* Write row heights (with offset) */
    for (i = 0; i < lem->baris; i++) {
        if (write_i32(fp, i + lem->kolom) != 0) return -1;
        if (write_i32(fp, lem->tinggi_baris[i]) != 0) return -1;
    }
    
    section_end = ftell(fp);
    section_size = (unsigned int)(section_end - section_start - 8);
    fseek(fp, section_start + 4, SEEK_SET);
    write_u32(fp, section_size);
    fseek(fp, section_end, SEEK_SET);
    
    return 0;
}

/* Write alignment section for a specific sheet */
static int tbl_write_align_sheet(FILE *fp, const struct lembar_tabel *lem) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->perataan_sel[r][c] != RATA_KIRI) count++;
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    if (tbl_write_section_header(fp, TBL_SECTION_ALIGN, count, 0) != 0) return -1;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->perataan_sel[r][c] != RATA_KIRI) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)lem->perataan_sel[r][c]) != 0) return -1;
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

/* Write merge section for a specific sheet */
static int tbl_write_merge_sheet(FILE *fp, const struct lembar_tabel *lem) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->tergabung[r][c].adalah_anchor) count++;
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    if (tbl_write_section_header(fp, TBL_SECTION_MERGE, count, 0) != 0) return -1;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->tergabung[r][c].adalah_anchor) {
                if (write_i32(fp, c) != 0) return -1;
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, lem->tergabung[r][c].lebar) != 0) return -1;
                if (write_i32(fp, lem->tergabung[r][c].tinggi) != 0) return -1;
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

/* Write border section for a specific sheet */
static int tbl_write_border_sheet(FILE *fp, const struct lembar_tabel *lem) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (!lem->border_sel) return 0;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            struct info_border *b = &lem->border_sel[r][c];
            if (b->top || b->bottom || b->left || b->right) count++;
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    if (tbl_write_section_header(fp, TBL_SECTION_BORDER, count, 0) != 0) return -1;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            struct info_border *b = &lem->border_sel[r][c];
            if (b->top || b->bottom || b->left || b->right) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->top) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->bottom) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->left) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->right) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->warna_depan) != 0) return -1;
                if (write_byte(fp, (unsigned char)b->warna_latar) != 0) return -1;
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

/* Write color section for a specific sheet */
static int tbl_write_color_sheet(FILE *fp, const struct lembar_tabel *lem) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (!lem->warna_sel) return 0;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            struct warna_kombinasi *wc = &lem->warna_sel[r][c];
            if (wc->depan != WARNAD_DEFAULT || wc->latar != WARNAL_DEFAULT) count++;
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    if (tbl_write_section_header(fp, TBL_SECTION_COLOR, count, 0) != 0) return -1;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            struct warna_kombinasi *wc = &lem->warna_sel[r][c];
            if (wc->depan != WARNAD_DEFAULT || wc->latar != WARNAL_DEFAULT) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)wc->depan) != 0) return -1;
                if (write_byte(fp, (unsigned char)wc->latar) != 0) return -1;
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

/* Write format section for a specific sheet */
static int tbl_write_format_sheet(FILE *fp, const struct lembar_tabel *lem) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (!lem->format_sel) return 0;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->format_sel[r][c] != FORMAT_UMUM) count++;
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    if (tbl_write_section_header(fp, TBL_SECTION_FORMAT, count, 0) != 0) return -1;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->format_sel[r][c] != FORMAT_UMUM) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)lem->format_sel[r][c]) != 0) return -1;
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

/* Write style section for a specific sheet */
static int tbl_write_style_sheet(FILE *fp, const struct lembar_tabel *lem) {
    int r, c;
    int count = 0;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (!lem->style_sel) return 0;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->style_sel[r][c] != STYLE_NORMAL) count++;
        }
    }
    
    if (count == 0) return 0;
    
    section_start = ftell(fp);
    if (tbl_write_section_header(fp, TBL_SECTION_STYLE, count, 0) != 0) return -1;
    
    for (r = 0; r < lem->baris; r++) {
        for (c = 0; c < lem->kolom; c++) {
            if (lem->style_sel[r][c] != STYLE_NORMAL) {
                if (write_i32(fp, r) != 0) return -1;
                if (write_i32(fp, c) != 0) return -1;
                if (write_byte(fp, (unsigned char)lem->style_sel[r][c]) != 0) return -1;
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

/* Write sheet info section (nama, dimensi, sticky per lembar) */
static int tbl_write_sheet_info(FILE *fp, const struct buffer_tabel *buf) {
    int i;
    long section_start;
    long section_end;
    unsigned int section_size;
    
    if (buf->jumlah_lembar <= 1) return 0;  /* Skip untuk single sheet */
    
    section_start = ftell(fp);
    if (tbl_write_section_header(fp, TBL_SECTION_SHEET, buf->jumlah_lembar, 0) != 0) return -1;
    
    for (i = 0; i < buf->jumlah_lembar; i++) {
        struct lembar_tabel *lem = buf->lembar[i];
        if (write_i32(fp, lem->baris) != 0) return -1;
        if (write_i32(fp, lem->kolom) != 0) return -1;
        if (write_i32(fp, lem->sticky_baris) != 0) return -1;
        if (write_i32(fp, lem->sticky_kolom) != 0) return -1;
        if (fwrite(lem->nama, 1, NAMA_LEMBAR_MAKS, fp) != NAMA_LEMBAR_MAKS) return -1;
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

/* Helper: write all data sections for one sheet, with sheet_idx as section flags */
static int tbl_write_all_sections_sheet(FILE *fp, const struct buffer_tabel *buf, int sheet_idx) {
    if (tbl_write_data_sheet(fp, buf, sheet_idx) != 0) return -1;
    if (tbl_write_layout_sheet(fp, buf->lembar[sheet_idx]) != 0) return -1;
    if (tbl_write_align_sheet(fp, buf->lembar[sheet_idx]) != 0) return -1;
    if (tbl_write_merge_sheet(fp, buf->lembar[sheet_idx]) != 0) return -1;
    if (tbl_write_border_sheet(fp, buf->lembar[sheet_idx]) != 0) return -1;
    if (tbl_write_color_sheet(fp, buf->lembar[sheet_idx]) != 0) return -1;
    if (tbl_write_format_sheet(fp, buf->lembar[sheet_idx]) != 0) return -1;
    if (tbl_write_style_sheet(fp, buf->lembar[sheet_idx]) != 0) return -1;
    return 0;
}

/* ============================================================
 * Main save function - Multi-Sheet aware
 * ============================================================ */

int simpan_tbl(const struct buffer_tabel *buf, const char *fn) {
    FILE *fp;
    unsigned short flags = 0;
    long header_pos;
    long data_start;
    long data_end;
    unsigned char *data_buf;
    size_t data_size;
    unsigned int checksum;
    int i;
    
    if (!buf || !fn) return -1;
    
    fp = fopen(fn, "wb");
    if (!fp) {
        tbl_set_error("Cannot create file: %s", fn);
        return -1;
    }
    
    /* Determine flags */
    for (i = 0; i < buf->jumlah_lembar; i++) {
        struct lembar_tabel *lem = buf->lembar[i];
        int r, c;
        
        if (lem->border_sel) flags |= TBL_FLAG_HAS_BORDER;
        if (lem->warna_sel) flags |= TBL_FLAG_HAS_COLOR;
        if (lem->format_sel) flags |= TBL_FLAG_HAS_FORMAT;
        if (lem->style_sel) flags |= TBL_FLAG_HAS_STYLE;
        if (lem->sticky_baris > 0 || lem->sticky_kolom > 0) flags |= TBL_FLAG_HAS_STICKY;
        
        for (r = 0; r < lem->baris && !(flags & TBL_FLAG_HAS_MERGE); r++) {
            for (c = 0; c < lem->kolom; c++) {
                if (lem->tergabung[r][c].adalah_anchor) {
                    flags |= TBL_FLAG_HAS_MERGE;
                    break;
                }
            }
        }
    }
    
    if (buf->jumlah_lembar > 1) flags |= TBL_FLAG_MULTI_SHEET;
    if (buf->cfg.sticky_baris > 0 || buf->cfg.sticky_kolom > 0) flags |= TBL_FLAG_HAS_STICKY;
    
    /* Write header */
    header_pos = ftell(fp);
    if (tbl_write_header(fp, flags) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Write config */
    if (tbl_write_config(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Write sheet info if multi-sheet */
    if (tbl_write_sheet_info(fp, buf) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Write all sections for each sheet */
    for (i = 0; i < buf->jumlah_lembar; i++) {
        if (tbl_write_all_sections_sheet(fp, buf, i) != 0) {
            fclose(fp);
            return -1;
        }
    }
    
    /* Write end */
    if (tbl_write_end(fp) != 0) {
        fclose(fp);
        return -1;
    }
    
    /* Calculate checksum */
    data_start = header_pos + 16; /* sizeof tbl_header */
    data_end = ftell(fp);
    data_size = data_end - data_start;
    
    data_buf = malloc(data_size);
    if (data_buf) {
        fseek(fp, data_start, SEEK_SET);
        fread(data_buf, 1, data_size, fp);
        checksum = tbl_calc_crc32(data_buf, data_size);
        free(data_buf);
        
        fseek(fp, header_pos + 12, SEEK_SET);
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

/* Read data section into a specific sheet */
static int tbl_read_data(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned short len;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_u16(fp, &len) != 0) return -1;
        
        if (row >= 0 && row < lem->baris && col >= 0 && col < lem->kolom) {
            if (len >= MAKS_TEKS) len = MAKS_TEKS - 1;
            if (fread(lem->isi[row][col], 1, len, fp) != len) return -1;
            lem->isi[row][col][len] = '\0';
        } else {
            fseek(fp, len, SEEK_CUR);
        }
    }
    
    return 0;
}

/* Read layout section into a specific sheet */
static int tbl_read_layout(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int index, value;
        
        if (read_i32(fp, &index) != 0) return -1;
        if (read_i32(fp, &value) != 0) return -1;
        
        if (index >= 0 && index < lem->kolom) {
            lem->lebar_kolom[index] = value;
        } else if (index >= lem->kolom && index < lem->kolom + lem->baris) {
            lem->tinggi_baris[index - lem->kolom] = value;
        }
    }
    
    return 0;
}

/* Read align section into a specific sheet */
static int tbl_read_align(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char align;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &align) != 0) return -1;
        fseek(fp, 3, SEEK_CUR); /* Skip padding */
        
        if (row >= 0 && row < lem->baris && col >= 0 && col < lem->kolom) {
            lem->perataan_sel[row][col] = (perataan_teks)align;
        }
    }
    
    return 0;
}

/* Read merge section into a specific sheet */
static int tbl_read_merge(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    struct buffer_tabel tmp_buf;
    
    /* We need a buffer_tabel for atur_blok_gabung_buf - create a temporary one */
    memset(&tmp_buf, 0, sizeof(tmp_buf));
    /* Point to this lembar's data */
    tmp_buf.tergabung = lem->tergabung;
    tmp_buf.cfg.kolom = lem->kolom;
    tmp_buf.cfg.baris = lem->baris;
    
    for (i = 0; i < count; i++) {
        int anchor_x, anchor_y, width, height;
        
        if (read_i32(fp, &anchor_x) != 0) return -1;
        if (read_i32(fp, &anchor_y) != 0) return -1;
        if (read_i32(fp, &width) != 0) return -1;
        if (read_i32(fp, &height) != 0) return -1;
        
        if (anchor_x >= 0 && anchor_x < lem->kolom &&
            anchor_y >= 0 && anchor_y < lem->baris &&
            width > 0 && height > 0) {
            atur_blok_gabung_buf(&tmp_buf, anchor_x, anchor_y, width, height);
            /* Sync back */
            lem->tergabung = tmp_buf.tergabung;
        }
    }
    
    return 0;
}

/* Read border section - FIXED: properly read all fields from file */
static int tbl_read_border(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char top, bottom, left, right;
        unsigned char wd_border, wl_border;
        
        /* PERBAIKAN: Baca row dan col dari file */
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        /* PERBAIKAN: Baca top, bottom, left, right dari file */
        if (read_byte(fp, &top) != 0) return -1;
        if (read_byte(fp, &bottom) != 0) return -1;
        if (read_byte(fp, &left) != 0) return -1;
        if (read_byte(fp, &right) != 0) return -1;
        /* PERBAIKAN: Baca warna depan dan latar dari file */
        if (read_byte(fp, &wd_border) != 0) return -1;
        if (read_byte(fp, &wl_border) != 0) return -1;
        
        if (row >= 0 && row < lem->baris && col >= 0 && col < lem->kolom) {
            if (lem->border_sel) {
                lem->border_sel[row][col].top = top;
                lem->border_sel[row][col].bottom = bottom;
                lem->border_sel[row][col].left = left;
                lem->border_sel[row][col].right = right;
                lem->border_sel[row][col].warna_depan = (warna_depan)wd_border;
                lem->border_sel[row][col].warna_latar = (warna_latar)wl_border;
            }
        }
    }
    
    return 0;
}

/* Read color section */
static int tbl_read_color(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char wd, wl;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &wd) != 0) return -1;
        if (read_byte(fp, &wl) != 0) return -1;
        fseek(fp, 2, SEEK_CUR); /* Skip padding */
        
        if (row >= 0 && row < lem->baris && col >= 0 && col < lem->kolom) {
            if (lem->warna_sel) {
                lem->warna_sel[row][col].depan = (warna_depan)wd;
                lem->warna_sel[row][col].latar = (warna_latar)wl;
            }
        }
    }
    
    return 0;
}

/* Read format section */
static int tbl_read_format(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char format;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &format) != 0) return -1;
        fseek(fp, 3, SEEK_CUR); /* Skip padding */
        
        if (row >= 0 && row < lem->baris && col >= 0 && col < lem->kolom) {
            if (lem->format_sel) {
                lem->format_sel[row][col] = (jenis_format_sel)format;
            }
        }
    }
    
    return 0;
}

/* Read style section */
static int tbl_read_style(FILE *fp, struct lembar_tabel *lem, unsigned short count) {
    int i;
    
    for (i = 0; i < count; i++) {
        int row, col;
        unsigned char style;
        
        if (read_i32(fp, &row) != 0) return -1;
        if (read_i32(fp, &col) != 0) return -1;
        if (read_byte(fp, &style) != 0) return -1;
        fseek(fp, 3, SEEK_CUR); /* Skip padding */
        
        if (row >= 0 && row < lem->baris && col >= 0 && col < lem->kolom) {
            if (lem->style_sel) {
                lem->style_sel[row][col] = (font_style)style;
            }
        }
    }
    
    return 0;
}

/* Ensure per-sheet arrays exist */
static void tbl_ensure_arrays(struct lembar_tabel *lem) {
    int r;
    
    if (!lem->border_sel && lem->baris > 0 && lem->kolom > 0) {
        lem->border_sel = calloc(lem->baris, sizeof(struct info_border *));
        if (lem->border_sel) {
            for (r = 0; r < lem->baris; r++) {
                lem->border_sel[r] = calloc(lem->kolom, sizeof(struct info_border));
            }
        }
    }
    if (!lem->warna_sel && lem->baris > 0 && lem->kolom > 0) {
        lem->warna_sel = calloc(lem->baris, sizeof(struct warna_kombinasi *));
        if (lem->warna_sel) {
            for (r = 0; r < lem->baris; r++) {
                lem->warna_sel[r] = calloc(lem->kolom, sizeof(struct warna_kombinasi));
            }
        }
    }
    if (!lem->format_sel && lem->baris > 0 && lem->kolom > 0) {
        lem->format_sel = calloc(lem->baris, sizeof(jenis_format_sel *));
        if (lem->format_sel) {
            for (r = 0; r < lem->baris; r++) {
                lem->format_sel[r] = calloc(lem->kolom, sizeof(jenis_format_sel));
            }
        }
    }
    if (!lem->style_sel && lem->baris > 0 && lem->kolom > 0) {
        lem->style_sel = calloc(lem->baris, sizeof(font_style *));
        if (lem->style_sel) {
            for (r = 0; r < lem->baris; r++) {
                lem->style_sel[r] = calloc(lem->kolom, sizeof(font_style));
            }
        }
    }
}

/* ============================================================
 * Main load function - Multi-Sheet aware
 * ============================================================ */

int buka_tbl(struct buffer_tabel **pbuf, const char *fn) {
    FILE *fp;
    struct tbl_header hdr;
    struct tbl_section sec;
    struct buffer_tabel *buf = NULL;
    int ret = -1;
    int current_sheet = 0;   /* Sheet yang sedang dibaca */
    int num_sheets = 1;      /* Default: 1 sheet */
    int active_sheet = 0;
    int i;
    
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
    
    /* Read sections */
    while (tbl_read_section(fp, &sec) == 0) {
        if (sec.type == TBL_SECTION_END) {
            ret = 0;
            break;
        }
        
        switch (sec.type) {
            case TBL_SECTION_CONFIG: {
                int cols, rows;
                int sticky_baris = 0, sticky_kolom = 0;
                unsigned int read_size;
                
                if (read_i32(fp, &cols) != 0 || read_i32(fp, &rows) != 0) {
                    tbl_set_error("Failed to read config dimensions");
                    goto cleanup;
                }
                
                /* Validasi dimensi */
                if (cols <= 0 || cols > MAKS_KOLOM || rows <= 0 || rows > MAKS_BARIS) {
                    tbl_set_error("Invalid dimensions: %d x %d", cols, rows);
                    goto cleanup;
                }
                
                /* Create buffer - hanya 1 lembar, lembar tambahan dari SHEET section */
                if (buf) bebas_buffer(buf);
                buf = buat_buffer_satu_lembar(rows, cols);
                if (!buf) {
                    tbl_set_error("Failed to create buffer");
                    goto cleanup;
                }
                
                buf->cfg.kolom = cols;
                buf->cfg.baris = rows;
                
                if (read_i32(fp, &buf->cfg.aktif_x) != 0 ||
                    read_i32(fp, &buf->cfg.aktif_y) != 0 ||
                    read_i32(fp, &buf->cfg.lihat_kolom) != 0 ||
                    read_i32(fp, &buf->cfg.lihat_baris) != 0 ||
                    read_i32(fp, &buf->cfg.offset_x) != 0 ||
                    read_i32(fp, &buf->cfg.offset_y) != 0) {
                    tbl_set_error("Failed to read config cursor/view");
                    goto cleanup;
                }
                
                if (fread(buf->cfg.csv_delim, 1, 4, fp) != 4) {
                    tbl_set_error("Failed to read config delimiter");
                    goto cleanup;
                }
                
                /* Read sticky and sheet info if v4+ */
                if (hdr.version >= 0x0004) {
                    if (read_i32(fp, &sticky_baris) != 0 ||
                        read_i32(fp, &sticky_kolom) != 0 ||
                        read_i32(fp, &active_sheet) != 0 ||
                        read_i32(fp, &num_sheets) != 0) {
                        tbl_set_error("Failed to read config v4 fields");
                        goto cleanup;
                    }
                    buf->cfg.sticky_baris = sticky_baris;
                    buf->cfg.sticky_kolom = sticky_kolom;
                    buf->lembar_aktif = active_sheet;
                    
                    /* Skip reserved */
                    {
                        char reserved[16];
                        if (fread(reserved, 1, 16, fp) != 16) {
                            tbl_set_error("Failed to read config reserved");
                            goto cleanup;
                        }
                    }
                } else {
                    /* v1-v3: skip remaining based on version */
                    read_size = 4 * 8 + 4; /* Old config: 8 ints + 4 delimiter */
                    if (sec.size > read_size) {
                        fseek(fp, sec.size - read_size, SEEK_CUR);
                    }
                }
                
                /* Set sticky pada lembar pertama juga */
                if (buf->lembar && buf->jumlah_lembar > 0) {
                    buf->lembar[0]->sticky_baris = sticky_baris;
                    buf->lembar[0]->sticky_kolom = sticky_kolom;
                }
                
                current_sheet = 0;
                break;
            }
            
            case TBL_SECTION_SHEET: {
                /* Baca info sheet untuk multi-sheet */
                for (i = 0; i < (int)sec.count && i < MAKS_LEMBAR; i++) {
                    int s_rows, s_cols, s_sticky_r, s_sticky_c;
                    char s_nama[NAMA_LEMBAR_MAKS];
                    
                    if (read_i32(fp, &s_rows) != 0 || read_i32(fp, &s_cols) != 0 ||
                        read_i32(fp, &s_sticky_r) != 0 || read_i32(fp, &s_sticky_c) != 0) {
                        goto cleanup;
                    }
                    if (fread(s_nama, 1, NAMA_LEMBAR_MAKS, fp) != NAMA_LEMBAR_MAKS) {
                        goto cleanup;
                    }
                    
                    if (i == 0 && buf && buf->jumlah_lembar > 0) {
                        /* Update lembar pertama (sudah dibuat) */
                        salin_str_aman(buf->lembar[0]->nama, s_nama, NAMA_LEMBAR_MAKS);
                        buf->lembar[0]->sticky_baris = s_sticky_r;
                        buf->lembar[0]->sticky_kolom = s_sticky_c;
                        /* Resize jika dimensi berbeda */
                        if (s_rows != buf->lembar[0]->baris || s_cols != buf->lembar[0]->kolom) {
                            ubah_ukuran_lembar(buf->lembar[0], s_rows, s_cols);
                            buf->cfg.baris = s_rows;
                            buf->cfg.kolom = s_cols;
                        }
                    } else if (i > 0 && buf && i >= buf->jumlah_lembar) {
                        /* Buat lembar tambahan */
                        struct lembar_tabel *lem = buat_lembar(s_rows, s_cols, s_nama);
                        if (lem) {
                            lem->sticky_baris = s_sticky_r;
                            lem->sticky_kolom = s_sticky_c;
                            tambah_lembar_ke_buffer(buf, lem);
                        }
                    } else if (i > 0 && buf && i < buf->jumlah_lembar) {
                        /* Lembar sudah ada, update nama dan sticky */
                        salin_str_aman(buf->lembar[i]->nama, s_nama, NAMA_LEMBAR_MAKS);
                        buf->lembar[i]->sticky_baris = s_sticky_r;
                        buf->lembar[i]->sticky_kolom = s_sticky_c;
                    }
                }
                break;
            }
            
            case TBL_SECTION_DATA:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    tbl_ensure_arrays(buf->lembar[current_sheet]);
                    if (tbl_read_data(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read data section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            case TBL_SECTION_LAYOUT:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    if (tbl_read_layout(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read layout section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            case TBL_SECTION_ALIGN:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    if (tbl_read_align(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read align section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            case TBL_SECTION_MERGE:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    if (tbl_read_merge(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read merge section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            case TBL_SECTION_BORDER:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    tbl_ensure_arrays(buf->lembar[current_sheet]);
                    if (tbl_read_border(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read border section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            case TBL_SECTION_COLOR:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    tbl_ensure_arrays(buf->lembar[current_sheet]);
                    if (tbl_read_color(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read color section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            case TBL_SECTION_FORMAT:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    tbl_ensure_arrays(buf->lembar[current_sheet]);
                    if (tbl_read_format(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read format section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            case TBL_SECTION_STYLE:
                if (buf && current_sheet < buf->jumlah_lembar) {
                    tbl_ensure_arrays(buf->lembar[current_sheet]);
                    if (tbl_read_style(fp, buf->lembar[current_sheet], sec.count) != 0) {
                        tbl_set_error("Failed to read style section");
                        goto cleanup;
                    }
                } else {
                    fseek(fp, sec.size, SEEK_CUR);
                }
                break;
                
            default:
                /* Unknown section, skip it */
                fseek(fp, sec.size, SEEK_CUR);
                break;
        }
    }
    
    if (ret == 0 && buf) {
        /* Pastikan jumlah_lembar sesuai dengan num_sheets dari file */
        if (num_sheets > 0 && num_sheets < buf->jumlah_lembar) {
            /* Bebaskan lembar berlebihan yang dibuat oleh buat_buffer */
            int j;
            for (j = num_sheets; j < buf->jumlah_lembar; j++) {
                bebas_lembar(buf->lembar[j]);
            }
            buf->jumlah_lembar = num_sheets;
        }

        /* Sync pointers */
        sinkronkan_pointer_lembar_aktif(buf);
        
        /* Set active sheet */
        if (active_sheet >= 0 && active_sheet < buf->jumlah_lembar) {
            set_lembar_aktif(buf, active_sheet);
            sinkronkan_pointer_lembar_aktif(buf);
        }
        
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
    if (ret != 0 && buf) {
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
            
            if (read_i32(fp, &c) != 0 || read_i32(fp, &r) != 0) {
                fclose(fp);
                return -1;
            }
            
            if (cols) *cols = c;
            if (rows) *rows = r;
            
            fclose(fp);
            return 0;
        }
        
        /* Skip unknown sections */
        fseek(fp, sec.size, SEEK_CUR);
    }
    
    fclose(fp);
    return -1;
}
