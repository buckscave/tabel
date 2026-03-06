/* ============================================================
 * TABEL v3.0 
 * Berkas: hitung.c - Formula Fungsi Hitung (Matematika & Agregat)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * FUNGSI HELPER STRING (dipakai internal)
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

/* ============================================================
 * FUNGSI AGREGAT (Range)
 * ============================================================ */

/* Struktur nilai lokal untuk ambil nilai sel */
typedef enum {
    NILAI_ANGKA = 0,
    NILAI_TEKS  = 1
} jenis_nilai_lokal;

struct nilai_lokal {
    jenis_nilai_lokal jenis;
    double angka;
    char teks[MAKS_TEKS];
};

/* Forward declaration */
static int ambil_nilai_sel_lokal(const struct buffer_tabel *buf, int x, int y,
                                 struct nilai_lokal *keluaran);

/* Helper: ambil nilai dari sel */
static int ambil_nilai_sel_lokal(const struct buffer_tabel *buf, int x, int y,
                                 struct nilai_lokal *keluaran) {
    const char *t;
    char *endp;
    double v;

    if (!buf || !keluaran) return -1;

    /* Di luar batas */
    if (x < 0 || x >= buf->cfg.kolom || y < 0 || y >= buf->cfg.baris) {
        keluaran->jenis = NILAI_TEKS;
        keluaran->teks[0] = '\0';
        keluaran->angka = 0.0;
        return 0;
    }

    t = buf->isi[y][x];

    /* Sel kosong */
    if (t[0] == '\0') {
        keluaran->jenis = NILAI_ANGKA;
        keluaran->angka = 0.0;
        keluaran->teks[0] = '\0';
        return 0;
    }

    /* Formula: evaluasi rekursif */
    if (t[0] == '=') {
        int adalah_num;
        char err[64];
        if (evaluasi_string(buf, x, y, t, &v, &adalah_num, err,
                            sizeof(err)) == 0 && adalah_num) {
            keluaran->jenis = NILAI_ANGKA;
            keluaran->angka = v;
            keluaran->teks[0] = '\0';
        } else {
            keluaran->jenis = NILAI_TEKS;
            keluaran->teks[0] = '\0';
            keluaran->angka = 0.0;
        }
        return 0;
    }

    /* Coba parse sebagai angka */
    endp = NULL;
    v = strtod(t, &endp);
    if (endp && endp != t && *endp == '\0') {
        keluaran->jenis = NILAI_ANGKA;
        keluaran->angka = v;
        keluaran->teks[0] = '\0';
    } else {
        keluaran->jenis = NILAI_TEKS;
        strncpy(keluaran->teks, t, MAKS_TEKS - 1);
        keluaran->teks[MAKS_TEKS - 1] = '\0';
        keluaran->angka = 0.0;
    }

    return 0;
}

/* SUM: Jumlahkan nilai dalam range */
static int fn_sum(const struct buffer_tabel *buf, int x1, int y1, int x2,
                  int y2, double *keluaran) {
    double sum = 0.0;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    sum += v.angka;
                }
            }
        }
    }

    *keluaran = sum;
    return 0;
}

/* COUNT: Hitung jumlah sel berisi angka */
static int fn_count(const struct buffer_tabel *buf, int x1, int y1, int x2,
                    int y2, double *keluaran) {
    int cnt = 0;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    cnt++;
                }
            }
        }
    }

    *keluaran = (double)cnt;
    return 0;
}

/* COUNTA: Hitung jumlah sel tidak kosong */
static int fn_counta(const struct buffer_tabel *buf, int x1, int y1, int x2,
                     int y2, double *keluaran) {
    int cnt = 0;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            const char *t = buf->isi[r][c];
            if (t[0] != '\0') {
                cnt++;
            }
        }
    }

    *keluaran = (double)cnt;
    return 0;
}

