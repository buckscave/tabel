/* ============================================================
 * TABEL v3.0 
 * Berkas: teks.c - Formula Fungsi Teks (String Manipulation)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "../tabel.h"

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

/* ============================================================
 * FUNGSI TEKS LANJUTAN
 * ============================================================ */

/* REPT: Ulangi teks n kali
 * REPT(teks, jumlah)
 */
int teks_rept(const char *teks, double jumlah, char *keluaran, size_t ukuran) {
    int jml;
    size_t teks_len;
    size_t i;
    char *out;

    if (!keluaran || ukuran == 0) return -1;
    if (!teks) teks = "";

    jml = (int)jumlah;
    if (jml <= 0) {
        keluaran[0] = '\0';
        return 0;
    }

    teks_len = strlen(teks);
    if (teks_len == 0) {
        keluaran[0] = '\0';
        return 0;
    }

    /* Cek apakah hasil muat */
    if ((size_t)jml * teks_len >= ukuran) {
        jml = (int)((ukuran - 1) / teks_len);
    }

    out = keluaran;
    for (i = 0; i < (size_t)jml; i++) {
        memcpy(out, teks, teks_len);
        out += teks_len;
    }
    *out = '\0';

    return 0;
}

/* CHAR: Kode ASCII ke karakter
 * CHAR(kode)
 */
int teks_char(double kode, char *keluaran, size_t ukuran) {
    int code;

    if (!keluaran || ukuran < 2) return -1;

    code = (int)kode;
    if (code < 1 || code > 255) return -1;

    /* Karakter ASCII printable */
    keluaran[0] = (char)code;
    keluaran[1] = '\0';

    return 0;
}

/* CODE: Karakter ke kode ASCII
 * CODE(teks)
 */
int teks_code(const char *teks, double *keluaran) {
    if (!keluaran) return -1;

    if (!teks || teks[0] == '\0') {
        *keluaran = 0.0;
        return 0;
    }

    *keluaran = (double)(unsigned char)teks[0];
    return 0;
}

/* CLEAN: Hapus karakter non-printable
 * CLEAN(teks)
 */
int teks_clean(const char *teks, char *keluaran, size_t ukuran) {
    const char *p;
    char *out;
    unsigned char c;

    if (!keluaran || ukuran == 0) return -1;
    if (!teks) {
        keluaran[0] = '\0';
        return 0;
    }

    p = teks;
    out = keluaran;

    while (*p && (size_t)(out - keluaran) < ukuran - 1) {
        c = (unsigned char)*p;

        /* Hapus karakter kontrol ASCII (0-31) kecuali baris baru (10) dan tab (9) */
        if (c >= 32 || c == 10 || c == 9) {
            *out++ = *p;
        }
        /* Hapus juga DEL (127) */
        if (c == 127) {
            /* skip */
        }
        p++;
    }
    *out = '\0';

    return 0;
}

/* EXACT: Perbandingan teks eksak (case-sensitive)
 * EXACT(teks1, teks2)
 */
int teks_exact(const char *teks1, const char *teks2, double *keluaran) {
    if (!keluaran) return -1;

    if (!teks1) teks1 = "";
    if (!teks2) teks2 = "";

    /* strcmp peka huruf besar/kecil */
    *keluaran = (strcmp(teks1, teks2) == 0) ? 1.0 : 0.0;
    return 0;
}

/* FIXED: Format angka dengan desimal tetap
 * FIXED(angka, desimal, [tanpa_koma])
 */
