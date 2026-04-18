/* ============================================================
 * TABEL v3.0
 * Berkas: sql.c - I/O SQL & Database (Multi-Dialect)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 *
 * Dukungan Multi-Dialect:
 * - SQLite (via libsqlite3)
 * - MySQL/MariaDB (backtick `, double-quote ", single-quote ')
 * - PostgreSQL (double-quote ", single-quote ')
 * - SQL Server (bracket [] identifier quoting)
 * - Standar SQL (single-quote ' untuk string)
 *
 * Fitur Parser:
 * - CREATE TABLE dengan IF NOT EXISTS
 * - INSERT INTO dengan VALUES
 * - Multi-row INSERT (VALUES (...), (...), ...)
 * - Komentar SQL: -- (baris), blok (slash-star to star-slash)
 * - Identifier quoting: backtick, double-quote, bracket
 * - Escaped single-quote and backslash
 * - Kolom dengan tipe data dan constraint
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA
 * ============================================================ */

#define MAKS_TABEL_SQL 256
#define BATAS_BARIS_DB 50000

/* ============================================================
 * STRUKTUR INTERNAL
 * ============================================================ */

struct info_tabel_sql {
    char nama[NAMA_LEMBAR_MAKS];
    int kolom;
    int baris;
    char **nama_kolom;   /* Nama kolom dari CREATE TABLE */
    long pos_create;     /* Posisi file awal CREATE TABLE */
};

/* ============================================================
 * HELPER - Lewati Komentar SQL
 * ============================================================ */

/* Lewati komentar blok /* ... */
static const char *lewat_komentar_blok(const char *p)
{
    if (*p == '/' && *(p + 1) == '*') {
        p += 2;
        while (*p) {
            if (*p == '*' && *(p + 1) == '/') {
                p += 2;
                return p;
            }
            p++;
        }
    }
    return p;
}

/* Lewati komentar baris -- ... */
static const char *lewat_komentar_baris(const char *p)
{
    if (*p == '-' && *(p + 1) == '-') {
        p += 2;
        while (*p && *p != '\n') p++;
        return p;
    }
    return p;
}

/* Lewati komentar MySQL # ... */
static const char *lewat_komentar_hash(const char *p)
{
    if (*p == '#') {
        p += 1;
        while (*p && *p != '\n') p++;
        return p;
    }
    return p;
}

/* Cek apakah karakter adalah identifier quote */
static int adalah_quote_identifier(char c)
{
    return (c == '`' || c == '"' || c == '[');
}

/* Cek karakter penutup identifier quote */
static char tutup_quote_identifier(char c)
{
    if (c == '`') return '`';
    if (c == '"') return '"';
    if (c == '[') return ']';
    return '\0';
}

/* Lewati identifier yang di-quote */
static const char *lewat_identifier_quote(const char *p)
{
    char tutup = tutup_quote_identifier(*p);
    if (!tutup) return p;
    p++; /* Lewati buka quote */
    while (*p && *p != tutup) {
        if (*p == tutup && *(p + 1) == tutup) {
            p += 2; /* Escaped quote */
            continue;
        }
        p++;
    }
    if (*p == tutup) p++; /* Lewati tutup quote */
    return p;
}

/* Lewati string literal (single-quote) */
static const char *lewat_string_literal(const char *p)
{
    if (*p != '\'') return p;
    p++; /* Lewati buka quote */
    while (*p) {
        if (*p == '\'') {
            if (*(p + 1) == '\'') {
                p += 2; /* SQL escaped quote '' */
                continue;
            }
            if (*(p + 1) == '\\') {
                /* MySQL escaped quote \' — cek apakah ini escape */
                p += 2; /* Lewati \' sebagai bagian string */
                continue;
            }
            p++; /* Lewati tutup quote */
            return p;
        }
        if (*p == '\\' && *(p + 1)) {
            /* MySQL backslash escape */
            p += 2;
            continue;
        }
        p++;
    }
    return p;
}

