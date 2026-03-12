/* ============================================================
 * TABEL v3.0
 * Berkas: logika.c - Formula Fungsi Logika
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * Fungsi logika yang didukung:
 * - IF      : Kondisi dasar
 * - IFS     : Kondisi berganda
 * - IFERROR : Tangani error
 * - IFNA    : Tangani #N/A
 * - IFBLANK : Tangani sel kosong
 * - AND     : Logika DAN
 * - OR      : Logika ATAU
 * - NOT     : Logika TIDAK
 * - XOR     : Exclusive OR
 * - SWITCH  : Pilih berdasarkan nilai
 * - CHOOSE  : Pilih berdasarkan indeks
 * - TRUE    : Nilai benar
 * - FALSE   : Nilai salah
 * - ISNUMBER: Cek apakah angka
 * - ISTEXT  : Cek apakah teks
 * - ISBLANK : Cek apakah kosong
 * - ISERROR : Cek apakah error
 * - ISNA    : Cek apakah #N/A
 * - ISLOGICAL: Cek apakah boolean
 * - ISREF   : Cek apakah referensi valid
 * - ISEVEN  : Cek apakah genap
 * - ISODD   : Cek apakah ganjil
 * - LET     : Variabel lokal (sederhana)
 * ============================================================ */

#include "../tabel.h"

/* ============================================================
 * KONSTANTA
 * ============================================================ */
#define LOGIKA_MAKS_ARGS    32
#define LOGIKA_TOLERANSI    1e-10

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
static double logika_abs(double x) {
    return (x < 0.0) ? -x : x;
}

/* Konversi ke boolean (0 = false, non-zero = true) */
static int ke_boolean(double nilai) {
    return (logika_abs(nilai) > LOGIKA_TOLERANSI);
}

/* ============================================================
 * FUNGSI IF DAN VARIAN
 * ============================================================ */

/* IF: Kondisi dasar
 * IF(kondisi, nilai_jika_benar, nilai_jika_salah)
 */
int logika_if(double kondisi, double benar, double salah,
              double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = ke_boolean(kondisi) ? benar : salah;
    return 0;
}

/* IF dengan teks
 * IF(kondisi, teks_benar, teks_salah, keluaran_teks)
 */