int teks_fixed(double angka, double desimal, int tanpa_koma,
               char *keluaran, size_t ukuran) {
    int dec;
    char format[16];
    char temp[128];
    char *p;
    size_t i;

    if (!keluaran || ukuran == 0) return -1;

    dec = (int)desimal;
    if (dec < 0) dec = 0;
    if (dec > 15) dec = 15;

    /* Buat format string */
    snprintf(format, sizeof(format), "%%.%df", dec);

    /* Format angka */
    snprintf(temp, sizeof(temp), format, angka);

    /* Hapus pemisah ribuan (koma) jika diminta */
    if (tanpa_koma) {
        strncpy(keluaran, temp, ukuran - 1);
        keluaran[ukuran - 1] = '\0';
    } else {
        /* Tambahkan pemisah ribuan (titik untuk Indonesia) */
        size_t len = strlen(temp);
        size_t new_len = len;
        size_t comma_pos = 0;
        size_t digits_before_decimal = 0;

        /* Cari posisi desimal */
        for (i = 0; i < len; i++) {
            if (temp[i] == '.') {
                comma_pos = i;
                break;
            }
        }

        if (comma_pos == 0) comma_pos = len;
        digits_before_decimal = comma_pos;

        /* Hitung panjang baru dengan pemisah */
        if (digits_before_decimal > 3) {
            new_len += (digits_before_decimal - 1) / 3;
        }

        if (new_len >= ukuran) {
            strncpy(keluaran, temp, ukuran - 1);
            keluaran[ukuran - 1] = '\0';
            return 0;
        }

        /* Salin dengan pemisah ribuan */
        p = keluaran;
        size_t count = 0;
        size_t first_group = digits_before_decimal % 3;
        if (first_group == 0) first_group = 3;

        /* Bagian sebelum desimal */
        for (i = 0; i < digits_before_decimal; i++) {
            *p++ = temp[i];
            count++;

            /* Tambah pemisah setelah grup pertama */
            if (i == first_group - 1 && i < digits_before_decimal - 1) {
                *p++ = ',';
            }
            /* Tambah pemisah setiap 3 digit setelah grup pertama */
            else if (count > first_group && (count - first_group) % 3 == 0 &&
                     i < digits_before_decimal - 1) {
                *p++ = ',';
            }
        }

        /* Salin bagian desimal */
        for (i = digits_before_decimal; i < len; i++) {
            *p++ = temp[i];
        }
        *p = '\0';
    }

    return 0;
}

/* DOLLAR: Format mata uang dolar
 * DOLLAR(angka, [desimal])
 */
int teks_dollar(double angka, double desimal, char *keluaran, size_t ukuran) {
    int dec;
    char temp[128];
    int is_negative;

    if (!keluaran || ukuran < 2) return -1;

    dec = (int)desimal;
    if (dec < 0) dec = 2; /* Default 2 desimal */
    if (dec > 15) dec = 15;

    is_negative = (angka < 0.0);
    if (is_negative) angka = -angka;

    /* Format angka */
    snprintf(temp, sizeof(temp), "$%.*f", dec, angka);

    /* Tambahkan tanda kurung untuk nilai negatif */
    if (is_negative) {
        if (strlen(temp) + 3 >= ukuran) {
            strncpy(keluaran, temp, ukuran - 1);
            keluaran[ukuran - 1] = '\0';
        } else {
            snprintf(keluaran, ukuran, "(%s)", temp);
        }
    } else {
        strncpy(keluaran, temp, ukuran - 1);
        keluaran[ukuran - 1] = '\0';
    }

    return 0;
}

/* PROPER: Ubah ke huruf kapital per kata (Title Case)
 * PROPER(teks)
 */
int teks_proper(const char *str, char *keluaran, size_t ukuran) {
    size_t i = 0;
    int kapital_berikutnya = 1;

    if (!str || !keluaran || ukuran == 0) return -1;

    while (str[i] && i < ukuran - 1) {
        unsigned char c = (unsigned char)str[i];

        if (c < 0x80) {
            /* Karakter ASCII */
            if (kapital_berikutnya && c >= 'a' && c <= 'z') {
                keluaran[i] = (char)(c - 'a' + 'A');
            } else if (!kapital_berikutnya && c >= 'A' && c <= 'Z') {
                keluaran[i] = (char)(c - 'A' + 'a');
            } else {
                keluaran[i] = str[i];
            }

            /* Spasi atau non-alphabet memicu kapital berikutnya */
            kapital_berikutnya = (c == ' ' || c == '\t' || c == '\n' ||
                                  !(c >= 'a' && c <= 'z') && !(c >= 'A' && c <= 'Z'));
            i++;
        } else {
            /* Karakter UTF-8, salin apa adanya */
            int l = panjang_cp_utf8(str + i);
            if (i + (size_t)l >= ukuran) break;
            memcpy(keluaran + i, str + i, (size_t)l);
            i += (size_t)l;
            kapital_berikutnya = 0;
        }
    }
    keluaran[i] = '\0';
    return 0;
}

