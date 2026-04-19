/* ============================================================
 * TABEL v3.0
 * Berkas: berkas.c - I/O Berkas, UTF-8, wcwidth
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * UTF-8 & wcwidth
 * ============================================================ */

static int adalah_kontinu(unsigned char c)
{
    return (c & 0xC0) == 0x80;
}

int panjang_cp_utf8(const char *p)
{
    unsigned char c;

    if (!p) return 1;
    c = (unsigned char)p[0];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) {
        if (!adalah_kontinu((unsigned char)p[1])) return 1;
        if ((c & 0xFE) == 0xC0) return 1;
        return 2;
    }
    if ((c & 0xF0) == 0xE0) {
        if (!adalah_kontinu((unsigned char)p[1])) return 1;
        if (!adalah_kontinu((unsigned char)p[2])) return 1;
        return 3;
    }
    if ((c & 0xF8) == 0xF0) {
        if (!adalah_kontinu((unsigned char)p[1])) return 1;
        if (!adalah_kontinu((unsigned char)p[2])) return 1;
        if (!adalah_kontinu((unsigned char)p[3])) return 1;
        return 4;
    }
    return 1;
}

unsigned int dekode_cp_utf8(const char *p, int *len)
{
    unsigned char c;
    unsigned int cp = 0;
    int l = panjang_cp_utf8(p);

    if (len) *len = l;
    if (!p) return 0;
    c = (unsigned char)p[0];
    if (l == 1) return (unsigned int)c;

    if ((c & 0xE0) == 0xC0) {
        cp = ((unsigned int)(p[0] & 0x1F) << 6) |
             (unsigned int)(p[1] & 0x3F);
    } else if ((c & 0xF0) == 0xE0) {
        cp = ((unsigned int)(p[0] & 0x0F) << 12) |
             ((unsigned int)(p[1] & 0x3F) << 6) |
             (unsigned int)(p[2] & 0x3F);
    } else if ((c & 0xF8) == 0xF0) {
        cp = ((unsigned int)(p[0] & 0x07) << 18) |
             ((unsigned int)(p[1] & 0x3F) << 12) |
             ((unsigned int)(p[2] & 0x3F) << 6) |
             (unsigned int)(p[3] & 0x3F);
    } else {
        cp = c;
    }
    return cp;
}

int apakah_menggabung(unsigned int cp)
{
    if (cp == 0x200D) return 1;
    if (cp >= 0x0300 && cp <= 0x036F) return 1;
    if (cp >= 0x1AB0 && cp <= 0x1AFF) return 1;
    if (cp >= 0x1DC0 && cp <= 0x1DFF) return 1;
    if (cp >= 0x20D0 && cp <= 0x20FF) return 1;
    if (cp >= 0xFE20 && cp <= 0xFE2F) return 1;
    return 0;
}

static int lebar_ucs(unsigned int cp)
{
    if (cp == 0) return 0;
    if (cp < 0x20 || (cp >= 0x7F && cp < 0xA0)) return 0;
    if (apakah_menggabung(cp)) return 0;

    /* Karakter lebar ganda (CJK, emoji, dll) */
    if (cp >= 0x1100 && cp <= 0x115F) return 2;
    if (cp >= 0x2E80 && cp <= 0xA4CF) return 2;
    if (cp >= 0xAC00 && cp <= 0xD7A3) return 2;
    if (cp >= 0xF900 && cp <= 0xFAFF) return 2;
    if (cp >= 0xFE10 && cp <= 0xFE19) return 2;
    if (cp >= 0xFE30 && cp <= 0xFE6F) return 2;
    if (cp >= 0xFF00 && cp <= 0xFF60) return 2;
    if (cp >= 0xFFE0 && cp <= 0xFFE6) return 2;
    if (cp >= 0x1F300 && cp <= 0x1FAFF) return 2;

    return 1;
}

int lebar_tampilan_string_utf8(const char *s)
{
    int w = 0;
    int l;
    unsigned int cp;
    const char *p = s;

    if (!s) return 0;
    while (*p) {
        cp = dekode_cp_utf8(p, &l);
        w += lebar_ucs(cp);
        p += l;
    }
    return w;
}

int lebar_tampilan_utf8(const char *p)
{
    return lebar_tampilan_string_utf8(p);
}

/* ============================================================
 * Helper String
 * ============================================================ */

