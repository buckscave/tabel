/* ============================================================
 * TABEL v3.0
 * Berkas: statistik.c - Formula Fungsi Statistik
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * Fungsi statistik yang didukung:
 * - MEDIAN   : Nilai tengah
 * - MODE     : Nilai yang paling sering muncul
 * - LARGE    : Nilai terbesar ke-n
 * - SMALL    : Nilai terkecil ke-n
 * - RANK     : Peringkat nilai dalam daftar
 * - PERCENTILE : Persentil ke-n
 * - QUARTILE : Kuartil ke-n
 * - AVEDEV   : Rata-rata deviasi absolut
 * - DEVSQ    : Sum of squared deviations
 * - KURT     : Kurtosis
 * - SKEW     : Skewness
 * - STDEVP   : Standar deviasi populasi
 * - VARP     : Varians populasi
 * - GEOMEAN  : Rata-rata geometrik
 * - HARMEAN  : Rata-rata harmonik
 * - TRIMMEAN : Rata-rata terpotong
 * - COUNTIF  : Hitung dengan kondisi
 * - SUMIF    : Jumlahkan dengan kondisi
 * - AVERAGEIF: Rata-rata dengan kondisi
 * - CORREL   : Koefisien korelasi
 * - COVAR    : Kovarians
 * - SLOPE    : Kemiringan regresi
 * - INTERCEPT: Intercept regresi
 * - FORECAST : Prediksi linear
 * - RSQ      : R-squared
 * - STEYX    : Standard error
 * ============================================================ */

#include "../tabel.h"

/* ============================================================
 * KONSTANTA
 * ============================================================ */
#define STAT_MAKS_DATA     4096
#define STAT_TOLERANSI     1e-10

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

/* Bandingkan string tidak peka huruf besar/kecil */
static int banding_str(const char *s1, const char *s2) {
    if (!s1 || !s2) return -1;
    while (*s1 && *s2) {
        char c1 = (char)toupper((unsigned char)*s1);
        char c2 = (char)toupper((unsigned char)*s2);
        if (c1 != c2) return (c1 < c2) ? -1 : 1;
        s1++;
        s2++;
    }
    if (*s1) return 1;
    if (*s2) return -1;
    return 0;
}

/* Nilai absolut */
static double stat_abs(double x) {
    return (x < 0.0) ? -x : x;
}

/* Bandingkan untuk qsort (ascending) */
static int banding_ascending(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da < db) return -1;
    if (da > db) return 1;
    return 0;
}

/* Bandingkan untuk qsort (descending) */
static int banding_descending(const void *a, const void *b) {
    double da = *(const double *)a;
    double db = *(const double *)b;
    if (da > db) return -1;
    if (da < db) return 1;
    return 0;
}

/* Parsing range */
static int parse_range_stat(const char *s, int *x1, int *y1,
                            int *x2, int *y2) {
    const char *p = strchr(s, ':');
    char buf1[64], buf2[64];
    size_t len1;

    if (!p) return -1;

    len1 = (size_t)(p - s);
    if (len1 >= sizeof(buf1)) return -1;

    strncpy(buf1, s, len1);
    buf1[len1] = '\0';
    strncpy(buf2, p + 1, sizeof(buf2) - 1);
    buf2[sizeof(buf2) - 1] = '\0';

    if (parse_ref_sel(buf1, x1, y1) <= 0) return -1;
    if (parse_ref_sel(buf2, x2, y2) <= 0) return -1;

    return 0;
}