/* REVERSE: Balik urutan teks
 * REVERSE(teks)
 */
int teks_reverse(const char *str, char *keluaran, size_t ukuran) {
    int len = 0;
    int i;
    int out_idx = 0;
    const char *p;

    if (!str || !keluaran || ukuran == 0) return -1;

    /* Hitung panjang dalam grapheme */
    p = str;
    while (*p) {
        p += panjang_cp_utf8(p);
        len++;
    }

    /* Balik urutan */
    for (i = len - 1; i >= 0 && (size_t)out_idx < ukuran - 1; i--) {
        /* Cari posisi karakter ke-i */
        int char_idx = 0;
        p = str;
        while (*p && char_idx < i) {
            p += panjang_cp_utf8(p);
            char_idx++;
        }

        if (*p) {
            int l = panjang_cp_utf8(p);
            if ((size_t)(out_idx + l) >= ukuran) break;
            memcpy(keluaran + out_idx, p, (size_t)l);
            out_idx += l;
        }
    }
    keluaran[out_idx] = '\0';

    return 0;
}

/* REPEAT: Alias untuk REPT
 * REPEAT(teks, jumlah)
 */
int teks_repeat(const char *teks, double jumlah, char *keluaran, size_t ukuran) {
    return teks_rept(teks, jumlah, keluaran, ukuran);
}

/* LENB: Panjang teks dalam byte
 * LENB(teks)
 */
int teks_lenb(const char *str, double *keluaran) {
    if (!str || !keluaran) return -1;

    *keluaran = (double)strlen(str);
    return 0;
}

/* T: Kembalikan teks jika teks, kosongkan jika angka
 * T(nilai)
 */
int teks_t(const char *teks, char *keluaran, size_t ukuran) {
    char *endp;
    double val;

    if (!keluaran || ukuran == 0) return -1;

    if (!teks || teks[0] == '\0') {
        keluaran[0] = '\0';
        return 0;
    }

    /* Cek apakah angka murni */
    val = strtod(teks, &endp);
    (void)val;
    if (endp && endp != teks && *endp == '\0') {
        /* Angka murni, kembalikan kosong */
        keluaran[0] = '\0';
    } else {
        /* Bukan angka, kembalikan teks */
        strncpy(keluaran, teks, ukuran - 1);
        keluaran[ukuran - 1] = '\0';
    }

    return 0;
}

/* N: Kembalikan angka jika angka, 0 jika teks
 * N(nilai)
 */
int teks_n(const char *teks, double *keluaran) {
    char *endp;
    double val;

    if (!keluaran) return -1;

    if (!teks || teks[0] == '\0') {
        *keluaran = 0.0;
        return 0;
    }

    val = strtod(teks, &endp);
    if (endp && endp != teks && *endp == '\0') {
        *keluaran = val;
    } else {
        *keluaran = 0.0;
    }

    return 0;
}

/* ASC: Konversi karakter double-byte ke single-byte (placeholder)
 * ASC(teks) - untuk kompatibilitas dengan Excel
 */
