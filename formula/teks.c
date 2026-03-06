/* ============================================================
 * TABEL v3.0 
 * Berkas: teks.c - Formula Fungsi Teks (String Manipulation)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

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

/* ============================================================
 * FUNGSI PANJANG DAN EKSTRAKSI
 * ============================================================ */

/* LEN: Panjang string (jumlah grapheme) */
int teks_len(const char *str, double *keluaran) {
    int len = 0;
    const char *p = str;

    if (!str || !keluaran) return -1;

    while (*p) {
        int l = panjang_cp_utf8(p);
        p += l;
        len++;
    }

    *keluaran = (double)len;
    return 0;
}

/* LEFT: Ambil n karakter dari kiri */
int teks_left(const char *str, double jumlah, char *keluaran,
              size_t ukuran) {
    int jml;
    const char *p;
    int i;

    if (!str || !keluaran || ukuran == 0) return -1;

    jml = (int)jumlah;
    p = str;
    i = 0;

    if (jml <= 0) {
        keluaran[0] = '\0';
        return 0;
    }

    while (*p && i < jml && i < (int)ukuran - 1) {
        int l = panjang_cp_utf8(p);
        if ((size_t)(i + l) >= ukuran) break;
        memcpy(keluaran + i, p, (size_t)l);
        keluaran[i + l] = '\0';
        p += l;
        i++;
    }
    keluaran[i] = '\0';
    return 0;
}

/* RIGHT: Ambil n karakter dari kanan */
int teks_right(const char *str, double jumlah, char *keluaran,
               size_t ukuran) {
    int jml;
    int len = 0;
    const char *p;
    int start;
    int i;

    if (!str || !keluaran || ukuran == 0) return -1;

    jml = (int)jumlah;
    p = str;

    if (jml <= 0) {
        keluaran[0] = '\0';
        return 0;
    }

    /* Hitung panjang dalam grapheme */
    while (*p) {
        p += panjang_cp_utf8(p);
        len++;
    }

    start = len - jml;
    if (start < 0) start = 0;

    p = str;
    for (i = 0; i < start && *p; i++) {
        p += panjang_cp_utf8(p);
    }

    strncpy(keluaran, p, ukuran - 1);
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* MID: Ambil n karakter dari posisi */
int teks_mid(const char *str, double mulai, double jumlah,
             char *keluaran, size_t ukuran) {
    int start_idx;
    int jml;
    const char *p;
    int i;
    int out_idx;

    if (!str || !keluaran || ukuran == 0) return -1;

    start_idx = (int)mulai - 1; /* 1-based */
    jml = (int)jumlah;
    p = str;
    i = 0;
    out_idx = 0;

    if (start_idx < 0 || jml <= 0) {
        keluaran[0] = '\0';
        return 0;
    }

    /* Lewati karakter sampai start */
    while (*p && i < start_idx) {
        p += panjang_cp_utf8(p);
        i++;
    }

    /* Salin karakter */
    while (*p && out_idx < jml && out_idx < (int)ukuran - 1) {
        int l = panjang_cp_utf8(p);
        if ((size_t)(out_idx + l) >= ukuran) break;
        memcpy(keluaran + out_idx, p, (size_t)l);
        p += l;
        out_idx++;
    }
    keluaran[out_idx] = '\0';
    return 0;
}

/* ============================================================
 * FUNGSI UBAH KASUS
 * ============================================================ */

/* UPPER: Ubah ke huruf besar */
int teks_upper(const char *str, char *keluaran, size_t ukuran) {
    size_t i = 0;

    if (!str || !keluaran || ukuran == 0) return -1;

    while (str[i] && i < ukuran - 1) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x80) {
            keluaran[i] = (char)toupper((int)c);
            i++;
        } else {
            int l = panjang_cp_utf8(str + i);
            if (i + (size_t)l >= ukuran) break;
            memcpy(keluaran + i, str + i, (size_t)l);
            i += (size_t)l;
        }
    }
    keluaran[i] = '\0';
    return 0;
}

/* LOWER: Ubah ke huruf kecil */
int teks_lower(const char *str, char *keluaran, size_t ukuran) {
    size_t i = 0;

    if (!str || !keluaran || ukuran == 0) return -1;

    while (str[i] && i < ukuran - 1) {
        unsigned char c = (unsigned char)str[i];
        if (c < 0x80) {
            keluaran[i] = (char)tolower((int)c);
            i++;
        } else {
            int l = panjang_cp_utf8(str + i);
            if (i + (size_t)l >= ukuran) break;
            memcpy(keluaran + i, str + i, (size_t)l);
            i += (size_t)l;
        }
    }
    keluaran[i] = '\0';
    return 0;
}

/* ============================================================
 * FUNGSI TRIM
 * ============================================================ */

/* TRIM: Hapus spasi berlebih */
int teks_trim(const char *str, char *keluaran, size_t ukuran) {
    const char *p;
    char *out;
    int spasi_sebelumnya;

    if (!str || !keluaran || ukuran == 0) return -1;

    p = str;
    out = keluaran;
    spasi_sebelumnya = 1; /* Awal dianggap spasi */

    /* Lewati spasi di awal */
    while (*p == ' ' || *p == '\t') p++;

    while (*p && (size_t)(out - keluaran) < ukuran - 1) {
        if (*p == ' ' || *p == '\t') {
            if (!spasi_sebelumnya) {
                *out++ = ' ';
                spasi_sebelumnya = 1;
            }
        } else {
            *out++ = *p;
            spasi_sebelumnya = 0;
        }
        p++;
    }

    /* Hapus spasi di akhir */
    if (out > keluaran && *(out - 1) == ' ') {
        out--;
    }
    *out = '\0';
    return 0;
}