int salin_str_aman(char *dst, const char *src, size_t n)
{
    size_t i = 0;

    if (!dst || !src || n == 0) return 0;
    while (i + 1 < n && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
    return (int)i;
}

int gabung_str_aman(char *dst, const char *src, size_t n)
{
    size_t dlen;
    size_t i = 0;

    if (!dst || !src || n == 0) return 0;
    dlen = strlen(dst);
    if (dlen >= n) return (int)dlen;

    while (dlen + i + 1 < n && src[i] != '\0') {
        dst[dlen + i] = src[i];
        i++;
    }
    dst[dlen + i] = '\0';
    return (int)(dlen + i);
}

/* ============================================================
 * fsync Aman
 * ============================================================ */

int fsync_berkas(FILE *f)
{
    int fd;

    if (!f) return -1;
    fd = fileno(f);
    if (fd < 0) return -1;
    if (fflush(f) != 0) return -1;
    if (fsync(fd) != 0) return -1;
    return 0;
}

/* ============================================================
 * Prompt Nama Berkas
 * ============================================================ */

void minta_nama_berkas(char *buf, size_t n, const char *label)
{
    char input_buf[MAKS_TEKS];
    int len = 0;
    int cursor = 0;
    int cols;
    int rows;
    int pos_x_start;
    int ret;
    int i;

    input_buf[0] = '\0';
    cols = ambil_lebar_terminal();
    rows = ambil_tinggi_terminal();
    pos_x_start = (int)strlen(label) + 2;

    /* Bersihkan status bar */
    atur_posisi_kursor(1, rows);
    for (i = 0; i < cols; i++) {
        write(STDOUT_FILENO, " ", 1);
    }

    /* Tulis label prompt di status bar */
    atur_posisi_kursor(2, rows);
    printf("%s", label);
    write(STDOUT_FILENO, "\033[?25h", 6);
    fflush(stdout);

    /* Panggil editor input di status bar */
    ret = baca_input_edit(input_buf, &len, &cursor, pos_x_start, rows);

    /* ESC ditekan - batal */
    if (ret == KEY_BATAL) {
        buf[0] = '\0';
        return;
    }

    /* Tab ditekan - autocomplete */
    if (ret == '\t') {
        lakukan_autocomplete(input_buf, &len, &cursor);
        /* Redraw hasil autocomplete */
        atur_posisi_kursor(1, rows);
        for (i = 0; i < cols; i++) {
            write(STDOUT_FILENO, " ", 1);
        }
        atur_posisi_kursor(2, rows);
        printf("%s %s", label, input_buf);
        write(STDOUT_FILENO, "\033[?25l", 6);
        fflush(stdout);
    }

    salin_str_aman(buf, input_buf, n);
}

/* ============================================================
 * I/O TXT
 * ============================================================ */

int simpan_txt(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;

    if (!buf || !fn) return -1;
    f = fopen(fn, "w");
    if (!f) return -1;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (fprintf(f, "%s", buf->isi[r][c]) < 0) {
                fclose(f);
                return -1;
            }
            if (c < buf->cfg.kolom - 1) fputc('\t', f);
        }
        fputc('\n', f);
    }

    if (fsync_berkas(f) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int buka_txt(struct buffer_tabel **pbuf, const char *fn)
{
    FILE *f;
    char line[MAKS_TEKS * MAKS_KOLOM];
    int baris = 0;
    int kolom = 0;
    int c;
    int r;
    char *tok;

    if (!pbuf || !fn) return -1;
    f = fopen(fn, "r");
    if (!f) return -1;

    /* Hitung dimensi */
    while (fgets(line, sizeof(line), f)) {
        baris++;
        c = 0;
        tok = strtok(line, "\t\n");
        while (tok) {
            if (strlen(tok) > 0) c++;
            tok = strtok(NULL, "\t\n");
        }
        if (c > kolom) kolom = c;
    }
    fclose(f);

    if (baris <= 0) return -1;
    if (kolom <= 0) kolom = 1;

    bebas_buffer(*pbuf);
    *pbuf = buat_buffer(baris, kolom);

    f = fopen(fn, "r");
    if (!f) return -1;

    r = 0;
    while (fgets(line, sizeof(line), f) && r < (*pbuf)->cfg.baris) {
        c = 0;
        tok = strtok(line, "\t\n");
        while (tok && c < (*pbuf)->cfg.kolom) {
            salin_str_aman((*pbuf)->isi[r][c], tok, MAKS_TEKS);
            tok = strtok(NULL, "\t\n");
            c++;
        }
        r++;
    }
    fclose(f);
    return 0;
}

/* ============================================================
 * I/O CSV
 * ============================================================ */

int simpan_csv(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;
    const char *val;
    const char *p;
    int perlu_quote;

    if (!buf || !fn) return -1;
    f = fopen(fn, "w");
    if (!f) return -1;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            val = buf->isi[r][c];
            perlu_quote = (strchr(val, ',') != NULL ||
                          strchr(val, '"') != NULL ||
                          strchr(val, '\n') != NULL);

            if (perlu_quote) fputc('"', f);
            for (p = val; *p; p++) {
                if (*p == '"') fputc('"', f);
                fputc(*p, f);
            }
            if (perlu_quote) fputc('"', f);
            if (c < buf->cfg.kolom - 1) fputc(',', f);
        }
        fputc('\n', f);
    }

    if (fsync_berkas(f) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int buka_csv(struct buffer_tabel **pbuf, const char *fn)
{
    FILE *f;
    char *line;
    size_t line_buf_size;
    int baris = 0;
    int kolom = 0;
    int r;
    int c;
    int last_nonempty;
    int fieldlen;
    int inquote;
    char *p;
    char cell[MAKS_TEKS];
    int idx;
    int quoted;

    if (!pbuf || !fn) return -1;
    f = fopen(fn, "r");
    if (!f) return -1;

    /* Alokasi buffer dinamis untuk baris panjang */
    line_buf_size = MAKS_TEKS * 64;
    line = malloc(line_buf_size);
    if (!line) { fclose(f); return -1; }

    /* Hitung dimensi */
    while (fgets(line, (int)line_buf_size, f)) {
        baris++;
        c = 0;
        last_nonempty = 0;
        p = line;
        inquote = 0;
        fieldlen = 0;
        while (*p) {
            if (*p == '"') inquote = !inquote;
            if (*p == ',' && !inquote) {
                if (fieldlen > 0) last_nonempty = c + 1;
                c++;
                fieldlen = 0;
            } else if (*p != '\n' && *p != '\r') {
                fieldlen++;
            }
            p++;
        }
        if (fieldlen > 0) last_nonempty = c + 1;
        if (last_nonempty > kolom) kolom = last_nonempty;
    }
    fclose(f);

    if (baris <= 0) { free(line); return -1; }
    if (kolom <= 0) kolom = 1;

    /* Gunakan buat_buffer_satu_lembar untuk menghemat memori
     * (hanya 1 lembar, bukan 3 lembar seperti buat_buffer) */
    bebas_buffer(*pbuf);
    *pbuf = buat_buffer_satu_lembar(baris, kolom);
    if (!*pbuf) { free(line); return -1; }

    f = fopen(fn, "r");
    if (!f) { free(line); return -1; }

    r = 0;
    while (fgets(line, (int)line_buf_size, f) && r < (*pbuf)->cfg.baris) {
        c = 0;
        p = line;
        while (*p && c < (*pbuf)->cfg.kolom) {
            idx = 0;
            quoted = 0;
            if (*p == '"') {
                quoted = 1;
                p++;
            }
            while (*p && idx < MAKS_TEKS - 1) {
                if (quoted) {
                    if (*p == '"') {
                        if (*(p + 1) == '"') {
                            cell[idx++] = '"';
                            p += 2;
                            continue;
                        } else {
                            p++;
                            break;
                        }
                    }
                } else {
                    if (*p == ',' || *p == '\n') break;
                }
                cell[idx++] = *p;
                p++;
            }
            cell[idx] = '\0';
            salin_str_aman((*pbuf)->isi[r][c], cell, MAKS_TEKS);
            if (*p == ',') p++;
            c++;
        }
        r++;
    }
    fclose(f);
    free(line);
    return 0;
}

/* ============================================================
 * I/O SQL
 * ============================================================ */

int simpan_sql(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;

    if (!buf || !fn) return -1;
    f = fopen(fn, "w");
    if (!f) return -1;

    fprintf(f, "CREATE TABLE data (\n");
    for (c = 0; c < buf->cfg.kolom; c++) {
        fprintf(f, "  col%d TEXT", c + 1);
        if (c < buf->cfg.kolom - 1) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, ");\n");

    for (r = 0; r < buf->cfg.baris; r++) {
        fprintf(f, "INSERT INTO data VALUES(");
        for (c = 0; c < buf->cfg.kolom; c++) {
            fprintf(f, "'%s'", buf->isi[r][c]);
            if (c < buf->cfg.kolom - 1) fprintf(f, ",");
        }
        fprintf(f, ");\n");
    }

    if (fsync_berkas(f) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int buka_sql(struct buffer_tabel **pbuf, const char *fn)
{
    /* Struktur untuk menyimpan info tabel SQL */
    #define MAKS_TABEL_SQL 256
    struct info_tabel_sql {
        char nama[NAMA_LEMBAR_MAKS];
        int kolom;
        int baris;
        char **nama_kolom;   /* Nama kolom dari CREATE TABLE */
        long pos_create;     /* Posisi file awal CREATE TABLE */
    };

    FILE *f;
    char *line;
    size_t line_buf_size;
    int jumlah_tabel = 0;
    struct info_tabel_sql tabel[MAKS_TABEL_SQL];
    int tabel_aktif = -1;
    int i, r, c;
    char *p, *q;
    char cell[MAKS_TEKS];
    int idx;
    int in_create = 0;
    int paren_depth = 0;
    int in_paren_quote = 0;  /* Quote di dalam tanda kurung CREATE TABLE */
    struct buffer_tabel *buf;
    struct lembar_tabel *lem;

    if (!pbuf || !fn) return -1;
    f = fopen(fn, "r");
    if (!f) return -1;

    line_buf_size = MAKS_TEKS * 64;
    line = malloc(line_buf_size);
    if (!line) { fclose(f); return -1; }

    memset(tabel, 0, sizeof(tabel));

    /* Pass 1: Identifikasi semua tabel dan hitung dimensi */
    while (fgets(line, (int)line_buf_size, f)) {
        /* Lewati whitespace di awal baris untuk deteksi CREATE TABLE */
        p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* Deteksi CREATE TABLE (case-insensitive) */
        if (strncasecmp(p, "CREATE TABLE", 12) == 0) {
            if (jumlah_tabel < MAKS_TABEL_SQL) {
                tabel_aktif = jumlah_tabel;
                tabel[tabel_aktif].kolom = 0;
                tabel[tabel_aktif].baris = 0;
                tabel[tabel_aktif].nama_kolom = NULL;
                tabel[tabel_aktif].pos_create = ftell(f);

                /* Ekstrak nama tabel (p sudah melewati whitespace) */
                p += 12; /* Lewati "CREATE TABLE" */
                while (*p == ' ' || *p == '\t') p++;
                /* Lewati IF NOT EXISTS jika ada */
                if (strncasecmp(p, "IF", 2) == 0) {
                    p += 2; while (*p == ' ') p++;
                    if (strncasecmp(p, "NOT", 3) == 0) {
                        p += 3; while (*p == ' ') p++;
                    }
                    if (strncasecmp(p, "EXISTS", 6) == 0) {
                        p += 6; while (*p == ' ') p++;
                    }
                }
                q = tabel[tabel_aktif].nama;
                while (*p && *p != '(' && *p != ' ' && *p != '\t' && 
                       *p != '\n' && *p != '\r' && q < tabel[tabel_aktif].nama + NAMA_LEMBAR_MAKS - 1) {
                    /* Hapus backtick dan tanda kutip jika ada */
                    if (*p != '`' && *p != '"' && *p != '[' && *p != ']') *q++ = *p;
                    p++;
                }
                *q = '\0';

                /* Cek apakah kolom ada di baris yang sama */
                p = strchr(line, '(');
                if (p) {
                    in_create = 1;
                    paren_depth = 1;
                    in_paren_quote = 0;
                    p++;
                    /* Hitung kolom dari baris ini.
                     * Comma dihitung hanya pada paren_depth == 1
                     * (di dalam kurung CREATE TABLE tapi bukan nested parens
                     *  seperti VARCHAR(255)). Stop saat kurung tutup terakhir. */
                    while (*p) {
                        if (*p == '\'' && !in_paren_quote) in_paren_quote = 1;
                        else if (*p == '\'' && in_paren_quote) in_paren_quote = 0;
                        else if (*p == '(' && !in_paren_quote) paren_depth++;
                        else if (*p == ')' && !in_paren_quote) {
                            paren_depth--;
                            if (paren_depth == 0) {
                                tabel[tabel_aktif].kolom++; /* Kolom terakhir */
                                in_create = 0;
                                jumlah_tabel++;
                                break;
                            }
                        }
                        else if (*p == ',' && paren_depth == 1 && !in_paren_quote) {
                            tabel[tabel_aktif].kolom++;
                        }
                        p++;
                    }
                } else {
                    in_create = 1;
                    paren_depth = 0;
                    in_paren_quote = 0;
                }
            }
            continue;
        }

        /* Lanjutkan multi-line CREATE TABLE */
        if (in_create && tabel_aktif >= 0) {
            p = line;
            in_paren_quote = 0;
            while (*p) {
                if (*p == '\'' && !in_paren_quote) in_paren_quote = 1;
                else if (*p == '\'' && in_paren_quote) in_paren_quote = 0;
                else if (*p == '(' && !in_paren_quote) paren_depth++;
                else if (*p == ')' && !in_paren_quote) {
                    paren_depth--;
                    if (paren_depth == 0) {
                        tabel[tabel_aktif].kolom++; /* Kolom terakhir */
                        in_create = 0;
                        jumlah_tabel++;
                        break;
                    }
                }
                else if (*p == ',' && paren_depth == 1 && !in_paren_quote) {
                    tabel[tabel_aktif].kolom++;
                }
                p++;
            }
            continue;
        }

        /* Deteksi INSERT INTO (case-insensitive, lewati whitespace) */
        if (strncasecmp(p, "INSERT", 6) == 0) {
            char nama_tabel[NAMA_LEMBAR_MAKS];
            int t_idx;
            int kolom_insert = 0;

            /* Ekstrak nama tabel dari INSERT INTO (p sudah melewati whitespace) */
            p += 6; /* Lewati "INSERT" */
            while (*p == ' ') p++;
            if (strncasecmp(p, "INTO", 4) == 0) p += 4;
            while (*p == ' ') p++;
            q = nama_tabel;
            while (*p && *p != '(' && *p != ' ' && *p != '\t' && 
                   *p != '\n' && *p != '\r' && q < nama_tabel + NAMA_LEMBAR_MAKS - 1) {
                if (*p != '`' && *p != '"' && *p != '[' && *p != ']') *q++ = *p;
                p++;
            }
            *q = '\0';

            /* Cari tabel yang sesuai */
            t_idx = -1;
            for (i = 0; i < jumlah_tabel; i++) {
                if (strcmp(tabel[i].nama, nama_tabel) == 0) {
                    t_idx = i;
                    break;
                }
            }

            /* Jika tabel belum terdaftar, buat entri baru */
            if (t_idx < 0) {
                if (jumlah_tabel < MAKS_TABEL_SQL) {
                    t_idx = jumlah_tabel;
                    salin_str_aman(tabel[t_idx].nama, nama_tabel, NAMA_LEMBAR_MAKS);
                    tabel[t_idx].kolom = 0;
                    tabel[t_idx].baris = 0;
                    tabel[t_idx].nama_kolom = NULL;
                    tabel[t_idx].pos_create = 0;
                    jumlah_tabel++;
                } else {
                    continue;
                }
            }

            /* Cari klausa VALUES untuk menghitung kolom */
            if (tabel[t_idx].kolom <= 0) {
                char *values_pos = strstr(line, "VALUES");
                if (!values_pos) values_pos = strstr(line, "values");
                if (!values_pos) {
                    /* Coba case-insensitive manual */
                    char *search = line;
                    while (*search) {
                        if (strncasecmp(search, "VALUES", 6) == 0) {
                            values_pos = search;
                            break;
                        }
                        search++;
                    }
                }
                if (values_pos) {
                    char *open_paren = strchr(values_pos, '(');
                    if (open_paren) {
                        int in_q = 0;
                        kolom_insert = 0;
                        p = open_paren + 1;
                        while (*p && *p != ')') {
                            if (*p == '\'' && !in_q) in_q = 1;
                            else if (*p == '\'' && in_q) {
                                /* Cek escaped quote */
                                if (*(p+1) == '\'') { p += 2; continue; }
                                in_q = 0;
                            }
                            else if (*p == ',' && !in_q) kolom_insert++;
                            p++;
                        }
                        kolom_insert++; /* Kolom terakhir */
                        tabel[t_idx].kolom = kolom_insert;
                    }
                }
            }

            /* Hitung baris - bisa multi-row INSERT */
            {
                char *values_pos = NULL;
                char *search = line;
                while (*search) {
                    if (strncasecmp(search, "VALUES", 6) == 0) {
                        values_pos = search;
                        break;
                    }
                    search++;
                }
                if (values_pos) {
                    /* Hitung jumlah set nilai (baris) dalam satu INSERT */
                    int num_sets = 0;
                    p = values_pos + 6;
                    while (*p) {
                        if (*p == '(') {
                            /* Lewati isi tanda kurung */
                            int depth = 1;
                            int in_q2 = 0;
                            p++;
                            while (*p && depth > 0) {
                                if (*p == '\'' && !in_q2) in_q2 = 1;
                                else if (*p == '\'' && in_q2) {
                                    if (*(p+1) == '\'') { p += 2; continue; }
                                    in_q2 = 0;
                                }
                                else if (*p == '(' && !in_q2) depth++;
                                else if (*p == ')' && !in_q2) depth--;
                                if (depth > 0) p++;
                            }
                            num_sets++;
                        }
                        p++;
                    }
                    if (num_sets <= 0) num_sets = 1;
                    tabel[t_idx].baris += num_sets;
                } else {
                    tabel[t_idx].baris++;
                }
            }
        }
    }

    fclose(f);

    if (jumlah_tabel <= 0) {
        free(line);
        return -1;
    }

    /* Validasi dan pastikan semua tabel punya kolom */
    for (i = 0; i < jumlah_tabel; i++) {
        if (tabel[i].kolom <= 0) tabel[i].kolom = 1;
        if (tabel[i].baris <= 0) tabel[i].baris = 1;
    }

    /* Buat buffer dengan satu lembar untuk tabel pertama */
    buf = buat_buffer_satu_lembar(tabel[0].baris, tabel[0].kolom);
    if (!buf) {
        free(line);
        return -1;
    }

    /* Tambahkan lembar untuk tabel berikutnya */
    for (i = 1; i < jumlah_tabel; i++) {
        lem = buat_lembar(tabel[i].baris, tabel[i].kolom, tabel[i].nama);
        if (lem) {
            tambah_lembar_ke_buffer(buf, lem);
        }
    }

    /* Rename lembar pertama sesuai nama tabel */
    if (tabel[0].nama[0]) {
        rename_lembar(buf, 0, tabel[0].nama);
    }

    /* Pass 2: Baca data INSERT ke lembar yang sesuai */
    f = fopen(fn, "r");
    if (!f) {
        bebas_buffer(buf);
        free(line);
        return -1;
    }

    /* Reset counter baris per tabel */
    {
        int baris_counter[MAKS_TABEL_SQL];
        for (i = 0; i < jumlah_tabel; i++) baris_counter[i] = 0;

        while (fgets(line, (int)line_buf_size, f)) {
            /* Lewati whitespace di awal baris */
            p = line;
            while (*p == ' ' || *p == '\t') p++;
            if (strncasecmp(p, "INSERT", 6) != 0) continue;

            /* Ekstrak nama tabel (p sudah melewati whitespace) */
            p += 6; /* Lewati "INSERT" */
            while (*p == ' ') p++;
            if (strncasecmp(p, "INTO", 4) == 0) p += 4;
            while (*p == ' ') p++;
            {
                char nama_tabel[NAMA_LEMBAR_MAKS];
                int t_idx;
                int lem_idx;

                q = nama_tabel;
                while (*p && *p != '(' && *p != ' ' && *p != '\t' && 
                       *p != '\n' && *p != '\r' && q < nama_tabel + NAMA_LEMBAR_MAKS - 1) {
                    if (*p != '`' && *p != '"' && *p != '[' && *p != ']') *q++ = *p;
                    p++;
                }
                *q = '\0';

                /* Cari index tabel */
                t_idx = -1;
                for (i = 0; i < jumlah_tabel; i++) {
                    if (strcmp(tabel[i].nama, nama_tabel) == 0) {
                        t_idx = i;
                        break;
                    }
                }
                if (t_idx < 0) continue;

                /* Set lembar aktif */
                set_lembar_aktif(buf, t_idx);
                lem_idx = t_idx;

                /* Cari klausa VALUES - bukan tanda kurung pertama */
                {
                    char *values_pos = NULL;
                    char *search = line;
                    while (*search) {
                        if (strncasecmp(search, "VALUES", 6) == 0) {
                            values_pos = search;
                            break;
                        }
                        search++;
                    }

                    if (!values_pos) continue;

                    /* Parse setiap set nilai (untuk multi-row INSERT) */
                    p = values_pos + 6;
                    while (*p) {
                        if (*p != '(') { p++; continue; }

                        /* Awal set nilai */
                        p++; /* Lewati '(' */
                        r = baris_counter[t_idx];
                        c = 0;

                        while (*p && *p != ')' && c < tabel[t_idx].kolom && r < tabel[t_idx].baris) {
                            idx = 0;
                            if (*p == '\'') {
                                p++;
                                while (*p && *p != '\'' && idx < MAKS_TEKS - 1) {
                                    if (*p == '\'' && *(p+1) == '\'') {
                                        cell[idx++] = '\'';
                                        p += 2;
                                    } else {
                                        cell[idx++] = *p++;
                                    }
                                }
                                if (*p == '\'') p++;
                            } else {
                                /* Nilai tanpa quote (angka, NULL, dll) */
                                while (*p && *p != ',' && *p != ')' && idx < MAKS_TEKS - 1) {
                                    cell[idx++] = *p++;
                                }
                            }
                            cell[idx] = '\0';

                            /* Trim spasi */
                            while (idx > 0 && (cell[idx-1] == ' ' || cell[idx-1] == '\t' || 
                                   cell[idx-1] == '\n' || cell[idx-1] == '\r')) {
                                cell[--idx] = '\0';
                            }

                            if (r < buf->lembar[lem_idx]->baris && c < buf->lembar[lem_idx]->kolom) {
                                salin_str_aman(buf->lembar[lem_idx]->isi[r][c], cell, MAKS_TEKS);
                            }
                            c++;
                            /* Lewati sisa nilai jika ada koma */
                            while (*p && *p != ',' && *p != ')') p++;
                            if (*p == ',') p++;
                        }

                        /* Lewati sampai akhir set */
                        while (*p && *p != ')') p++;
                        if (*p == ')') p++;

                        baris_counter[t_idx]++;

                        /* Lewati koma antar set pada multi-row INSERT */
                        while (*p && (*p == ' ' || *p == ',' || *p == '\t')) p++;
                    }
                }
            }
        }
    }

    fclose(f);
    free(line);

    /* Kembali ke lembar pertama */
    set_lembar_aktif(buf, 0);
    sinkronkan_pointer_lembar_aktif(buf);

    bebas_buffer(*pbuf);
    *pbuf = buf;
    return 0;
}

/* ============================================================
 * I/O DB (Biner)
 * ============================================================ */

int simpan_db(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;
    size_t len;

    if (!buf || !fn) return -1;
    f = fopen(fn, "wb");
    if (!f) return -1;

    fwrite(&buf->cfg.kolom, sizeof(int), 1, f);
    fwrite(&buf->cfg.baris, sizeof(int), 1, f);

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            len = strlen(buf->isi[r][c]) + 1;
            fwrite(&len, sizeof(size_t), 1, f);
            fwrite(buf->isi[r][c], 1, len, f);
        }
    }

    if (fsync_berkas(f) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int buka_db(struct buffer_tabel **pbuf, const char *fn)
{
    FILE *f;
    unsigned char header[16];
    size_t bytes;

    if (!pbuf || !fn) return -1;

    /* Cek apakah file adalah SQLite database */
    f = fopen(fn, "rb");
    if (!f) return -1;

    bytes = fread(header, 1, 16, f);
    fclose(f);

    if (bytes >= 16 && memcmp(header, "SQLite format 3\000", 16) == 0) {
        /* File adalah SQLite database - gunakan sqlite3 */
        return buka_sqlite(pbuf, fn);
    }

    /* File bukan SQLite - coba format biner native */
    return buka_db_native(pbuf, fn);
}

/* ============================================================
 * I/O DB Native (Format Biner Tabel)
 * ============================================================ */

int buka_db_native(struct buffer_tabel **pbuf, const char *fn)
{
    FILE *f;
    int cols, rows;
    int r, c;
    size_t len;

    if (!pbuf || !fn) return -1;
    f = fopen(fn, "rb");
    if (!f) return -1;

    if (fread(&cols, sizeof(int), 1, f) != 1) { fclose(f); return -1; }
    if (fread(&rows, sizeof(int), 1, f) != 1) { fclose(f); return -1; }

    /* Validasi dimensi agar tidak hang pada file tidak valid */
    if (cols <= 0 || cols > MAKS_KOLOM || rows <= 0 || rows > MAKS_BARIS) {
        fclose(f);
        return -1;
    }

    bebas_buffer(*pbuf);
    *pbuf = buat_buffer_satu_lembar(rows, cols);

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            if (fread(&len, sizeof(size_t), 1, f) != 1) break;
            if (len == 0 || len > MAKS_TEKS) break;
            if (fread((*pbuf)->isi[r][c], 1, len, f) != len) break;
        }
    }
    fclose(f);
    return 0;
}

/* ============================================================
 * I/O SQLite Database
 * ============================================================ */

#include <sqlite3.h>

int buka_sqlite(struct buffer_tabel **pbuf, const char *fn)
{
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int rc;
    struct buffer_tabel *buf;
    struct lembar_tabel *lem;
    int jumlah_tabel = 0;
    char *nama_tabel[256];
    int kolom_tabel[256];
    int baris_tabel[256];
    int i, r, c, col_count;
    char query[512];
    /* Batas maksimum baris per tabel untuk mencegah hang
     * pada database sangat besar (alokasi memori berlebihan) */
    #define BATAS_BARIS_DB 50000

    if (!pbuf || !fn) return -1;

    rc = sqlite3_open_v2(fn, &db, SQLITE_OPEN_READONLY, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return -1;
    }

    /* Set busy timeout agar tidak hang jika DB terkunci */
    sqlite3_busy_timeout(db, 5000);

    /* Dapatkan daftar tabel */
    rc = sqlite3_prepare_v2(db, 
        "SELECT name FROM sqlite_master WHERE type='table' AND name NOT LIKE 'sqlite_%' ORDER BY name",
        -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_close(db);
        return -1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW && jumlah_tabel < 256) {
        const char *name = (const char *)sqlite3_column_text(stmt, 0);
        if (name) {
            nama_tabel[jumlah_tabel] = strdup(name);
            kolom_tabel[jumlah_tabel] = 0;
            baris_tabel[jumlah_tabel] = 0;
            jumlah_tabel++;
        }
    }
    sqlite3_finalize(stmt);

    if (jumlah_tabel <= 0) {
        sqlite3_close(db);
        return -1;
    }

    /* Hitung baris dan kolom per tabel */
    for (i = 0; i < jumlah_tabel; i++) {
        /* Hitung kolom */
        snprintf(query, sizeof(query), "SELECT * FROM \"%s\" LIMIT 0", nama_tabel[i]);
        rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            kolom_tabel[i] = sqlite3_column_count(stmt);
            sqlite3_finalize(stmt);
        }

        /* Hitung baris */
        snprintf(query, sizeof(query), "SELECT COUNT(*) FROM \"%s\"", nama_tabel[i]);
        rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
        if (rc == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                baris_tabel[i] = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }

        if (kolom_tabel[i] <= 0) kolom_tabel[i] = 1;
        if (baris_tabel[i] <= 0) baris_tabel[i] = 1;
        /* Batasi baris untuk mencegah alokasi memori berlebihan */
        if (baris_tabel[i] > BATAS_BARIS_DB) baris_tabel[i] = BATAS_BARIS_DB;
    }

    /* Buat buffer dengan satu lembar untuk tabel pertama */
    buf = buat_buffer_satu_lembar(baris_tabel[0], kolom_tabel[0]);
    if (!buf) {
        for (i = 0; i < jumlah_tabel; i++) free(nama_tabel[i]);
        sqlite3_close(db);
        return -1;
    }

    /* Rename lembar pertama */
    if (nama_tabel[0]) {
        rename_lembar(buf, 0, nama_tabel[0]);
    }

    /* Tambahkan lembar untuk tabel berikutnya */
    for (i = 1; i < jumlah_tabel; i++) {
        lem = buat_lembar(baris_tabel[i], kolom_tabel[i], nama_tabel[i]);
        if (lem) {
            tambah_lembar_ke_buffer(buf, lem);
        }
    }

    /* Baca data dari setiap tabel - gunakan LIMIT untuk efisiensi */
    for (i = 0; i < jumlah_tabel; i++) {
        snprintf(query, sizeof(query), "SELECT * FROM \"%s\" LIMIT %d", 
                 nama_tabel[i], BATAS_BARIS_DB);
        rc = sqlite3_prepare_v2(db, query, -1, &stmt, NULL);
        if (rc != SQLITE_OK) continue;

        col_count = sqlite3_column_count(stmt);
        r = 0;
        while (sqlite3_step(stmt) == SQLITE_ROW && r < baris_tabel[i]) {
            for (c = 0; c < col_count && c < kolom_tabel[i]; c++) {
                const char *val = (const char *)sqlite3_column_text(stmt, c);
                if (val && r < buf->lembar[i]->baris && c < buf->lembar[i]->kolom) {
                    salin_str_aman(buf->lembar[i]->isi[r][c], val, MAKS_TEKS);
                }
            }
            r++;
        }
        sqlite3_finalize(stmt);
    }

    /* Kembali ke lembar pertama */
    set_lembar_aktif(buf, 0);
    sinkronkan_pointer_lembar_aktif(buf);

    bebas_buffer(*pbuf);
    *pbuf = buf;

    /* Bersihkan */
    for (i = 0; i < jumlah_tabel; i++) free(nama_tabel[i]);
    sqlite3_close(db);
    return 0;
}