/* Kumpulkan nilai numerik dari range */
static int kumpulkan_nilai(const struct buffer_tabel *buf,
                           int x1, int y1, int x2, int y2,
                           double *data, int maks) {
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int count = 0;
    int c, r;

    for (r = ys; r <= ye && count < maks; r++) {
        for (c = xs; c <= xe && count < maks; c++) {
            const char *t;
            double val;
            char *endp;

            if (c >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

            t = buf->isi[r][c];
            if (t[0] == '\0') continue;

            /* Lewati formula, evaluasi nanti */
            if (t[0] == '=') {
                int adalah_num;
                char err[64];
                if (evaluasi_string(buf, c, r, t, &val, &adalah_num,
                                   err, sizeof(err)) == 0 && adalah_num) {
                    data[count++] = val;
                }
                continue;
            }

            val = strtod(t, &endp);
            if (endp && endp != t && *endp == '\0') {
                data[count++] = val;
            }
        }
    }

    return count;
}

/* ============================================================
 * FUNGSI POSISI (MEDIAN, MODE, PERCENTILE, QUARTILE)
 * ============================================================ */

/* MEDIAN: Nilai tengah */
int statistik_median(const struct buffer_tabel *buf, const char *range,
                     double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double median;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    qsort(data, (size_t)count, sizeof(double), banding_ascending);

    if (count % 2 == 0) {
        median = (data[count / 2 - 1] + data[count / 2]) / 2.0;
    } else {
        median = data[count / 2];
    }

    *keluaran = median;
    return 0;
}

/* MODE: Nilai yang paling sering muncul */
int statistik_mode(const struct buffer_tabel *buf, const char *range,
                   double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double mode_val;
    int max_freq = 0;
    int i, j;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    qsort(data, (size_t)count, sizeof(double), banding_ascending);

    mode_val = data[0];
    max_freq = 1;

    for (i = 0; i < count; ) {
        int freq = 1;
        for (j = i + 1; j < count && stat_abs(data[j] - data[i]) < STAT_TOLERANSI; j++) {
            freq++;
        }
        if (freq > max_freq) {
            max_freq = freq;
            mode_val = data[i];
        }
        i = j;
    }

    if (max_freq == 1) {
        /* Tidak ada mode, kembalikan nilai pertama */
        *keluaran = data[0];
    } else {
        *keluaran = mode_val;
    }
    return 0;
}

/* PERCENTILE: Persentil ke-n (0-1) */
int statistik_percentile(const struct buffer_tabel *buf, const char *range,
                         double k, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double pos;
    int idx;
    double frac;

    if (!buf || !range || !keluaran) return -1;
    if (k < 0.0 || k > 1.0) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    qsort(data, (size_t)count, sizeof(double), banding_ascending);

    /* Metode interpolasi linear */
    pos = k * (double)(count - 1);
    idx = (int)pos;
    frac = pos - (double)idx;

    if (idx >= count - 1) {
        *keluaran = data[count - 1];
    } else {
        *keluaran = data[idx] + frac * (data[idx + 1] - data[idx]);
    }

    return 0;
}

/* QUARTILE: Kuartil ke-n (0-4) */
int statistik_quartile(const struct buffer_tabel *buf, const char *range,
                       double quart, double *keluaran) {
    double k;

    if (!keluaran) return -1;
    if (quart < 0.0 || quart > 4.0) return -1;

    k = quart / 4.0;
    return statistik_percentile(buf, range, k, keluaran);
}

/* ============================================================
 * FUNGSI RANKING (LARGE, SMALL, RANK)
 * ============================================================ */

/* LARGE: Nilai terbesar ke-n */
int statistik_large(const struct buffer_tabel *buf, const char *range,
                    double k, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    int idx;

    if (!buf || !range || !keluaran) return -1;
    if (k < 1.0) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0 || (int)k > count) return -1;

    qsort(data, (size_t)count, sizeof(double), banding_descending);

    idx = (int)k - 1;
    *keluaran = data[idx];
    return 0;
}

/* SMALL: Nilai terkecil ke-n */
int statistik_small(const struct buffer_tabel *buf, const char *range,
                    double k, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    int idx;

    if (!buf || !range || !keluaran) return -1;
    if (k < 1.0) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0 || (int)k > count) return -1;

    qsort(data, (size_t)count, sizeof(double), banding_ascending);

    idx = (int)k - 1;
    *keluaran = data[idx];
    return 0;
}

/* RANK: Peringkat nilai dalam daftar */
int statistik_rank(const struct buffer_tabel *buf, const char *range,
                   double nilai, int descending, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    int rank = 1;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    for (i = 0; i < count; i++) {
        if (descending) {
            if (data[i] > nilai) rank++;
        } else {
            if (data[i] < nilai) rank++;
        }
    }

    *keluaran = (double)rank;
    return 0;
}

/* ============================================================
 * FUNGSI DISTRIBUSI (AVEDEV, DEVSQ, KURT, SKEW)
 * ============================================================ */

/* AVEDEV: Rata-rata deviasi absolut */
int statistik_avedev(const struct buffer_tabel *buf, const char *range,
                     double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double sum = 0.0;
    double mean;
    double dev_sum = 0.0;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / (double)count;

    for (i = 0; i < count; i++) {
        dev_sum += stat_abs(data[i] - mean);
    }

    *keluaran = dev_sum / (double)count;
    return 0;
}

