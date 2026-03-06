/* ============================================================
 * TABEL v3.0
 * Berkas: pdf.c - Native Portable Document Format (PDF) Generator
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * 
 * PERBAIKAN: Dukungan warna depan dan latar (foreground/background)
 * Tanggal: 2025
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA PDF
 * ============================================================ */
#define PDF_VERSION      "1.4"
#define PDF_MAX_OBJECTS  8192
#define PDF_MAX_PAGES    1024
#define PDF_MAX_CONTENT  (1024 * 1024)  /* 1MB per content stream */
#define PDF_STREAM_BUF   (512 * 1024)   /* 512KB buffer */

/* Dimensi kertas dalam points (1 point = 1/72 inch) */
#define PAPER_A4_W       595.28
#define PAPER_A4_H       841.89
#define PAPER_LETTER_W   612.00
#define PAPER_LETTER_H   792.00
#define PAPER_LEGAL_W    612.00
#define PAPER_LEGAL_H    1008.00
#define PAPER_A3_W       841.89
#define PAPER_A3_H       1190.55
#define PAPER_TABLOID_W  792.00
#define PAPER_TABLOID_H  1224.00

/* Margin default dalam points */
#define MARGIN_TOP       36.0    /* 0.5 inch */
#define MARGIN_BOTTOM    36.0
#define MARGIN_LEFT      36.0
#define MARGIN_RIGHT     36.0

/* Baris per halaman default (akan dihitung otomatis) */
#define ROWS_PER_PAGE_AUTO  0

/* ============================================================
 * TIPE KERTAS DAN ORIENTASI
 * ============================================================ */
typedef enum {
    KERTAS_A4 = 0,
    KERTAS_LETTER,
    KERTAS_LEGAL,
    KERTAS_A3,
    KERTAS_TABLOID,
    KERTAS_CUSTOM
} jenis_kertas;

typedef enum {
    ORIENTASI_PORTRAIT = 0,
    ORIENTASI_LANDSCAPE
} jenis_orientasi;

/* ============================================================
 * STRUKTUR KONFIGURASI PDF
 * ============================================================ */
struct pdf_config {
    jenis_kertas kertas;
    jenis_orientasi orientasi;
    double lebar_kertas;      /* dalam points */
    double tinggi_kertas;     /* dalam points */
    double margin_atas;
    double margin_bawah;
    double margin_kiri;
    double margin_kanan;
    double ukuran_font;       /* dalam points */
    double jarak_baris;       /* dalam points */
    double skala_sel;         /* skala lebar sel ke points */
    int baris_per_halaman;    /* 0 = otomatis */
    int tampilkan_header;     /* tampilkan header kolom */
    int tampilkan_nomor_baris; /* tampilkan nomor baris */
    int tampilkan_border;     /* tampilkan border tabel */
    int tampilkan_grid;       /* tampilkan garis grid */
    int tampilkan_warna;      /* PERBAIKAN: tampilkan warna */
    int tampilkan_background; /* PERBAIKAN: tampilkan warna latar */
    char judul[256];          /* judul dokumen */
};

/* ============================================================
 * STRUKTUR INTERNAL PDF
 * ============================================================ */
struct pdf_object {
    int nomor;                /* Object number */
    long offset;              /* Byte offset dalam file */
    int tipe;                 /* 1=page, 2=content, 3=font, 4=other */
};

struct pdf_writer {
    FILE *fp;
    int jumlah_objek;
    struct pdf_object objects[PDF_MAX_OBJECTS];
    int jumlah_halaman;
    int pages_obj_num;        /* Object number untuk Pages */
    int catalog_obj_num;      /* Object number untuk Catalog */
    int font_obj_num;         /* Object number untuk Font */
    long xref_offset;         /* Offset untuk cross-reference table */
    char content_buf[PDF_STREAM_BUF];
    int content_len;
};

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static struct pdf_config pdf_cfg_default = {
    KERTAS_A4,
    ORIENTASI_PORTRAIT,
    PAPER_A4_W,
    PAPER_A4_H,
    MARGIN_TOP,
    MARGIN_BOTTOM,
    MARGIN_LEFT,
    MARGIN_RIGHT,
    10.0,       /* ukuran font */
    14.0,       /* jarak baris */
    8.0,        /* skala sel */
    0,          /* baris per halaman otomatis */
    1,          /* tampilkan header */
    1,          /* tampilkan nomor baris */
    1,          /* tampilkan border */
    1,          /* tampilkan grid */
    1,          /* PERBAIKAN: tampilkan warna */
    1,          /* PERBAIKAN: tampilkan warna latar */
    "TABEL Export"
};

/* ============================================================
 * PERBAIKAN: FUNGSI KONVERSI WARNA
 * ============================================================ */

/* Konversi warna depan (foreground) ke PDF RGB */
static void warna_depan_ke_pdf_rgb(warna_depan wd, double *r, double *g, double *b) {
    switch (wd) {
        case WARNAD_GELAP:
            *r = 0.12; *g = 0.16; *b = 0.20;
            break;
        case WARNAD_ABU:
            *r = 0.31; *g = 0.35; *b = 0.39;
            break;
        case WARNAD_PUTIH:
            *r = 0.0; *g = 0.0; *b = 0.0;  /* Hitam di PDF untuk kontras */
            break;
        case WARNAD_HIJAU:
            *r = 0.0; *g = 0.6; *b = 0.0;
            break;
        case WARNAD_KUNING:
            *r = 0.85; *g = 0.65; *b = 0.0;
            break;
        case WARNAD_BIRU:
            *r = 0.0; *g = 0.55; *b = 0.55;
            break;
        case WARNAD_MERAH:
            *r = 0.85; *g = 0.0; *b = 0.0;
            break;
        case WARNAD_DEFAULT:
        default:
            *r = 0.0; *g = 0.0; *b = 0.0;  /* Hitam default */
            break;
    }
}