int logika_if_teks(double kondisi, const char *benar,
                   const char *salah, char *keluaran, size_t ukuran) {
    if (!keluaran || ukuran == 0) return -1;

    if (ke_boolean(kondisi)) {
        strncpy(keluaran, benar ? benar : "", ukuran - 1);
    } else {
        strncpy(keluaran, salah ? salah : "", ukuran - 1);
    }
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* IFS: Kondisi berganda
 * IFS(kondisi1, nilai1, kondisi2, nilai2, ...)
 * Mengembalikan nilai dari kondisi pertama yang TRUE
 */
int logika_ifs(const double *kondisi, const double *nilai,
               int jumlah, double *keluaran) {
    int i;

    if (!kondisi || !nilai || !keluaran || jumlah <= 0) return -1;

    for (i = 0; i < jumlah; i++) {
        if (ke_boolean(kondisi[i])) {
            *keluaran = nilai[i];
            return 0;
        }
    }

    /* Tidak ada kondisi yang terpenuhi, kembalikan 0 */
    *keluaran = 0.0;
    return 0;
}

/* IFERROR: Tangani error
 * IFERROR(nilai, nilai_jika_error)
 */
int logika_iferror(int ada_error, double nilai, double nilai_error,
                   double *keluaran) {
    if (!keluaran) return -1;

    if (ada_error) {
        *keluaran = nilai_error;
    } else {
        *keluaran = nilai;
    }
    return 0;
}

/* IFNA: Tangani #N/A
 * IFNA(nilai, nilai_jika_na)
 */
int logika_ifna(int ada_na, double nilai, double nilai_na,
                double *keluaran) {
    if (!keluaran) return -1;

    if (ada_na) {
        *keluaran = nilai_na;
    } else {
        *keluaran = nilai;
    }
    return 0;
}

/* IFBLANK: Tangani sel kosong
 * IFBLANK(nilai, nilai_jika_kosong)
 */
int logika_ifblank(int kosong, double nilai, double nilai_kosong,
                   double *keluaran) {
    if (!keluaran) return -1;

    if (kosong) {
        *keluaran = nilai_kosong;
    } else {
        *keluaran = nilai;
    }
    return 0;
}

/* ============================================================
 * FUNGSI LOGIKA DASAR
 * ============================================================ */

/* AND: Logika DAN (semua harus TRUE) */
int logika_and(const double *nilai, int jumlah, double *keluaran) {
    int i;
    int hasil = 1;

    if (!nilai || !keluaran || jumlah <= 0) return -1;

    for (i = 0; i < jumlah; i++) {
        if (!ke_boolean(nilai[i])) {
            hasil = 0;
            break;
        }
    }

    *keluaran = hasil ? 1.0 : 0.0;
    return 0;
}

/* OR: Logika ATAU (salah satu TRUE) */
int logika_or(const double *nilai, int jumlah, double *keluaran) {
    int i;
    int hasil = 0;

    if (!nilai || !keluaran || jumlah <= 0) return -1;

    for (i = 0; i < jumlah; i++) {
        if (ke_boolean(nilai[i])) {
            hasil = 1;
            break;
        }
    }

    *keluaran = hasil ? 1.0 : 0.0;
    return 0;
}

/* NOT: Logika TIDAK (invert) */
int logika_not(double nilai, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = ke_boolean(nilai) ? 0.0 : 1.0;
    return 0;
}

/* XOR: Exclusive OR (ganjil TRUE = TRUE) */
int logika_xor(const double *nilai, int jumlah, double *keluaran) {
    int i;
    int count_true = 0;

    if (!nilai || !keluaran || jumlah <= 0) return -1;

    for (i = 0; i < jumlah; i++) {
        if (ke_boolean(nilai[i])) {
            count_true++;
        }
    }

    /* XOR: TRUE jika jumlah TRUE ganjil */
    *keluaran = (count_true % 2 == 1) ? 1.0 : 0.0;
    return 0;
}

/* NAND: NOT AND */
int logika_nand(const double *nilai, int jumlah, double *keluaran) {
    int i;
    int hasil = 0;

    if (!nilai || !keluaran || jumlah <= 0) return -1;

    for (i = 0; i < jumlah; i++) {
        if (!ke_boolean(nilai[i])) {
            hasil = 1;
            break;
        }
    }

    *keluaran = hasil ? 1.0 : 0.0;
    return 0;
}

/* NOR: NOT OR */
int logika_nor(const double *nilai, int jumlah, double *keluaran) {
    int i;
    int hasil = 1;

    if (!nilai || !keluaran || jumlah <= 0) return -1;

    for (i = 0; i < jumlah; i++) {
        if (ke_boolean(nilai[i])) {
            hasil = 0;
            break;
        }
    }

    *keluaran = hasil ? 1.0 : 0.0;
    return 0;
}

/* IMPLIES: Implication (A -> B = NOT A OR B) */
int logika_implies(double a, double b, double *keluaran) {
    if (!keluaran) return -1;

    /* A -> B = NOT A OR B */
    *keluaran = (!ke_boolean(a) || ke_boolean(b)) ? 1.0 : 0.0;
    return 0;
}

/* EQUIV: Equivalence (A <-> B = (A AND B) OR (NOT A AND NOT B)) */
int logika_equiv(double a, double b, double *keluaran) {
    if (!keluaran) return -1;

    int ba = ke_boolean(a);
    int bb = ke_boolean(b);

    *keluaran = (ba == bb) ? 1.0 : 0.0;
    return 0;
}

/* ============================================================
 * FUNGSI SWITCH DAN CHOOSE
 * ============================================================ */

/* SWITCH: Pilih berdasarkan nilai
 * SWITCH(ekspresi, nilai1, hasil1, nilai2, hasil2, ..., [default])
 */
int logika_switch(double ekspresi, const double *nilai,
                  const double *hasil, int jumlah,
                  double def, double *keluaran) {
    int i;

    if (!nilai || !hasil || !keluaran || jumlah <= 0) {
        if (keluaran) *keluaran = def;
        return 0;
    }

    for (i = 0; i < jumlah; i++) {
        if (logika_abs(ekspresi - nilai[i]) < LOGIKA_TOLERANSI) {
            *keluaran = hasil[i];
            return 0;
        }
    }

    /* Tidak ditemukan, gunakan default */
    *keluaran = def;
    return 0;
}

/* SWITCH dengan teks */
int logika_switch_teks(const char *ekspresi, const char **nilai,
                       const double *hasil, int jumlah,
                       double def, double *keluaran) {
    int i;

    if (!ekspresi || !nilai || !hasil || !keluaran || jumlah <= 0) {
        if (keluaran) *keluaran = def;
        return 0;
    }

    for (i = 0; i < jumlah; i++) {
        if (nilai[i] && strcmp(ekspresi, nilai[i]) == 0) {
            *keluaran = hasil[i];
            return 0;
        }
    }

    *keluaran = def;
    return 0;
}

/* CHOOSE: Pilih berdasarkan indeks (1-based)
 * CHOOSE(indeks, nilai1, nilai2, nilai3, ...)
 */
int logika_choose(int indeks, const double *nilai, int jumlah,
                  double *keluaran) {
    if (!nilai || !keluaran || jumlah <= 0) return -1;

    /* Indeks 1-based */
    if (indeks < 1 || indeks > jumlah) return -1;

    *keluaran = nilai[indeks - 1];
    return 0;
}

/* CHOOSE dengan teks */
int logika_choose_teks(int indeks, const char **nilai, int jumlah,
                       char *keluaran, size_t ukuran) {
    if (!nilai || !keluaran || ukuran == 0 || jumlah <= 0) return -1;

    /* Indeks 1-based */
    if (indeks < 1 || indeks > jumlah) return -1;

    if (nilai[indeks - 1]) {
        strncpy(keluaran, nilai[indeks - 1], ukuran - 1);
        keluaran[ukuran - 1] = '\0';
    } else {
        keluaran[0] = '\0';
    }

    return 0;
}

/* ============================================================
 * FUNGSI TRUE DAN FALSE
 * ============================================================ */

/* TRUE: Kembalikan nilai TRUE (1.0) */
int logika_true(double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = 1.0;
    return 0;
}

/* FALSE: Kembalikan nilai FALSE (0.0) */
int logika_false(double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = 0.0;
    return 0;
}

/* ============================================================
 * FUNGSI PENGECEKAN TIPE
 * ============================================================ */

/* ISNUMBER: Cek apakah nilai adalah angka */
int logika_isnumber(const char *teks, double *keluaran) {
    char *endp;
    double val;

    if (!keluaran) return -1;

    if (!teks || teks[0] == '\0') {
        *keluaran = 0.0;
        return 0;
    }

    /* Coba parse sebagai angka */
    endp = NULL;
    val = strtod(teks, &endp);

    (void)val; /* Suppress unused warning */

    *keluaran = (endp && endp != teks && *endp == '\0') ? 1.0 : 0.0;
    return 0;
}

/* ISTEXT: Cek apakah nilai adalah teks */
int logika_istext(const char *teks, double *keluaran) {
    if (!keluaran) return -1;

    /* Teks valid jika tidak kosong dan bukan angka murni */
    if (!teks || teks[0] == '\0') {
        *keluaran = 0.0;
        return 0;
    }

    /* Jika dimulai dengan tanda kutip, itu teks */
    if (teks[0] == '"') {
        *keluaran = 1.0;
        return 0;
    }

    /* Cek apakah bukan angka murni */
    {
        char *endp;
        strtod(teks, &endp);
        *keluaran = (endp && *endp != '\0') ? 1.0 : 0.0;
    }

    return 0;
}

/* ISBLANK: Cek apakah sel kosong */
int logika_isblank(const char *teks, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = (!teks || teks[0] == '\0') ? 1.0 : 0.0;
    return 0;
}

/* ISERROR: Cek apakah ada error */
int logika_iserror(int error_code, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = (error_code != 0) ? 1.0 : 0.0;
    return 0;
}

/* ISNA: Cek apakah #N/A */
int logika_isna(int na_code, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = (na_code == -2) ? 1.0 : 0.0; /* -2 adalah kode #N/A */
    return 0;
}

/* ISLOGICAL: Cek apakah nilai boolean */
int logika_islogical(double nilai, double *keluaran) {
    if (!keluaran) return -1;

    /* TRUE atau FALSE (1.0 atau 0.0) */
    *keluaran = (logika_abs(nilai - 1.0) < LOGIKA_TOLERANSI ||
                 logika_abs(nilai) < LOGIKA_TOLERANSI) ? 1.0 : 0.0;
    return 0;
}

/* ISREF: Cek apakah referensi valid */
int logika_isref(const char *ref, double *keluaran) {
    int x, y;

    if (!keluaran) return -1;

    *keluaran = (ref && parse_ref_sel(ref, &x, &y) > 0) ? 1.0 : 0.0;
    return 0;
}

/* ISEVEN: Cek apakah bilangan genap */
int logika_iseven(double nilai, double *keluaran) {
    int int_val;

    if (!keluaran) return -1;

    int_val = (int)nilai;
    *keluaran = (int_val % 2 == 0) ? 1.0 : 0.0;
    return 0;
}

/* ISODD: Cek apakah bilangan ganjil */
int logika_isodd(double nilai, double *keluaran) {
    int int_val;

    if (!keluaran) return -1;

    int_val = (int)nilai;
    *keluaran = (int_val % 2 != 0) ? 1.0 : 0.0;
    return 0;
}

/* ISNONTEXT: Cek apakah bukan teks */
int logika_isnontext(const char *teks, double *keluaran) {
    double is_text;

    if (!keluaran) return -1;

    logika_istext(teks, &is_text);
    *keluaran = (is_text < 0.5) ? 1.0 : 0.0;
    return 0;
}

/* ISFORMULA: Cek apakah sel berisi formula */
int logika_isformula(const char *teks, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = (teks && teks[0] == '=') ? 1.0 : 0.0;
    return 0;
}

/* ============================================================
 * FUNGSI PERBANDINGAN
 * ============================================================ */

/* EQUAL: Perbandingan sama dengan */
int logika_equal(double a, double b, double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (logika_abs(a - b) < LOGIKA_TOLERANSI) ? 1.0 : 0.0;
    return 0;
}

/* NOTEQUAL: Perbandingan tidak sama dengan */
int logika_notequal(double a, double b, double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (logika_abs(a - b) >= LOGIKA_TOLERANSI) ? 1.0 : 0.0;
    return 0;
}

/* GREATER: Perbandingan lebih besar */
int logika_greater(double a, double b, double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (a > b + LOGIKA_TOLERANSI) ? 1.0 : 0.0;
    return 0;
}

/* LESS: Perbandingan lebih kecil */
int logika_less(double a, double b, double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (a < b - LOGIKA_TOLERANSI) ? 1.0 : 0.0;
    return 0;
}

/* GREATEREQ: Perbandingan lebih besar atau sama */
int logika_greatereq(double a, double b, double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (a >= b - LOGIKA_TOLERANSI) ? 1.0 : 0.0;
    return 0;
}

/* LESSEQ: Perbandingan lebih kecil atau sama */
int logika_lesseq(double a, double b, double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (a <= b + LOGIKA_TOLERANSI) ? 1.0 : 0.0;
    return 0;
}

/* BETWEEN: Cek apakah nilai dalam rentang */
int logika_between(double nilai, double batas_bawah,
                   double batas_atas, double *keluaran) {
    if (!keluaran) return -1;

    *keluaran = (nilai >= batas_bawah && nilai <= batas_atas) ? 1.0 : 0.0;
    return 0;
}

/* ============================================================
 * FUNGSI KONDISI LANJUTAN
 * ============================================================ */

/* MAXIFS: Maksimum dengan kondisi ganda */
int logika_maxifs(const struct buffer_tabel *buf, const char *range_nilai,
                  const char *range_kondisi, const char *kondisi,
                  double *keluaran) {
    /* Implementasi sederhana - akan dihubungkan ke statistik */
    (void)buf;
    (void)range_nilai;
    (void)range_kondisi;
    (void)kondisi;
    if (!keluaran) return -1;
    *keluaran = 0.0;
    return 0;
}

/* MINIFS: Minimum dengan kondisi ganda */
int logika_minifs(const struct buffer_tabel *buf, const char *range_nilai,
                  const char *range_kondisi, const char *kondisi,
                  double *keluaran) {
    /* Implementasi sederhana - akan dihubungkan ke statistik */
    (void)buf;
    (void)range_nilai;
    (void)range_kondisi;
    (void)kondisi;
    if (!keluaran) return -1;
    *keluaran = 0.0;
    return 0;
}

/* COUNTIFS: Hitung dengan kondisi ganda */
int logika_countifs(const struct buffer_tabel *buf,
                    const char **ranges, const char **kondisi,
                    int jumlah, double *keluaran) {
    /* Implementasi sederhana - akan dihubungkan ke statistik */
    (void)buf;
    (void)ranges;
    (void)kondisi;
    (void)jumlah;
    if (!keluaran) return -1;
    *keluaran = 0.0;
    return 0;
}

/* SUMIFS: Jumlahkan dengan kondisi ganda */
int logika_sumifs(const struct buffer_tabel *buf, const char *range_nilai,
                  const char **ranges, const char **kondisi,
                  int jumlah, double *keluaran) {
    /* Implementasi sederhana - akan dihubungkan ke statistik */
    (void)buf;
    (void)range_nilai;
    (void)ranges;
    (void)kondisi;
    (void)jumlah;
    if (!keluaran) return -1;
    *keluaran = 0.0;
    return 0;
}

/* AVERAGEIFS: Rata-rata dengan kondisi ganda */
int logika_averageifs(const struct buffer_tabel *buf,
                      const char *range_nilai, const char **ranges,
                      const char **kondisi, int jumlah,
                      double *keluaran) {
    /* Implementasi sederhana - akan dihubungkan ke statistik */
    (void)buf;
    (void)range_nilai;
    (void)ranges;
    (void)kondisi;
    (void)jumlah;
    if (!keluaran) return -1;
    *keluaran = 0.0;
    return 0;
}

/* ============================================================
 * FUNGSI NULL DAN ERROR
 * ============================================================ */

/* NA: Kembalikan #N/A */
int logika_na(double *keluaran) {
    if (!keluaran) return -1;
    /* Gunakan nilai khusus untuk #N/A */
    *keluaran = 0.0;
    return -2; /* Kode error #N/A */
}

/* ERROR.TYPE: Jenis error */
int logika_error_type(int error_code, double *keluaran) {
    if (!keluaran) return -1;

    /* Kode error standar:
     * 1 = #DIV/0!
     * 2 = #VALUE!
     * 3 = #REF!
     * 4 = #NAME?
     * 5 = #NUM!
     * 6 = #N/A
     * 7 = #NULL!
     */
    *keluaran = (double)error_code;
    return 0;
}

/* ============================================================
 * DISPATCHER FUNGSI LOGIKA
 * ============================================================ */

/* Evaluasi fungsi logika berdasarkan nama */
int logika_eval(const char *nama, const double *args, int nargs,
                double *keluaran) {
    if (!nama || !keluaran) return -1;

    /* TRUE */
    if (banding_str(nama, "TRUE") == 0) {
        return logika_true(keluaran);
    }

    /* FALSE */
    if (banding_str(nama, "FALSE") == 0) {
        return logika_false(keluaran);
    }

    /* IF */
    if (banding_str(nama, "IF") == 0) {
        if (nargs < 2) return -1;
        return logika_if(args[0], args[1],
                        (nargs > 2) ? args[2] : 0.0,
                        keluaran);
    }

    /* NOT */
    if (banding_str(nama, "NOT") == 0) {
        if (nargs < 1) return -1;
        return logika_not(args[0], keluaran);
    }

    /* AND */
    if (banding_str(nama, "AND") == 0) {
        if (nargs < 1) return -1;
        return logika_and(args, nargs, keluaran);
    }

    /* OR */
    if (banding_str(nama, "OR") == 0) {
        if (nargs < 1) return -1;
        return logika_or(args, nargs, keluaran);
    }

    /* XOR */
    if (banding_str(nama, "XOR") == 0) {
        if (nargs < 1) return -1;
        return logika_xor(args, nargs, keluaran);
    }

    /* NAND */
    if (banding_str(nama, "NAND") == 0) {
        if (nargs < 1) return -1;
        return logika_nand(args, nargs, keluaran);
    }

    /* NOR */
    if (banding_str(nama, "NOR") == 0) {
        if (nargs < 1) return -1;
        return logika_nor(args, nargs, keluaran);
    }

    /* ISEVEN */
    if (banding_str(nama, "ISEVEN") == 0) {
        if (nargs < 1) return -1;
        return logika_iseven(args[0], keluaran);
    }

    /* ISODD */
    if (banding_str(nama, "ISODD") == 0) {
        if (nargs < 1) return -1;
        return logika_isodd(args[0], keluaran);
    }

    /* ISLOGICAL */
    if (banding_str(nama, "ISLOGICAL") == 0) {
        if (nargs < 1) return -1;
        return logika_islogical(args[0], keluaran);
    }

    /* CHOOSE */
    if (banding_str(nama, "CHOOSE") == 0) {
        if (nargs < 2) return -1;
        return logika_choose((int)args[0], args + 1, nargs - 1, keluaran);
    }

    /* BETWEEN */
    if (banding_str(nama, "BETWEEN") == 0) {
        if (nargs < 3) return -1;
        return logika_between(args[0], args[1], args[2], keluaran);
    }

    /* IMPLIES */
    if (banding_str(nama, "IMPLIES") == 0) {
        if (nargs < 2) return -1;
        return logika_implies(args[0], args[1], keluaran);
    }

    /* EQUIV */
    if (banding_str(nama, "EQUIV") == 0 ||
        banding_str(nama, "EQUIVALENT") == 0) {
        if (nargs < 2) return -1;
        return logika_equiv(args[0], args[1], keluaran);
    }

    return -1;
}

/* Evaluasi fungsi logika dengan teks */
int logika_eval_teks(const char *nama, const char *teks,
                     double *keluaran) {
    if (!nama || !keluaran) return -1;

    /* ISNUMBER */
    if (banding_str(nama, "ISNUMBER") == 0) {
        return logika_isnumber(teks, keluaran);
    }

    /* ISTEXT */
    if (banding_str(nama, "ISTEXT") == 0) {
        return logika_istext(teks, keluaran);
    }

    /* ISBLANK */
    if (banding_str(nama, "ISBLANK") == 0 ||
        banding_str(nama, "ISKOSONG") == 0) {
        return logika_isblank(teks, keluaran);
    }

    /* ISNONTEXT */
    if (banding_str(nama, "ISNONTEXT") == 0) {
        return logika_isnontext(teks, keluaran);
    }

    /* ISFORMULA */
    if (banding_str(nama, "ISFORMULA") == 0) {
        return logika_isformula(teks, keluaran);
    }

    /* ISREF */
    if (banding_str(nama, "ISREF") == 0) {
        return logika_isref(teks, keluaran);
    }

    return -1;
}