/* ============================================================
 * I/O TSV
 * ============================================================ */

int simpan_tsv(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;

    if (!buf || !fn) return -1;
    f = fopen(fn, "w");
    if (!f) return -1;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (fprintf(f, "%s", buf->isi[r][c]) < 0) {
                fclose(f);
                return -1;
            }
            if (c < buf->cfg.kolom - 1) fputc('\t', f);
        }
        fputc('\n', f);
    }

    if (fsync_berkas(f) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int buka_tsv(struct buffer_tabel **pbuf, const char *fn)
{
    /* Sama persis dengan TXT karena delimiter tab */
    return buka_txt(pbuf, fn);
}

/* ============================================================
 * I/O MARKDOWN
 * ============================================================ */

int simpan_md(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;

    if (!buf || !fn) return -1;
    f = fopen(fn, "w");
    if (!f) return -1;

    /* Header */
    for (c = 0; c < buf->cfg.kolom; c++) {
        fprintf(f, "| %s ", buf->isi[0][c]);
    }
    fprintf(f, "|\n");

    /* Separator */
    for (c = 0; c < buf->cfg.kolom; c++) {
        fprintf(f, "|---");
    }
    fprintf(f, "|\n");

    /* Isi */
    for (r = 1; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            fprintf(f, "| %s ", buf->isi[r][c]);
        }
        fprintf(f, "|\n");
    }

    if (fsync_berkas(f) != 0) {
        fclose(f);
        return -1;
    }
    fclose(f);
    return 0;
}