/* Konversi warna latar (background) ke PDF RGB */
static void warna_latar_ke_pdf_rgb(warna_latar wl, double *r, double *g, double *b) {
    switch (wl) {
        case WARNAL_HITAM:
            *r = 0.02; *g = 0.04; *b = 0.06;
            break;
        case WARNAL_GELAP:
            *r = 0.12; *g = 0.16; *b = 0.20;
            break;
        case WARNAL_ABU:
            *r = 0.31; *g = 0.35; *b = 0.39;
            break;
        case WARNAL_HIJAU:
            *r = 0.4; *g = 0.8; *b = 0.4;
            break;
        case WARNAL_HIJAU_GELAP:
            *r = 0.16; *g = 0.31; *b = 0.16;
            break;
        case WARNAL_HIJAU_TUA:
            *r = 0.08; *g = 0.16; *b = 0.16;
            break;
        case WARNAL_KUNING:
            *r = 0.9; *g = 0.7; *b = 0.3;
            break;
        case WARNAL_BIRU:
            *r = 0.2; *g = 0.7; *b = 0.7;
            break;
        case WARNAL_MERAH:
            *r = 0.9; *g = 0.4; *b = 0.4;
            break;
        case WARNAL_DEFAULT:
        default:
            *r = 1.0; *g = 1.0; *b = 1.0;  /* Putih default (tidak ada background) */
            break;
    }
}

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

/* Dapatkan dimensi kertas */
static void dapatkan_dimensi_kertas(jenis_kertas kertas, 
                                    jenis_orientasi orientasi,
                                    double *lebar, double *tinggi) {
    double w = 0, h = 0;

    switch (kertas) {
        case KERTAS_A4:      w = PAPER_A4_W;      h = PAPER_A4_H;      break;
        case KERTAS_LETTER:  w = PAPER_LETTER_W;  h = PAPER_LETTER_H;  break;
        case KERTAS_LEGAL:   w = PAPER_LEGAL_W;   h = PAPER_LEGAL_H;   break;
        case KERTAS_A3:      w = PAPER_A3_W;      h = PAPER_A3_H;      break;
        case KERTAS_TABLOID: w = PAPER_TABLOID_W; h = PAPER_TABLOID_H; break;
        default:             w = PAPER_A4_W;      h = PAPER_A4_H;      break;
    }

    if (orientasi == ORIENTASI_LANDSCAPE) {
        double tmp = w;
        w = h;
        h = tmp;
    }

    *lebar = w;
    *tinggi = h;
}

/* Escape string untuk PDF */
static int pdf_escape_string(const char *src, char *dst, size_t dst_size) {
    size_t i = 0;
    size_t j = 0;

    while (src[i] && j < dst_size - 2) {
        unsigned char c = (unsigned char)src[i];

        /* Karakter yang perlu di-escape */
        if (c == '(' || c == ')' || c == '\\') {
            dst[j++] = '\\';
            if (j < dst_size - 1) {
                dst[j++] = (char)c;
            }
        } else if (c == '\n') {
            dst[j++] = '\\';
            if (j < dst_size - 1) {
                dst[j++] = 'n';
            }
        } else if (c == '\r') {
            dst[j++] = '\\';
            if (j < dst_size - 1) {
                dst[j++] = 'r';
            }
        } else if (c == '\t') {
            dst[j++] = '\\';
            if (j < dst_size - 1) {
                dst[j++] = 't';
            }
        } else if (c >= 32 && c < 127) {
            /* ASCII printable */
            dst[j++] = (char)c;
        } else if (c < 32 || c >= 127) {
            /* Non-ASCII: gunakan octal escape untuk UTF-8 */
            dst[j++] = (char)c;
        } else {
            dst[j++] = (char)c;
        }
        i++;
    }
    dst[j] = '\0';
    return (int)j;
}

/* Hitung lebar teks dalam points (approximate) */
static double hitung_lebar_teks(const char *teks, double ukuran_font) {
    int len = 0;
    const char *p = teks;
    int char_width = 500;  /* Approximate width untuk Helvetica */

    /* Hitung jumlah karakter */
    while (*p) {
        int l = panjang_cp_utf8(p);
        unsigned int cp = dekode_cp_utf8(p, NULL);
        p += l;
        len++;

        /* Karakter lebar (CJK, emoji) */
        if ((cp >= 0x1100 && cp <= 0x115F) ||
            (cp >= 0x2E80 && cp <= 0xA4CF) ||
            (cp >= 0xAC00 && cp <= 0xD7A3) ||
            (cp >= 0xF900 && cp <= 0xFAFF) ||
            (cp >= 0x1F300 && cp <= 0x1FAFF)) {
            len++;  /* Double width */
        }
    }

    return (double)len * ukuran_font * (double)char_width / 1000.0;
}

/* ============================================================
 * FUNGSI LOW-LEVEL PDF WRITING
 * ============================================================ */

/* Tulis ke file PDF */
static int pdf_write(struct pdf_writer *pdf, const char *fmt, ...) {
    va_list args;
    int ret;

    va_start(args, fmt);
    ret = vfprintf(pdf->fp, fmt, args);
    va_end(args);

    return ret;
}

/* Tulis raw bytes */
static int pdf_write_raw(struct pdf_writer *pdf, const void *buf, size_t len) {
    return (int)fwrite(buf, 1, len, pdf->fp);
}

/* Registrasi object baru */
static int pdf_new_object(struct pdf_writer *pdf) {
    int num = pdf->jumlah_objek + 1;
    if (num >= PDF_MAX_OBJECTS) return -1;

    pdf->objects[num].nomor = num;
    pdf->objects[num].offset = ftell(pdf->fp);
    pdf->objects[num].tipe = 0;
    pdf->jumlah_objek = num;

    return num;
}

