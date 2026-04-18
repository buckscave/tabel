/* ============================================================
 * TABEL v3.0
 * Berkas: csv.c - Format Berkas CSV/TSV (RFC 4180)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 *
 * Penanganan CSV/TSV sesuai RFC 4180:
 *   - Field yang mengandung delimiter, kutip ganda, atau
 *     baris baru harus diapit kutip ganda.
 *   - Kutip ganda dalam field di-escape dengan menggandakannya.
 *   - Field terkuotasi boleh mengandung baris baru (multi-baris).
 *   - Deteksi otomatis delimiter dari baris pertama.
 *   - Dukungan delimiter kustom via buf->cfg.csv_delim.
 *   - Multi-lembar: hanya simpan lembar aktif (CSV tidak
 *     mendukung multi-lembar). Saat baca, buat satu lembar.
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA
 * ============================================================ */
#define SCSV_BUF_AWAL      (MAKS_TEKS * 64)   /* Ukuran awal buffer baca */
#define SCSV_BUF_MAKS      (4 * 1024 * 1024)  /* Batas maksimum 4 MB */
#define SCSV_DETEKSI_MAKS  8                    /* Jumlah kandidat delimiter */

/* ============================================================
 * HELPER: Ambil delimiter efektif
 * ============================================================ */
static char delim_efektif(const struct konfigurasi *cfg, char delim_default)
{
    if (cfg->csv_delim[0] != '\0') return cfg->csv_delim[0];
    if (delim_default != '\0')      return delim_default;
    return ',';
}

/* ============================================================
 * HELPER: Apakah field perlu di-quote?
 * ============================================================ */
static int perlu_quote(const char *val, char delim)
{
    const char *p;
    for (p = val; *p; p++) {
        if (*p == delim || *p == '"' || *p == '\n' || *p == '\r')
            return 1;
    }
    return 0;
}

/* ============================================================
 * HELPER: Tulis satu field ke berkas (dengan quoting RFC 4180)
 * ============================================================ */
static void tulis_field(FILE *f, const char *val, char delim)
{
    const char *p;
    int qp;

    qp = perlu_quote(val, delim);

    if (qp) fputc('"', f);
    for (p = val; *p; p++) {
        if (*p == '"') fputc('"', f);   /* Escape: "" */
        fputc(*p, f);
    }
    if (qp) fputc('"', f);
}

/* ============================================================
 * HELPER: Baca seluruh berkas ke memori
 * ============================================================ */
static char *baca_seluruh_berkas(const char *fn, size_t *panjang)
{
    FILE *f;
    char *buf;
    size_t ukuran, terbaca;

    f = fopen(fn, "r");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    ukuran = (size_t)ftell(f);
    fseek(f, 0, SEEK_SET);

    if (ukuran == 0) { fclose(f); return NULL; }

    buf = malloc(ukuran + 1);
    if (!buf) { fclose(f); return NULL; }

    terbaca = fread(buf, 1, ukuran, f);
    fclose(f);

    buf[terbaca] = '\0';
    if (panjang) *panjang = terbaca;
    return buf;
}

/* ============================================================
 * HELPER: Deteksi otomatis delimiter dari baris pertama
 * ============================================================ */
static char deteksi_delimiter(const char *data, size_t panjang)
{
    static const char kandidat[] = ",;\t|";
    int hitung[SCSV_DETEKSI_MAKS];
    int nkandidat;
    int i, j, terbaik;
    char hasil;
    int inquote;
    const char *p;

    nkandidat = (int)strlen(kandidat);

    for (i = 0; i < nkandidat; i++) hitung[i] = 0;

    inquote = 0;
    for (p = data; p < data + panjang && *p != '\0'; p++) {
        if (*p == '"') { inquote = !inquote; continue; }
        if (!inquote && (*p == '\n' || *p == '\r')) break;
        if (!inquote) {
            for (j = 0; j < nkandidat; j++) {
                if (*p == kandidat[j]) { hitung[j]++; break; }
            }
        }
    }

    terbaik = 0;
    hasil = ',';
    for (i = 0; i < nkandidat; i++) {
        if (hitung[i] > terbaik) {
            terbaik = hitung[i];
            hasil = kandidat[i];
        }
    }

    return hasil;
}

/* ============================================================
 * HELPER: Parser RFC 4180 - hitung dimensi
 * ============================================================ */
static int hitung_dimensi_csv(const char *data, size_t panjang,
                              char delim, int *baris, int *kolom)
{
    const char *p;
    int inquote;
    int c;
    int last_nonempty;
    int fieldlen;
    int r;

    r = 0;
    *kolom = 0;
    inquote = 0;
    c = 0;
    last_nonempty = 0;
    fieldlen = 0;

    for (p = data; p < data + panjang && *p; ) {
        if (inquote) {
            if (*p == '"') {
                if (p + 1 < data + panjang && *(p + 1) == '"') {
                    fieldlen++;
                    p += 2;
                    continue;
                } else {
                    inquote = 0;
                    p++;
                    continue;
                }
            } else {
                fieldlen++;
                p++;
                continue;
            }
        }

        if (*p == '"') {
            inquote = 1;
            p++;
            continue;
        }

        if (*p == delim) {
            if (fieldlen > 0) last_nonempty = c + 1;
            c++;
            fieldlen = 0;
            p++;
            continue;
        }

        if (*p == '\r') {
            p++;
            continue;
        }

        if (*p == '\n') {
            if (fieldlen > 0) last_nonempty = c + 1;
            if (last_nonempty > *kolom) *kolom = last_nonempty;
            if (c > 0 || fieldlen > 0 || last_nonempty > 0) r++;
            c = 0;
            fieldlen = 0;
            last_nonempty = 0;
            p++;
            continue;
        }

        fieldlen++;
        p++;
    }

    if (fieldlen > 0) last_nonempty = c + 1;
    if (c > 0 || fieldlen > 0 || last_nonempty > 0) r++;
    if (last_nonempty > *kolom) *kolom = last_nonempty;

    *baris = r;
    return 0;
}