/* MIN: Nilai minimum */
static int fn_min(const struct buffer_tabel *buf, int x1, int y1, int x2,
                  int y2, double *keluaran) {
    double minv = 0.0;
    int ada = 0;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    if (!ada) {
                        minv = v.angka;
                        ada = 1;
                    } else if (v.angka < minv) {
                        minv = v.angka;
                    }
                }
            }
        }
    }

    *keluaran = ada ? minv : 0.0;
    return 0;
}

/* MAX: Nilai maksimum */
static int fn_max(const struct buffer_tabel *buf, int x1, int y1, int x2,
                  int y2, double *keluaran) {
    double maxv = 0.0;
    int ada = 0;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    if (!ada) {
                        maxv = v.angka;
                        ada = 1;
                    } else if (v.angka > maxv) {
                        maxv = v.angka;
                    }
                }
            }
        }
    }

    *keluaran = ada ? maxv : 0.0;
    return 0;
}

/* AVG/AVERAGE: Rata-rata */
static int fn_avg(const struct buffer_tabel *buf, int x1, int y1, int x2,
                  int y2, double *keluaran) {
    double sum = 0.0;
    double cnt = 0.0;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    sum += v.angka;
                    cnt += 1.0;
                }
            }
        }
    }

    *keluaran = (cnt > 0.0) ? (sum / cnt) : 0.0;
    return 0;
}

/* PRODUCT: Perkalian semua nilai */
static int fn_product(const struct buffer_tabel *buf, int x1, int y1, int x2,
                      int y2, double *keluaran) {
    double prod = 1.0;
    int ada = 0;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    prod *= v.angka;
                    ada = 1;
                }
            }
        }
    }

    *keluaran = ada ? prod : 0.0;
    return 0;
}

/* STDEV: Standar deviasi (sampel) */
static int fn_stdev(const struct buffer_tabel *buf, int x1, int y1, int x2,
                    int y2, double *keluaran) {
    double sum = 0.0, sum_sq = 0.0, cnt = 0.0;
    double mean, variance;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    /* Pass 1: hitung mean */
    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    sum += v.angka;
                    cnt += 1.0;
                }
            }
        }
    }

    if (cnt < 2.0) {
        *keluaran = 0.0;
        return 0;
    }

    mean = sum / cnt;

    /* Pass 2: hitung variance */
    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    double diff = v.angka - mean;
                    sum_sq += diff * diff;
                }
            }
        }
    }

    variance = sum_sq / (cnt - 1.0);
    *keluaran = sqrt(variance);
    return 0;
}

/* VAR: Variance (sampel) */
static int fn_var(const struct buffer_tabel *buf, int x1, int y1, int x2,
                  int y2, double *keluaran) {
    double sum = 0.0, sum_sq = 0.0, cnt = 0.0;
    double mean;
    int xs = (x1 < x2) ? x1 : x2;
    int xe = (x1 > x2) ? x1 : x2;
    int ys = (y1 < y2) ? y1 : y2;
    int ye = (y1 > y2) ? y1 : y2;
    int r, c;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    sum += v.angka;
                    cnt += 1.0;
                }
            }
        }
    }

    if (cnt < 2.0) {
        *keluaran = 0.0;
        return 0;
    }

    mean = sum / cnt;

    for (r = ys; r <= ye; r++) {
        for (c = xs; c <= xe; c++) {
            struct nilai_lokal v;
            if (ambil_nilai_sel_lokal(buf, c, r, &v) == 0) {
                if (v.jenis == NILAI_ANGKA) {
                    double diff = v.angka - mean;
                    sum_sq += diff * diff;
                }
            }
        }
    }

    *keluaran = sum_sq / (cnt - 1.0);
    return 0;
}

/* ============================================================
 * FUNGSI MATEMATIKA 1 ARGUMEN
 * ============================================================ */

/* ABS: Nilai absolut */
static int fn_abs(double arg, double *keluaran) {
    *keluaran = fabs(arg);
    return 0;
}

/* SQRT: Akar kuadrat */
static int fn_sqrt(double arg, double *keluaran) {
    if (arg < 0.0) return -1;
    *keluaran = sqrt(arg);
    return 0;
}

