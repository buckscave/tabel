/* ============================================================
 * TABEL v3.0 
 * Berkas: waktu.c - Formula Fungsi Waktu (Date/Time)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * FUNGSI HELPER STRING
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
 * FUNGSI DASAR WAKTU
 * ============================================================ */

/* Dapatkan waktu sekarang sebagai epoch */
int waktu_sekarang_epoch(void) {
    return (int)time(NULL);
}

/* Buat tanggal dari komponen */
int waktu_buat_tanggal(int tahun, int bulan, int hari, time_t *hasil) {
    struct tm tm_val;

    if (!hasil) return -1;
    if (tahun < 1900 || tahun > 9999) return -1;
    if (bulan < 1 || bulan > 12) return -1;
    if (hari < 1 || hari > 31) return -1;

    memset(&tm_val, 0, sizeof(tm_val));
    tm_val.tm_year = tahun - 1900;
    tm_val.tm_mon = bulan - 1;
    tm_val.tm_mday = hari;
    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;
    tm_val.tm_isdst = -1;
    *hasil = mktime(&tm_val);

    if (*hasil == (time_t)-1) return -1;
    return 0;
}

/* Buat waktu dari komponen */
int waktu_buat_waktu(int jam, int menit, int detik, time_t *hasil) {
    struct tm tm_val;
    time_t sekarang;

    if (!hasil) return -1;
    if (jam < 0 || jam > 23) return -1;
    if (menit < 0 || menit > 59) return -1;
    if (detik < 0 || detik > 59) return -1;

    sekarang = time(NULL);
    if (localtime_r(&sekarang, &tm_val) == NULL) return -1;

    tm_val.tm_hour = jam;
    tm_val.tm_min = menit;
    tm_val.tm_sec = detik;
    tm_val.tm_isdst = -1;
    *hasil = mktime(&tm_val);

    if (*hasil == (time_t)-1) return -1;
    return 0;
}

/* Ekstrak komponen tanggal */
int waktu_ekstrak_tanggal(time_t epoch, int *tahun, int *bulan, int *hari) {
    struct tm tm_val;

    if (localtime_r(&epoch, &tm_val) == NULL) return -1;

    if (tahun) *tahun = tm_val.tm_year + 1900;
    if (bulan) *bulan = tm_val.tm_mon + 1;
    if (hari) *hari = tm_val.tm_mday;
    return 0;
}

/* Ekstrak komponen waktu */
int waktu_ekstrak_waktu(time_t epoch, int *jam, int *menit, int *detik) {
    struct tm tm_val;

    if (localtime_r(&epoch, &tm_val) == NULL) return -1;

    if (jam) *jam = tm_val.tm_hour;
    if (menit) *menit = tm_val.tm_min;
    if (detik) *detik = tm_val.tm_sec;
    return 0;
}

/* ============================================================
 * FUNGSI FORMAT TANGGAL
 * ============================================================ */