int buka_md(struct buffer_tabel **pbuf, const char *fn)
{
    FILE *f;
    char line[MAKS_TEKS * MAKS_KOLOM];
    int baris = 0;
    int kolom = 0;
    int r;
    int c;
    char *tok;
    char *end;

    if (!pbuf || !fn) return -1;
    f = fopen(fn, "r");
    if (!f) return -1;

    /* Hitung dimensi */
    while (fgets(line, sizeof(line), f)) {
        if (strchr(line, '|')) {
            baris++;
            c = 0;
            tok = strtok(line, "|");
            while (tok) {
                if (strlen(tok) > 0) c++;
                tok = strtok(NULL, "|");
            }
            if (c > kolom) kolom = c;
        }
    }
    fclose(f);

    if (baris <= 0) return -1;
    if (kolom <= 0) kolom = 1;

    bebas_buffer(*pbuf);
    *pbuf = buat_buffer(baris, kolom);

    f = fopen(fn, "r");
    if (!f) return -1;

    r = 0;
    while (fgets(line, sizeof(line), f) && r < (*pbuf)->cfg.baris) {
        if (strchr(line, '|')) {
            c = 0;
            tok = strtok(line, "|");
            while (tok && c < (*pbuf)->cfg.kolom) {
                /* Trim whitespace */
                while (*tok == ' ' || *tok == '\t') tok++;
                end = tok + strlen(tok) - 1;
                while (end > tok && (*end == ' ' || *end == '\t' ||
                       *end == '\n')) {
                    *end = '\0';
                    end--;
                }
                if (strlen(tok) > 0) {
                    salin_str_aman((*pbuf)->isi[r][c], tok, MAKS_TEKS);
                }
                tok = strtok(NULL, "|");
                c++;
            }
            r++;
        }
    }
    fclose(f);
    return 0;
}

