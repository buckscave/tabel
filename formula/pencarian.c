/* ============================================================
 * TABEL v3.0
 * Berkas: pencarian.c - Formula Fungsi Pencarian (Lookup)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * Fungsi pencarian yang didukung:
 * - VLOOKUP  : Pencarian vertikal
 * - HLOOKUP  : Pencarian horizontal
 * - LOOKUP   : Pencarian vektor
 * - INDEX    : Ambil nilai dari posisi
 * - MATCH    : Cari posisi nilai
 * - CHOOSE   : Pilih nilai dari daftar
 * - OFFSET   : Ambil nilai dengan offset
 * - INDIRECT : Referensi tidak langsung
 * - ROW      : Nomor baris
 * - COLUMN   : Nomor kolom
 * - ROWS     : Jumlah baris range
 * - COLUMNS  : Jumlah kolom range
 * - ADDRESS  : Buat alamat sel
 * - AREAS    : Jumlah area
 * - GETPIVOTDATA : Data pivot (placeholder)
 * - HYPERLINK : Link (placeholder)
 * ============================================================ */

#include "../tabel.h"

/* ============================================================
 * KONSTANTA
 * ============================================================ */
#define PENCARIAN_TOLERANSI    1e-10

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
static double pencarian_abs(double x) {
    return (x < 0.0) ? -x : x;
}