/* Format tanggal sesuai pola */
int waktu_format_tanggal(const char *id_format, time_t epoch,
                          char *keluaran, size_t ukuran) {
    struct tm tm_val;
    int tahun, bulan, hari;

    if (!keluaran || ukuran == 0 || !id_format) return -1;

    if (localtime_r(&epoch, &tm_val) == NULL) {
        keluaran[0] = '\0';
        return -1;
    }

    tahun = tm_val.tm_year + 1900;
    bulan = tm_val.tm_mon + 1;
    hari = tm_val.tm_mday;

    /* Format: YYYY-MM-DD (ISO 8601) */
    if (banding_str(id_format, "YYYY-MM-DD") == 0 ||
        banding_str(id_format, "ISO") == 0) {
        snprintf(keluaran, ukuran, "%04d-%02d-%02d", tahun, bulan, hari);
        return 0;
    }

    /* Format: DD/MM/YYYY */
    if (banding_str(id_format, "DD/MM/YYYY") == 0) {
        snprintf(keluaran, ukuran, "%02d/%02d/%04d", hari, bulan, tahun);
        return 0;
    }

    /* Format: DD/MM/YY */
    if (banding_str(id_format, "DD/MM/YY") == 0) {
        snprintf(keluaran, ukuran, "%02d/%02d/%02d",
                 hari, bulan, tahun % 100);
        return 0;
    }

    /* Format: YY/MM/DD */
    if (banding_str(id_format, "YY/MM/DD") == 0) {
        snprintf(keluaran, ukuran, "%02d/%02d/%02d",
                 tahun % 100, bulan, hari);
        return 0;
    }

    /* Format: YY/DD/MM */
    if (banding_str(id_format, "YY/DD/MM") == 0) {
        snprintf(keluaran, ukuran, "%02d/%02d/%02d",
                 tahun % 100, hari, bulan);
        return 0;
    }

    /* Format: MM/DD/YYYY */
    if (banding_str(id_format, "MM/DD/YYYY") == 0) {
        snprintf(keluaran, ukuran, "%02d/%02d/%04d", bulan, hari, tahun);
        return 0;
    }

    /* Format: YYYY/MM/DD */
    if (banding_str(id_format, "YYYY/MM/DD") == 0) {
        snprintf(keluaran, ukuran, "%04d/%02d/%02d", tahun, bulan, hari);
        return 0;
    }

    /* Format: DD-MM-YYYY */
    if (banding_str(id_format, "DD-MM-YYYY") == 0) {
        snprintf(keluaran, ukuran, "%02d-%02d-%04d", hari, bulan, tahun);
        return 0;
    }

    /* Format: DD.MM.YYYY */
    if (banding_str(id_format, "DD.MM.YYYY") == 0) {
        snprintf(keluaran, ukuran, "%02d.%02d.%04d", hari, bulan, tahun);
        return 0;
    }

    /* Format: Nama (misal: 15 Januari 2024) */
    if (banding_str(id_format, "NAMA") == 0 ||
        banding_str(id_format, "PANJANG") == 0) {
        static const char *nama_bulan[12] = {
            "Januari", "Februari", "Maret", "April", "Mei", "Juni",
            "Juli", "Agustus", "September", "Oktober", "November",
            "Desember"
        };
        snprintf(keluaran, ukuran, "%d %s %d", hari,
                 nama_bulan[tm_val.tm_mon], tahun);
        return 0;
    }

    /* Format: Pendek (misal: 15 Jan 2024) */
    if (banding_str(id_format, "PENDEK") == 0) {
        static const char *nama_bulan_pendek[12] = {
            "Jan", "Feb", "Mar", "Apr", "Mei", "Jun",
            "Jul", "Agt", "Sep", "Okt", "Nov", "Des"
        };
        snprintf(keluaran, ukuran, "%d %s %d", hari,
                 nama_bulan_pendek[tm_val.tm_mon], tahun);
        return 0;
    }

    /* Format: Hari (nama hari) */
    if (banding_str(id_format, "HARI") == 0) {
        static const char *nama_hari[7] = {
            "Minggu", "Senin", "Selasa", "Rabu",
            "Kamis", "Jumat", "Sabtu"
        };
        strncpy(keluaran, nama_hari[tm_val.tm_wday], ukuran - 1);
        keluaran[ukuran - 1] = '\0';
        return 0;
    }

    /* Default: ISO 8601 */
    snprintf(keluaran, ukuran, "%04d-%02d-%02d", tahun, bulan, hari);
    return 0;
}

/* ============================================================
 * FUNGSI FORMAT WAKTU
 * ============================================================ */