/* ============================================================
 * I/O PDF
 * ============================================================ */

int simpan_pdf(const struct buffer_tabel *buf, const char *fn)
{
    return simpan_pdf_baru(buf, fn);
}

/* ============================================================
 * I/O XLSX (Excel 2007+ Format)
 * Implementasi ada di berkas/xlsx.c
 * ============================================================ */

/* ============================================================
 * I/O XLS (Excel 97-2003 Format)
 * Implementasi ada di berkas/xls.c
 * ============================================================ */

/* ============================================================
 * I/O XML Spreadsheet (Excel 2002-2003 XML Format)
 * Implementasi ada di berkas/xmlss.c
 * ============================================================ */

/* ============================================================
 * I/O ODS (OpenDocument Spreadsheet)
 * Implementasi ada di berkas/ods.c
 * ============================================================ */

/* ============================================================
 * Deteksi Format File
 * ============================================================ */

int deteksi_format_file(const char *fn)
{
    const char *ext;

    if (!fn) return 0;

    ext = strrchr(fn, '.');
    if (!ext) return 0;

    /* Konversi ke lowercase untuk perbandingan */
    if (strcasecmp(ext, ".tbl") == 0) return 1;   /* TBL (Native) */
    if (strcasecmp(ext, ".xlsx") == 0) return 2;  /* XLSX */
    if (strcasecmp(ext, ".xls") == 0) return 3;   /* XLS */
    if (strcasecmp(ext, ".xml") == 0) return 4;   /* XML Spreadsheet */
    if (strcasecmp(ext, ".ods") == 0) return 5;   /* ODS */
    if (strcasecmp(ext, ".csv") == 0) return 6;   /* CSV */
    if (strcasecmp(ext, ".tsv") == 0) return 7;   /* TSV */
    if (strcasecmp(ext, ".txt") == 0) return 8;   /* TXT */
    if (strcasecmp(ext, ".sql") == 0) return 9;   /* SQL */
    if (strcasecmp(ext, ".db") == 0) return 10;   /* DB */
    if (strcasecmp(ext, ".md") == 0) return 11;   /* Markdown */
    if (strcasecmp(ext, ".pdf") == 0) return 12;  /* PDF */
    if (strcasecmp(ext, ".html") == 0) return 13; /* HTML */
    if (strcasecmp(ext, ".htm") == 0) return 13;  /* HTML (alternatif) */
    if (strcasecmp(ext, ".json") == 0) return 14; /* JSON */

    return 0;  /* Tidak dikenal */
}