/* Tulis PDF header */
static int pdf_write_header(struct pdf_writer *pdf) {
    return pdf_write(pdf, "%%PDF-%s\n", PDF_VERSION);
}

/* Tulis catalog object */
static int pdf_write_catalog(struct pdf_writer *pdf) {
    int obj_num = pdf_new_object(pdf);
    if (obj_num < 0) return -1;

    pdf->catalog_obj_num = obj_num;
    pdf->objects[obj_num].tipe = 4;

    pdf_write(pdf, "%d 0 obj\n", obj_num);
    pdf_write(pdf, "<<\n");
    pdf_write(pdf, "  /Type /Catalog\n");
    pdf_write(pdf, "  /Pages %d 0 R\n", pdf->pages_obj_num);
    pdf_write(pdf, "  /PageLayout /SinglePage\n");
    pdf_write(pdf, "  /PageMode /UseNone\n");
    pdf_write(pdf, ">>\n");
    pdf_write(pdf, "endobj\n");

    return 0;
}

/* Tulis pages tree object */
static int pdf_write_pages_tree(struct pdf_writer *pdf, int *page_objs, 
                                 int num_pages) {
    int obj_num = pdf_new_object(pdf);
    int i;

    if (obj_num < 0) return -1;

    pdf->pages_obj_num = obj_num;
    pdf->objects[obj_num].tipe = 4;

    pdf_write(pdf, "%d 0 obj\n", obj_num);
    pdf_write(pdf, "<<\n");
    pdf_write(pdf, "  /Type /Pages\n");
    pdf_write(pdf, "  /Kids [");
    for (i = 0; i < num_pages; i++) {
        pdf_write(pdf, " %d 0 R", page_objs[i]);
    }
    pdf_write(pdf, " ]\n");
    pdf_write(pdf, "  /Count %d\n", num_pages);
    pdf_write(pdf, ">>\n");
    pdf_write(pdf, "endobj\n");

    return 0;
}

/* Tulis font object */
static int pdf_write_font(struct pdf_writer *pdf) {
    int obj_num = pdf_new_object(pdf);
    if (obj_num < 0) return -1;

    pdf->font_obj_num = obj_num;
    pdf->objects[obj_num].tipe = 3;

    /* Helvetica - standard Type 1 font */
    pdf_write(pdf, "%d 0 obj\n", obj_num);
    pdf_write(pdf, "<<\n");
    pdf_write(pdf, "  /Type /Font\n");
    pdf_write(pdf, "  /Subtype /Type1\n");
    pdf_write(pdf, "  /BaseFont /Helvetica\n");
    pdf_write(pdf, "  /Encoding /WinAnsiEncoding\n");
    pdf_write(pdf, ">>\n");
    pdf_write(pdf, "endobj\n");

    return 0;
}

/* Tulis content stream */
static int pdf_write_content_stream(struct pdf_writer *pdf, 
                                     const char *content, int content_len,
                                     int *stream_obj_num) {
    int obj_num = pdf_new_object(pdf);
    if (obj_num < 0) return -1;

    *stream_obj_num = obj_num;
    pdf->objects[obj_num].tipe = 2;

    pdf_write(pdf, "%d 0 obj\n", obj_num);
    pdf_write(pdf, "<<\n");
    pdf_write(pdf, "  /Length %d\n", content_len);
    pdf_write(pdf, ">>\n");
    pdf_write(pdf, "stream\n");
    pdf_write_raw(pdf, content, (size_t)content_len);
    pdf_write(pdf, "\nendstream\n");
    pdf_write(pdf, "endobj\n");

    return 0;
}

/* Tulis page object */
static int pdf_write_page(struct pdf_writer *pdf, int content_obj_num,
                          double lebar, double tinggi, int *page_obj_num) {
    int obj_num = pdf_new_object(pdf);
    if (obj_num < 0) return -1;

    *page_obj_num = obj_num;
    pdf->objects[obj_num].tipe = 1;

    pdf_write(pdf, "%d 0 obj\n", obj_num);
    pdf_write(pdf, "<<\n");
    pdf_write(pdf, "  /Type /Page\n");
    pdf_write(pdf, "  /Parent %d 0 R\n", pdf->pages_obj_num);
    pdf_write(pdf, "  /MediaBox [0 0 %.2f %.2f]\n", lebar, tinggi);
    pdf_write(pdf, "  /CropBox [0 0 %.2f %.2f]\n", lebar, tinggi);
    pdf_write(pdf, "  /Contents %d 0 R\n", content_obj_num);
    pdf_write(pdf, "  /Resources <<\n");
    pdf_write(pdf, "    /Font << /F1 %d 0 R >>\n", pdf->font_obj_num);
    pdf_write(pdf, "  >>\n");
    pdf_write(pdf, ">>\n");
    pdf_write(pdf, "endobj\n");

    return 0;
}

/* Tulis cross-reference table */
static int pdf_write_xref(struct pdf_writer *pdf) {
    int i;

    pdf->xref_offset = ftell(pdf->fp);

    pdf_write(pdf, "xref\n");
    pdf_write(pdf, "0 %d\n", pdf->jumlah_objek + 1);
    pdf_write(pdf, "0000000000 65535 f \n");

    for (i = 1; i <= pdf->jumlah_objek; i++) {
        pdf_write(pdf, "%010ld 00000 n \n", pdf->objects[i].offset);
    }

    return 0;
}