/* SIN: Sinus */
static int fn_sin(double arg, double *keluaran) {
    *keluaran = sin(arg);
    return 0;
}

/* COS: Kosinus */
static int fn_cos(double arg, double *keluaran) {
    *keluaran = cos(arg);
    return 0;
}

/* TAN: Tangen */
static int fn_tan(double arg, double *keluaran) {
    *keluaran = tan(arg);
    return 0;
}

/* ASIN: Arcus sinus */
static int fn_asin(double arg, double *keluaran) {
    if (arg < -1.0 || arg > 1.0) return -1;
    *keluaran = asin(arg);
    return 0;
}

/* ACOS: Arcus kosinus */
static int fn_acos(double arg, double *keluaran) {
    if (arg < -1.0 || arg > 1.0) return -1;
    *keluaran = acos(arg);
    return 0;
}

/* ATAN: Arcus tangen */
static int fn_atan(double arg, double *keluaran) {
    *keluaran = atan(arg);
    return 0;
}

/* EXP: Eksponensial */
static int fn_exp(double arg, double *keluaran) {
    *keluaran = exp(arg);
    return 0;
}

/* LN/LOG: Logaritma natural */
static int fn_ln(double arg, double *keluaran) {
    if (arg <= 0.0) return -1;
    *keluaran = log(arg);
    return 0;
}

/* LOG10: Logaritma basis 10 */
static int fn_log10(double arg, double *keluaran) {
    if (arg <= 0.0) return -1;
    *keluaran = log10(arg);
    return 0;
}

/* INT/FLOOR: Pembulatan ke bawah */
static int fn_floor(double arg, double *keluaran) {
    *keluaran = floor(arg);
    return 0;
}

/* CEILING/CEIL: Pembulatan ke atas */
static int fn_ceil(double arg, double *keluaran) {
    *keluaran = ceil(arg);
    return 0;
}

/* ROUND: Pembulatan ke integer terdekat */
static int fn_round(double arg, double *keluaran) {
    *keluaran = floor(arg + 0.5);
    return 0;
}

/* TRUNC: Potong desimal */
static int fn_trunc(double arg, double *keluaran) {
    *keluaran = (arg >= 0) ? floor(arg) : ceil(arg);
    return 0;
}

/* SIGN: Tanda bilangan */
static int fn_sign(double arg, double *keluaran) {
    *keluaran = (arg > 0) ? 1.0 : ((arg < 0) ? -1.0 : 0.0);
    return 0;
}

/* DEGREES: Radian ke derajat */
static int fn_degrees(double arg, double *keluaran) {
    *keluaran = arg * 180.0 / 3.14159265358979323846;
    return 0;
}

/* RADIANS: Derajat ke radian */
static int fn_radians(double arg, double *keluaran) {
    *keluaran = arg * 3.14159265358979323846 / 180.0;
    return 0;
}

/* ============================================================
 * FUNGSI MATEMATIKA 2 ARGUMEN
 * ============================================================ */

/* POWER/POW: Pangkat */
static int fn_power(double arg1, double arg2, double *keluaran) {
    *keluaran = pow(arg1, arg2);
    return 0;
}

/* MOD: Sisa pembagian */
static int fn_mod(double arg1, double arg2, double *keluaran) {
    if (arg2 == 0.0) return -1;
    *keluaran = fmod(arg1, arg2);
    return 0;
}

/* GCD: Faktor persekutuan terbesar */
static int fn_gcd(double arg1, double arg2, double *keluaran) {
    int a = abs((int)arg1);
    int b = abs((int)arg2);
    while (b != 0) {
        int t = b;
        b = a % b;
        a = t;
    }
    *keluaran = (double)a;
    return 0;
}

/* LCM: Kelipatan persekutuan terkecil */
static int fn_lcm(double arg1, double arg2, double *keluaran) {
    int a = abs((int)arg1);
    int b = abs((int)arg2);
    int gcd_val = a;
    int temp = b;
    while (temp != 0) {
        int t = temp;
        temp = gcd_val % temp;
        gcd_val = t;
    }
    if (gcd_val == 0) {
        *keluaran = 0.0;
    } else {
        *keluaran = (double)(a * b / gcd_val);
    }
    return 0;
}