/* ============================================================
 * Buka File dengan Deteksi Format Otomatis
 * ============================================================ */

int buka_file_otomatis(struct buffer_tabel **pbuf, const char *fn)
{
    int format;
    int ok = -1;
    char cwd[PANJANG_PATH_MAKS];
    FILE *fp;
    unsigned char buf[256];
    size_t bytes;
    unsigned int magic;
    int i;
    int is_html;
    int is_json;
    char *p;

    if (!pbuf || !fn) return -1;

    format = deteksi_format_file(fn);

    switch (format) {
    case 1:  /* TBL (Native) */
        ok = buka_tbl(pbuf, fn);
        break;
    case 2:  /* XLSX */
        ok = buka_xlsx(pbuf, fn);
        break;
    case 3:  /* XLS */
        ok = buka_xls(pbuf, fn);
        break;
    case 4:  /* XML Spreadsheet */
        ok = buka_xml(pbuf, fn);
        break;
    case 5:  /* ODS */
        ok = buka_ods(pbuf, fn);
        break;
    case 6:  /* CSV */
        ok = buka_csv(pbuf, fn);
        break;
    case 7:  /* TSV */
        ok = buka_tsv(pbuf, fn);
        break;
    case 8:  /* TXT */
        ok = buka_txt(pbuf, fn);
        break;
    case 9:  /* SQL */
        ok = buka_sql(pbuf, fn);
        break;
    case 10: /* DB */
        ok = buka_db(pbuf, fn);
        break;
    case 11: /* Markdown */
        ok = buka_md(pbuf, fn);
        break;
    case 12: /* HTML */
        ok = buka_html(pbuf, fn);
        break;
    case 13: /* JSON */
        ok = buka_json(pbuf, fn);
        break;
    default:
        /* Coba deteksi berdasarkan isi file */
        fp = fopen(fn, "rb");
        if (fp) {
            bytes = fread(buf, 1, 256, fp);
            fclose(fp);

            /* Cek signature TBL (Native) - "TBL3" = 0x334C4254 LE */
            if (bytes >= 4) {
                magic = buf[0] | (buf[1] << 8) |
                        (buf[2] << 16) | (buf[3] << 24);
                if (magic == 0x334C4254) {
                    ok = buka_tbl(pbuf, fn);
                    break;
                }
            }

            /* Cek signature OLE2 (XLS) */
            if (bytes >= 8 && memcmp(buf, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1",
                8) == 0) {
                ok = buka_xls(pbuf, fn);
                break;
            }

            /* Cek signature ZIP (XLSX/ODS) */
            if (bytes >= 4 && memcmp(buf, "PK\x03\x04", 4) == 0) {
                ok = buka_xlsx(pbuf, fn);
                if (ok != 0) {
                    ok = buka_ods(pbuf, fn);
                }
                break;
            }

            /* Cek signature XML */
            if (bytes >= 5 && memcmp(buf, "<?xml", 5) == 0) {
                ok = buka_xml(pbuf, fn);
                break;
            }

            /* Cek HTML atau JSON */
            is_html = 0;
            is_json = 0;

            /* Skip whitespace untuk deteksi */
            for (i = 0; i < (int)bytes && isspace(buf[i]); i++) ;

            /* Cek JSON (dimulai dengan { atau [) */
            if (i < (int)bytes && (buf[i] == '{' || buf[i] == '[')) {
                is_json = 1;
            }

            /* Cek HTML (<!DOCTYPE, <html, <table) */
            if (!is_json && i < (int)bytes) {
                if (strncasecmp((char*)buf + i, "<!doctype", 9) == 0 ||
                    strncasecmp((char*)buf + i, "<html", 5) == 0 ||
                    strncasecmp((char*)buf + i, "<table", 6) == 0 ||
                    strncasecmp((char*)buf + i, "<head", 5) == 0 ||
                    strncasecmp((char*)buf + i, "<body", 5) == 0) {
                    is_html = 1;
                }
                /* Cari <table di seluruh buffer */
                if (!is_html) {
                    p = (char*)buf;
                    while (p < (char*)buf + bytes - 6) {
                        if (strncasecmp(p, "<table", 6) == 0) {
                            is_html = 1;
                            break;
                        }
                        p++;
                    }
                }
            }

            if (is_json) {
                ok = buka_json(pbuf, fn);
                break;
            }
            if (is_html) {
                ok = buka_html(pbuf, fn);
                break;
            }
        }
        /* Default: coba sebagai TXT */
        ok = buka_txt(pbuf, fn);
        break;
    }

    if (ok == 0) {
        if (getcwd(cwd, sizeof(cwd))) {
            snprintf((*pbuf)->cfg.path_file,
                     sizeof((*pbuf)->cfg.path_file), "%s/%s", cwd, fn);
        } else {
            salin_str_aman((*pbuf)->cfg.path_file, fn,
                          sizeof((*pbuf)->cfg.path_file));
        }
    }

    return ok;
}