/* Tulis trailer */
static int pdf_write_trailer(struct pdf_writer *pdf) {
    pdf_write(pdf, "trailer\n");
    pdf_write(pdf, "<<\n");
    pdf_write(pdf, "  /Size %d\n", pdf->jumlah_objek + 1);
    pdf_write(pdf, "  /Root %d 0 R\n", pdf->catalog_obj_num);
    pdf_write(pdf, "  /Info <<\n");
    pdf_write(pdf, "    /Title (%s)\n", "TABEL Export");
    pdf_write(pdf, "    /Creator (TABEL v3.0)\n");
    pdf_write(pdf, "    /Producer (TABEL PDF Generator)\n");
    pdf_write(pdf, "  >>\n");
    pdf_write(pdf, ">>\n");
    pdf_write(pdf, "startxref\n");
    pdf_write(pdf, "%ld\n", pdf->xref_offset);
    pdf_write(pdf, "%%%%EOF\n");

    return 0;
}

/* ============================================================
 * FUNGSI RENDERING KE CONTENT STREAM
 * ============================================================ */

/* Struktur untuk mengakumulasi content stream */
struct content_builder {
    char *buf;
    int size;
    int capacity;
};

/* Inisialisasi content builder */
static void cb_init(struct content_builder *cb, char *buffer, int capacity) {
    cb->buf = buffer;
    cb->size = 0;
    cb->capacity = capacity;
    cb->buf[0] = '\0';
}

/* Tambah teks ke content builder */
static void cb_append(struct content_builder *cb, const char *fmt, ...) {
    va_list args;
    int written;
    int remaining;

    if (cb->size >= cb->capacity - 1) return;

    remaining = cb->capacity - cb->size - 1;
    va_start(args, fmt);
    written = vsnprintf(cb->buf + cb->size, (size_t)remaining, fmt, args);
    va_end(args);

    if (written > 0) {
        cb->size += written;
        if (cb->size >= cb->capacity) {
            cb->size = cb->capacity - 1;
        }
        cb->buf[cb->size] = '\0';
    }
}

/* Set warna stroke (garis) */
static void cb_set_stroke_color(struct content_builder *cb, double r, 
                                 double g, double b) {
    cb_append(cb, "%.3f %.3f %.3f RG\n", r, g, b);
}

/* Set warna fill (isi) */
static void cb_set_fill_color(struct content_builder *cb, double r,
                               double g, double b) {
    cb_append(cb, "%.3f %.3f %.3f rg\n", r, g, b);
}

/* Pindah ke posisi */
static void cb_move_to(struct content_builder *cb, double x, double y) {
    cb_append(cb, "%.2f %.2f m\n", x, y);
}

/* Gambar garis ke posisi */
static void cb_line_to(struct content_builder *cb, double x, double y) {
    cb_append(cb, "%.2f %.2f l\n", x, y);
}

/* Stroke path */
static void cb_stroke(struct content_builder *cb) {
    cb_append(cb, "S\n");
}

/* Fill path */
static void cb_fill(struct content_builder *cb) {
    cb_append(cb, "f\n");
}

/* Set lebar garis */
static void cb_set_line_width(struct content_builder *cb, double width) {
    cb_append(cb, "%.2f w\n", width);
}

/* Gambar rectangle */
static void cb_rect(struct content_builder *cb, double x, double y,
                    double w, double h) {
    cb_append(cb, "%.2f %.2f %.2f %.2f re\n", x, y, w, h);
}

/* PERBAIKAN: Gambar teks dengan warna */
static void cb_text(struct content_builder *cb, double x, double y,
                    const char *teks, double font_size, 
                    double r, double g, double b) {
    char escaped[MAKS_TEKS * 2];

    pdf_escape_string(teks, escaped, sizeof(escaped));
    cb_append(cb, "BT\n");
    cb_append(cb, "/F1 %.2f Tf\n", font_size);
    cb_append(cb, "%.3f %.3f %.3f rg\n", r, g, b);
    cb_append(cb, "%.2f %.2f Td\n", x, y);
    cb_append(cb, "(%s) Tj\n", escaped);
    cb_append(cb, "ET\n");
}

/* PERBAIKAN: Gambar teks dengan background */
static void cb_text_with_bg(struct content_builder *cb, double x, double y,
                             const char *teks, double font_size,
                             double fg_r, double fg_g, double fg_b,
                             double bg_r, double bg_g, double bg_b,
                             double cell_width, double cell_height,
                             int has_bg) {
    char escaped[MAKS_TEKS * 2];
    double text_width;
    double bg_x, bg_y;

    pdf_escape_string(teks, escaped, sizeof(escaped));
    
    /* Gambar background terlebih dahulu jika ada */
    if (has_bg && bg_r < 0.99 && bg_g < 0.99 && bg_b < 0.99) {
        bg_x = x - 1;
        bg_y = y - font_size * 0.2;
        cb_set_fill_color(cb, bg_r, bg_g, bg_b);
        cb_rect(cb, bg_x, bg_y - cell_height + font_size * 0.2, 
                cell_width + 2, cell_height);
        cb_fill(cb);
    }
    
    /* Gambar teks */
    cb_append(cb, "BT\n");
    cb_append(cb, "/F1 %.2f Tf\n", font_size);
    cb_append(cb, "%.3f %.3f %.3f rg\n", fg_r, fg_g, fg_b);
    cb_append(cb, "%.2f %.2f Td\n", x, y - font_size);
    cb_append(cb, "(%s) Tj\n", escaped);
    cb_append(cb, "ET\n");
}

/* Gambar garis horizontal */
static void cb_hline(struct content_builder *cb, double x1, double x2,
                     double y, double width, double r, double g, double b) {
    cb_set_line_width(cb, width);
    cb_set_stroke_color(cb, r, g, b);
    cb_move_to(cb, x1, y);
    cb_line_to(cb, x2, y);
    cb_stroke(cb);
}