/* Format waktu sesuai pola */
int waktu_format_waktu(const char *id_format, time_t epoch,
                        char *keluaran, size_t ukuran) {
    struct tm tm_val;
    int jam, menit, detik;
    int jam12;
    const char *am_pm;

    if (!keluaran || ukuran == 0 || !id_format) return -1;

    if (localtime_r(&epoch, &tm_val) == NULL) {
        keluaran[0] = '\0';
        return -1;
    }

    jam = tm_val.tm_hour;
    menit = tm_val.tm_min;
    detik = tm_val.tm_sec;

    /* Format: HH:MM:SS (24 jam, dengan detik) */
    if (banding_str(id_format, "HH:MM:SS") == 0) {
        snprintf(keluaran, ukuran, "%02d:%02d:%02d", jam, menit, detik);
        return 0;
    }

    /* Format: HH:MM (24 jam, tanpa detik) */
    if (banding_str(id_format, "HH:MM") == 0) {
        snprintf(keluaran, ukuran, "%02d:%02d", jam, menit);
        return 0;
    }

    /* Format: H:M (tanpa nol depan) */
    if (banding_str(id_format, "H:M") == 0) {
        snprintf(keluaran, ukuran, "%d:%d", jam, menit);
        return 0;
    }

    /* Format: HH (hanya jam) */
    if (banding_str(id_format, "HH") == 0) {
        snprintf(keluaran, ukuran, "%02d", jam);
        return 0;
    }

    /* Format: MM (hanya menit) */
    if (banding_str(id_format, "MM") == 0) {
        snprintf(keluaran, ukuran, "%02d", menit);
        return 0;
    }

    /* Format: SS (hanya detik) */
    if (banding_str(id_format, "SS") == 0) {
        snprintf(keluaran, ukuran, "%02d", detik);
        return 0;
    }

    /* Format: hh:mm:ss AM/PM (12 jam) */
    if (banding_str(id_format, "hh:mm:ss") == 0) {
        jam12 = (jam == 0) ? 12 : (jam > 12 ? jam - 12 : jam);
        am_pm = (jam < 12) ? "AM" : "PM";
        snprintf(keluaran, ukuran, "%02d:%02d:%02d %s",
                 jam12, menit, detik, am_pm);
        return 0;
    }

    /* Format: hh:mm AM/PM (12 jam, tanpa detik) */
    if (banding_str(id_format, "hh:mm") == 0) {
        jam12 = (jam == 0) ? 12 : (jam > 12 ? jam - 12 : jam);
        am_pm = (jam < 12) ? "AM" : "PM";
        snprintf(keluaran, ukuran, "%02d:%02d %s", jam12, menit, am_pm);
        return 0;
    }

    /* Format: TIMESTAMP (tanggal + waktu) */
    if (banding_str(id_format, "TIMESTAMP") == 0 ||
        banding_str(id_format, "LENGKAP") == 0) {
        snprintf(keluaran, ukuran, "%04d-%02d-%02d %02d:%02d:%02d",
                 tm_val.tm_year + 1900, tm_val.tm_mon + 1,
                 tm_val.tm_mday, jam, menit, detik);
        return 0;
    }

    /* Default: HH:MM:SS */
    snprintf(keluaran, ukuran, "%02d:%02d:%02d", jam, menit, detik);
    return 0;
}

/* ============================================================
 * FUNGSI PARSE
 * ============================================================ */

/* Parse string tanggal ke epoch */
int waktu_parse_tanggal(const char *str, time_t *hasil) {
    int tahun, bulan, hari;
    int jumlah;
    char pemisah;

    if (!str || !hasil) return -1;

    /* Coba format YYYY-MM-DD */
    jumlah = sscanf(str, "%d%c%d%c%d", &tahun, &pemisah, &bulan,
                    &pemisah, &hari);
    if (jumlah == 5) {
        if (pemisah == '-' || pemisah == '/' || pemisah == '.') {
            /* Deteksi format berdasarkan nilai */
            if (tahun > 31) {
                /* Format YYYY-MM-DD */
                return waktu_buat_tanggal(tahun, bulan, hari, hasil);
            } else if (hari > 31) {
                /* Format DD-MM-YYYY */
                return waktu_buat_tanggal(hari, bulan, tahun, hasil);
            } else {
                /* Asumsi YYYY-MM-DD */
                return waktu_buat_tanggal(tahun, bulan, hari, hasil);
            }
        }
    }

    /* Coba format DD/MM/YYYY atau DD-MM-YYYY */
    jumlah = sscanf(str, "%d%c%d%c%d", &hari, &pemisah, &bulan,
                    &pemisah, &tahun);
    if (jumlah == 5) {
        if (pemisah == '/' || pemisah == '-' || pemisah == '.') {
            return waktu_buat_tanggal(tahun, bulan, hari, hasil);
        }
    }

    return -1;
}