/* ============================================================
 * FUNGSI GABUNG
 * ============================================================ */

/* CONCAT: Gabung dua string */
int teks_concat(const char *s1, const char *s2, char *keluaran,
                size_t ukuran) {
    size_t len1;
    size_t len2;

    if (!keluaran || ukuran == 0) return -1;
    if (!s1) s1 = "";
    if (!s2) s2 = "";

    len1 = strlen(s1);
    len2 = strlen(s2);

    if (len1 + len2 >= ukuran) {
        len2 = ukuran - len1 - 1;
    }

    memcpy(keluaran, s1, len1);
    memcpy(keluaran + len1, s2, len2);
    keluaran[len1 + len2] = '\0';
    return 0;
}

/* ============================================================
 * FUNGSI PENCARIAN
 * ============================================================ */

/* FIND/SEARCH: Cari posisi substring */
int teks_find(const char *cari, const char *dalam, double mulai,
              double *keluaran) {
    int start_idx;
    const char *p;
    int pos;

    if (!cari || !dalam || !keluaran) return -1;

    start_idx = (int)mulai - 1;
    if (start_idx < 0) start_idx = 0;

    /* Lewati sampai start */
    p = dalam;
    pos = 0;
    while (*p && pos < start_idx) {
        p += panjang_cp_utf8(p);
        pos++;
    }

    /* Cari substring */
    if (cari[0] == '\0') {
        *keluaran = (double)(pos + 1);
        return 0;
    }

    while (*p) {
        if (strncmp(p, cari, strlen(cari)) == 0) {
            *keluaran = (double)(pos + 1);
            return 0;
        }
        p += panjang_cp_utf8(p);
        pos++;
    }

    *keluaran = 0.0; /* Tidak ditemukan */
    return 0;
}

/* ============================================================
 * FUNGSI PENGGANTIAN
 * ============================================================ */

/* SUBSTITUTE: Ganti semua kemunculan substring */
int teks_substitute(const char *teks, const char *cari, const char *ganti,
                    char *keluaran, size_t ukuran) {
    const char *p;
    char *out;
    size_t cari_len;
    size_t ganti_len;

    if (!teks || !cari || !ganti || !keluaran || ukuran == 0) return -1;
    if (cari[0] == '\0') {
        strncpy(keluaran, teks, ukuran - 1);
        keluaran[ukuran - 1] = '\0';
        return 0;
    }

    p = teks;
    out = keluaran;
    cari_len = strlen(cari);
    ganti_len = strlen(ganti);

    while (*p && (size_t)(out - keluaran) < ukuran - 1) {
        if (strncmp(p, cari, cari_len) == 0) {
            if ((size_t)(out - keluaran) + ganti_len >= ukuran) break;
            memcpy(out, ganti, ganti_len);
            out += ganti_len;
            p += cari_len;
        } else {
            *out++ = *p++;
        }
    }
    *out = '\0';
    return 0;
}

/* REPLACE: Ganti bagian string dari posisi */
int teks_replace(const char *teks, double pos, double jumlah,
                 const char *baru, char *keluaran, size_t ukuran) {
    int start;
    int jml;
    const char *p;
    char *out;
    int i;
    size_t baru_len;

    if (!teks || !baru || !keluaran || ukuran == 0) return -1;

    start = (int)pos - 1;
    jml = (int)jumlah;
    p = teks;
    out = keluaran;
    i = 0;
    baru_len = strlen(baru);

    if (start < 0) start = 0;

    /* Salin bagian sebelum posisi */
    while (*p && i < start && (size_t)(out - keluaran) < ukuran - 1) {
        *out++ = *p++;
        i++;
    }

    /* Lewati bagian yang diganti */
    while (*p && jml > 0) {
        p += panjang_cp_utf8(p);
        jml--;
    }

    /* Sisipkan string baru */
    if ((size_t)(out - keluaran) + baru_len < ukuran) {
        memcpy(out, baru, baru_len);
        out += baru_len;
    }

    /* Salin sisa string */
    while (*p && (size_t)(out - keluaran) < ukuran - 1) {
        *out++ = *p++;
    }
    *out = '\0';
    return 0;
}

/* ============================================================
 * FUNGSI KONVERSI
 * ============================================================ */

/* VALUE: Konversi teks ke angka */
int teks_value(const char *teks, double *keluaran) {
    char *endp;
    double val;

    if (!teks || !keluaran) return -1;

    endp = NULL;
    val = strtod(teks, &endp);
    if (endp == teks || *endp != '\0') {
        return -1;
    }
    *keluaran = val;
    return 0;
}

/* TEXT: Format angka ke teks */
int teks_format(double angka, const char *format, char *keluaran,
                size_t ukuran) {
    (void)format; /* Format sederhana: abaikan pola kompleks */

    if (!keluaran || ukuran == 0) return -1;

    snprintf(keluaran, ukuran, "%.10g", angka);
    return 0;
}

/* ============================================================
 * FUNGSI PENGECEKAN
 * ============================================================ */

/* ISNUMBER: Apakah teks bisa jadi angka */
int teks_isnumber(const char *teks, double *keluaran) {
    double val;

    if (!teks || !keluaran) return -1;

    *keluaran = (teks_value(teks, &val) == 0) ? 1.0 : 0.0;
    return 0;
}

/* ISTEXT: Apakah teks valid */
int teks_istext(const char *teks, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = (teks && teks[0] != '\0') ? 1.0 : 0.0;
    return 0;
}

/* ISBLANK: Apakah teks kosong */
int teks_isblank(const char *teks, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = (!teks || teks[0] == '\0') ? 1.0 : 0.0;
    return 0;
}