/* DEVSQ: Sum of squared deviations */
int statistik_devsq(const struct buffer_tabel *buf, const char *range,
                    double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double sum = 0.0;
    double mean;
    double sq_sum = 0.0;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / (double)count;

    for (i = 0; i < count; i++) {
        double diff = data[i] - mean;
        sq_sum += diff * diff;
    }

    *keluaran = sq_sum;
    return 0;
}

/* SKEW: Skewness (asymetri distribusi) */
int statistik_skew(const struct buffer_tabel *buf, const char *range,
                   double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double sum = 0.0;
    double mean;
    double stdev;
    double skew = 0.0;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count < 3) return -1;

    /* Hitung mean */
    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / (double)count;

    /* Hitung standar deviasi */
    sum = 0.0;
    for (i = 0; i < count; i++) {
        double diff = data[i] - mean;
        sum += diff * diff;
    }
    stdev = sqrt(sum / (double)count);

    if (stdev < STAT_TOLERANSI) return -1;

    /* Hitung skewness */
    for (i = 0; i < count; i++) {
        double z = (data[i] - mean) / stdev;
        skew += z * z * z;
    }

    *keluaran = skew * (double)count / ((double)(count - 1) * (double)(count - 2));
    return 0;
}

/* KURT: Kurtosis (keruncingan distribusi) */
int statistik_kurt(const struct buffer_tabel *buf, const char *range,
                   double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double sum = 0.0;
    double mean;
    double stdev;
    double kurt = 0.0;
    int i;
    double n;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count < 4) return -1;

    n = (double)count;

    /* Hitung mean */
    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / n;

    /* Hitung standar deviasi */
    sum = 0.0;
    for (i = 0; i < count; i++) {
        double diff = data[i] - mean;
        sum += diff * diff;
    }
    stdev = sqrt(sum / n);

    if (stdev < STAT_TOLERANSI) return -1;

    /* Hitung kurtosis */
    for (i = 0; i < count; i++) {
        double z = (data[i] - mean) / stdev;
        kurt += z * z * z * z;
    }

    /* Excess kurtosis (kurtosis - 3) */
    *keluaran = kurt * n * (n + 1.0) / ((n - 1.0) * (n - 2.0) * (n - 3.0)) -
                3.0 * (n - 1.0) * (n - 1.0) / ((n - 2.0) * (n - 3.0));
    return 0;
}

/* ============================================================
 * FUNGSI STDEVP DAN VARP
 * ============================================================ */

/* STDEVP: Standar deviasi populasi */
int statistik_stdevp(const struct buffer_tabel *buf, const char *range,
                     double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double sum = 0.0;
    double mean;
    double sq_sum = 0.0;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / (double)count;

    for (i = 0; i < count; i++) {
        double diff = data[i] - mean;
        sq_sum += diff * diff;
    }

    *keluaran = sqrt(sq_sum / (double)count);
    return 0;
}

/* VARP: Varians populasi */
int statistik_varp(const struct buffer_tabel *buf, const char *range,
                   double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double sum = 0.0;
    double mean;
    double sq_sum = 0.0;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    for (i = 0; i < count; i++) {
        sum += data[i];
    }
    mean = sum / (double)count;

    for (i = 0; i < count; i++) {
        double diff = data[i] - mean;
        sq_sum += diff * diff;
    }

    *keluaran = sq_sum / (double)count;
    return 0;
}

/* ============================================================
 * FUNGSI RATA-RATA KHUSUS
 * ============================================================ */

/* GEOMEAN: Rata-rata geometrik */
int statistik_geomean(const struct buffer_tabel *buf, const char *range,
                      double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double log_sum = 0.0;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    for (i = 0; i < count; i++) {
        if (data[i] <= 0.0) return -1;
        log_sum += log(data[i]);
    }

    *keluaran = exp(log_sum / (double)count);
    return 0;
}

/* HARMEAN: Rata-rata harmonik */
int statistik_harmean(const struct buffer_tabel *buf, const char *range,
                      double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    double sum = 0.0;
    int i;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    for (i = 0; i < count; i++) {
        if (data[i] == 0.0) return -1;
        sum += 1.0 / data[i];
    }

    *keluaran = (double)count / sum;
    return 0;
}