/* Parse string waktu ke epoch (dengan tanggal hari ini) */
int waktu_parse_waktu(const char *str, time_t *hasil) {
    int jam, menit, detik;
    int jumlah;

    if (!str || !hasil) return -1;

    /* Coba format HH:MM:SS */
    detik = 0;
    jumlah = sscanf(str, "%d:%d:%d", &jam, &menit, &detik);
    if (jumlah >= 2) {
        return waktu_buat_waktu(jam, menit, detik, hasil);
    }

    return -1;
}

/* ============================================================
 * FUNGSI TANGGAL (FORMULA)
 * ============================================================ */

/* DATE: Buat tanggal dari tahun, bulan, hari */
int waktu_fn_date(double tahun, double bulan, double hari,
                  double *keluaran) {
    time_t t;
    int t_int = (int)tahun;
    int b_int = (int)bulan;
    int h_int = (int)hari;

    if (!keluaran) return -1;

    if (waktu_buat_tanggal(t_int, b_int, h_int, &t) != 0) {
        return -1;
    }

    /* Kembalikan sebagai serial number */
    *keluaran = (double)t / (double)DETIK_HARI;
    return 0;
}

/* TIME: Buat waktu dari jam, menit, detik */
int waktu_fn_time(double jam, double menit, double detik,
                  double *keluaran) {
    if (!keluaran) return -1;

    /* Kembalikan sebagai fraksi hari */
    *keluaran = (jam * DETIK_JAM + menit * DETIK_MENIT + detik) /
                (double)DETIK_HARI;
    return 0;
}

/* TODAY: Tanggal hari ini */
int waktu_fn_today(double *keluaran) {
    time_t t;
    struct tm tm_val;
    time_t midnight;

    if (!keluaran) return -1;

    t = time(NULL);
    if (localtime_r(&t, &tm_val) == NULL) return -1;

    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;
    tm_val.tm_isdst = -1;
    midnight = mktime(&tm_val);

    *keluaran = (double)midnight / (double)DETIK_HARI;
    return 0;
}

/* NOW: Tanggal dan waktu sekarang */
int waktu_fn_now(double *keluaran) {
    if (!keluaran) return -1;
    *keluaran = (double)time(NULL) / (double)DETIK_HARI;
    return 0;
}

/* YEAR: Ekstrak tahun */
int waktu_fn_year(double serial, double *keluaran) {
    time_t t;
    int tahun;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    if (waktu_ekstrak_tanggal(t, &tahun, NULL, NULL) != 0) {
        return -1;
    }
    *keluaran = (double)tahun;
    return 0;
}

/* MONTH: Ekstrak bulan */
int waktu_fn_month(double serial, double *keluaran) {
    time_t t;
    int bulan;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    if (waktu_ekstrak_tanggal(t, NULL, &bulan, NULL) != 0) {
        return -1;
    }
    *keluaran = (double)bulan;
    return 0;
}

/* DAY: Ekstrak hari */
int waktu_fn_day(double serial, double *keluaran) {
    time_t t;
    int hari;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    if (waktu_ekstrak_tanggal(t, NULL, NULL, &hari) != 0) {
        return -1;
    }
    *keluaran = (double)hari;
    return 0;
}

/* HOUR: Ekstrak jam */
int waktu_fn_hour(double serial, double *keluaran) {
    time_t t;
    int jam;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    if (waktu_ekstrak_waktu(t, &jam, NULL, NULL) != 0) {
        return -1;
    }
    *keluaran = (double)jam;
    return 0;
}

/* MINUTE: Ekstrak menit */
int waktu_fn_minute(double serial, double *keluaran) {
    time_t t;
    int menit;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    if (waktu_ekstrak_waktu(t, NULL, &menit, NULL) != 0) {
        return -1;
    }
    *keluaran = (double)menit;
    return 0;
}

/* SECOND: Ekstrak detik */
int waktu_fn_second(double serial, double *keluaran) {
    time_t t;
    int detik;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    if (waktu_ekstrak_waktu(t, NULL, NULL, &detik) != 0) {
        return -1;
    }
    *keluaran = (double)detik;
    return 0;
}

