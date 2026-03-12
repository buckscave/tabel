/* ============================================================
 * TABEL v3.0
 * Berkas: konversi.c - Formula Fungsi Konversi Satuan
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * Fungsi konversi yang didukung:
 * - CONVERT  : Konversi satuan umum (panjang, berat, suhu, dll)
 * - DEC2BIN  : Desimal ke Biner
 * - DEC2OCT  : Desimal ke Oktal
 * - DEC2HEX  : Desimal ke Heksadesimal
 * - BIN2DEC  : Biner ke Desimal
 * - BIN2OCT  : Biner ke Oktal
 * - BIN2HEX  : Biner ke Heksadesimal
 * - OCT2DEC  : Oktal ke Desimal
 * - OCT2BIN  : Oktal ke Biner
 * - OCT2HEX  : Oktal ke Heksadesimal
 * - HEX2DEC  : Heksadesimal ke Desimal
 * - HEX2BIN  : Heksadesimal ke Biner
 * - HEX2OCT  : Heksadesimal ke Oktal
 * ============================================================ */

#include "../tabel.h"

/* ============================================================
 * KONSTANTA
 * ============================================================ */
#define KONVERSI_TOLERANSI    1e-10

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
static double konversi_abs(double x) {
    return (x < 0.0) ? -x : x;
}

/* ============================================================
 * KONVERSI SUHU
 * ============================================================ */

/* Celsius ke Fahrenheit */
static double c_to_f(double c) {
    return c * 9.0 / 5.0 + 32.0;
}

/* Fahrenheit ke Celsius */
static double f_to_c(double f) {
    return (f - 32.0) * 5.0 / 9.0;
}

/* Celsius ke Kelvin */
static double c_to_k(double c) {
    return c + 273.15;
}

/* Kelvin ke Celsius */
static double k_to_c(double k) {
    return k - 273.15;
}

/* Fahrenheit ke Kelvin */
static double f_to_k(double f) {
    return c_to_k(f_to_c(f));
}

/* Kelvin ke Fahrenheit */
static double k_to_f(double k) {
    return c_to_f(k_to_c(k));
}

/* Celsius ke Rankine */
static double c_to_rank(double c) {
    return (c + 273.15) * 9.0 / 5.0;
}

/* Rankine ke Celsius */
static double rank_to_c(double r) {
    return (r - 491.67) * 5.0 / 9.0;
}

/* ============================================================
 * KONVERSI PANJANG
 * ============================================================ */

/* Konversi ke meter (basis) */
static double ke_meter(double nilai, const char *satuan) {
    if (banding_str(satuan, "m") == 0 || banding_str(satuan, "mtr") == 0 ||
        banding_str(satuan, "meter") == 0 || banding_str(satuan, "metre") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "km") == 0 || banding_str(satuan, "kilometer") == 0) {
        return nilai * 1000.0;
    }
    if (banding_str(satuan, "cm") == 0 || banding_str(satuan, "centimeter") == 0) {
        return nilai / 100.0;
    }
    if (banding_str(satuan, "mm") == 0 || banding_str(satuan, "millimeter") == 0) {
        return nilai / 1000.0;
    }
    if (banding_str(satuan, "mi") == 0 || banding_str(satuan, "mile") == 0 ||
        banding_str(satuan, "miles") == 0) {
        return nilai * 1609.344;
    }
    if (banding_str(satuan, "yd") == 0 || banding_str(satuan, "yard") == 0 ||
        banding_str(satuan, "yards") == 0) {
        return nilai * 0.9144;
    }
    if (banding_str(satuan, "ft") == 0 || banding_str(satuan, "foot") == 0 ||
        banding_str(satuan, "feet") == 0) {
        return nilai * 0.3048;
    }
    if (banding_str(satuan, "in") == 0 || banding_str(satuan, "inch") == 0 ||
        banding_str(satuan, "inches") == 0) {
        return nilai * 0.0254;
    }
    if (banding_str(satuan, "nm") == 0 || banding_str(satuan, "nmi") == 0 ||
        banding_str(satuan, "nauticalmile") == 0) {
        return nilai * 1852.0;
    }
    if (banding_str(satuan, "ly") == 0 || banding_str(satuan, "lightyear") == 0) {
        return nilai * 9.461e15;
    }
    if (banding_str(satuan, "au") == 0 || banding_str(satuan, "astronomicalunit") == 0) {
        return nilai * 1.496e11;
    }
    if (banding_str(satuan, "parsec") == 0 || banding_str(satuan, "pc") == 0) {
        return nilai * 3.086e16;
    }
    if (banding_str(satuan, "dm") == 0 || banding_str(satuan, "decimeter") == 0) {
        return nilai / 10.0;
    }
    if (banding_str(satuan, "dam") == 0 || banding_str(satuan, "decameter") == 0) {
        return nilai * 10.0;
    }
    if (banding_str(satuan, "hm") == 0 || banding_str(satuan, "hectometer") == 0) {
        return nilai * 100.0;
    }
    if (banding_str(satuan, "micron") == 0 || banding_str(satuan, "um") == 0 ||
        banding_str(satuan, "micrometer") == 0) {
        return nilai / 1000000.0;
    }
    if (banding_str(satuan, "angstrom") == 0 || banding_str(satuan, "ang") == 0) {
        return nilai / 10000000000.0;
    }
    return -1.0; /* Satuan tidak dikenal */
}