/* TRIMMEAN: Rata-rata terpotong */
int statistik_trimmean(const struct buffer_tabel *buf, const char *range,
                       double percent, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    int trim_count;
    double sum = 0.0;
    int i;
    int start, end;

    if (!buf || !range || !keluaran) return -1;
    if (percent < 0.0 || percent >= 1.0) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);
    if (count == 0) return -1;

    qsort(data, (size_t)count, sizeof(double), banding_ascending);

    trim_count = (int)((double)count * percent / 2.0);
    start = trim_count;
    end = count - trim_count;

    if (start >= end) return -1;

    for (i = start; i < end; i++) {
        sum += data[i];
    }

    *keluaran = sum / (double)(end - start);
    return 0;
}

/* ============================================================
 * FUNGSI KONDISIONAL (COUNTIF, SUMIF, AVERAGEIF)
 * ============================================================ */

/* Evaluasi kondisi sederhana */
static int evaluasi_kondisi(double nilai, const char *op, double batas) {
    if (strcmp(op, "=") == 0 || strcmp(op, "==") == 0) {
        return (stat_abs(nilai - batas) < STAT_TOLERANSI);
    }
    if (strcmp(op, "<>") == 0 || strcmp(op, "!=") == 0) {
        return (stat_abs(nilai - batas) >= STAT_TOLERANSI);
    }
    if (strcmp(op, "<") == 0) return (nilai < batas);
    if (strcmp(op, ">") == 0) return (nilai > batas);
    if (strcmp(op, "<=") == 0) return (nilai <= batas);
    if (strcmp(op, ">=") == 0) return (nilai >= batas);
    return 0;
}

/* Parse kondisi string (misal: ">5", "<=10", "=test") */
static int parse_kondisi(const char *kondisi, char *op, double *batas) {
    const char *p = kondisi;
    int i = 0;

    if (!kondisi || !op || !batas) return -1;

    /* Baca operator */
    if (*p == '<' || *p == '>' || *p == '=') {
        op[i++] = *p++;
        if (*p == '=' || *p == '>') {
            op[i++] = *p++;
        }
    }
    op[i] = '\0';

    /* Jika tidak ada operator, default adalah "=" */
    if (i == 0) {
        op[0] = '=';
        op[1] = '\0';
    }

    /* Baca nilai */
    *batas = atof(p);

    return 0;
}