/* Lewati whitespace */
static const char *lewat_spasi(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

/* ============================================================
 * HELPER - Ekstrak Nama Identifier (tanpa quote)
 * ============================================================ */

/* Ekstrak nama identifier, menghapus backtick/double-quote/bracket */
static void ekstrak_nama_identifier(const char *p, char *dst, size_t ukuran)
{
    size_t i = 0;

    while (*p && *p != '(' && *p != ' ' && *p != '\t' &&
           *p != '\n' && *p != '\r' && i < ukuran - 1) {
        if (*p == '`' || *p == '"' || *p == '[' || *p == ']') {
            p++; /* Lewati karakter quote */
            continue;
        }
        dst[i++] = *p++;
    }
    dst[i] = '\0';
}

/* ============================================================
 * HELPER - Lewati IF NOT EXISTS setelah CREATE TABLE
 * ============================================================ */

static const char *lewat_if_not_exists(const char *p)
{
    if (strncasecmp(p, "IF", 2) == 0) {
        p += 2; p = lewat_spasi(p);
        if (strncasecmp(p, "NOT", 3) == 0) {
            p += 3; p = lewat_spasi(p);
        }
        if (strncasecmp(p, "EXISTS", 6) == 0) {
            p += 6; p = lewat_spasi(p);
        }
    }
    return p;
}

/* ============================================================
 * HELPER - Cari klausa VALUES (case-insensitive)
 * ============================================================ */

static char *cari_values(char *line)
{
    char *p = line;
    while (*p) {
        /* Lewati string literal */
        if (*p == '\'') {
            p = (char *)lewat_string_literal(p);
            continue;
        }
        /* Lewati identifier quote */
        if (adalah_quote_identifier(*p)) {
            p = (char *)lewat_identifier_quote(p);
            continue;
        }
        if (strncasecmp(p, "VALUES", 6) == 0) {
            /* Pastikan bukan bagian dari nama identifier */
            char sebelum = (p > line) ? *(p - 1) : ' ';
            char sesudah = *(p + 6);
            if ((sebelum == ' ' || sebelum == '\t' || sebelum == '\n' || sebelum == '\r' ||
                 sebelum == ')' || sebelum == '*') &&
                (sesudah == ' ' || sesudah == '\t' || sesudah == '\n' || sesudah == '\r' ||
                 sesudah == '(' || sesudah == '\0')) {
                return p;
            }
        }
        p++;
    }
    return NULL;
}

/* ============================================================
 * I/O SQL - SIMPAN
 * ============================================================ */

int simpan_sql(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;
    const char *val;
    const char *p;

    if (!buf || !fn) return -1;
    f = fopen(fn, "w");
    if (!f) return -1;

    /* Header komentar */
    fprintf(f, "-- Dibuat oleh TABEL v3.0\n");
    fprintf(f, "-- Format SQL Standar (kompatibel SQLite, MySQL, PostgreSQL)\n\n");

    /* CREATE TABLE - gunakan identifier quote untuk kompatibilitas */
    fprintf(f, "CREATE TABLE IF NOT EXISTS \"data\" (\n");
    for (c = 0; c < buf->cfg.kolom; c++) {
        char nama_kol[8];
        kolom_ke_nama(c, nama_kol, sizeof(nama_kol));
        fprintf(f, "  \"%s\" TEXT", nama_kol);
        if (c < buf->cfg.kolom - 1) fprintf(f, ",\n");
        else fprintf(f, "\n");
    }
    fprintf(f, ");\n\n");

    /* INSERT INTO - gunakan format standar */
    for (r = 0; r < buf->cfg.baris; r++) {
        fprintf(f, "INSERT INTO \"data\" VALUES (");
        for (c = 0; c < buf->cfg.kolom; c++) {
            val = buf->isi[r][c];
            if (val[0] == '\0') {
                fprintf(f, "NULL");
            } else {
                fputc('\'', f);
                /* Escape single-quote: '' untuk standar SQL */
                for (p = val; *p; p++) {
                    if (*p == '\'') {
                        fputc('\'', f); /* Escape dengan '' */
                    } else if (*p == '\\') {
                        /* Backslash escape untuk MySQL compat */
                        fprintf(f, "\\\\");
                    } else if (*p == '\n') {
                        fprintf(f, "\\n");
                    } else if (*p == '\r') {
                        fprintf(f, "\\r");
                    } else if (*p == '\t') {
                        fprintf(f, "\\t");
                    } else if (*p == '\0') {
                        break;
                    } else {
                        fputc(*p, f);
                    }
                }
                fputc('\'', f);
            }
            if (c < buf->cfg.kolom - 1) fprintf(f, ", ");
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

/* ============================================================
 * I/O SQL - BUKA (Multi-Dialect Parser)
 * ============================================================ */

int buka_sql(struct buffer_tabel **pbuf, const char *fn)
{
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
        /* Lewati whitespace di awal baris */
        p = line;
        while (*p == ' ' || *p == '\t') p++;

        /* Lewati komentar baris (-- dan #) */
        if (*p == '-' && *(p + 1) == '-') continue;
        if (*p == '#') continue;

        /* Deteksi CREATE TABLE (case-insensitive) */
        if (strncasecmp(p, "CREATE TABLE", 12) == 0) {
            if (jumlah_tabel < MAKS_TABEL_SQL) {
                tabel_aktif = jumlah_tabel;
                tabel[tabel_aktif].kolom = 0;
                tabel[tabel_aktif].baris = 0;
                tabel[tabel_aktif].nama_kolom = NULL;
                tabel[tabel_aktif].pos_create = ftell(f);

                /* Ekstrak nama tabel */
                p += 12; /* Lewati "CREATE TABLE" */
                p = (char *)lewat_spasi(p);

                /* Lewati IF NOT EXISTS jika ada */
                p = (char *)lewat_if_not_exists(p);

                /* Lewati TEMPORARY / TEMP jika ada */
                if (strncasecmp(p, "TEMPORARY", 9) == 0) {
                    p += 9; p = (char *)lewat_spasi(p);
                } else if (strncasecmp(p, "TEMP", 4) == 0) {
                    p += 4; p = (char *)lewat_spasi(p);
                }

                /* Lewati identifier quote jika ada */
                if (adalah_quote_identifier(*p)) {
                    char tutup = tutup_quote_identifier(*p);
                    p++; /* Lewati buka quote */
                    q = tabel[tabel_aktif].nama;
                    while (*p && *p != tutup && q < tabel[tabel_aktif].nama + NAMA_LEMBAR_MAKS - 1) {
                        if (*p == tutup && *(p + 1) == tutup) {
                            *q++ = *p; /* Escaped quote */
                            p += 2;
                            continue;
                        }
                        *q++ = *p++;
                    }
                    *q = '\0';
                    if (*p == tutup) p++; /* Lewati tutup quote */
                } else {
                    ekstrak_nama_identifier(p, tabel[tabel_aktif].nama, NAMA_LEMBAR_MAKS);
                }

                /* Cek apakah kolom ada di baris yang sama */
                p = strchr(line, '(');
                if (p) {
                    in_create = 1;
                    paren_depth = 1;
                    in_paren_quote = 0;
                    p++;
                    /* Hitung kolom dari baris ini.
                     * Comma dihitung hanya pada paren_depth == 1
                     * Lewati komentar, string, dan nested parens */
                    while (*p) {
                        /* Lewati komentar */
                        if (*p == '-' && *(p + 1) == '-') {
                            while (*p && *p != '\n') p++;
                            continue;
                        }
                        if (*p == '/' && *(p + 1) == '*') {
                            p = (char *)lewat_komentar_blok(p);
                            continue;
                        }
                        /* Lewati string literal */
                        if (*p == '\'') {
                            p = (char *)lewat_string_literal(p);
                            continue;
                        }
                        /* Lewati identifier quote */
                        if (adalah_quote_identifier(*p)) {
                            p = (char *)lewat_identifier_quote(p);
                            continue;
                        }
                        if (*p == '(') {
                            paren_depth++;
                        } else if (*p == ')') {
                            paren_depth--;
                            if (paren_depth == 0) {
                                tabel[tabel_aktif].kolom++; /* Kolom terakhir */
                                in_create = 0;
                                jumlah_tabel++;
                                break;
                            }
                        }
                        else if (*p == ',' && paren_depth == 1) {
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
            while (*p) {
                /* Lewati komentar */
                if (*p == '-' && *(p + 1) == '-') break; /* Rest of line is comment */
                if (*p == '/' && *(p + 1) == '*') {
                    p = (char *)lewat_komentar_blok(p);
                    continue;
                }
                /* Lewati string */
                if (*p == '\'') {
                    p = (char *)lewat_string_literal(p);
                    continue;
                }
                /* Lewati identifier */
                if (adalah_quote_identifier(*p)) {
                    p = (char *)lewat_identifier_quote(p);
                    continue;
                }
                if (*p == '(') {
                    paren_depth++;
                } else if (*p == ')') {
                    paren_depth--;
                    if (paren_depth == 0) {
                        tabel[tabel_aktif].kolom++; /* Kolom terakhir */
                        in_create = 0;
                        jumlah_tabel++;
                        break;
                    }
                }
                else if (*p == ',' && paren_depth == 1) {
                    tabel[tabel_aktif].kolom++;
                }
                p++;
            }
            continue;
        }

        /* Deteksi INSERT INTO (case-insensitive) */
        if (strncasecmp(p, "INSERT", 6) == 0) {
            char nama_tabel[NAMA_LEMBAR_MAKS];
            int t_idx;
            int kolom_insert = 0;

            /* Ekstrak nama tabel dari INSERT INTO */
            p += 6; /* Lewati "INSERT" */
            p = (char *)lewat_spasi(p);

            /* Lewati IGNORE / LOW_PRIORITY / DELAYED / HIGH_PRIORITY (MySQL) */
            if (strncasecmp(p, "IGNORE", 6) == 0) { p += 6; p = (char *)lewat_spasi(p); }
            if (strncasecmp(p, "LOW_PRIORITY", 12) == 0) { p += 12; p = (char *)lewat_spasi(p); }
            if (strncasecmp(p, "DELAYED", 7) == 0) { p += 7; p = (char *)lewat_spasi(p); }
            if (strncasecmp(p, "HIGH_PRIORITY", 13) == 0) { p += 13; p = (char *)lewat_spasi(p); }

            if (strncasecmp(p, "INTO", 4) == 0) { p += 4; p = (char *)lewat_spasi(p); }

            /* Lewati OVERWRITE (Hive) */
            if (strncasecmp(p, "OVERWRITE", 9) == 0) { p += 9; p = (char *)lewat_spasi(p); }

            /* Ekstrak nama tabel (support quoted identifier) */
            if (adalah_quote_identifier(*p)) {
                char tutup = tutup_quote_identifier(*p);
                p++; /* Lewati buka quote */
                q = nama_tabel;
                while (*p && *p != tutup && q < nama_tabel + NAMA_LEMBAR_MAKS - 1) {
                    if (*p == tutup && *(p + 1) == tutup) {
                        *q++ = *p; p += 2; continue;
                    }
                    *q++ = *p++;
                }
                *q = '\0';
                if (*p == tutup) p++;
            } else {
                ekstrak_nama_identifier(p, nama_tabel, NAMA_LEMBAR_MAKS);
            }

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
                char *values_pos = cari_values(line);
                if (values_pos) {
                    char *open_paren = strchr(values_pos, '(');
                    if (open_paren) {
                        int in_q = 0;
                        kolom_insert = 0;
                        p = open_paren + 1;
                        while (*p && *p != ')') {
                            /* Lewati string */
                            if (*p == '\'') {
                                p = (char *)lewat_string_literal(p);
                                continue;
                            }
                            /* Lewati identifier */
                            if (adalah_quote_identifier(*p)) {
                                p = (char *)lewat_identifier_quote(p);
                                continue;
                            }
                            if (*p == ',' && !in_q) kolom_insert++;
                            p++;
                        }
                        kolom_insert++; /* Kolom terakhir */
                        tabel[t_idx].kolom = kolom_insert;
                    }
                }
            }

            /* Hitung baris - bisa multi-row INSERT */
            {
                char *values_pos = cari_values(line);
                if (values_pos) {
                    int num_sets = 0;
                    p = values_pos + 6;
                    while (*p) {
                        if (*p == '(') {
                            /* Lewati isi tanda kurung */
                            int depth = 1;
                            int in_q2 = 0;
                            p++;
                            while (*p && depth > 0) {
                                if (*p == '\'') {
                                    p = (char *)lewat_string_literal(p);
                                    continue;
                                }
                                if (adalah_quote_identifier(*p)) {
                                    p = (char *)lewat_identifier_quote(p);
                                    continue;
                                }
                                if (*p == '(') depth++;
                                else if (*p == ')') depth--;
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

    /* Tambahkan 1 baris untuk header (nama kolom dari CREATE TABLE) */
    for (i = 0; i < jumlah_tabel; i++) {
        if (tabel[i].nama_kolom && tabel[i].kolom > 0) {
            tabel[i].baris += 1;
        }
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

    /* Isi baris header (nama kolom) untuk setiap lembar */
    for (i = 0; i < jumlah_tabel; i++) {
        if (tabel[i].nama_kolom) {
            int h;
            for (h = 0; h < tabel[i].kolom && h < buf->lembar[i]->kolom; h++) {
                if (tabel[i].nama_kolom[h]) {
                    salin_str_aman(buf->lembar[i]->isi[0][h], tabel[i].nama_kolom[h], MAKS_TEKS);
                }
            }
        }
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

            /* Lewati komentar */
            if (*p == '-' && *(p + 1) == '-') continue;
            if (*p == '#') continue;

            if (strncasecmp(p, "INSERT", 6) != 0) continue;

            /* Ekstrak nama tabel */
            p += 6;
            p = (char *)lewat_spasi(p);

            /* Lewati modifier MySQL */
            if (strncasecmp(p, "IGNORE", 6) == 0) { p += 6; p = (char *)lewat_spasi(p); }
            if (strncasecmp(p, "LOW_PRIORITY", 12) == 0) { p += 12; p = (char *)lewat_spasi(p); }
            if (strncasecmp(p, "DELAYED", 7) == 0) { p += 7; p = (char *)lewat_spasi(p); }
            if (strncasecmp(p, "HIGH_PRIORITY", 13) == 0) { p += 13; p = (char *)lewat_spasi(p); }

            if (strncasecmp(p, "INTO", 4) == 0) p += 4;
            p = (char *)lewat_spasi(p);

            {
                char nama_tabel[NAMA_LEMBAR_MAKS];
                int t_idx;
                int lem_idx;

                /* Ekstrak nama tabel (support quoted identifier) */
                if (adalah_quote_identifier(*p)) {
                    char tutup = tutup_quote_identifier(*p);
                    p++;
                    q = nama_tabel;
                    while (*p && *p != tutup && q < nama_tabel + NAMA_LEMBAR_MAKS - 1) {
                        if (*p == tutup && *(p + 1) == tutup) {
                            *q++ = *p; p += 2; continue;
                        }
                        *q++ = *p++;
                    }
                    *q = '\0';
                    if (*p == tutup) p++;
                } else {
                    ekstrak_nama_identifier(p, nama_tabel, NAMA_LEMBAR_MAKS);
                }

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

                /* Cari klausa VALUES */
                {
                    char *values_pos = cari_values(line);
                    if (!values_pos) continue;

                    /* Parse setiap set nilai (untuk multi-row INSERT) */
                    p = values_pos + 6;
                    while (*p) {
                        if (*p != '(') { p++; continue; }

                        /* Awal set nilai */
                        p++; /* Lewati '(' */
                        r = baris_counter[t_idx];
                        if (tabel[t_idx].nama_kolom) r += 1;  /* Offset untuk header */
                        c = 0;

                        while (*p && *p != ')' && c < tabel[t_idx].kolom && r < tabel[t_idx].baris) {
                            idx = 0;
                            if (*p == '\'') {
                                /* String literal */
                                p++;
                                while (*p && *p != '\'' && idx < MAKS_TEKS - 1) {
                                    if (*p == '\'' && *(p + 1) == '\'') {
                                        /* SQL escaped quote '' */
                                        cell[idx++] = '\'';
                                        p += 2;
                                    } else if (*p == '\\' && *(p + 1)) {
                                        /* MySQL backslash escape */
                                        char next = *(p + 1);
                                        if (next == 'n') { cell[idx++] = '\n'; p += 2; }
                                        else if (next == 'r') { cell[idx++] = '\r'; p += 2; }
                                        else if (next == 't') { cell[idx++] = '\t'; p += 2; }
                                        else if (next == '\\') { cell[idx++] = '\\'; p += 2; }
                                        else if (next == '\'') { cell[idx++] = '\''; p += 2; }
                                        else if (next == '0') { cell[idx++] = '\0'; p += 2; }
                                        else { cell[idx++] = next; p += 2; }
                                    } else {
                                        cell[idx++] = *p++;
                                    }
                                }
                                if (*p == '\'') p++;
                            } else if (*p == '`' || *p == '"') {
                                /* Identifier quote bukan string — perlakukan sebagai nilai */
                                char tutup = tutup_quote_identifier(*p);
                                p++; /* Lewati buka quote */
                                while (*p && *p != tutup && idx < MAKS_TEKS - 1) {
                                    if (*p == tutup && *(p + 1) == tutup) {
                                        cell[idx++] = *p; p += 2; continue;
                                    }
                                    cell[idx++] = *p++;
                                }
                                if (*p == tutup) p++;
                            } else {
                                /* Nilai tanpa quote (angka, NULL, TRUE, FALSE, dll) */
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

                            /* Konversi NULL ke string kosong */
                            if (strncasecmp(cell, "NULL", 4) == 0 &&
                                (cell[4] == '\0' || cell[4] == ' ')) {
                                cell[0] = '\0';
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

    /* Tambahkan 1 baris untuk header kolom */
    for (i = 0; i < jumlah_tabel; i++) {
        baris_tabel[i] += 1;
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

        /* Isi baris header (nama kolom) di baris 0 */
        for (c = 0; c < col_count && c < kolom_tabel[i]; c++) {
            const char *col_name = sqlite3_column_name(stmt, c);
            if (col_name && c < buf->lembar[i]->kolom) {
                salin_str_aman(buf->lembar[i]->isi[0][c], col_name, MAKS_TEKS);
            }
        }

        /* Data dimulai dari baris 1 (baris 0 = header) */
        r = 1;
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
 * I/O DB (Biner) - Dispatcher
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