/* WEEKDAY: Hari dalam minggu (1=Minggu, 7=Sabtu) */
int waktu_fn_weekday(double serial, double *keluaran) {
    time_t t;
    struct tm tm_val;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    if (localtime_r(&t, &tm_val) == NULL) return -1;

    *keluaran = (double)(tm_val.tm_wday + 1);
    return 0;
}

/* DATEDIF: Selisih antara dua tanggal */
int waktu_fn_datedif(double serial1, double serial2, const char *unit,
                      double *keluaran) {
    time_t t1;
    time_t t2;
    double diff;

    if (!unit || !keluaran) return -1;

    t1 = (time_t)(serial1 * DETIK_HARI);
    t2 = (time_t)(serial2 * DETIK_HARI);
    diff = difftime(t2, t1);

    if (banding_str(unit, "D") == 0) {
        *keluaran = diff / (double)DETIK_HARI;
    } else if (banding_str(unit, "M") == 0) {
        struct tm tm1, tm2;
        int bulan;
        localtime_r(&t1, &tm1);
        localtime_r(&t2, &tm2);
        bulan = (tm2.tm_year - tm1.tm_year) * 12 + tm2.tm_mon - tm1.tm_mon;
        *keluaran = (double)bulan;
    } else if (banding_str(unit, "Y") == 0) {
        struct tm tm1, tm2;
        localtime_r(&t1, &tm1);
        localtime_r(&t2, &tm2);
        *keluaran = (double)(tm2.tm_year - tm1.tm_year);
    } else if (banding_str(unit, "H") == 0) {
        *keluaran = diff / (double)DETIK_JAM;
    } else if (banding_str(unit, "N") == 0 ||
               banding_str(unit, "MI") == 0) {
        *keluaran = diff / (double)DETIK_MENIT;
    } else if (banding_str(unit, "S") == 0) {
        *keluaran = diff;
    } else {
        return -1;
    }
    return 0;
}

/* EDATE: Tambah/kurangi bulan */
int waktu_fn_edate(double serial, double bulan, double *keluaran) {
    time_t t;
    struct tm tm_val;
    int bulan_int;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    bulan_int = (int)bulan;

    if (localtime_r(&t, &tm_val) == NULL) return -1;

    tm_val.tm_mon += bulan_int;
    tm_val.tm_isdst = -1;
    t = mktime(&tm_val);

    if (t == (time_t)-1) return -1;

    *keluaran = (double)t / (double)DETIK_HARI;
    return 0;
}

/* EOMONTH: Akhir bulan */
int waktu_fn_eomonth(double serial, double bulan, double *keluaran) {
    time_t t;
    struct tm tm_val;
    int bulan_int;

    if (!keluaran) return -1;

    t = (time_t)(serial * DETIK_HARI);
    bulan_int = (int)bulan;

    if (localtime_r(&t, &tm_val) == NULL) return -1;

    tm_val.tm_mon += bulan_int + 1;
    tm_val.tm_mday = 0; /* Hari 0 = hari terakhir bulan sebelumnya */
    tm_val.tm_hour = 0;
    tm_val.tm_min = 0;
    tm_val.tm_sec = 0;
    tm_val.tm_isdst = -1;
    t = mktime(&tm_val);

    if (t == (time_t)-1) return -1;

    *keluaran = (double)t / (double)DETIK_HARI;
    return 0;
}

/* ============================================================
 * FORMAT SERIAL
 * ============================================================ */

/* Format tanggal dari serial number */
int waktu_format_tanggal_serial(double serial, const char *format,
                                 char *keluaran, size_t ukuran) {
    time_t t = (time_t)(serial * DETIK_HARI);
    return waktu_format_tanggal(format, t, keluaran, ukuran);
}

/* Format waktu dari serial number */
int waktu_format_waktu_serial(double serial, const char *format,
                               char *keluaran, size_t ukuran) {
    time_t t = (time_t)(serial * DETIK_HARI);
    return waktu_format_waktu(format, t, keluaran, ukuran);
}