/* COUNTIF: Hitung sel yang memenuhi kondisi */
int statistik_countif(const struct buffer_tabel *buf, const char *range,
                      const char *kondisi, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    char op[4];
    double batas;
    int hasil = 0;
    int i;

    if (!buf || !range || !kondisi || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;
    if (parse_kondisi(kondisi, op, &batas) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);

    for (i = 0; i < count; i++) {
        if (evaluasi_kondisi(data[i], op, batas)) {
            hasil++;
        }
    }

    *keluaran = (double)hasil;
    return 0;
}

/* SUMIF: Jumlahkan sel yang memenuhi kondisi */
int statistik_sumif(const struct buffer_tabel *buf, const char *range,
                    const char *kondisi, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    char op[4];
    double batas;
    double sum = 0.0;
    int i;

    if (!buf || !range || !kondisi || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;
    if (parse_kondisi(kondisi, op, &batas) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);

    for (i = 0; i < count; i++) {
        if (evaluasi_kondisi(data[i], op, batas)) {
            sum += data[i];
        }
    }

    *keluaran = sum;
    return 0;
}

/* AVERAGEIF: Rata-rata sel yang memenuhi kondisi */
int statistik_averageif(const struct buffer_tabel *buf, const char *range,
                        const char *kondisi, double *keluaran) {
    int x1, y1, x2, y2;
    double data[STAT_MAKS_DATA];
    int count;
    char op[4];
    double batas;
    double sum = 0.0;
    int n = 0;
    int i;

    if (!buf || !range || !kondisi || !keluaran) return -1;

    if (parse_range_stat(range, &x1, &y1, &x2, &y2) != 0) return -1;
    if (parse_kondisi(kondisi, op, &batas) != 0) return -1;

    count = kumpulkan_nilai(buf, x1, y1, x2, y2, data, STAT_MAKS_DATA);

    for (i = 0; i < count; i++) {
        if (evaluasi_kondisi(data[i], op, batas)) {
            sum += data[i];
            n++;
        }
    }

    if (n == 0) return -1;

    *keluaran = sum / (double)n;
    return 0;
}

/* ============================================================
 * FUNGSI KORELASI DAN REGRESI
 * ============================================================ */

/* CORREL: Koefisien korelasi Pearson */
int statistik_correl(const struct buffer_tabel *buf, const char *range1,
                     const char *range2, double *keluaran) {
    int x1a, y1a, x2a, y2a;
    int x1b, y1b, x2b, y2b;
    double data1[STAT_MAKS_DATA];
    double data2[STAT_MAKS_DATA];
    int count1, count2;
    double sum1 = 0.0, sum2 = 0.0;
    double mean1, mean2;
    double num = 0.0;
    double den1 = 0.0, den2 = 0.0;
    int i;

    if (!buf || !range1 || !range2 || !keluaran) return -1;

    if (parse_range_stat(range1, &x1a, &y1a, &x2a, &y2a) != 0) return -1;
    if (parse_range_stat(range2, &x1b, &y1b, &x2b, &y2b) != 0) return -1;

    count1 = kumpulkan_nilai(buf, x1a, y1a, x2a, y2a, data1, STAT_MAKS_DATA);
    count2 = kumpulkan_nilai(buf, x1b, y1b, x2b, y2b, data2, STAT_MAKS_DATA);

    if (count1 != count2 || count1 < 2) return -1;

    for (i = 0; i < count1; i++) {
        sum1 += data1[i];
        sum2 += data2[i];
    }
    mean1 = sum1 / (double)count1;
    mean2 = sum2 / (double)count2;

    for (i = 0; i < count1; i++) {
        double diff1 = data1[i] - mean1;
        double diff2 = data2[i] - mean2;
        num += diff1 * diff2;
        den1 += diff1 * diff1;
        den2 += diff2 * diff2;
    }

    if (den1 < STAT_TOLERANSI || den2 < STAT_TOLERANSI) return -1;

    *keluaran = num / sqrt(den1 * den2);
    return 0;
}

/* COVAR: Kovarians */
int statistik_covar(const struct buffer_tabel *buf, const char *range1,
                    const char *range2, double *keluaran) {
    int x1a, y1a, x2a, y2a;
    int x1b, y1b, x2b, y2b;
    double data1[STAT_MAKS_DATA];
    double data2[STAT_MAKS_DATA];
    int count1, count2;
    double sum1 = 0.0, sum2 = 0.0;
    double mean1, mean2;
    double cov = 0.0;
    int i;

    if (!buf || !range1 || !range2 || !keluaran) return -1;

    if (parse_range_stat(range1, &x1a, &y1a, &x2a, &y2a) != 0) return -1;
    if (parse_range_stat(range2, &x1b, &y1b, &x2b, &y2b) != 0) return -1;

    count1 = kumpulkan_nilai(buf, x1a, y1a, x2a, y2a, data1, STAT_MAKS_DATA);
    count2 = kumpulkan_nilai(buf, x1b, y1b, x2b, y2b, data2, STAT_MAKS_DATA);

    if (count1 != count2 || count1 < 2) return -1;

    for (i = 0; i < count1; i++) {
        sum1 += data1[i];
        sum2 += data2[i];
    }
    mean1 = sum1 / (double)count1;
    mean2 = sum2 / (double)count2;

    for (i = 0; i < count1; i++) {
        cov += (data1[i] - mean1) * (data2[i] - mean2);
    }

    *keluaran = cov / (double)count1;
    return 0;
}

/* SLOPE: Kemiringan garis regresi */
int statistik_slope(const struct buffer_tabel *buf, const char *range_y,
                    const char *range_x, double *keluaran) {
    int x1y, y1y, x2y, y2y;
    int x1x, y1x, x2x, y2x;
    double y_data[STAT_MAKS_DATA];
    double x_data[STAT_MAKS_DATA];
    int count_y, count_x;
    double sum_x = 0.0, sum_y = 0.0;
    double mean_x, mean_y;
    double num = 0.0, den = 0.0;
    int i;

    if (!buf || !range_y || !range_x || !keluaran) return -1;

    if (parse_range_stat(range_y, &x1y, &y1y, &x2y, &y2y) != 0) return -1;
    if (parse_range_stat(range_x, &x1x, &y1x, &x2x, &y2x) != 0) return -1;

    count_y = kumpulkan_nilai(buf, x1y, y1y, x2y, y2y, y_data, STAT_MAKS_DATA);
    count_x = kumpulkan_nilai(buf, x1x, y1x, x2x, y2x, x_data, STAT_MAKS_DATA);

    if (count_y != count_x || count_y < 2) return -1;

    for (i = 0; i < count_y; i++) {
        sum_x += x_data[i];
        sum_y += y_data[i];
    }
    mean_x = sum_x / (double)count_y;
    mean_y = sum_y / (double)count_y;

    for (i = 0; i < count_y; i++) {
        double diff_x = x_data[i] - mean_x;
        num += diff_x * (y_data[i] - mean_y);
        den += diff_x * diff_x;
    }

    if (den < STAT_TOLERANSI) return -1;

    *keluaran = num / den;
    return 0;
}

/* INTERCEPT: Intercept garis regresi */
int statistik_intercept(const struct buffer_tabel *buf, const char *range_y,
                        const char *range_x, double *keluaran) {
    double slope;
    int x1y, y1y, x2y, y2y;
    int x1x, y1x, x2x, y2x;
    double y_data[STAT_MAKS_DATA];
    double x_data[STAT_MAKS_DATA];
    int count_y, count_x;
    double sum_x = 0.0, sum_y = 0.0;
    double mean_x, mean_y;
    int i;

    if (!buf || !range_y || !range_x || !keluaran) return -1;

    if (statistik_slope(buf, range_y, range_x, &slope) != 0) return -1;

    if (parse_range_stat(range_y, &x1y, &y1y, &x2y, &y2y) != 0) return -1;
    if (parse_range_stat(range_x, &x1x, &y1x, &x2x, &y2x) != 0) return -1;

    count_y = kumpulkan_nilai(buf, x1y, y1y, x2y, y2y, y_data, STAT_MAKS_DATA);
    count_x = kumpulkan_nilai(buf, x1x, y1x, x2x, y2x, x_data, STAT_MAKS_DATA);

    for (i = 0; i < count_y; i++) {
        sum_x += x_data[i];
        sum_y += y_data[i];
    }
    mean_x = sum_x / (double)count_y;
    mean_y = sum_y / (double)count_y;

    *keluaran = mean_y - slope * mean_x;
    return 0;
}

/* FORECAST: Prediksi linear */
int statistik_forecast(const struct buffer_tabel *buf, double x,
                       const char *range_y, const char *range_x,
                       double *keluaran) {
    double slope, intercept;

    if (!buf || !range_y || !range_x || !keluaran) return -1;

    if (statistik_slope(buf, range_y, range_x, &slope) != 0) return -1;
    if (statistik_intercept(buf, range_y, range_x, &intercept) != 0) return -1;

    *keluaran = intercept + slope * x;
    return 0;
}

/* RSQ: R-squared (koefisien determinasi) */
int statistik_rsq(const struct buffer_tabel *buf, const char *range_y,
                  const char *range_x, double *keluaran) {
    double corr;

    if (!buf || !range_y || !range_x || !keluaran) return -1;

    if (statistik_correl(buf, range_y, range_x, &corr) != 0) return -1;

    *keluaran = corr * corr;
    return 0;
}

/* STEYX: Standard error of estimate */
int statistik_steyx(const struct buffer_tabel *buf, const char *range_y,
                    const char *range_x, double *keluaran) {
    int x1y, y1y, x2y, y2y;
    int x1x, y1x, x2x, y2x;
    double y_data[STAT_MAKS_DATA];
    double x_data[STAT_MAKS_DATA];
    int count_y, count_x;
    double sum_x = 0.0, sum_y = 0.0;
    double mean_x, mean_y;
    double slope;
    double ss_res = 0.0;
    int i;

    if (!buf || !range_y || !range_x || !keluaran) return -1;

    if (statistik_slope(buf, range_y, range_x, &slope) != 0) return -1;

    if (parse_range_stat(range_y, &x1y, &y1y, &x2y, &y2y) != 0) return -1;
    if (parse_range_stat(range_x, &x1x, &y1x, &x2x, &y2x) != 0) return -1;

    count_y = kumpulkan_nilai(buf, x1y, y1y, x2y, y2y, y_data, STAT_MAKS_DATA);
    count_x = kumpulkan_nilai(buf, x1x, y1x, x2x, y2x, x_data, STAT_MAKS_DATA);

    if (count_y < 3) return -1;

    for (i = 0; i < count_y; i++) {
        sum_x += x_data[i];
        sum_y += y_data[i];
    }
    mean_x = sum_x / (double)count_y;
    mean_y = sum_y / (double)count_y;

    for (i = 0; i < count_y; i++) {
        double y_pred = mean_y + slope * (x_data[i] - mean_x);
        double diff = y_data[i] - y_pred;
        ss_res += diff * diff;
    }

    *keluaran = sqrt(ss_res / (double)(count_y - 2));
    return 0;
}

/* ============================================================
 * DISPATCHER FUNGSI STATISTIK
 * ============================================================ */

/* Evaluasi fungsi statistik agregat */
int statistik_eval_agregat(const struct buffer_tabel *buf, const char *nama,
                           const char *range, double *keluaran) {
    if (!buf || !nama || !range || !keluaran) return -1;

    if (banding_str(nama, "MEDIAN") == 0) {
        return statistik_median(buf, range, keluaran);
    }
    if (banding_str(nama, "MODE") == 0 || banding_str(nama, "MODE.SNGL") == 0) {
        return statistik_mode(buf, range, keluaran);
    }
    if (banding_str(nama, "AVEDEV") == 0) {
        return statistik_avedev(buf, range, keluaran);
    }
    if (banding_str(nama, "DEVSQ") == 0) {
        return statistik_devsq(buf, range, keluaran);
    }
    if (banding_str(nama, "SKEW") == 0 || banding_str(nama, "SKEWNESS") == 0) {
        return statistik_skew(buf, range, keluaran);
    }
    if (banding_str(nama, "KURT") == 0 || banding_str(nama, "KURTOSIS") == 0) {
        return statistik_kurt(buf, range, keluaran);
    }
    if (banding_str(nama, "STDEVP") == 0 || banding_str(nama, "STDEV.P") == 0) {
        return statistik_stdevp(buf, range, keluaran);
    }
    if (banding_str(nama, "VARP") == 0 || banding_str(nama, "VAR.P") == 0) {
        return statistik_varp(buf, range, keluaran);
    }
    if (banding_str(nama, "GEOMEAN") == 0) {
        return statistik_geomean(buf, range, keluaran);
    }
    if (banding_str(nama, "HARMEAN") == 0) {
        return statistik_harmean(buf, range, keluaran);
    }

    return -1;
}

/* Evaluasi fungsi statistik dengan 1 argumen tambahan */
int statistik_eval_1arg(const struct buffer_tabel *buf, const char *nama,
                        const char *range, double arg, double *keluaran) {
    if (!buf || !nama || !range || !keluaran) return -1;

    if (banding_str(nama, "LARGE") == 0) {
        return statistik_large(buf, range, arg, keluaran);
    }
    if (banding_str(nama, "SMALL") == 0) {
        return statistik_small(buf, range, arg, keluaran);
    }
    if (banding_str(nama, "PERCENTILE") == 0 || banding_str(nama, "PERCENTILE.INC") == 0) {
        return statistik_percentile(buf, range, arg, keluaran);
    }
    if (banding_str(nama, "QUARTILE") == 0 || banding_str(nama, "QUARTILE.INC") == 0) {
        return statistik_quartile(buf, range, arg, keluaran);
    }
    if (banding_str(nama, "TRIMMEAN") == 0) {
        return statistik_trimmean(buf, range, arg, keluaran);
    }

    return -1;
}

/* Evaluasi fungsi statistik dengan 2 range */
int statistik_eval_2range(const struct buffer_tabel *buf, const char *nama,
                          const char *range1, const char *range2,
                          double *keluaran) {
    if (!buf || !nama || !range1 || !range2 || !keluaran) return -1;

    if (banding_str(nama, "CORREL") == 0 || banding_str(nama, "CORRELATION") == 0) {
        return statistik_correl(buf, range1, range2, keluaran);
    }
    if (banding_str(nama, "COVAR") == 0 || banding_str(nama, "COVARIANCE") == 0) {
        return statistik_covar(buf, range1, range2, keluaran);
    }
    if (banding_str(nama, "SLOPE") == 0) {
        return statistik_slope(buf, range1, range2, keluaran);
    }
    if (banding_str(nama, "INTERCEPT") == 0) {
        return statistik_intercept(buf, range1, range2, keluaran);
    }
    if (banding_str(nama, "RSQ") == 0 || banding_str(nama, "RSQUARED") == 0) {
        return statistik_rsq(buf, range1, range2, keluaran);
    }
    if (banding_str(nama, "STEYX") == 0) {
        return statistik_steyx(buf, range1, range2, keluaran);
    }

    return -1;
}