/* ROUND dengan presisi */
static int fn_round_prec(double arg1, double arg2, double *keluaran) {
    double faktor = pow(10.0, arg2);
    *keluaran = floor(arg1 * faktor + 0.5) / faktor;
    return 0;
}

/* FLOOR dengan presisi */
static int fn_floor_prec(double arg1, double arg2, double *keluaran) {
    double faktor = pow(10.0, arg2);
    *keluaran = floor(arg1 * faktor) / faktor;
    return 0;
}

/* CEILING dengan presisi */
static int fn_ceil_prec(double arg1, double arg2, double *keluaran) {
    double faktor = pow(10.0, arg2);
    *keluaran = ceil(arg1 * faktor) / faktor;
    return 0;
}

/* QUOTIENT: Hasil bagi integer */
static int fn_quotient(double arg1, double arg2, double *keluaran) {
    if (arg2 == 0.0) return -1;
    *keluaran = (double)((int)(arg1 / arg2));
    return 0;
}

/* ATAN2: Arcus tangen 2 argumen */
static int fn_atan2(double arg1, double arg2, double *keluaran) {
    *keluaran = atan2(arg1, arg2);
    return 0;
}

/* ============================================================
 * FUNGSI LOGIKA
 * ============================================================ */

/* IF: Kondisi */
int hitung_if(double kondisi, double benar, double salah,
              double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (kondisi != 0.0) ? benar : salah;
    return 0;
}

/* ============================================================
 * DISPATCHER FUNGSI AGREGAT
 * ============================================================ */