/* ============================================================
 * Simpan File dengan Deteksi Format Otomatis
 * ============================================================ */

int simpan_file_otomatis(const struct buffer_tabel *buf, const char *fn)
{
    int format;

    if (!buf || !fn) return -1;

    format = deteksi_format_file(fn);

    switch (format) {
    case 1:  /* TBL */
        return simpan_tbl(buf, fn);
    case 2:  /* XLSX */
        return simpan_xlsx(buf, fn);
    case 3:  /* XLS */
        return simpan_xls(buf, fn);
    case 4:  /* XML Spreadsheet */
        return simpan_xml(buf, fn);
    case 5:  /* ODS */
        return simpan_ods(buf, fn);
    case 6:  /* CSV */
        return simpan_csv(buf, fn);
    case 7:  /* TSV */
        return simpan_tsv(buf, fn);
    case 8:  /* TXT */
        return simpan_txt(buf, fn);
    case 9:  /* SQL */
        return simpan_sql(buf, fn);
    case 10: /* DB */
        return simpan_db(buf, fn);
    case 11: /* Markdown */
        return simpan_md(buf, fn);
    case 12: /* HTML */
        return simpan_html(buf, fn);
    case 13: /* JSON */
        return simpan_json(buf, fn);
    default:
        /* Default: TBL */
        return simpan_tbl(buf, fn);
    }
}