int teks_asc(const char *teks, char *keluaran, size_t ukuran) {
    if (!keluaran || ukuran == 0) return -1;
    if (!teks) {
        keluaran[0] = '\0';
        return 0;
    }

    /* Sederhana: salin apa adanya */
    strncpy(keluaran, teks, ukuran - 1);
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* JIS/WIDECHAR: Konversi ke double-byte (placeholder)
 * JIS(teks) - untuk kompatibilitas dengan Excel
 */
int teks_jis(const char *teks, char *keluaran, size_t ukuran) {
    if (!keluaran || ukuran == 0) return -1;
    if (!teks) {
        keluaran[0] = '\0';
        return 0;
    }

    /* Sederhana: salin apa adanya */
    strncpy(keluaran, teks, ukuran - 1);
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* BAHTTEXT: Format mata uang Baht Thailand (placeholder)
 * BAHTTEXT(angka)
 */
int teks_bahttext(double angka, char *keluaran, size_t ukuran) {
    if (!keluaran || ukuran == 0) return -1;

    /* Placeholder: format sederhana */
    snprintf(keluaran, ukuran, "%.2f Baht", angka);
    return 0;
}

/* PHONETIC: Ambil fonetik (placeholder)
 * PHONETIC(referensi)
 */
int teks_phonetic(const char *teks, char *keluaran, size_t ukuran) {
    if (!keluaran || ukuran == 0) return -1;
    if (!teks) {
        keluaran[0] = '\0';
        return 0;
    }

    /* Placeholder: salin apa adanya */
    strncpy(keluaran, teks, ukuran - 1);
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* UNICHAR: Kode Unicode ke karakter
 * UNICHAR(kode)
 */
int teks_unichar(double kode, char *keluaran, size_t ukuran) {
    int code;
    int len;

    if (!keluaran || ukuran < 5) return -1;

    code = (int)kode;
    if (code < 1 || code > 0x10FFFF) return -1;

    /* Encode UTF-8 */
    if (code < 0x80) {
        keluaran[0] = (char)code;
        keluaran[1] = '\0';
    } else if (code < 0x800) {
        keluaran[0] = (char)(0xC0 | (code >> 6));
        keluaran[1] = (char)(0x80 | (code & 0x3F));
        keluaran[2] = '\0';
    } else if (code < 0x10000) {
        keluaran[0] = (char)(0xE0 | (code >> 12));
        keluaran[1] = (char)(0x80 | ((code >> 6) & 0x3F));
        keluaran[2] = (char)(0x80 | (code & 0x3F));
        keluaran[3] = '\0';
    } else {
        keluaran[0] = (char)(0xF0 | (code >> 18));
        keluaran[1] = (char)(0x80 | ((code >> 12) & 0x3F));
        keluaran[2] = (char)(0x80 | ((code >> 6) & 0x3F));
        keluaran[3] = (char)(0x80 | (code & 0x3F));
        keluaran[4] = '\0';
    }

    return 0;
}

/* UNICODE: Karakter ke kode Unicode
 * UNICODE(teks)
 */
int teks_unicode(const char *teks, double *keluaran) {
    unsigned char c;
    int code = 0;

    if (!keluaran) return -1;

    if (!teks || teks[0] == '\0') {
        *keluaran = 0.0;
        return 0;
    }

    c = (unsigned char)teks[0];

    /* Decode UTF-8 */
    if (c < 0x80) {
        code = c;
    } else if ((c & 0xE0) == 0xC0) {
        code = (c & 0x1F) << 6;
        if (teks[1]) code |= (unsigned char)teks[1] & 0x3F;
    } else if ((c & 0xF0) == 0xE0) {
        code = (c & 0x0F) << 12;
        if (teks[1]) code |= ((unsigned char)teks[1] & 0x3F) << 6;
        if (teks[2]) code |= (unsigned char)teks[2] & 0x3F;
    } else if ((c & 0xF8) == 0xF0) {
        code = (c & 0x07) << 18;
        if (teks[1]) code |= ((unsigned char)teks[1] & 0x3F) << 12;
        if (teks[2]) code |= ((unsigned char)teks[2] & 0x3F) << 6;
        if (teks[3]) code |= (unsigned char)teks[3] & 0x3F;
    }

    *keluaran = (double)code;
    return 0;
}

/* CONCATENATE: Gabung banyak teks
 * CONCATENATE(teks1, teks2, ...)
 * Implementasi sederhana untuk 2 argumen
 */
int teks_concatenate(const char **teks_array, int jumlah,
                     char *keluaran, size_t ukuran) {
    int i;
    char *out;
    size_t sisa;

    if (!keluaran || ukuran == 0) return -1;

    out = keluaran;
    sisa = ukuran - 1;

    for (i = 0; i < jumlah && sisa > 0; i++) {
        const char *teks = teks_array[i];
        if (!teks) teks = "";

        size_t len = strlen(teks);
        if (len > sisa) len = sisa;

        memcpy(out, teks, len);
        out += len;
        sisa -= len;
    }

    *out = '\0';
    return 0;
}

/* JOIN: Gabung teks dengan pemisah
 * JOIN(pemisah, teks1, teks2, ...)
 */
int teks_join(const char *pemisah, const char **teks_array, int jumlah,
              char *keluaran, size_t ukuran) {
    int i;
    char *out;
    size_t sisa;
    size_t pemisah_len;

    if (!keluaran || ukuran == 0) return -1;
    if (!pemisah) pemisah = "";

    pemisah_len = strlen(pemisah);
    out = keluaran;
    sisa = ukuran - 1;

    for (i = 0; i < jumlah && sisa > 0; i++) {
        const char *teks = teks_array[i];
        if (!teks) teks = "";

        /* Tambah pemisah setelah teks pertama */
        if (i > 0 && pemisah_len > 0 && sisa > 0) {
            size_t copy_len = (pemisah_len > sisa) ? sisa : pemisah_len;
            memcpy(out, pemisah, copy_len);
            out += copy_len;
            sisa -= copy_len;
        }

        if (sisa > 0) {
            size_t len = strlen(teks);
            if (len > sisa) len = sisa;

            memcpy(out, teks, len);
            out += len;
            sisa -= len;
        }
    }

    *out = '\0';
    return 0;
}

/* TEXTJOIN: Gabung teks dengan pemisah dan opsi kosong
 * TEXTJOIN(pemisah, abaikan_kosong, teks1, teks2, ...)
 */
int teks_textjoin(const char *pemisah, int abaikan_kosong,
                  const char **teks_array, int jumlah,
                  char *keluaran, size_t ukuran) {
    int i;
    char *out;
    size_t sisa;
    size_t pemisah_len;
    int pertama = 1;

    if (!keluaran || ukuran == 0) return -1;
    if (!pemisah) pemisah = "";

    pemisah_len = strlen(pemisah);
    out = keluaran;
    sisa = ukuran - 1;

    for (i = 0; i < jumlah && sisa > 0; i++) {
        const char *teks = teks_array[i];
        if (!teks) teks = "";

        /* Skip kosong jika diminta */
        if (abaikan_kosong && teks[0] == '\0') {
            continue;
        }

        /* Tambah pemisah */
        if (!pertama && pemisah_len > 0 && sisa > 0) {
            size_t copy_len = (pemisah_len > sisa) ? sisa : pemisah_len;
            memcpy(out, pemisah, copy_len);
            out += copy_len;
            sisa -= copy_len;
        }
        pertama = 0;

        if (sisa > 0) {
            size_t len = strlen(teks);
            if (len > sisa) len = sisa;

            memcpy(out, teks, len);
            out += len;
            sisa -= len;
        }
    }

    *out = '\0';
    return 0;
}

/* SPLIT: Pecah teks berdasarkan pemisah
 * SPLIT(teks, pemisah, indeks)
 * Mengembalikan bagian ke-indeks (1-based)
 */
int teks_split(const char *teks, const char *pemisah, int indeks,
               char *keluaran, size_t ukuran) {
    const char *p;
    const char *start;
    size_t pemisah_len;
    int current_idx = 1;
    size_t len;

    if (!keluaran || ukuran == 0) return -1;
    if (!teks || !pemisah || indeks < 1) {
        keluaran[0] = '\0';
        return -1;
    }

    pemisah_len = strlen(pemisah);
    if (pemisah_len == 0) {
        /* Tidak ada pemisah, kembalikan seluruh teks */
        strncpy(keluaran, teks, ukuran - 1);
        keluaran[ukuran - 1] = '\0';
        return 0;
    }

    start = teks;
    p = strstr(start, pemisah);

    while (p && current_idx < indeks) {
        start = p + pemisah_len;
        p = strstr(start, pemisah);
        current_idx++;
    }

    if (current_idx == indeks) {
        if (p) {
            len = (size_t)(p - start);
        } else {
            len = strlen(start);
        }

        if (len >= ukuran) len = ukuran - 1;
        memcpy(keluaran, start, len);
        keluaran[len] = '\0';
        return 0;
    }

    /* Indeks tidak ditemukan */
    keluaran[0] = '\0';
    return -1;
}