/* Parsing range teks ke koordinat */
static int parse_range_teks(const char *s, int *x1, int *y1,
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

/* Hitung fungsi agregat dari range */
int hitung_agregat(const struct buffer_tabel *buf, const char *nama,
                   const char *range, double *keluaran) {
    int x1, y1, x2, y2;
    int ret;

    if (!buf || !nama || !range || !keluaran) return -1;

    ret = parse_range_teks(range, &x1, &y1, &x2, &y2);
    if (ret != 0) return -1;

    if (banding_str(nama, "SUM") == 0) {
        return fn_sum(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "AVG") == 0 || banding_str(nama, "AVERAGE") == 0) {
        return fn_avg(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "MIN") == 0) {
        return fn_min(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "MAX") == 0) {
        return fn_max(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "COUNT") == 0) {
        return fn_count(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "COUNTA") == 0) {
        return fn_counta(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "PRODUCT") == 0) {
        return fn_product(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "STDEV") == 0 ||
        banding_str(nama, "STDEV.S") == 0) {
        return fn_stdev(buf, x1, y1, x2, y2, keluaran);
    }
    if (banding_str(nama, "VAR") == 0 || banding_str(nama, "VAR.S") == 0) {
        return fn_var(buf, x1, y1, x2, y2, keluaran);
    }

    return -1;
}

/* ============================================================
 * DISPATCHER FUNGSI MATEMATIKA 1 ARGUMEN
 * ============================================================ */

int hitung_math_1_arg(const char *nama, double arg, double *keluaran) {
    if (!nama || !keluaran) return -1;

    if (banding_str(nama, "ABS") == 0) return fn_abs(arg, keluaran);
    if (banding_str(nama, "SQRT") == 0) return fn_sqrt(arg, keluaran);
    if (banding_str(nama, "SIN") == 0) return fn_sin(arg, keluaran);
    if (banding_str(nama, "COS") == 0) return fn_cos(arg, keluaran);
    if (banding_str(nama, "TAN") == 0) return fn_tan(arg, keluaran);
    if (banding_str(nama, "ASIN") == 0) return fn_asin(arg, keluaran);
    if (banding_str(nama, "ACOS") == 0) return fn_acos(arg, keluaran);
    if (banding_str(nama, "ATAN") == 0) return fn_atan(arg, keluaran);
    if (banding_str(nama, "EXP") == 0) return fn_exp(arg, keluaran);
    if (banding_str(nama, "LN") == 0 || banding_str(nama, "LOG") == 0) {
        return fn_ln(arg, keluaran);
    }
    if (banding_str(nama, "LOG10") == 0) return fn_log10(arg, keluaran);
    if (banding_str(nama, "INT") == 0 || banding_str(nama, "FLOOR") == 0) {
        return fn_floor(arg, keluaran);
    }
    if (banding_str(nama, "CEILING") == 0 || banding_str(nama, "CEIL") == 0) {
        return fn_ceil(arg, keluaran);
    }
    if (banding_str(nama, "ROUND") == 0) return fn_round(arg, keluaran);
    if (banding_str(nama, "TRUNC") == 0 ||
        banding_str(nama, "TRUNCATE") == 0) {
        return fn_trunc(arg, keluaran);
    }
    if (banding_str(nama, "SIGN") == 0) return fn_sign(arg, keluaran);
    if (banding_str(nama, "DEGREES") == 0 || banding_str(nama, "DEG") == 0) {
        return fn_degrees(arg, keluaran);
    }
    if (banding_str(nama, "RADIANS") == 0 || banding_str(nama, "RAD") == 0) {
        return fn_radians(arg, keluaran);
    }

    /* Fungsi tanggal/waktu dengan 1 argumen */
    if (banding_str(nama, "YEAR") == 0) return waktu_fn_year(arg, keluaran);
    if (banding_str(nama, "MONTH") == 0) return waktu_fn_month(arg, keluaran);
    if (banding_str(nama, "DAY") == 0) return waktu_fn_day(arg, keluaran);
    if (banding_str(nama, "HOUR") == 0) return waktu_fn_hour(arg, keluaran);
    if (banding_str(nama, "MINUTE") == 0 ||
        banding_str(nama, "MIN") == 0) {
        return waktu_fn_minute(arg, keluaran);
    }
    if (banding_str(nama, "SECOND") == 0 ||
        banding_str(nama, "SEC") == 0) {
        return waktu_fn_second(arg, keluaran);
    }
    if (banding_str(nama, "WEEKDAY") == 0) {
        return waktu_fn_weekday(arg, keluaran);
    }

    return -1;
}

/* ============================================================
 * DISPATCHER FUNGSI MATEMATIKA 2 ARGUMEN
 * ============================================================ */

int hitung_math_2_arg(const char *nama, double arg1, double arg2,
                      double *keluaran) {
    if (!nama || !keluaran) return -1;

    if (banding_str(nama, "POWER") == 0 || banding_str(nama, "POW") == 0) {
        return fn_power(arg1, arg2, keluaran);
    }
    if (banding_str(nama, "MOD") == 0) return fn_mod(arg1, arg2, keluaran);
    if (banding_str(nama, "GCD") == 0) return fn_gcd(arg1, arg2, keluaran);
    if (banding_str(nama, "LCM") == 0) return fn_lcm(arg1, arg2, keluaran);
    if (banding_str(nama, "ROUND") == 0) {
        return fn_round_prec(arg1, arg2, keluaran);
    }
    if (banding_str(nama, "FLOOR") == 0) {
        return fn_floor_prec(arg1, arg2, keluaran);
    }
    if (banding_str(nama, "CEILING") == 0) {
        return fn_ceil_prec(arg1, arg2, keluaran);
    }
    if (banding_str(nama, "QUOTIENT") == 0) {
        return fn_quotient(arg1, arg2, keluaran);
    }
    if (banding_str(nama, "ATAN2") == 0) return fn_atan2(arg1, arg2, keluaran);

    /* Fungsi tanggal dengan 2 argumen */
    if (banding_str(nama, "EDATE") == 0) {
        return waktu_fn_edate(arg1, arg2, keluaran);
    }
    if (banding_str(nama, "EOMONTH") == 0) {
        return waktu_fn_eomonth(arg1, arg2, keluaran);
    }

    return -1;
}