/* Gambar garis vertikal */
static void cb_vline(struct content_builder *cb, double x, double y1,
                     double y2, double width, double r, double g, double b) {
    cb_set_line_width(cb, width);
    cb_set_stroke_color(cb, r, g, b);
    cb_move_to(cb, x, y1);
    cb_line_to(cb, x, y2);
    cb_stroke(cb);
}

/* ============================================================
 * FUNGSI RENDERING TABEL
 * ============================================================ */

/* Hitung layout kolom untuk halaman */
static double hitung_lebar_kolom_total(const struct buffer_tabel *buf,
                                        int kolom_mulai, int kolom_akhir,
                                        double skala) {
    double total = 0;
    int c;

    for (c = kolom_mulai; c <= kolom_akhir; c++) {
        total += (double)buf->lebar_kolom[c] * skala;
    }

    return total;
}

/* Hitung baris per halaman */
static int hitung_baris_per_halaman(const struct buffer_tabel *buf,
                                     struct pdf_config *cfg) {
    double area_tinggi;
    int baris;

    area_tinggi = cfg->tinggi_kertas - cfg->margin_atas - cfg->margin_bawah;

    /* Kurangi untuk header jika aktif */
    if (cfg->tampilkan_header) {
        area_tinggi -= cfg->jarak_baris * 1.5;
    }

    baris = (int)(area_tinggi / cfg->jarak_baris);

    return baris;
}

/* Hitung kolom yang muat per halaman */
static int hitung_kolom_per_halaman(const struct buffer_tabel *buf,
                                     struct pdf_config *cfg,
                                     int kolom_mulai,
                                     double *lebar_total) {
    double area_lebar;
    double lebar_kolom_buf;
    int c;
    int kolom_akhir = kolom_mulai;

    area_lebar = cfg->lebar_kertas - cfg->margin_kiri - cfg->margin_kanan;

    /* Kurangi untuk nomor baris jika aktif */
    if (cfg->tampilkan_nomor_baris) {
        area_lebar -= 30.0;  /* Lebar untuk nomor baris */
    }

    *lebar_total = 0;

    for (c = kolom_mulai; c < buf->cfg.kolom; c++) {
        lebar_kolom_buf = (double)buf->lebar_kolom[c] * cfg->skala_sel;
        if (*lebar_total + lebar_kolom_buf > area_lebar) {
            break;
        }
        *lebar_total += lebar_kolom_buf;
        kolom_akhir = c;
    }

    return kolom_akhir;
}

/* Render header kolom */
static void render_header_kolom(struct content_builder *cb,
                                 const struct buffer_tabel *buf,
                                 int kolom_mulai, int kolom_akhir,
                                 double x_start, double y_start,
                                 struct pdf_config *cfg) {
    double x = x_start;
    double y = y_start;
    char label[8];
    int c;

    if (!cfg->tampilkan_header) return;

    /* Background header */
    cb_set_fill_color(cb, 0.85, 0.85, 0.85);
    cb_rect(cb, x_start, y_start - cfg->jarak_baris + 2,
            hitung_lebar_kolom_total(buf, kolom_mulai, kolom_akhir, cfg->skala_sel),
            cfg->jarak_baris - 2);
    cb_append(cb, "f\n");

    /* Label kolom */
    for (c = kolom_mulai; c <= kolom_akhir; c++) {
        double kolom_lebar = (double)buf->lebar_kolom[c] * cfg->skala_sel;

        label[0] = (char)('A' + c);
        label[1] = '\0';

        cb_text(cb, x + kolom_lebar / 2 - 3, y - cfg->ukuran_font,
                label, cfg->ukuran_font, 0, 0, 0);

        x += kolom_lebar;
    }

    /* Garis bawah header */
    cb_hline(cb, x_start, x, y_start - cfg->jarak_baris + 2, 1.0, 0, 0, 0);
}

/* Render nomor baris */
static void render_nomor_baris(struct content_builder *cb,
                                int baris_nomor,
                                double x, double y,
                                struct pdf_config *cfg) {
    char num[16];

    if (!cfg->tampilkan_nomor_baris) return;

    snprintf(num, sizeof(num), "%d", baris_nomor + 1);
    cb_text(cb, x, y - cfg->ukuran_font, num, cfg->ukuran_font * 0.8,
            0.5, 0.5, 0.5);
}

/* ============================================================
 * PERBAIKAN: Render isi sel dengan warna depan dan latar
 * ============================================================ */