/* Konversi dari meter ke satuan lain */
static double dari_meter(double meter, const char *satuan) {
    if (banding_str(satuan, "m") == 0 || banding_str(satuan, "mtr") == 0 ||
        banding_str(satuan, "meter") == 0 || banding_str(satuan, "metre") == 0) {
        return meter;
    }
    if (banding_str(satuan, "km") == 0 || banding_str(satuan, "kilometer") == 0) {
        return meter / 1000.0;
    }
    if (banding_str(satuan, "cm") == 0 || banding_str(satuan, "centimeter") == 0) {
        return meter * 100.0;
    }
    if (banding_str(satuan, "mm") == 0 || banding_str(satuan, "millimeter") == 0) {
        return meter * 1000.0;
    }
    if (banding_str(satuan, "mi") == 0 || banding_str(satuan, "mile") == 0 ||
        banding_str(satuan, "miles") == 0) {
        return meter / 1609.344;
    }
    if (banding_str(satuan, "yd") == 0 || banding_str(satuan, "yard") == 0 ||
        banding_str(satuan, "yards") == 0) {
        return meter / 0.9144;
    }
    if (banding_str(satuan, "ft") == 0 || banding_str(satuan, "foot") == 0 ||
        banding_str(satuan, "feet") == 0) {
        return meter / 0.3048;
    }
    if (banding_str(satuan, "in") == 0 || banding_str(satuan, "inch") == 0 ||
        banding_str(satuan, "inches") == 0) {
        return meter / 0.0254;
    }
    if (banding_str(satuan, "nm") == 0 || banding_str(satuan, "nmi") == 0 ||
        banding_str(satuan, "nauticalmile") == 0) {
        return meter / 1852.0;
    }
    if (banding_str(satuan, "ly") == 0 || banding_str(satuan, "lightyear") == 0) {
        return meter / 9.461e15;
    }
    if (banding_str(satuan, "au") == 0 || banding_str(satuan, "astronomicalunit") == 0) {
        return meter / 1.496e11;
    }
    if (banding_str(satuan, "parsec") == 0 || banding_str(satuan, "pc") == 0) {
        return meter / 3.086e16;
    }
    if (banding_str(satuan, "dm") == 0 || banding_str(satuan, "decimeter") == 0) {
        return meter * 10.0;
    }
    if (banding_str(satuan, "dam") == 0 || banding_str(satuan, "decameter") == 0) {
        return meter / 10.0;
    }
    if (banding_str(satuan, "hm") == 0 || banding_str(satuan, "hectometer") == 0) {
        return meter / 100.0;
    }
    if (banding_str(satuan, "micron") == 0 || banding_str(satuan, "um") == 0 ||
        banding_str(satuan, "micrometer") == 0) {
        return meter * 1000000.0;
    }
    if (banding_str(satuan, "angstrom") == 0 || banding_str(satuan, "ang") == 0) {
        return meter * 10000000000.0;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI BERAT/MASSA
 * ============================================================ */

/* Konversi ke gram (basis) */
static double ke_gram(double nilai, const char *satuan) {
    if (banding_str(satuan, "g") == 0 || banding_str(satuan, "gram") == 0 ||
        banding_str(satuan, "gr") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "kg") == 0 || banding_str(satuan, "kilogram") == 0) {
        return nilai * 1000.0;
    }
    if (banding_str(satuan, "mg") == 0 || banding_str(satuan, "milligram") == 0) {
        return nilai / 1000.0;
    }
    if (banding_str(satuan, "lb") == 0 || banding_str(satuan, "pound") == 0 ||
        banding_str(satuan, "lbs") == 0) {
        return nilai * 453.592;
    }
    if (banding_str(satuan, "oz") == 0 || banding_str(satuan, "ounce") == 0) {
        return nilai * 28.3495;
    }
    if (banding_str(satuan, "ton") == 0 || banding_str(satuan, "t") == 0 ||
        banding_str(satuan, "metricton") == 0) {
        return nilai * 1000000.0;
    }
    if (banding_str(satuan, "st") == 0 || banding_str(satuan, "stone") == 0) {
        return nilai * 6350.29;
    }
    if (banding_str(satuan, "slug") == 0) {
        return nilai * 14593.9;
    }
    if (banding_str(satuan, "carat") == 0 || banding_str(satuan, "ct") == 0) {
        return nilai * 0.2;
    }
    if (banding_str(satuan, "cg") == 0 || banding_str(satuan, "centigram") == 0) {
        return nilai / 100.0;
    }
    if (banding_str(satuan, "dag") == 0 || banding_str(satuan, "decagram") == 0) {
        return nilai * 10.0;
    }
    if (banding_str(satuan, "dg") == 0 || banding_str(satuan, "decigram") == 0) {
        return nilai / 10.0;
    }
    if (banding_str(satuan, "hg") == 0 || banding_str(satuan, "hectogram") == 0) {
        return nilai * 100.0;
    }
    return -1.0;
}

/* Konversi dari gram ke satuan lain */
static double dari_gram(double gram, const char *satuan) {
    if (banding_str(satuan, "g") == 0 || banding_str(satuan, "gram") == 0 ||
        banding_str(satuan, "gr") == 0) {
        return gram;
    }
    if (banding_str(satuan, "kg") == 0 || banding_str(satuan, "kilogram") == 0) {
        return gram / 1000.0;
    }
    if (banding_str(satuan, "mg") == 0 || banding_str(satuan, "milligram") == 0) {
        return gram * 1000.0;
    }
    if (banding_str(satuan, "lb") == 0 || banding_str(satuan, "pound") == 0 ||
        banding_str(satuan, "lbs") == 0) {
        return gram / 453.592;
    }
    if (banding_str(satuan, "oz") == 0 || banding_str(satuan, "ounce") == 0) {
        return gram / 28.3495;
    }
    if (banding_str(satuan, "ton") == 0 || banding_str(satuan, "t") == 0 ||
        banding_str(satuan, "metricton") == 0) {
        return gram / 1000000.0;
    }
    if (banding_str(satuan, "st") == 0 || banding_str(satuan, "stone") == 0) {
        return gram / 6350.29;
    }
    if (banding_str(satuan, "slug") == 0) {
        return gram / 14593.9;
    }
    if (banding_str(satuan, "carat") == 0 || banding_str(satuan, "ct") == 0) {
        return gram / 0.2;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI WAKTU
 * ============================================================ */

/* Konversi ke detik (basis) */
static double ke_detik(double nilai, const char *satuan) {
    if (banding_str(satuan, "s") == 0 || banding_str(satuan, "sec") == 0 ||
        banding_str(satuan, "second") == 0 || banding_str(satuan, "seconds") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "min") == 0 || banding_str(satuan, "minute") == 0 ||
        banding_str(satuan, "minutes") == 0) {
        return nilai * 60.0;
    }
    if (banding_str(satuan, "hr") == 0 || banding_str(satuan, "h") == 0 ||
        banding_str(satuan, "hour") == 0 || banding_str(satuan, "hours") == 0) {
        return nilai * 3600.0;
    }
    if (banding_str(satuan, "d") == 0 || banding_str(satuan, "day") == 0 ||
        banding_str(satuan, "days") == 0) {
        return nilai * 86400.0;
    }
    if (banding_str(satuan, "wk") == 0 || banding_str(satuan, "week") == 0 ||
        banding_str(satuan, "weeks") == 0) {
        return nilai * 604800.0;
    }
    if (banding_str(satuan, "mo") == 0 || banding_str(satuan, "month") == 0 ||
        banding_str(satuan, "months") == 0) {
        return nilai * 2592000.0; /* 30 hari */
    }
    if (banding_str(satuan, "yr") == 0 || banding_str(satuan, "year") == 0 ||
        banding_str(satuan, "years") == 0) {
        return nilai * 31536000.0; /* 365 hari */
    }
    if (banding_str(satuan, "ms") == 0 || banding_str(satuan, "msec") == 0 ||
        banding_str(satuan, "millisecond") == 0) {
        return nilai / 1000.0;
    }
    if (banding_str(satuan, "us") == 0 || banding_str(satuan, "microsecond") == 0) {
        return nilai / 1000000.0;
    }
    if (banding_str(satuan, "ns") == 0 || banding_str(satuan, "nanosecond") == 0) {
        return nilai / 1000000000.0;
    }
    return -1.0;
}

/* Konversi dari detik ke satuan lain */
static double dari_detik(double detik, const char *satuan) {
    if (banding_str(satuan, "s") == 0 || banding_str(satuan, "sec") == 0 ||
        banding_str(satuan, "second") == 0 || banding_str(satuan, "seconds") == 0) {
        return detik;
    }
    if (banding_str(satuan, "min") == 0 || banding_str(satuan, "minute") == 0 ||
        banding_str(satuan, "minutes") == 0) {
        return detik / 60.0;
    }
    if (banding_str(satuan, "hr") == 0 || banding_str(satuan, "h") == 0 ||
        banding_str(satuan, "hour") == 0 || banding_str(satuan, "hours") == 0) {
        return detik / 3600.0;
    }
    if (banding_str(satuan, "d") == 0 || banding_str(satuan, "day") == 0 ||
        banding_str(satuan, "days") == 0) {
        return detik / 86400.0;
    }
    if (banding_str(satuan, "wk") == 0 || banding_str(satuan, "week") == 0 ||
        banding_str(satuan, "weeks") == 0) {
        return detik / 604800.0;
    }
    if (banding_str(satuan, "mo") == 0 || banding_str(satuan, "month") == 0 ||
        banding_str(satuan, "months") == 0) {
        return detik / 2592000.0;
    }
    if (banding_str(satuan, "yr") == 0 || banding_str(satuan, "year") == 0 ||
        banding_str(satuan, "years") == 0) {
        return detik / 31536000.0;
    }
    if (banding_str(satuan, "ms") == 0 || banding_str(satuan, "msec") == 0 ||
        banding_str(satuan, "millisecond") == 0) {
        return detik * 1000.0;
    }
    if (banding_str(satuan, "us") == 0 || banding_str(satuan, "microsecond") == 0) {
        return detik * 1000000.0;
    }
    if (banding_str(satuan, "ns") == 0 || banding_str(satuan, "nanosecond") == 0) {
        return detik * 1000000000.0;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI VOLUME
 * ============================================================ */

/* Konversi ke liter (basis) */
static double ke_liter(double nilai, const char *satuan) {
    if (banding_str(satuan, "l") == 0 || banding_str(satuan, "L") == 0 ||
        banding_str(satuan, "liter") == 0 || banding_str(satuan, "litre") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "ml") == 0 || banding_str(satuan, "mL") == 0 ||
        banding_str(satuan, "milliliter") == 0) {
        return nilai / 1000.0;
    }
    if (banding_str(satuan, "kl") == 0 || banding_str(satuan, "kL") == 0 ||
        banding_str(satuan, "kiloliter") == 0) {
        return nilai * 1000.0;
    }
    if (banding_str(satuan, "gal") == 0 || banding_str(satuan, "gallon") == 0) {
        return nilai * 3.78541; /* US gallon */
    }
    if (banding_str(satuan, "qt") == 0 || banding_str(satuan, "quart") == 0) {
        return nilai * 0.946353;
    }
    if (banding_str(satuan, "pt") == 0 || banding_str(satuan, "pint") == 0) {
        return nilai * 0.473176;
    }
    if (banding_str(satuan, "cup") == 0 || banding_str(satuan, "cups") == 0) {
        return nilai * 0.236588;
    }
    if (banding_str(satuan, "floz") == 0 || banding_str(satuan, "fluidoz") == 0 ||
        banding_str(satuan, "fluidounce") == 0) {
        return nilai * 0.0295735;
    }
    if (banding_str(satuan, "tbsp") == 0 || banding_str(satuan, "tablespoon") == 0) {
        return nilai * 0.0147868;
    }
    if (banding_str(satuan, "tsp") == 0 || banding_str(satuan, "teaspoon") == 0) {
        return nilai * 0.00492892;
    }
    if (banding_str(satuan, "m3") == 0 || banding_str(satuan, "cubicmeter") == 0) {
        return nilai * 1000.0;
    }
    if (banding_str(satuan, "cm3") == 0 || banding_str(satuan, "cubiccm") == 0 ||
        banding_str(satuan, "cc") == 0) {
        return nilai / 1000.0;
    }
    if (banding_str(satuan, "mm3") == 0 || banding_str(satuan, "cubicmm") == 0) {
        return nilai / 1000000.0;
    }
    if (banding_str(satuan, "in3") == 0 || banding_str(satuan, "cubicin") == 0) {
        return nilai * 0.0163871;
    }
    if (banding_str(satuan, "ft3") == 0 || banding_str(satuan, "cubicft") == 0) {
        return nilai * 28.3168;
    }
    if (banding_str(satuan, "yd3") == 0 || banding_str(satuan, "cubicyd") == 0) {
        return nilai * 764.555;
    }
    if (banding_str(satuan, "ukgal") == 0 || banding_str(satuan, "imperialgallon") == 0) {
        return nilai * 4.54609;
    }
    return -1.0;
}

/* Konversi dari liter ke satuan lain */
static double dari_liter(double liter, const char *satuan) {
    if (banding_str(satuan, "l") == 0 || banding_str(satuan, "L") == 0 ||
        banding_str(satuan, "liter") == 0 || banding_str(satuan, "litre") == 0) {
        return liter;
    }
    if (banding_str(satuan, "ml") == 0 || banding_str(satuan, "mL") == 0 ||
        banding_str(satuan, "milliliter") == 0) {
        return liter * 1000.0;
    }
    if (banding_str(satuan, "kl") == 0 || banding_str(satuan, "kL") == 0 ||
        banding_str(satuan, "kiloliter") == 0) {
        return liter / 1000.0;
    }
    if (banding_str(satuan, "gal") == 0 || banding_str(satuan, "gallon") == 0) {
        return liter / 3.78541;
    }
    if (banding_str(satuan, "qt") == 0 || banding_str(satuan, "quart") == 0) {
        return liter / 0.946353;
    }
    if (banding_str(satuan, "pt") == 0 || banding_str(satuan, "pint") == 0) {
        return liter / 0.473176;
    }
    if (banding_str(satuan, "cup") == 0 || banding_str(satuan, "cups") == 0) {
        return liter / 0.236588;
    }
    if (banding_str(satuan, "floz") == 0 || banding_str(satuan, "fluidoz") == 0 ||
        banding_str(satuan, "fluidounce") == 0) {
        return liter / 0.0295735;
    }
    if (banding_str(satuan, "tbsp") == 0 || banding_str(satuan, "tablespoon") == 0) {
        return liter / 0.0147868;
    }
    if (banding_str(satuan, "tsp") == 0 || banding_str(satuan, "teaspoon") == 0) {
        return liter / 0.00492892;
    }
    if (banding_str(satuan, "m3") == 0 || banding_str(satuan, "cubicmeter") == 0) {
        return liter / 1000.0;
    }
    if (banding_str(satuan, "cm3") == 0 || banding_str(satuan, "cubiccm") == 0 ||
        banding_str(satuan, "cc") == 0) {
        return liter * 1000.0;
    }
    if (banding_str(satuan, "mm3") == 0 || banding_str(satuan, "cubicmm") == 0) {
        return liter * 1000000.0;
    }
    if (banding_str(satuan, "in3") == 0 || banding_str(satuan, "cubicin") == 0) {
        return liter / 0.0163871;
    }
    if (banding_str(satuan, "ft3") == 0 || banding_str(satuan, "cubicft") == 0) {
        return liter / 28.3168;
    }
    if (banding_str(satuan, "yd3") == 0 || banding_str(satuan, "cubicyd") == 0) {
        return liter / 764.555;
    }
    if (banding_str(satuan, "ukgal") == 0 || banding_str(satuan, "imperialgallon") == 0) {
        return liter / 4.54609;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI AREA
 * ============================================================ */

/* Konversi ke meter persegi (basis) */
static double ke_m2(double nilai, const char *satuan) {
    if (banding_str(satuan, "m2") == 0 || banding_str(satuan, "sqm") == 0 ||
        banding_str(satuan, "squaremeter") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "km2") == 0 || banding_str(satuan, "sqkm") == 0) {
        return nilai * 1000000.0;
    }
    if (banding_str(satuan, "cm2") == 0 || banding_str(satuan, "sqcm") == 0) {
        return nilai / 10000.0;
    }
    if (banding_str(satuan, "mm2") == 0 || banding_str(satuan, "sqmm") == 0) {
        return nilai / 1000000.0;
    }
    if (banding_str(satuan, "ha") == 0 || banding_str(satuan, "hectare") == 0) {
        return nilai * 10000.0;
    }
    if (banding_str(satuan, "ac") == 0 || banding_str(satuan, "acre") == 0) {
        return nilai * 4046.86;
    }
    if (banding_str(satuan, "ft2") == 0 || banding_str(satuan, "sqft") == 0 ||
        banding_str(satuan, "squarefeet") == 0) {
        return nilai * 0.092903;
    }
    if (banding_str(satuan, "in2") == 0 || banding_str(satuan, "sqin") == 0) {
        return nilai * 0.00064516;
    }
    if (banding_str(satuan, "yd2") == 0 || banding_str(satuan, "sqyd") == 0) {
        return nilai * 0.836127;
    }
    if (banding_str(satuan, "mi2") == 0 || banding_str(satuan, "sqmi") == 0) {
        return nilai * 2589988.11;
    }
    return -1.0;
}

/* Konversi dari meter persegi ke satuan lain */
static double dari_m2(double m2, const char *satuan) {
    if (banding_str(satuan, "m2") == 0 || banding_str(satuan, "sqm") == 0 ||
        banding_str(satuan, "squaremeter") == 0) {
        return m2;
    }
    if (banding_str(satuan, "km2") == 0 || banding_str(satuan, "sqkm") == 0) {
        return m2 / 1000000.0;
    }
    if (banding_str(satuan, "cm2") == 0 || banding_str(satuan, "sqcm") == 0) {
        return m2 * 10000.0;
    }
    if (banding_str(satuan, "mm2") == 0 || banding_str(satuan, "sqmm") == 0) {
        return m2 * 1000000.0;
    }
    if (banding_str(satuan, "ha") == 0 || banding_str(satuan, "hectare") == 0) {
        return m2 / 10000.0;
    }
    if (banding_str(satuan, "ac") == 0 || banding_str(satuan, "acre") == 0) {
        return m2 / 4046.86;
    }
    if (banding_str(satuan, "ft2") == 0 || banding_str(satuan, "sqft") == 0 ||
        banding_str(satuan, "squarefeet") == 0) {
        return m2 / 0.092903;
    }
    if (banding_str(satuan, "in2") == 0 || banding_str(satuan, "sqin") == 0) {
        return m2 / 0.00064516;
    }
    if (banding_str(satuan, "yd2") == 0 || banding_str(satuan, "sqyd") == 0) {
        return m2 / 0.836127;
    }
    if (banding_str(satuan, "mi2") == 0 || banding_str(satuan, "sqmi") == 0) {
        return m2 / 2589988.11;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI KECEPATAN
 * ============================================================ */

/* Konversi ke m/s (basis) */
static double ke_mps(double nilai, const char *satuan) {
    if (banding_str(satuan, "m/s") == 0 || banding_str(satuan, "mps") == 0 ||
        banding_str(satuan, "meterpersecond") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "km/h") == 0 || banding_str(satuan, "kph") == 0 ||
        banding_str(satuan, "kilometerperhour") == 0) {
        return nilai / 3.6;
    }
    if (banding_str(satuan, "mph") == 0 || banding_str(satuan, "mileperhour") == 0) {
        return nilai * 0.44704;
    }
    if (banding_str(satuan, "knot") == 0 || banding_str(satuan, "kn") == 0) {
        return nilai * 0.514444;
    }
    if (banding_str(satuan, "ft/s") == 0 || banding_str(satuan, "fps") == 0 ||
        banding_str(satuan, "footpersecond") == 0) {
        return nilai * 0.3048;
    }
    if (banding_str(satuan, "mach") == 0 || banding_str(satuan, "ma") == 0) {
        return nilai * 340.29; /* Pada tingkat laut, 20°C */
    }
    if (banding_str(satuan, "c") == 0 || banding_str(satuan, "speedoflight") == 0) {
        return nilai * 299792458.0;
    }
    return -1.0;
}

/* Konversi dari m/s ke satuan lain */
static double dari_mps(double mps, const char *satuan) {
    if (banding_str(satuan, "m/s") == 0 || banding_str(satuan, "mps") == 0 ||
        banding_str(satuan, "meterpersecond") == 0) {
        return mps;
    }
    if (banding_str(satuan, "km/h") == 0 || banding_str(satuan, "kph") == 0 ||
        banding_str(satuan, "kilometerperhour") == 0) {
        return mps * 3.6;
    }
    if (banding_str(satuan, "mph") == 0 || banding_str(satuan, "mileperhour") == 0) {
        return mps / 0.44704;
    }
    if (banding_str(satuan, "knot") == 0 || banding_str(satuan, "kn") == 0) {
        return mps / 0.514444;
    }
    if (banding_str(satuan, "ft/s") == 0 || banding_str(satuan, "fps") == 0 ||
        banding_str(satuan, "footpersecond") == 0) {
        return mps / 0.3048;
    }
    if (banding_str(satuan, "mach") == 0 || banding_str(satuan, "ma") == 0) {
        return mps / 340.29;
    }
    if (banding_str(satuan, "c") == 0 || banding_str(satuan, "speedoflight") == 0) {
        return mps / 299792458.0;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI TEKANAN
 * ============================================================ */

/* Konversi ke Pascal (basis) */
static double ke_pa(double nilai, const char *satuan) {
    if (banding_str(satuan, "Pa") == 0 || banding_str(satuan, "pascal") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "kPa") == 0 || banding_str(satuan, "kilopascal") == 0) {
        return nilai * 1000.0;
    }
    if (banding_str(satuan, "MPa") == 0 || banding_str(satuan, "megapascal") == 0) {
        return nilai * 1000000.0;
    }
    if (banding_str(satuan, "bar") == 0) {
        return nilai * 100000.0;
    }
    if (banding_str(satuan, "mbar") == 0 || banding_str(satuan, "millibar") == 0) {
        return nilai * 100.0;
    }
    if (banding_str(satuan, "psi") == 0 || banding_str(satuan, "lb/in2") == 0) {
        return nilai * 6894.76;
    }
    if (banding_str(satuan, "atm") == 0 || banding_str(satuan, "atmosphere") == 0) {
        return nilai * 101325.0;
    }
    if (banding_str(satuan, "mmHg") == 0 || banding_str(satuan, "torr") == 0) {
        return nilai * 133.322;
    }
    if (banding_str(satuan, "inHg") == 0) {
        return nilai * 3386.39;
    }
    return -1.0;
}

/* Konversi dari Pascal ke satuan lain */
static double dari_pa(double pa, const char *satuan) {
    if (banding_str(satuan, "Pa") == 0 || banding_str(satuan, "pascal") == 0) {
        return pa;
    }
    if (banding_str(satuan, "kPa") == 0 || banding_str(satuan, "kilopascal") == 0) {
        return pa / 1000.0;
    }
    if (banding_str(satuan, "MPa") == 0 || banding_str(satuan, "megapascal") == 0) {
        return pa / 1000000.0;
    }
    if (banding_str(satuan, "bar") == 0) {
        return pa / 100000.0;
    }
    if (banding_str(satuan, "mbar") == 0 || banding_str(satuan, "millibar") == 0) {
        return pa / 100.0;
    }
    if (banding_str(satuan, "psi") == 0 || banding_str(satuan, "lb/in2") == 0) {
        return pa / 6894.76;
    }
    if (banding_str(satuan, "atm") == 0 || banding_str(satuan, "atmosphere") == 0) {
        return pa / 101325.0;
    }
    if (banding_str(satuan, "mmHg") == 0 || banding_str(satuan, "torr") == 0) {
        return pa / 133.322;
    }
    if (banding_str(satuan, "inHg") == 0) {
        return pa / 3386.39;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI ENERGI
 * ============================================================ */

/* Konversi ke Joule (basis) */
static double ke_joule(double nilai, const char *satuan) {
    if (banding_str(satuan, "J") == 0 || banding_str(satuan, "joule") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "kJ") == 0 || banding_str(satuan, "kilojoule") == 0) {
        return nilai * 1000.0;
    }
    if (banding_str(satuan, "MJ") == 0 || banding_str(satuan, "megajoule") == 0) {
        return nilai * 1000000.0;
    }
    if (banding_str(satuan, "cal") == 0 || banding_str(satuan, "calorie") == 0) {
        return nilai * 4.184;
    }
    if (banding_str(satuan, "kcal") == 0 || banding_str(satuan, "kilocalorie") == 0 ||
        banding_str(satuan, "Cal") == 0) {
        return nilai * 4184.0;
    }
    if (banding_str(satuan, "Wh") == 0 || banding_str(satuan, "watthour") == 0) {
        return nilai * 3600.0;
    }
    if (banding_str(satuan, "kWh") == 0 || banding_str(satuan, "kilowatthour") == 0) {
        return nilai * 3600000.0;
    }
    if (banding_str(satuan, "eV") == 0 || banding_str(satuan, "electronvolt") == 0) {
        return nilai * 1.602176634e-19;
    }
    if (banding_str(satuan, "BTU") == 0 || banding_str(satuan, "btu") == 0) {
        return nilai * 1055.06;
    }
    if (banding_str(satuan, "erg") == 0) {
        return nilai * 1e-7;
    }
    return -1.0;
}

/* Konversi dari Joule ke satuan lain */
static double dari_joule(double joule, const char *satuan) {
    if (banding_str(satuan, "J") == 0 || banding_str(satuan, "joule") == 0) {
        return joule;
    }
    if (banding_str(satuan, "kJ") == 0 || banding_str(satuan, "kilojoule") == 0) {
        return joule / 1000.0;
    }
    if (banding_str(satuan, "MJ") == 0 || banding_str(satuan, "megajoule") == 0) {
        return joule / 1000000.0;
    }
    if (banding_str(satuan, "cal") == 0 || banding_str(satuan, "calorie") == 0) {
        return joule / 4.184;
    }
    if (banding_str(satuan, "kcal") == 0 || banding_str(satuan, "kilocalorie") == 0 ||
        banding_str(satuan, "Cal") == 0) {
        return joule / 4184.0;
    }
    if (banding_str(satuan, "Wh") == 0 || banding_str(satuan, "watthour") == 0) {
        return joule / 3600.0;
    }
    if (banding_str(satuan, "kWh") == 0 || banding_str(satuan, "kilowatthour") == 0) {
        return joule / 3600000.0;
    }
    if (banding_str(satuan, "eV") == 0 || banding_str(satuan, "electronvolt") == 0) {
        return joule / 1.602176634e-19;
    }
    if (banding_str(satuan, "BTU") == 0 || banding_str(satuan, "btu") == 0) {
        return joule / 1055.06;
    }
    if (banding_str(satuan, "erg") == 0) {
        return joule / 1e-7;
    }
    return -1.0;
}

/* ============================================================
 * KONVERSI DAYA
 * ============================================================ */

/* Konversi ke Watt (basis) */
static double ke_watt(double nilai, const char *satuan) {
    if (banding_str(satuan, "W") == 0 || banding_str(satuan, "watt") == 0) {
        return nilai;
    }
    if (banding_str(satuan, "kW") == 0 || banding_str(satuan, "kilowatt") == 0) {
        return nilai * 1000.0;
    }
    if (banding_str(satuan, "MW") == 0 || banding_str(satuan, "megawatt") == 0) {
        return nilai * 1000000.0;
    }
    if (banding_str(satuan, "hp") == 0 || banding_str(satuan, "horsepower") == 0) {
        return nilai * 745.7; /* Mechanical horsepower */
    }
    if (banding_str(satuan, "PS") == 0 || banding_str(satuan, "hpm") == 0) {
        return nilai * 735.5; /* Metric horsepower */
    }
    if (banding_str(satuan, "mW") == 0 || banding_str(satuan, "milliwatt") == 0) {
        return nilai / 1000.0;
    }
    return -1.0;
}

/* Konversi dari Watt ke satuan lain */
static double dari_watt(double watt, const char *satuan) {
    if (banding_str(satuan, "W") == 0 || banding_str(satuan, "watt") == 0) {
        return watt;
    }
    if (banding_str(satuan, "kW") == 0 || banding_str(satuan, "kilowatt") == 0) {
        return watt / 1000.0;
    }
    if (banding_str(satuan, "MW") == 0 || banding_str(satuan, "megawatt") == 0) {
        return watt / 1000000.0;
    }
    if (banding_str(satuan, "hp") == 0 || banding_str(satuan, "horsepower") == 0) {
        return watt / 745.7;
    }
    if (banding_str(satuan, "PS") == 0 || banding_str(satuan, "hpm") == 0) {
        return watt / 735.5;
    }
    if (banding_str(satuan, "mW") == 0 || banding_str(satuan, "milliwatt") == 0) {
        return watt * 1000.0;
    }
    return -1.0;
}

/* ============================================================
 * FUNGSI KONVERSI BASIS BILANGAN
 * ============================================================ */

/* DEC2BIN: Desimal ke Biner */
int konversi_dec2bin(double angka, char *keluaran, size_t ukuran) {
    static const char digits[] = "01";
    long long num;
    char buffer[65];
    int idx = 64;

    if (!keluaran || ukuran < 2) return -1;
    num = (long long)angka;
    if (num < 0) return -1;

    buffer[idx] = '\0';
    if (num == 0) {
        buffer[--idx] = '0';
    } else {
        while (num > 0) {
            buffer[--idx] = digits[num % 2];
            num /= 2;
        }
    }

    strncpy(keluaran, buffer + idx, ukuran - 1);
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* DEC2OCT: Desimal ke Oktal */
int konversi_dec2oct(double angka, char *keluaran, size_t ukuran) {
    static const char digits[] = "01234567";
    long long num;
    char buffer[65];
    int idx = 64;

    if (!keluaran || ukuran < 2) return -1;
    num = (long long)angka;
    if (num < 0) return -1;

    buffer[idx] = '\0';
    if (num == 0) {
        buffer[--idx] = '0';
    } else {
        while (num > 0) {
            buffer[--idx] = digits[num % 8];
            num /= 8;
        }
    }

    strncpy(keluaran, buffer + idx, ukuran - 1);
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* DEC2HEX: Desimal ke Heksadesimal */
int konversi_dec2hex(double angka, char *keluaran, size_t ukuran) {
    static const char digits[] = "0123456789ABCDEF";
    long long num;
    char buffer[65];
    int idx = 64;

    if (!keluaran || ukuran < 2) return -1;
    num = (long long)angka;
    if (num < 0) return -1;

    buffer[idx] = '\0';
    if (num == 0) {
        buffer[--idx] = '0';
    } else {
        while (num > 0) {
            buffer[--idx] = digits[num % 16];
            num /= 16;
        }
    }

    strncpy(keluaran, buffer + idx, ukuran - 1);
    keluaran[ukuran - 1] = '\0';
    return 0;
}

/* BIN2DEC: Biner ke Desimal */
int konversi_bin2dec(const char *biner, double *keluaran) {
    long long result = 0;
    int i;

    if (!biner || !keluaran) return -1;

    for (i = 0; biner[i]; i++) {
        char c = biner[i];
        if (c != '0' && c != '1') return -1;
        result = result * 2 + (c - '0');
    }

    *keluaran = (double)result;
    return 0;
}

/* OCT2DEC: Oktal ke Desimal */
int konversi_oct2dec(const char *oktal, double *keluaran) {
    long long result = 0;
    int i;

    if (!oktal || !keluaran) return -1;

    for (i = 0; oktal[i]; i++) {
        char c = oktal[i];
        if (c < '0' || c > '7') return -1;
        result = result * 8 + (c - '0');
    }

    *keluaran = (double)result;
    return 0;
}

/* HEX2DEC: Heksadesimal ke Desimal */
int konversi_hex2dec(const char *hexa, double *keluaran) {
    long long result = 0;
    int i;

    if (!hexa || !keluaran) return -1;

    for (i = 0; hexa[i]; i++) {
        char c = (char)toupper((unsigned char)hexa[i]);
        int digit;

        if (c >= '0' && c <= '9') {
            digit = c - '0';
        } else if (c >= 'A' && c <= 'F') {
            digit = c - 'A' + 10;
        } else {
            return -1;
        }
        result = result * 16 + digit;
    }

    *keluaran = (double)result;
    return 0;
}

/* BIN2OCT: Biner ke Oktal */
int konversi_bin2oct(const char *biner, char *keluaran, size_t ukuran) {
    double dec;
    if (konversi_bin2dec(biner, &dec) != 0) return -1;
    return konversi_dec2oct(dec, keluaran, ukuran);
}

/* BIN2HEX: Biner ke Heksadesimal */
int konversi_bin2hex(const char *biner, char *keluaran, size_t ukuran) {
    double dec;
    if (konversi_bin2dec(biner, &dec) != 0) return -1;
    return konversi_dec2hex(dec, keluaran, ukuran);
}

/* OCT2BIN: Oktal ke Biner */
int konversi_oct2bin(const char *oktal, char *keluaran, size_t ukuran) {
    double dec;
    if (konversi_oct2dec(oktal, &dec) != 0) return -1;
    return konversi_dec2bin(dec, keluaran, ukuran);
}

/* OCT2HEX: Oktal ke Heksadesimal */
int konversi_oct2hex(const char *oktal, char *keluaran, size_t ukuran) {
    double dec;
    if (konversi_oct2dec(oktal, &dec) != 0) return -1;
    return konversi_dec2hex(dec, keluaran, ukuran);
}

/* HEX2BIN: Heksadesimal ke Biner */
int konversi_hex2bin(const char *hexa, char *keluaran, size_t ukuran) {
    double dec;
    if (konversi_hex2dec(hexa, &dec) != 0) return -1;
    return konversi_dec2bin(dec, keluaran, ukuran);
}

/* HEX2OCT: Heksadesimal ke Oktal */
int konversi_hex2oct(const char *hexa, char *keluaran, size_t ukuran) {
    double dec;
    if (konversi_hex2dec(hexa, &dec) != 0) return -1;
    return konversi_dec2oct(dec, keluaran, ukuran);
}

/* ============================================================
 * FUNGSI CONVERT UTAMA
 * ============================================================ */

/* CONVERT: Konversi satuan umum */
int konversi_satuan(double nilai, const char *dari, const char *ke,
                    double *keluaran) {
    double base;
    double result;

    if (!dari || !ke || !keluaran) return -1;

    /* Suhu - kategori khusus */
    if (banding_str(dari, "C") == 0 || banding_str(dari, "Celsius") == 0 ||
        banding_str(dari, "degC") == 0) {
        if (banding_str(ke, "F") == 0 || banding_str(ke, "Fahrenheit") == 0 ||
            banding_str(ke, "degF") == 0) {
            *keluaran = c_to_f(nilai);
            return 0;
        }
        if (banding_str(ke, "K") == 0 || banding_str(ke, "Kelvin") == 0) {
            *keluaran = c_to_k(nilai);
            return 0;
        }
        if (banding_str(ke, "Rank") == 0 || banding_str(ke, "Rankine") == 0) {
            *keluaran = c_to_rank(nilai);
            return 0;
        }
    }
    if (banding_str(dari, "F") == 0 || banding_str(dari, "Fahrenheit") == 0 ||
        banding_str(dari, "degF") == 0) {
        if (banding_str(ke, "C") == 0 || banding_str(ke, "Celsius") == 0 ||
            banding_str(ke, "degC") == 0) {
            *keluaran = f_to_c(nilai);
            return 0;
        }
        if (banding_str(ke, "K") == 0 || banding_str(ke, "Kelvin") == 0) {
            *keluaran = f_to_k(nilai);
            return 0;
        }
    }
    if (banding_str(dari, "K") == 0 || banding_str(dari, "Kelvin") == 0) {
        if (banding_str(ke, "C") == 0 || banding_str(ke, "Celsius") == 0 ||
            banding_str(ke, "degC") == 0) {
            *keluaran = k_to_c(nilai);
            return 0;
        }
        if (banding_str(ke, "F") == 0 || banding_str(ke, "Fahrenheit") == 0 ||
            banding_str(ke, "degF") == 0) {
            *keluaran = k_to_f(nilai);
            return 0;
        }
    }

    /* Panjang */
    base = ke_meter(nilai, dari);
    if (base >= 0.0) {
        result = dari_meter(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Berat/Massa */
    base = ke_gram(nilai, dari);
    if (base >= 0.0) {
        result = dari_gram(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Waktu */
    base = ke_detik(nilai, dari);
    if (base >= 0.0) {
        result = dari_detik(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Volume */
    base = ke_liter(nilai, dari);
    if (base >= 0.0) {
        result = dari_liter(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Area */
    base = ke_m2(nilai, dari);
    if (base >= 0.0) {
        result = dari_m2(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Kecepatan */
    base = ke_mps(nilai, dari);
    if (base >= 0.0) {
        result = dari_mps(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Tekanan */
    base = ke_pa(nilai, dari);
    if (base >= 0.0) {
        result = dari_pa(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Energi */
    base = ke_joule(nilai, dari);
    if (base >= 0.0) {
        result = dari_joule(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    /* Daya */
    base = ke_watt(nilai, dari);
    if (base >= 0.0) {
        result = dari_watt(base, ke);
        if (result >= 0.0) {
            *keluaran = result;
            return 0;
        }
    }

    return -1; /* Satuan tidak dikenal */
}

/* ============================================================
 * FUNGSI KONVERSI MATA UANG
 * ============================================================ */

/* Placeholder untuk konversi mata uang
 * Implementasi nyata memerlukan API atau database kurs
 */
int konversi_mata_uang(const char *dari, double nilai, const char *ke,
                       double *keluaran) {
    /* Kurs statis (placeholder - dalam implementasi nyata gunakan API) */
    static const struct {
        const char *kode;
        double ke_usd; /* Nilai tukar ke USD */
    } kurs[] = {
        {"USD", 1.0},
        {"IDR", 0.000065},  /* 1 IDR = 0.000065 USD */
        {"EUR", 1.08},
        {"GBP", 1.27},
        {"JPY", 0.0067},
        {"CNY", 0.14},
        {"SGD", 0.74},
        {"AUD", 0.65},
        {"CAD", 0.74},
        {"CHF", 1.12},
        {"MYR", 0.21},
        {"THB", 0.028},
        {"KRW", 0.00075},
        {"INR", 0.012},
        {"PHP", 0.018},
        {"VND", 0.00004},
        {NULL, 0.0}
    };

    double dari_rate = 0.0, ke_rate = 0.0;
    int i;

    if (!dari || !ke || !keluaran) return -1;

    /* Cari kurs dari */
    for (i = 0; kurs[i].kode != NULL; i++) {
        if (banding_str(dari, kurs[i].kode) == 0) {
            dari_rate = kurs[i].ke_usd;
            break;
        }
    }

    /* Cari kurs ke */
    for (i = 0; kurs[i].kode != NULL; i++) {
        if (banding_str(ke, kurs[i].kode) == 0) {
            ke_rate = kurs[i].ke_usd;
            break;
        }
    }

    if (dari_rate == 0.0 || ke_rate == 0.0) return -1;

    /* Konversi: nilai * dari_rate / ke_rate */
    *keluaran = nilai * dari_rate / ke_rate;
    return 0;
}

/* ============================================================
 * DISPATCHER FUNGSI KONVERSI
 * ============================================================ */

/* Evaluasi fungsi konversi berdasarkan nama */
int konversi_eval(const char *nama, const double *args, int nargs,
                  const char *teks_arg, char *teks_keluaran, size_t ukuran,
                  double *keluaran) {
    if (!nama) return -1;

    /* DEC2BIN */
    if (banding_str(nama, "DEC2BIN") == 0) {
        if (nargs < 1 || !teks_keluaran) return -1;
        return konversi_dec2bin(args[0], teks_keluaran, ukuran);
    }

    /* DEC2OCT */
    if (banding_str(nama, "DEC2OCT") == 0) {
        if (nargs < 1 || !teks_keluaran) return -1;
        return konversi_dec2oct(args[0], teks_keluaran, ukuran);
    }

    /* DEC2HEX */
    if (banding_str(nama, "DEC2HEX") == 0) {
        if (nargs < 1 || !teks_keluaran) return -1;
        return konversi_dec2hex(args[0], teks_keluaran, ukuran);
    }

    /* BIN2DEC */
    if (banding_str(nama, "BIN2DEC") == 0) {
        if (!teks_arg || !keluaran) return -1;
        return konversi_bin2dec(teks_arg, keluaran);
    }

    /* OCT2DEC */
    if (banding_str(nama, "OCT2DEC") == 0) {
        if (!teks_arg || !keluaran) return -1;
        return konversi_oct2dec(teks_arg, keluaran);
    }

    /* HEX2DEC */
    if (banding_str(nama, "HEX2DEC") == 0) {
        if (!teks_arg || !keluaran) return -1;
        return konversi_hex2dec(teks_arg, keluaran);
    }

    /* BIN2OCT */
    if (banding_str(nama, "BIN2OCT") == 0) {
        if (!teks_arg || !teks_keluaran) return -1;
        return konversi_bin2oct(teks_arg, teks_keluaran, ukuran);
    }

    /* BIN2HEX */
    if (banding_str(nama, "BIN2HEX") == 0) {
        if (!teks_arg || !teks_keluaran) return -1;
        return konversi_bin2hex(teks_arg, teks_keluaran, ukuran);
    }

    /* OCT2BIN */
    if (banding_str(nama, "OCT2BIN") == 0) {
        if (!teks_arg || !teks_keluaran) return -1;
        return konversi_oct2bin(teks_arg, teks_keluaran, ukuran);
    }

    /* OCT2HEX */
    if (banding_str(nama, "OCT2HEX") == 0) {
        if (!teks_arg || !teks_keluaran) return -1;
        return konversi_oct2hex(teks_arg, teks_keluaran, ukuran);
    }

    /* HEX2BIN */
    if (banding_str(nama, "HEX2BIN") == 0) {
        if (!teks_arg || !teks_keluaran) return -1;
        return konversi_hex2bin(teks_arg, teks_keluaran, ukuran);
    }

    /* HEX2OCT */
    if (banding_str(nama, "HEX2OCT") == 0) {
        if (!teks_arg || !teks_keluaran) return -1;
        return konversi_hex2oct(teks_arg, teks_keluaran, ukuran);
    }

    return -1;
}