/* Parsing range */
static int parse_range_pencarian(const char *s, int *x1, int *y1,
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

/* Konversi kolom ke huruf (0 = A, 1 = B, dst) */
static void kolom_ke_huruf(int kolom, char *buf, size_t ukuran) {
    int i = 0;
    int temp;

    if (!buf || ukuran < 2) return;

    buf[ukuran - 1] = '\0';
    temp = ukuran - 2;

    if (kolom < 0) {
        buf[0] = '?';
        buf[1] = '\0';
        return;
    }

    kolom++; /* Konversi ke 1-based */

    if (kolom <= 0) {
        buf[0] = '\0';
        return;
    }

    i = temp;
    buf[i--] = '\0';

    while (kolom > 0 && i >= 0) {
        int sisa = (kolom - 1) % 26;
        buf[i--] = (char)('A' + sisa);
        kolom = (kolom - 1) / 26;
    }

    if (i >= 0) {
        memmove(buf, buf + i + 1, (size_t)(temp - i));
    }
}

/* ============================================================
 * VLOOKUP: Pencarian Vertikal
 * VLOOKUP(nilai_cari, range, kolom_hasil, [pencocokan])
 * pencocokan: 0 = exact, 1 = approximate (default)
 * ============================================================ */
int pencarian_vlookup(const struct buffer_tabel *buf, double nilai_cari,
                      const char *range, int kolom_hasil, int exact,
                      double *keluaran) {
    int x1, y1, x2, y2;
    int baris_awal, baris_akhir;
    int kolom_awal, kolom_akhir;
    int r;
    int baris_ketemu = -1;
    double nilai_terdekat = 0.0;
    int ada_terdekat = 0;
    double selisih_min = 1e308;
    const char *t;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    kolom_awal = (x1 < x2) ? x1 : x2;
    kolom_akhir = (x1 > x2) ? x1 : x2;
    baris_awal = (y1 < y2) ? y1 : y2;
    baris_akhir = (y1 > y2) ? y1 : y2;

    /* Validasi kolom hasil */
    if (kolom_hasil < 1 || kolom_awal + kolom_hasil - 1 > kolom_akhir) {
        return -1;
    }

    /* Cari di kolom pertama */
    for (r = baris_awal; r <= baris_akhir; r++) {
        double nilai_sel;
        char *endp;

        if (kolom_awal >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

        t = buf->isi[r][kolom_awal];
        if (t[0] == '\0') continue;

        nilai_sel = strtod(t, &endp);
        if (endp == t) continue;

        if (exact) {
            /* Pencocokan eksak */
            if (pencarian_abs(nilai_sel - nilai_cari) < PENCARIAN_TOLERANSI) {
                baris_ketemu = r;
                break;
            }
        } else {
            /* Pencocokan approximate (nilai terdekat yang <= nilai_cari) */
            if (nilai_sel <= nilai_cari) {
                double selisih = nilai_cari - nilai_sel;
                if (selisih < selisih_min) {
                    selisih_min = selisih;
                    nilai_terdekat = nilai_sel;
                    baris_ketemu = r;
                    ada_terdekat = 1;
                }
            }
        }
    }

    if (baris_ketemu < 0) {
        return -1;
    }

    /* Ambil nilai dari kolom hasil */
    t = buf->isi[baris_ketemu][kolom_awal + kolom_hasil - 1];
    if (t[0] == '\0') {
        *keluaran = 0.0;
    } else {
        char *endp;
        *keluaran = strtod(t, &endp);
    }

    return 0;
}

/* VLOOKUP dengan teks */
int pencarian_vlookup_teks(const struct buffer_tabel *buf, const char *teks_cari,
                           const char *range, int kolom_hasil, int exact,
                           char *keluaran, size_t ukuran) {
    int x1, y1, x2, y2;
    int baris_awal, baris_akhir;
    int kolom_awal, kolom_akhir;
    int r;
    int baris_ketemu = -1;
    const char *t;

    if (!buf || !teks_cari || !range || !keluaran || ukuran == 0) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    kolom_awal = (x1 < x2) ? x1 : x2;
    kolom_akhir = (x1 > x2) ? x1 : x2;
    baris_awal = (y1 < y2) ? y1 : y2;
    baris_akhir = (y1 > y2) ? y1 : y2;

    if (kolom_hasil < 1 || kolom_awal + kolom_hasil - 1 > kolom_akhir) {
        return -1;
    }

    /* Cari teks di kolom pertama */
    for (r = baris_awal; r <= baris_akhir; r++) {
        if (kolom_awal >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

        t = buf->isi[r][kolom_awal];
        if (t[0] == '\0') continue;

        if (exact) {
            if (strcmp(t, teks_cari) == 0) {
                baris_ketemu = r;
                break;
            }
        } else {
            if (banding_str(t, teks_cari) == 0) {
                baris_ketemu = r;
                break;
            }
        }
    }

    if (baris_ketemu < 0) {
        return -1;
    }

    /* Ambil nilai dari kolom hasil */
    t = buf->isi[baris_ketemu][kolom_awal + kolom_hasil - 1];
    strncpy(keluaran, t ? t : "", ukuran - 1);
    keluaran[ukuran - 1] = '\0';

    return 0;
}

/* ============================================================
 * HLOOKUP: Pencarian Horizontal
 * HLOOKUP(nilai_cari, range, baris_hasil, [pencocokan])
 * ============================================================ */
int pencarian_hlookup(const struct buffer_tabel *buf, double nilai_cari,
                      const char *range, int baris_hasil, int exact,
                      double *keluaran) {
    int x1, y1, x2, y2;
    int baris_awal, baris_akhir;
    int kolom_awal, kolom_akhir;
    int c;
    int kolom_ketemu = -1;
    double nilai_terdekat = 0.0;
    int ada_terdekat = 0;
    double selisih_min = 1e308;
    const char *t;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    kolom_awal = (x1 < x2) ? x1 : x2;
    kolom_akhir = (x1 > x2) ? x1 : x2;
    baris_awal = (y1 < y2) ? y1 : y2;
    baris_akhir = (y1 > y2) ? y1 : y2;

    /* Validasi baris hasil */
    if (baris_hasil < 1 || baris_awal + baris_hasil - 1 > baris_akhir) {
        return -1;
    }

    /* Cari di baris pertama */
    for (c = kolom_awal; c <= kolom_akhir; c++) {
        double nilai_sel;
        char *endp;

        if (c >= buf->cfg.kolom || baris_awal >= buf->cfg.baris) continue;

        t = buf->isi[baris_awal][c];
        if (t[0] == '\0') continue;

        nilai_sel = strtod(t, &endp);
        if (endp == t) continue;

        if (exact) {
            if (pencarian_abs(nilai_sel - nilai_cari) < PENCARIAN_TOLERANSI) {
                kolom_ketemu = c;
                break;
            }
        } else {
            if (nilai_sel <= nilai_cari) {
                double selisih = nilai_cari - nilai_sel;
                if (selisih < selisih_min) {
                    selisih_min = selisih;
                    nilai_terdekat = nilai_sel;
                    kolom_ketemu = c;
                    ada_terdekat = 1;
                }
            }
        }
    }

    if (kolom_ketemu < 0) {
        return -1;
    }

    /* Ambil nilai dari baris hasil */
    t = buf->isi[baris_awal + baris_hasil - 1][kolom_ketemu];
    if (t[0] == '\0') {
        *keluaran = 0.0;
    } else {
        char *endp;
        *keluaran = strtod(t, &endp);
    }

    return 0;
}

/* HLOOKUP dengan teks */
int pencarian_hlookup_teks(const struct buffer_tabel *buf, const char *teks_cari,
                           const char *range, int baris_hasil, int exact,
                           char *keluaran, size_t ukuran) {
    int x1, y1, x2, y2;
    int baris_awal, baris_akhir;
    int kolom_awal, kolom_akhir;
    int c;
    int kolom_ketemu = -1;
    const char *t;

    if (!buf || !teks_cari || !range || !keluaran || ukuran == 0) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    kolom_awal = (x1 < x2) ? x1 : x2;
    kolom_akhir = (x1 > x2) ? x1 : x2;
    baris_awal = (y1 < y2) ? y1 : y2;
    baris_akhir = (y1 > y2) ? y1 : y2;

    if (baris_hasil < 1 || baris_awal + baris_hasil - 1 > baris_akhir) {
        return -1;
    }

    /* Cari teks di baris pertama */
    for (c = kolom_awal; c <= kolom_akhir; c++) {
        if (c >= buf->cfg.kolom || baris_awal >= buf->cfg.baris) continue;

        t = buf->isi[baris_awal][c];
        if (t[0] == '\0') continue;

        if (exact) {
            if (strcmp(t, teks_cari) == 0) {
                kolom_ketemu = c;
                break;
            }
        } else {
            if (banding_str(t, teks_cari) == 0) {
                kolom_ketemu = c;
                break;
            }
        }
    }

    if (kolom_ketemu < 0) {
        return -1;
    }

    /* Ambil nilai dari baris hasil */
    t = buf->isi[baris_awal + baris_hasil - 1][kolom_ketemu];
    strncpy(keluaran, t ? t : "", ukuran - 1);
    keluaran[ukuran - 1] = '\0';

    return 0;
}

/* ============================================================
 * LOOKUP: Pencarian Vektor
 * LOOKUP(nilai_cari, range_cari, range_hasil)
 * ============================================================ */
int pencarian_lookup(const struct buffer_tabel *buf, double nilai_cari,
                     const char *range_cari, const char *range_hasil,
                     double *keluaran) {
    int x1a, y1a, x2a, y2a;
    int x1b, y1b, x2b, y2b;
    int r, c;
    int pos_ketemu = -1;
    const char *t;

    if (!buf || !range_cari || !range_hasil || !keluaran) return -1;

    if (parse_range_pencarian(range_cari, &x1a, &y1a, &x2a, &y2a) != 0) return -1;
    if (parse_range_pencarian(range_hasil, &x1b, &y1b, &x2b, &y2b) != 0) return -1;

    /* Tentukan apakah vertikal atau horizontal */
    if (y1a == y2a) {
        /* Horizontal: cari di baris */
        for (c = x1a; c <= x2a; c++) {
            double nilai_sel;
            char *endp;

            if (c >= buf->cfg.kolom || y1a >= buf->cfg.baris) continue;

            t = buf->isi[y1a][c];
            if (t[0] == '\0') continue;

            nilai_sel = strtod(t, &endp);
            if (endp == t) continue;

            if (nilai_sel <= nilai_cari) {
                pos_ketemu = c - x1a;
            } else {
                break;
            }
        }

        if (pos_ketemu < 0) return -1;

        /* Ambil dari range hasil */
        c = x1b + pos_ketemu;
        if (c > x2b) return -1;

        t = buf->isi[y1b][c];
        if (t[0] == '\0') {
            *keluaran = 0.0;
        } else {
            char *endp;
            *keluaran = strtod(t, &endp);
        }
    } else {
        /* Vertikal: cari di kolom */
        for (r = y1a; r <= y2a; r++) {
            double nilai_sel;
            char *endp;

            if (x1a >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

            t = buf->isi[r][x1a];
            if (t[0] == '\0') continue;

            nilai_sel = strtod(t, &endp);
            if (endp == t) continue;

            if (nilai_sel <= nilai_cari) {
                pos_ketemu = r - y1a;
            } else {
                break;
            }
        }

        if (pos_ketemu < 0) return -1;

        /* Ambil dari range hasil */
        r = y1b + pos_ketemu;
        if (r > y2b) return -1;

        t = buf->isi[r][x1b];
        if (t[0] == '\0') {
            *keluaran = 0.0;
        } else {
            char *endp;
            *keluaran = strtod(t, &endp);
        }
    }

    return 0;
}

/* ============================================================
 * INDEX: Ambil Nilai dari Posisi
 * INDEX(range, baris, [kolom])
 * ============================================================ */
int pencarian_index(const struct buffer_tabel *buf, const char *range,
                    int baris, int kolom, double *keluaran) {
    int x1, y1, x2, y2;
    int kolom_awal, kolom_akhir;
    int baris_awal, baris_akhir;
    const char *t;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    kolom_awal = (x1 < x2) ? x1 : x2;
    kolom_akhir = (x1 > x2) ? x1 : x2;
    baris_awal = (y1 < y2) ? y1 : y2;
    baris_akhir = (y1 > y2) ? y1 : y2;

    /* INDEX 1-based */
    if (baris < 1) return -1;

    /* Jika hanya 1 kolom, baris adalah indeks */
    if (kolom_awal == kolom_akhir || kolom <= 0) {
        kolom = 1;
    }

    if (baris > baris_akhir - baris_awal + 1) return -1;
    if (kolom > kolom_akhir - kolom_awal + 1) return -1;

    t = buf->isi[baris_awal + baris - 1][kolom_awal + kolom - 1];
    if (t[0] == '\0') {
        *keluaran = 0.0;
    } else {
        char *endp;
        *keluaran = strtod(t, &endp);
    }

    return 0;
}

/* INDEX dengan teks */
int pencarian_index_teks(const struct buffer_tabel *buf, const char *range,
                         int baris, int kolom, char *keluaran, size_t ukuran) {
    int x1, y1, x2, y2;
    int kolom_awal, kolom_akhir;
    int baris_awal, baris_akhir;
    const char *t;

    if (!buf || !range || !keluaran || ukuran == 0) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    kolom_awal = (x1 < x2) ? x1 : x2;
    kolom_akhir = (x1 > x2) ? x1 : x2;
    baris_awal = (y1 < y2) ? y1 : y2;
    baris_akhir = (y1 > y2) ? y1 : y2;

    if (baris < 1) return -1;

    if (kolom_awal == kolom_akhir || kolom <= 0) {
        kolom = 1;
    }

    if (baris > baris_akhir - baris_awal + 1) return -1;
    if (kolom > kolom_akhir - kolom_awal + 1) return -1;

    t = buf->isi[baris_awal + baris - 1][kolom_awal + kolom - 1];
    strncpy(keluaran, t ? t : "", ukuran - 1);
    keluaran[ukuran - 1] = '\0';

    return 0;
}

/* ============================================================
 * MATCH: Cari Posisi Nilai
 * MATCH(nilai_cari, range, [tipe_pencocokan])
 * tipe: 1 = kurang dari, 0 = exact, -1 = lebih dari
 * ============================================================ */
int pencarian_match(const struct buffer_tabel *buf, double nilai_cari,
                    const char *range, int tipe, double *keluaran) {
    int x1, y1, x2, y2;
    int r, c;
    int pos = -1;
    const char *t;

    if (!buf || !range || !keluaran) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    /* Default: exact match */
    if (tipe == 0) {
        /* Exact match */
        /* Cek apakah vertikal atau horizontal */
        if (x1 == x2) {
            /* Vertikal */
            for (r = y1; r <= y2; r++) {
                double nilai_sel;
                char *endp;

                if (x1 >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

                t = buf->isi[r][x1];
                if (t[0] == '\0') continue;

                nilai_sel = strtod(t, &endp);
                if (endp == t) continue;

                if (pencarian_abs(nilai_sel - nilai_cari) < PENCARIAN_TOLERANSI) {
                    pos = r - y1 + 1; /* 1-based */
                    break;
                }
            }
        } else {
            /* Horizontal */
            for (c = x1; c <= x2; c++) {
                double nilai_sel;
                char *endp;

                if (c >= buf->cfg.kolom || y1 >= buf->cfg.baris) continue;

                t = buf->isi[y1][c];
                if (t[0] == '\0') continue;

                nilai_sel = strtod(t, &endp);
                if (endp == t) continue;

                if (pencarian_abs(nilai_sel - nilai_cari) < PENCARIAN_TOLERANSI) {
                    pos = c - x1 + 1; /* 1-based */
                    break;
                }
            }
        }
    } else if (tipe == 1) {
        /* Less than or equal (nilai terbesar yang <= nilai_cari) */
        double nilai_terdekat = -1e308;

        if (x1 == x2) {
            for (r = y1; r <= y2; r++) {
                double nilai_sel;
                char *endp;

                if (x1 >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

                t = buf->isi[r][x1];
                if (t[0] == '\0') continue;

                nilai_sel = strtod(t, &endp);
                if (endp == t) continue;

                if (nilai_sel <= nilai_cari && nilai_sel > nilai_terdekat) {
                    nilai_terdekat = nilai_sel;
                    pos = r - y1 + 1;
                }
            }
        } else {
            for (c = x1; c <= x2; c++) {
                double nilai_sel;
                char *endp;

                if (c >= buf->cfg.kolom || y1 >= buf->cfg.baris) continue;

                t = buf->isi[y1][c];
                if (t[0] == '\0') continue;

                nilai_sel = strtod(t, &endp);
                if (endp == t) continue;

                if (nilai_sel <= nilai_cari && nilai_sel > nilai_terdekat) {
                    nilai_terdekat = nilai_sel;
                    pos = c - x1 + 1;
                }
            }
        }
    } else {
        /* Greater than or equal (nilai terkecil yang >= nilai_cari) */
        double nilai_terdekat = 1e308;

        if (x1 == x2) {
            for (r = y1; r <= y2; r++) {
                double nilai_sel;
                char *endp;

                if (x1 >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

                t = buf->isi[r][x1];
                if (t[0] == '\0') continue;

                nilai_sel = strtod(t, &endp);
                if (endp == t) continue;

                if (nilai_sel >= nilai_cari && nilai_sel < nilai_terdekat) {
                    nilai_terdekat = nilai_sel;
                    pos = r - y1 + 1;
                }
            }
        } else {
            for (c = x1; c <= x2; c++) {
                double nilai_sel;
                char *endp;

                if (c >= buf->cfg.kolom || y1 >= buf->cfg.baris) continue;

                t = buf->isi[y1][c];
                if (t[0] == '\0') continue;

                nilai_sel = strtod(t, &endp);
                if (endp == t) continue;

                if (nilai_sel >= nilai_cari && nilai_sel < nilai_terdekat) {
                    nilai_terdekat = nilai_sel;
                    pos = c - x1 + 1;
                }
            }
        }
    }

    if (pos < 0) return -1;

    *keluaran = (double)pos;
    return 0;
}

/* MATCH dengan teks */
int pencarian_match_teks(const struct buffer_tabel *buf, const char *teks_cari,
                         const char *range, int exact, double *keluaran) {
    int x1, y1, x2, y2;
    int r, c;
    int pos = -1;
    const char *t;

    if (!buf || !teks_cari || !range || !keluaran) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    /* Cek apakah vertikal atau horizontal */
    if (x1 == x2) {
        /* Vertikal */
        for (r = y1; r <= y2; r++) {
            if (x1 >= buf->cfg.kolom || r >= buf->cfg.baris) continue;

            t = buf->isi[r][x1];
            if (t[0] == '\0') continue;

            if (exact) {
                if (strcmp(t, teks_cari) == 0) {
                    pos = r - y1 + 1;
                    break;
                }
            } else {
                if (banding_str(t, teks_cari) == 0) {
                    pos = r - y1 + 1;
                    break;
                }
            }
        }
    } else {
        /* Horizontal */
        for (c = x1; c <= x2; c++) {
            if (c >= buf->cfg.kolom || y1 >= buf->cfg.baris) continue;

            t = buf->isi[y1][c];
            if (t[0] == '\0') continue;

            if (exact) {
                if (strcmp(t, teks_cari) == 0) {
                    pos = c - x1 + 1;
                    break;
                }
            } else {
                if (banding_str(t, teks_cari) == 0) {
                    pos = c - x1 + 1;
                    break;
                }
            }
        }
    }

    if (pos < 0) return -1;

    *keluaran = (double)pos;
    return 0;
}

/* ============================================================
 * OFFSET: Ambil Nilai dengan Offset
 * OFFSET(referensi, baris, kolom, [tinggi], [lebar])
 * ============================================================ */
int pencarian_offset(const struct buffer_tabel *buf, const char *referensi,
                     int offset_baris, int offset_kolom,
                     double *keluaran) {
    int x, y;
    int target_x, target_y;
    const char *t;

    if (!buf || !referensi || !keluaran) return -1;

    if (parse_ref_sel(referensi, &x, &y) <= 0) return -1;

    target_x = x + offset_kolom;
    target_y = y + offset_baris;

    /* Validasi batas */
    if (target_x < 0 || target_x >= buf->cfg.kolom) return -1;
    if (target_y < 0 || target_y >= buf->cfg.baris) return -1;

    t = buf->isi[target_y][target_x];
    if (t[0] == '\0') {
        *keluaran = 0.0;
    } else {
        char *endp;
        *keluaran = strtod(t, &endp);
    }

    return 0;
}

/* ============================================================
 * INDIRECT: Referensi Tidak Langsung
 * INDIRECT(teks_referensi)
 * ============================================================ */
int pencarian_indirect(const struct buffer_tabel *buf, const char *teks_ref,
                       double *keluaran) {
    int x, y;
    const char *t;

    if (!buf || !teks_ref || !keluaran) return -1;

    if (parse_ref_sel(teks_ref, &x, &y) <= 0) return -1;

    /* Validasi batas */
    if (x < 0 || x >= buf->cfg.kolom) return -1;
    if (y < 0 || y >= buf->cfg.baris) return -1;

    t = buf->isi[y][x];
    if (t[0] == '\0') {
        *keluaran = 0.0;
    } else {
        char *endp;
        *keluaran = strtod(t, &endp);
    }

    return 0;
}

/* ============================================================
 * ROW dan COLUMN
 * ============================================================ */

/* ROW: Nomor baris */
int pencarian_row(const char *referensi, double *keluaran) {
    int x, y;

    if (!keluaran) return -1;

    if (!referensi || referensi[0] == '\0') {
        /* Tanpa referensi, kembalikan baris aktif (placeholder) */
        *keluaran = 1.0;
        return 0;
    }

    if (parse_ref_sel(referensi, &x, &y) <= 0) return -1;

    *keluaran = (double)(y + 1); /* 1-based */
    return 0;
}

/* COLUMN: Nomor kolom */
int pencarian_column(const char *referensi, double *keluaran) {
    int x, y;

    if (!keluaran) return -1;

    if (!referensi || referensi[0] == '\0') {
        /* Tanpa referensi, kembalikan kolom aktif (placeholder) */
        *keluaran = 1.0;
        return 0;
    }

    if (parse_ref_sel(referensi, &x, &y) <= 0) return -1;

    *keluaran = (double)(x + 1); /* 1-based */
    return 0;
}

/* ROWS: Jumlah baris dalam range */
int pencarian_rows(const char *range, double *keluaran) {
    int x1, y1, x2, y2;

    if (!range || !keluaran) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    *keluaran = (double)(y2 - y1 + 1);
    return 0;
}

/* COLUMNS: Jumlah kolom dalam range */
int pencarian_columns(const char *range, double *keluaran) {
    int x1, y1, x2, y2;

    if (!range || !keluaran) return -1;

    if (parse_range_pencarian(range, &x1, &y1, &x2, &y2) != 0) return -1;

    *keluaran = (double)(x2 - x1 + 1);
    return 0;
}

/* ============================================================
 * ADDRESS: Buat Alamat Sel
 * ADDRESS(baris, kolom, [tipe], [style], [sheet])
 * tipe: 1=absolut, 2=baris absolut, 3=kolom absolut, 4=relatif
 * ============================================================ */
int pencarian_address(int baris, int kolom, int tipe,
                      char *keluaran, size_t ukuran) {
    char kolom_str[8];

    if (!keluaran || ukuran < 4 || baris < 1 || kolom < 1) return -1;

    kolom_ke_huruf(kolom - 1, kolom_str, sizeof(kolom_str));

    switch (tipe) {
        case 1: /* $A$1 */
            snprintf(keluaran, ukuran, "$%s$%d", kolom_str, baris);
            break;
        case 2: /* A$1 */
            snprintf(keluaran, ukuran, "%s$%d", kolom_str, baris);
            break;
        case 3: /* $A1 */
            snprintf(keluaran, ukuran, "$%s%d", kolom_str, baris);
            break;
        case 4: /* A1 */
        default:
            snprintf(keluaran, ukuran, "%s%d", kolom_str, baris);
            break;
    }

    return 0;
}

/* ============================================================
 * AREAS: Jumlah Area
 * ============================================================ */
int pencarian_areas(const char *referensi, double *keluaran) {
    int count = 1;
    const char *p;

    if (!referensi || !keluaran) return -1;

    /* Hitung jumlah koma sebagai pemisah area */
    for (p = referensi; *p; p++) {
        if (*p == ',') count++;
    }

    *keluaran = (double)count;
    return 0;
}

/* ============================================================
 * DISPATCHER FUNGSI PENCARIAN
 * ============================================================ */

/* Evaluasi fungsi pencarian */
int pencarian_eval(const struct buffer_tabel *buf, const char *nama,
                   const double *args, int nargs, const char *range,
                   double *keluaran) {
    if (!nama || !keluaran) return -1;

    /* VLOOKUP */
    if (banding_str(nama, "VLOOKUP") == 0) {
        if (nargs < 2 || !range) return -1;
        return pencarian_vlookup(buf, args[0], range,
                                (int)args[1],
                                (nargs > 2) ? (int)args[2] : 1,
                                keluaran);
    }

    /* HLOOKUP */
    if (banding_str(nama, "HLOOKUP") == 0) {
        if (nargs < 2 || !range) return -1;
        return pencarian_hlookup(buf, args[0], range,
                                (int)args[1],
                                (nargs > 2) ? (int)args[2] : 1,
                                keluaran);
    }

    /* INDEX */
    if (banding_str(nama, "INDEX") == 0) {
        if (nargs < 2 || !range) return -1;
        return pencarian_index(buf, range, (int)args[0],
                              (nargs > 1) ? (int)args[1] : 1,
                              keluaran);
    }

    /* MATCH */
    if (banding_str(nama, "MATCH") == 0) {
        if (nargs < 1 || !range) return -1;
        return pencarian_match(buf, args[0], range,
                              (nargs > 1) ? (int)args[1] : 0,
                              keluaran);
    }

    /* ROW */
    if (banding_str(nama, "ROW") == 0) {
        return pencarian_row(range, keluaran);
    }

    /* COLUMN */
    if (banding_str(nama, "COLUMN") == 0 ||
        banding_str(nama, "COL") == 0) {
        return pencarian_column(range, keluaran);
    }

    /* ROWS */
    if (banding_str(nama, "ROWS") == 0) {
        if (!range) return -1;
        return pencarian_rows(range, keluaran);
    }

    /* COLUMNS */
    if (banding_str(nama, "COLUMNS") == 0 ||
        banding_str(nama, "COLS") == 0) {
        if (!range) return -1;
        return pencarian_columns(range, keluaran);
    }

    /* OFFSET */
    if (banding_str(nama, "OFFSET") == 0) {
        if (nargs < 2 || !range) return -1;
        return pencarian_offset(buf, range, (int)args[0], (int)args[1],
                               keluaran);
    }

    /* INDIRECT */
    if (banding_str(nama, "INDIRECT") == 0) {
        /* INDIRECT memerlukan teks referensi */
        return -1;
    }

    /* ADDRESS */
    if (banding_str(nama, "ADDRESS") == 0) {
        if (nargs < 2) return -1;
        {
            char alamat[32];
            int ret = pencarian_address((int)args[0], (int)args[1],
                                        (nargs > 2) ? (int)args[2] : 1,
                                        alamat, sizeof(alamat));
            if (ret == 0) {
                /* Kembalikan sebagai referensi (sementara 0) */
                *keluaran = 0.0;
            }
            return ret;
        }
    }

    /* AREAS */
    if (banding_str(nama, "AREAS") == 0) {
        if (!range) return -1;
        return pencarian_areas(range, keluaran);
    }

    return -1;
}

/* Evaluasi fungsi pencarian dengan teks */
int pencarian_eval_teks(const struct buffer_tabel *buf, const char *nama,
                        const char *teks_cari, const char *range,
                        double *keluaran) {
    if (!nama || !keluaran) return -1;

    /* VLOOKUP dengan teks */
    if (banding_str(nama, "VLOOKUP") == 0) {
        return -1; /* Perlu argumen tambahan */
    }

    /* MATCH dengan teks */
    if (banding_str(nama, "MATCH") == 0) {
        if (!teks_cari || !range) return -1;
        return pencarian_match_teks(buf, teks_cari, range, 0, keluaran);
    }

    /* INDIRECT */
    if (banding_str(nama, "INDIRECT") == 0) {
        if (!teks_cari) return -1;
        return pencarian_indirect(buf, teks_cari, keluaran);
    }

    return -1;
}