static void render_isi_sel(struct content_builder *cb,
                            const struct buffer_tabel *buf,
                            int kolom, int baris,
                            double x, double y,
                            double lebar_kolom,
                            struct pdf_config *cfg) {
    const char *teks;
    char teks_display[MAKS_TEKS];
    double fg_r = 0, fg_g = 0, fg_b = 0;
    double bg_r = 1, bg_g = 1, bg_b = 1;
    int has_bg = 0;
    perataan_teks perataan;
    double teks_lebar;
    double x_offset = 0;
    warna_depan wd = WARNAD_DEFAULT;
    warna_latar wl = WARNAL_DEFAULT;

    /* Skip jika bagian dari merge cell tapi bukan anchor */
    if (apakah_ada_gabung_buf(buf, kolom, baris) &&
        !apakah_sel_anchor_buf(buf, kolom, baris)) {
        return;
    }

    teks = buf->isi[baris][kolom];
    perataan = buf->perataan_sel[baris][kolom];
    
    /* PERBAIKAN: Ambil warna dari struktur warna_sel */
    if (buf->warna_sel) {
        struct warna_kombinasi *wc = &buf->warna_sel[baris][kolom];
        wd = wc->depan;
        wl = wc->latar;
    }

    /* Jika formula, evaluasi */
    if (teks[0] == '=') {
        double val;
        int adalah_angka;
        char err[64];

        if (evaluasi_string(buf, kolom, baris, teks, &val, &adalah_angka,
                            err, sizeof(err)) == 0 && adalah_angka) {
            snprintf(teks_display, sizeof(teks_display), "%.10g", val);
            teks = teks_display;
        } else {
            teks = "#ERR";
        }
    }

    /* PERBAIKAN: Konversi warna depan dan latar */
    if (cfg->tampilkan_warna) {
        warna_depan_ke_pdf_rgb(wd, &fg_r, &fg_g, &fg_b);
    }
    
    if (cfg->tampilkan_background && wl != WARNAL_DEFAULT) {
        warna_latar_ke_pdf_rgb(wl, &bg_r, &bg_g, &bg_b);
        has_bg = 1;
    }

    /* Hitung offset berdasarkan perataan */
    teks_lebar = hitung_lebar_teks(teks, cfg->ukuran_font);

    if (perataan == RATA_TENGAH) {
        x_offset = (lebar_kolom - teks_lebar) / 2;
        if (x_offset < 0) x_offset = 0;
    } else if (perataan == RATA_KANAN) {
        x_offset = lebar_kolom - teks_lebar - 3;
        if (x_offset < 0) x_offset = 0;
    } else {
        x_offset = 3;  /* Padding kiri */
    }

    /* PERBAIKAN: Gunakan fungsi dengan background support */
    cb_text_with_bg(cb, x + x_offset, y, teks, cfg->ukuran_font,
                    fg_r, fg_g, fg_b, bg_r, bg_g, bg_b,
                    lebar_kolom, cfg->jarak_baris, has_bg);
}

/* Render garis grid */
static void render_grid(struct content_builder *cb,
                        const struct buffer_tabel *buf,
                        int kolom_mulai, int kolom_akhir,
                        int baris_mulai, int baris_akhir,
                        double x_start, double y_start,
                        struct pdf_config *cfg) {
    double x, y;
    double lebar_total;
    double tinggi_total;
    int c, r;
    double kolom_lebar;

    if (!cfg->tampilkan_grid) return;

    /* Hitung total lebar dan tinggi */
    lebar_total = hitung_lebar_kolom_total(buf, kolom_mulai, kolom_akhir,
                                            cfg->skala_sel);
    tinggi_total = (double)(baris_akhir - baris_mulai + 1) * cfg->jarak_baris;

    /* Garis vertikal */
    x = x_start;
    cb_set_line_width(cb, 0.5);
    cb_set_stroke_color(cb, 0.7, 0.7, 0.7);

    for (c = kolom_mulai; c <= kolom_akhir; c++) {
        cb_vline(cb, x, y_start, y_start - tinggi_total, 0.5, 0.7, 0.7, 0.7);
        x += (double)buf->lebar_kolom[c] * cfg->skala_sel;
    }
    cb_vline(cb, x, y_start, y_start - tinggi_total, 0.5, 0.7, 0.7, 0.7);

    /* Garis horizontal */
    y = y_start;
    for (r = baris_mulai; r <= baris_akhir; r++) {
        cb_hline(cb, x_start, x_start + lebar_total, y, 0.5, 0.7, 0.7, 0.7);
        y -= cfg->jarak_baris;
    }
    cb_hline(cb, x_start, x_start + lebar_total, y, 0.5, 0.7, 0.7, 0.7);
}

/* ============================================================
 * PERBAIKAN: Render border custom dengan warna depan dan latar
 * ============================================================ */
static void render_border(struct content_builder *cb,
                           const struct buffer_tabel *buf,
                           int kolom_mulai, int kolom_akhir,
                           int baris_mulai, int baris_akhir,
                           double x_start, double y_start,
                           struct pdf_config *cfg) {
    double x, y;
    double kolom_lebar;
    int c, r;
    struct info_border *border;
    double br, bg, bb;

    if (!cfg->tampilkan_border) return;

    x = x_start;

    for (c = kolom_mulai; c <= kolom_akhir; c++) {
        kolom_lebar = (double)buf->lebar_kolom[c] * cfg->skala_sel;
        y = y_start;

        for (r = baris_mulai; r <= baris_akhir; r++) {
            border = &buf->border_sel[r][c];

            /* Hanya render jika ada border */
            if (border->top || border->bottom || border->left || border->right) {
                /* PERBAIKAN: Konversi warna border (gunakan warna depan) */
                if (cfg->tampilkan_warna) {
                    warna_depan_ke_pdf_rgb(border->warna_depan, &br, &bg, &bb);
                } else {
                    br = 0; bg = 0; bb = 0;
                }

                /* Border atas */
                if (border->top) {
                    cb_hline(cb, x, x + kolom_lebar, y, 1.5, br, bg, bb);
                }

                /* Border bawah */
                if (border->bottom) {
                    cb_hline(cb, x, x + kolom_lebar, y - cfg->jarak_baris,
                             1.5, br, bg, bb);
                }

                /* Border kiri */
                if (border->left) {
                    cb_vline(cb, x, y, y - cfg->jarak_baris, 1.5, br, bg, bb);
                }

                /* Border kanan */
                if (border->right) {
                    cb_vline(cb, x + kolom_lebar, y, y - cfg->jarak_baris,
                             1.5, br, bg, bb);
                }
            }

            y -= cfg->jarak_baris;
        }

        x += kolom_lebar;
    }
}