/* ============================================================
 * HELPER: Parser RFC 4180 - isi data ke buffer
 * ============================================================ */
static void isi_data_csv(const char *data, size_t panjang,
                         struct buffer_tabel *buf, char delim)
{
    const char *p;
    int inquote;
    int r, c;
    char cell[MAKS_TEKS];
    int idx;

    r = 0;
    c = 0;
    idx = 0;
    inquote = 0;

    for (p = data; p < data + panjang && *p; ) {
        if (inquote) {
            if (*p == '"') {
                if (p + 1 < data + panjang && *(p + 1) == '"') {
                    if (idx < MAKS_TEKS - 1) cell[idx++] = '"';
                    p += 2;
                    continue;
                } else {
                    inquote = 0;
                    p++;
                    continue;
                }
            } else {
                if (idx < MAKS_TEKS - 1) cell[idx++] = *p;
                p++;
                continue;
            }
        }

        if (*p == '"') {
            inquote = 1;
            p++;
            continue;
        }

        if (*p == delim) {
            cell[idx] = '\0';
            if (r < buf->cfg.baris && c < buf->cfg.kolom) {
                salin_str_aman(buf->isi[r][c], cell, MAKS_TEKS);
            }
            c++;
            idx = 0;
            p++;
            continue;
        }

        if (*p == '\r') {
            p++;
            continue;
        }

        if (*p == '\n') {
            cell[idx] = '\0';
            if (r < buf->cfg.baris && c < buf->cfg.kolom) {
                salin_str_aman(buf->isi[r][c], cell, MAKS_TEKS);
            }
            r++;
            c = 0;
            idx = 0;
            p++;
            continue;
        }

        if (idx < MAKS_TEKS - 1) cell[idx++] = *p;
        p++;
    }

    cell[idx] = '\0';
    if (r < buf->cfg.baris && c < buf->cfg.kolom) {
        salin_str_aman(buf->isi[r][c], cell, MAKS_TEKS);
    }
}

/* ============================================================
 * SIMPAN CSV
 * ============================================================ */
int simpan_csv(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;
    char delim;

    if (!buf || !fn) return -1;

    delim = delim_efektif(&buf->cfg, ',');

    f = fopen(fn, "w");
    if (!f) return -1;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            tulis_field(f, buf->isi[r][c], delim);
            if (c < buf->cfg.kolom - 1) fputc(delim, f);
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

/* ============================================================
 * BUKA CSV
 * ============================================================ */
int buka_csv(struct buffer_tabel **pbuf, const char *fn)
{
    char *data;
    size_t panjang;
    int baris, kolom;
    char delim;

    if (!pbuf || !fn) return -1;

    data = baca_seluruh_berkas(fn, &panjang);
    if (!data) return -1;

    if (panjang == 0) { free(data); return -1; }

    if ((*pbuf) && (*pbuf)->cfg.csv_delim[0] != '\0') {
        delim = (*pbuf)->cfg.csv_delim[0];
    } else {
        delim = deteksi_delimiter(data, panjang);
    }

    if (hitung_dimensi_csv(data, panjang, delim, &baris, &kolom) != 0) {
        free(data);
        return -1;
    }

    if (baris <= 0) { free(data); return -1; }
    if (kolom <= 0) kolom = 1;

    bebas_buffer(*pbuf);
    *pbuf = buat_buffer_satu_lembar(baris, kolom);
    if (!*pbuf) { free(data); return -1; }

    (*pbuf)->cfg.csv_delim[0] = delim;
    (*pbuf)->cfg.csv_delim[1] = '\0';

    isi_data_csv(data, panjang, *pbuf, delim);

    free(data);
    return 0;
}

/* ============================================================
 * SIMPAN TSV
 * ============================================================ */
int simpan_tsv(const struct buffer_tabel *buf, const char *fn)
{
    FILE *f;
    int r, c;
    char delim;

    if (!buf || !fn) return -1;

    delim = delim_efektif(&buf->cfg, '\t');

    f = fopen(fn, "w");
    if (!f) return -1;

    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            tulis_field(f, buf->isi[r][c], delim);
            if (c < buf->cfg.kolom - 1) fputc(delim, f);
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

/* ============================================================
 * BUKA TSV
 * ============================================================ */
int buka_tsv(struct buffer_tabel **pbuf, const char *fn)
{
    char *data;
    size_t panjang;
    int baris, kolom;
    char delim;

    if (!pbuf || !fn) return -1;

    data = baca_seluruh_berkas(fn, &panjang);
    if (!data) return -1;

    if (panjang == 0) { free(data); return -1; }

    if ((*pbuf) && (*pbuf)->cfg.csv_delim[0] != '\0') {
        delim = (*pbuf)->cfg.csv_delim[0];
    } else {
        delim = deteksi_delimiter(data, panjang);
        if (delim != '\t') delim = '\t';
    }

    if (hitung_dimensi_csv(data, panjang, delim, &baris, &kolom) != 0) {
        free(data);
        return -1;
    }

    if (baris <= 0) { free(data); return -1; }
    if (kolom <= 0) kolom = 1;

    bebas_buffer(*pbuf);
    *pbuf = buat_buffer_satu_lembar(baris, kolom);
    if (!*pbuf) { free(data); return -1; }

    (*pbuf)->cfg.csv_delim[0] = delim;
    (*pbuf)->cfg.csv_delim[1] = '\0';

    isi_data_csv(data, panjang, *pbuf, delim);

    free(data);
    return 0;
}