/* Render satu halaman tabel */
static void render_halaman_tabel(struct content_builder *cb,
                                  const struct buffer_tabel *buf,
                                  int kolom_mulai, int kolom_akhir,
                                  int baris_mulai, int baris_akhir,
                                  struct pdf_config *cfg) {
    double x_start = cfg->margin_kiri;
    double y_start = cfg->tinggi_kertas - cfg->margin_atas;
    double x, y;
    double kolom_lebar;
    int c, r;

    /* Jika tampilkan nomor baris, geser x_start */
    if (cfg->tampilkan_nomor_baris) {
        x_start += 30.0;
    }

    /* Render header kolom */
    if (cfg->tampilkan_header) {
        render_header_kolom(cb, buf, kolom_mulai, kolom_akhir,
                           x_start, y_start, cfg);
        y_start -= cfg->jarak_baris * 1.5;
    }

    /* Render grid */
    render_grid(cb, buf, kolom_mulai, kolom_akhir, baris_mulai, baris_akhir,
                x_start, y_start, cfg);

    /* Render isi baris */
    y = y_start;
    for (r = baris_mulai; r <= baris_akhir && r < buf->cfg.baris; r++) {
        x = x_start;

        /* Render nomor baris */
        render_nomor_baris(cb, r, cfg->margin_kiri + 5, y, cfg);

        /* Render isi kolom */
        for (c = kolom_mulai; c <= kolom_akhir && c < buf->cfg.kolom; c++) {
            kolom_lebar = (double)buf->lebar_kolom[c] * cfg->skala_sel;

            render_isi_sel(cb, buf, c, r, x, y, kolom_lebar, cfg);

            x += kolom_lebar;
        }

        y -= cfg->jarak_baris;
    }

    /* Render border custom */
    render_border(cb, buf, kolom_mulai, kolom_akhir, baris_mulai, baris_akhir,
                  x_start, y_start, cfg);

    /* Render nomor halaman (footer) */
    /* Akan ditambahkan di render utama */
}

/* ============================================================
 * FUNGSI UTAMA EXPORT PDF
 * ============================================================ */

/* Prompt untuk konfigurasi PDF */
void prompt_pdf_config(struct pdf_config *cfg) {
    char input[64];
    int pilihan;

    /* Salin default */
    memcpy(cfg, &pdf_cfg_default, sizeof(struct pdf_config));

    /* Tampilkan prompt di status bar */
    snprintf(pesan_status, sizeof(pesan_status),
             "Export PDF - Pilih kertas: 1=A4, 2=Letter, 3=Legal, 4=A3, 5=Tabloid");

    /* Baca input */
    minta_nama_berkas(input, sizeof(input), "Kertas (1-5): ");
    pilihan = atoi(input);

    switch (pilihan) {
        case 1: cfg->kertas = KERTAS_A4; break;
        case 2: cfg->kertas = KERTAS_LETTER; break;
        case 3: cfg->kertas = KERTAS_LEGAL; break;
        case 4: cfg->kertas = KERTAS_A3; break;
        case 5: cfg->kertas = KERTAS_TABLOID; break;
        default: cfg->kertas = KERTAS_A4; break;
    }

    /* Tanya orientasi */
    minta_nama_berkas(input, sizeof(input), "Orientasi (1=Portrait, 2=Landscape): ");
    cfg->orientasi = (atoi(input) == 2) ? ORIENTASI_LANDSCAPE : ORIENTASI_PORTRAIT;

    /* Update dimensi */
    dapatkan_dimensi_kertas(cfg->kertas, cfg->orientasi,
                            &cfg->lebar_kertas, &cfg->tinggi_kertas);

    /* Tanya ukuran font */
    minta_nama_berkas(input, sizeof(input), "Ukuran font (default 10): ");
    if (input[0] != '\0') {
        cfg->ukuran_font = atof(input);
        if (cfg->ukuran_font < 6) cfg->ukuran_font = 6;
        if (cfg->ukuran_font > 24) cfg->ukuran_font = 24;
    }
    
    /* PERBAIKAN: Tanya apakah ingin warna */
    minta_nama_berkas(input, sizeof(input), "Tampilkan warna? (y/n, default y): ");
    if (input[0] == 'n' || input[0] == 'N') {
        cfg->tampilkan_warna = 0;
        cfg->tampilkan_background = 0;
    }
    
    /* Tanya apakah ingin background */
    if (cfg->tampilkan_warna) {
        minta_nama_berkas(input, sizeof(input), "Tampilkan warna latar? (y/n, default y): ");
        if (input[0] == 'n' || input[0] == 'N') {
            cfg->tampilkan_background = 0;
        }
    }

    /* Hitung jarak baris otomatis */
    cfg->jarak_baris = cfg->ukuran_font * 1.4;
}

/* Export buffer ke PDF */
int simpan_pdf_v2(const struct buffer_tabel *buf, const char *fn,
                   struct pdf_config *cfg) {
    struct pdf_writer pdf;
    struct content_builder cb;
    int page_objs[PDF_MAX_PAGES];
    int content_objs[PDF_MAX_PAGES];
    int num_pages = 0;
    int baris_per_halaman;
    int baris_mulai, baris_akhir;
    int kolom_mulai, kolom_akhir;
    double lebar_kolom_total;
    int halaman;
    char *content_buffer;
    int i;

    if (!buf || !fn || !cfg) return -1;

    /* Alokasi buffer content */
    content_buffer = malloc(PDF_STREAM_BUF);
    if (!content_buffer) return -1;

    /* Buka file */
    memset(&pdf, 0, sizeof(pdf));
    pdf.fp = fopen(fn, "wb");
    if (!pdf.fp) {
        free(content_buffer);
        return -1;
    }

    /* Hitung baris per halaman */
    baris_per_halaman = hitung_baris_per_halaman(buf, cfg);
    if (baris_per_halaman < 1) baris_per_halaman = 1;

    /* Hitung total halaman (baris) */
    num_pages = (buf->cfg.baris + baris_per_halaman - 1) / baris_per_halaman;
    if (num_pages > PDF_MAX_PAGES) num_pages = PDF_MAX_PAGES;

    /* Tulis header PDF */
    pdf_write_header(&pdf);

    /* Tulis font */
    pdf_write_font(&pdf);

    /* Tentukan kolom yang muat */
    kolom_mulai = 0;
    kolom_akhir = hitung_kolom_per_halaman(buf, cfg, kolom_mulai,
                                            &lebar_kolom_total);
    if (kolom_akhir < kolom_mulai) kolom_akhir = kolom_mulai;

    /* Render setiap halaman */
    halaman = 0;
    for (baris_mulai = 0; baris_mulai < buf->cfg.baris && halaman < PDF_MAX_PAGES;
         baris_mulai += baris_per_halaman) {

        baris_akhir = baris_mulai + baris_per_halaman - 1;
        if (baris_akhir >= buf->cfg.baris) baris_akhir = buf->cfg.baris - 1;

        /* Inisialisasi content builder */
        cb_init(&cb, content_buffer, PDF_STREAM_BUF);

        /* Render halaman */
        render_halaman_tabel(&cb, buf, kolom_mulai, kolom_akhir,
                            baris_mulai, baris_akhir, cfg);

        /* Tulis content stream */
        pdf_write_content_stream(&pdf, cb.buf, cb.size, &content_objs[halaman]);

        /* Tulis page object */
        pdf_write_page(&pdf, content_objs[halaman],
                       cfg->lebar_kertas, cfg->tinggi_kertas,
                       &page_objs[halaman]);

        halaman++;
    }

    num_pages = halaman;

    /* Tulis pages tree */
    pdf_write_pages_tree(&pdf, page_objs, num_pages);

    /* Tulis catalog */
    pdf_write_catalog(&pdf);

    /* Tulis cross-reference dan trailer */
    pdf_write_xref(&pdf);
    pdf_write_trailer(&pdf);

    /* Tutup file */
    fclose(pdf.fp);
    free(content_buffer);

    snprintf(pesan_status, sizeof(pesan_status),
             "PDF disimpan: %d halaman, %s %s, warna=%s",
             num_pages,
             cfg->kertas == KERTAS_A4 ? "A4" :
             cfg->kertas == KERTAS_LETTER ? "Letter" :
             cfg->kertas == KERTAS_LEGAL ? "Legal" :
             cfg->kertas == KERTAS_A3 ? "A3" : "Tabloid",
             cfg->orientasi == ORIENTASI_PORTRAIT ? "Portrait" : "Landscape",
             cfg->tampilkan_warna ? "ya" : "tidak");

    return 0;
}

/* Fungsi wrapper untuk kompatibilitas */
int simpan_pdf_baru(const struct buffer_tabel *buf, const char *fn) {
    struct pdf_config cfg;

    /* Gunakan konfigurasi default */
    memcpy(&cfg, &pdf_cfg_default, sizeof(cfg));
    dapatkan_dimensi_kertas(cfg.kertas, cfg.orientasi,
                            &cfg.lebar_kertas, &cfg.tinggi_kertas);

    return simpan_pdf_v2(buf, fn, &cfg);
}

/* Export dengan prompt konfigurasi */
int export_pdf_interaktif(const struct buffer_tabel *buf, const char *fn) {
    struct pdf_config cfg;

    prompt_pdf_config(&cfg);
    return simpan_pdf_v2(buf, fn, &cfg);
}

/* ============================================================
 * FUNGSI TAMBAHAN: PREVIEW DAN INFO
 * ============================================================ */

/* Hitung jumlah halaman tanpa menulis file */
int hitung_jumlah_halaman_pdf(const struct buffer_tabel *buf,
                               struct pdf_config *cfg) {
    int baris_per_halaman;

    baris_per_halaman = hitung_baris_per_halaman(buf, cfg);
    if (baris_per_halaman < 1) baris_per_halaman = 1;

    return (buf->cfg.baris + baris_per_halaman - 1) / baris_per_halaman;
}

/* Dapatkan info export PDF */
void info_export_pdf(const struct buffer_tabel *buf,
                     struct pdf_config *cfg,
                     char *info, size_t info_size) {
    int halaman;
    double lebar_cm, tinggi_cm;

    halaman = hitung_jumlah_halaman_pdf(buf, cfg);
    lebar_cm = cfg->lebar_kertas * 2.54 / 72.0;
    tinggi_cm = cfg->tinggi_kertas * 2.54 / 72.0;

    snprintf(info, info_size,
             "Export PDF Info:\n"
             "  Kertas: %s\n"
             "  Orientasi: %s\n"
             "  Ukuran: %.1f x %.1f cm\n"
             "  Jumlah halaman: %d\n"
             "  Baris per halaman: %d\n"
             "  Ukuran font: %.1f pt\n"
             "  Warna: %s\n"
             "  Warna latar: %s",
             cfg->kertas == KERTAS_A4 ? "A4" :
             cfg->kertas == KERTAS_LETTER ? "Letter" :
             cfg->kertas == KERTAS_LEGAL ? "Legal" :
             cfg->kertas == KERTAS_A3 ? "A3" : "Tabloid",
             cfg->orientasi == ORIENTASI_PORTRAIT ? "Portrait" : "Landscape",
             lebar_cm, tinggi_cm,
             halaman,
             hitung_baris_per_halaman(buf, cfg),
             cfg->ukuran_font,
             cfg->tampilkan_warna ? "ya" : "tidak",
             cfg->tampilkan_background ? "ya" : "tidak");
}

/* Set konfigurasi default */
void set_pdf_config_default(struct pdf_config *cfg) {
    memcpy(cfg, &pdf_cfg_default, sizeof(struct pdf_config));
    dapatkan_dimensi_kertas(cfg->kertas, cfg->orientasi,
                            &cfg->lebar_kertas, &cfg->tinggi_kertas);
}
