/* ============================================================
 * TABEL v3.0
 * Berkas: keuangan.c - Formula Fungsi Keuangan (Financial)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================
 * Fungsi keuangan yang didukung:
 * - PV   : Present Value (Nilai Sekarang)
 * - FV   : Future Value (Nilai Masa Depan)
 * - PMT  : Payment (Pembayaran Periodik)
 * - RATE : Interest Rate (Suku Bunga)
 * - NPER : Number of Periods (Jumlah Periode)
 * - NPV  : Net Present Value
 * - IRR  : Internal Rate of Return
 * - SLN  : Straight-Line Depreciation
 * - DDB  : Double-Declining Balance Depreciation
 * - SYD  : Sum-of-Years-Digits Depreciation
 * - IPMT : Interest Payment
 * - PPMT : Principal Payment
 * - CUMIPMT : Cumulative Interest Payment
 * - CUMPRINC : Cumulative Principal Payment
 * - DB   : Declining Balance Depreciation
 * - VDB  : Variable Declining Balance
 * - MIRR : Modified Internal Rate of Return
 * - XIRR : Internal Rate of Return for irregular cash flows
 * - XNPV : Net Present Value for irregular cash flows
 * ============================================================ */

#include "../tabel.h"

/* ============================================================
 * KONSTANTA
 * ============================================================ */
#define KEUANGAN_TOLERANSI    1e-10
#define KEUANGAN_MAKS_ITERASI 1000

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
static double keuangan_abs(double x) {
    return (x < 0.0) ? -x : x;
}

/* Pangkat integer */
static double keuangan_pow_int(double base, int exp) {
    double hasil = 1.0;
    int i;
    int negatif = exp < 0;

    if (negatif) exp = -exp;
    for (i = 0; i < exp; i++) {
        hasil *= base;
    }
    return negatif ? (1.0 / hasil) : hasil;
}

/* ============================================================
 * FUNGSI NILAI WAKTU UANG (Time Value of Money)
 * ============================================================ */

/* PV: Present Value (Nilai Sekarang)
 * PV(rate, nper, pmt, [fv], [type])
 * type: 0 = pembayaran di akhir periode, 1 = di awal
 */
int keuangan_pv(double rate, double nper, double pmt, double fv,
                int type, double *keluaran) {
    double pv;
    double faktor;

    if (!keluaran) return -1;
    if (nper <= 0.0) return -1;

    if (keuangan_abs(rate) < KEUANGAN_TOLERANSI) {
        /* Tanpa bunga */
        pv = -fv - pmt * nper;
    } else {
        faktor = keuangan_pow_int(1.0 + rate, (int)nper);
        /* PV = -(FV + PMT * ((1 + rate)^n - 1) / rate * (1 + rate*type)) / (1 + rate)^n */
        if (type == 1) {
            pv = -(fv + pmt * (faktor - 1.0) / rate * (1.0 + rate)) / faktor;
        } else {
            pv = -(fv + pmt * (faktor - 1.0) / rate) / faktor;
        }
    }

    *keluaran = pv;
    return 0;
}

/* FV: Future Value (Nilai Masa Depan)
 * FV(rate, nper, pmt, [pv], [type])
 */
int keuangan_fv(double rate, double nper, double pmt, double pv,
                int type, double *keluaran) {
    double fv;
    double faktor;

    if (!keluaran) return -1;
    if (nper <= 0.0) return -1;

    if (keuangan_abs(rate) < KEUANGAN_TOLERANSI) {
        fv = -pv - pmt * nper;
    } else {
        faktor = keuangan_pow_int(1.0 + rate, (int)nper);
        if (type == 1) {
            fv = -pv * faktor - pmt * (faktor - 1.0) / rate * (1.0 + rate);
        } else {
            fv = -pv * faktor - pmt * (faktor - 1.0) / rate;
        }
    }

    *keluaran = fv;
    return 0;
}

/* PMT: Payment (Pembayaran Periodik)
 * PMT(rate, nper, pv, [fv], [type])
 */
int keuangan_pmt(double rate, double nper, double pv, double fv,
                 int type, double *keluaran) {
    double pmt;
    double faktor;

    if (!keluaran) return -1;
    if (nper <= 0.0) return -1;

    if (keuangan_abs(rate) < KEUANGAN_TOLERANSI) {
        pmt = -(pv + fv) / nper;
    } else {
        faktor = keuangan_pow_int(1.0 + rate, (int)nper);
        if (type == 1) {
            pmt = -(pv * faktor + fv) * rate / ((faktor - 1.0) * (1.0 + rate));
        } else {
            pmt = -(pv * faktor + fv) * rate / (faktor - 1.0);
        }
    }

    *keluaran = pmt;
    return 0;
}

/* RATE: Suku Bunga per Periode
 * RATE(nper, pmt, pv, [fv], [type], [guess])
 * Menggunakan metode Newton-Raphson
 */
int keuangan_rate(double nper, double pmt, double pv, double fv,
                  int type, double guess, double *keluaran) {
    double rate;
    double y, dy;
    double faktor;
    int i;

    if (!keluaran) return -1;
    if (nper <= 0.0) return -1;

    rate = guess;
    if (keuangan_abs(rate) < KEUANGAN_TOLERANSI) rate = 0.1;

    for (i = 0; i < KEUANGAN_MAKS_ITERASI; i++) {
        double fn, dfn;

        if (keuangan_abs(rate) < KEUANGAN_TOLERANSI) {
            fn = pv + pmt * nper + fv;
            dfn = 0.0;
        } else {
            int n = (int)nper;
            faktor = keuangan_pow_int(1.0 + rate, n);

            if (type == 1) {
                fn = pv * faktor + pmt * (faktor - 1.0) / rate * (1.0 + rate) + fv;
                dfn = pv * n * faktor / (1.0 + rate) +
                      pmt * ((n * faktor * (1.0 + rate) / rate - (faktor - 1.0) * (1.0 + 2.0 * rate)) /
                             (rate * rate * (1.0 + rate)));
            } else {
                fn = pv * faktor + pmt * (faktor - 1.0) / rate + fv;
                dfn = pv * n * faktor / (1.0 + rate) +
                      pmt * (n * faktor / rate - (faktor - 1.0) / (rate * rate));
            }
        }

        if (keuangan_abs(dfn) < KEUANGAN_TOLERANSI) break;

        y = fn;
        dy = dfn;
        rate = rate - y / dy;

        if (keuangan_abs(y) < KEUANGAN_TOLERANSI) break;
    }

    *keluaran = rate;
    return 0;
}

/* NPER: Jumlah Periode
 * NPER(rate, pmt, pv, [fv], [type])
 */
int keuangan_nper(double rate, double pmt, double pv, double fv,
                  int type, double *keluaran) {
    double nper;

    if (!keluaran) return -1;

    if (keuangan_abs(rate) < KEUANGAN_TOLERANSI) {
        if (keuangan_abs(pmt) < KEUANGAN_TOLERANSI) return -1;
        nper = -(pv + fv) / pmt;
    } else {
        double a, b;
        if (type == 1) {
            a = pmt * (1.0 + rate) / rate;
        } else {
            a = pmt / rate;
        }
        b = fv - a;
        a = -pv - a;

        if (keuangan_abs(a) < KEUANGAN_TOLERANSI ||
            keuangan_abs(b) < KEUANGAN_TOLERANSI) return -1;

        nper = log(b / a) / log(1.0 + rate);
    }

    if (nper < 0.0) return -1;

    *keluaran = nper;
    return 0;
}

/* ============================================================
 * FUNGSI NPV DAN IRR
 * ============================================================ */

/* Struktur untuk menyimpan cash flow */
struct cash_flow {
    double nilai;
    int periode;
};

/* NPV: Net Present Value
 * NPV(rate, nilai1, nilai2, ...)
 * rate: discount rate per periode
 */
int keuangan_npv(double rate, const double *cashflows, int jumlah,
                 double *keluaran) {
    double npv = 0.0;
    int i;

    if (!cashflows || !keluaran || jumlah <= 0) return -1;

    for (i = 0; i < jumlah; i++) {
        double faktor = keuangan_pow_int(1.0 + rate, i + 1);
        npv += cashflows[i] / faktor;
    }

    *keluaran = npv;
    return 0;
}

/* NPV dengan periode berbeda (XNPV) */
int keuangan_xnpv(double rate, const double *cashflows, const double *dates,
                  int jumlah, double *keluaran) {
    double xnpv = 0.0;
    double tanggal_awal;
    int i;

    if (!cashflows || !dates || !keluaran || jumlah <= 0) return -1;

    tanggal_awal = dates[0];

    for (i = 0; i < jumlah; i++) {
        double hari = dates[i] - tanggal_awal;
        double faktor = hari / 365.0;
        xnpv += cashflows[i] / keuangan_pow_int(1.0 + rate, (int)(faktor * 100)) / 100.0;
    }

    *keluaran = xnpv;
    return 0;
}

/* IRR: Internal Rate of Return
 * IRR(nilai1, nilai2, ..., [guess])
 * Menggunakan metode Newton-Raphson
 */
int keuangan_irr(const double *cashflows, int jumlah, double guess,
                 double *keluaran) {
    double irr;
    int i, iter;

    if (!cashflows || !keluaran || jumlah <= 1) return -1;

    irr = guess;
    if (keuangan_abs(irr) < KEUANGAN_TOLERANSI) irr = 0.1;

    for (iter = 0; iter < KEUANGAN_MAKS_ITERASI; iter++) {
        double npv = 0.0;
        double dnpv = 0.0;

        for (i = 0; i < jumlah; i++) {
            double faktor = keuangan_pow_int(1.0 + irr, i);
            npv += cashflows[i] / faktor;
            if (i > 0) {
                dnpv -= (double)i * cashflows[i] / keuangan_pow_int(1.0 + irr, i + 1);
            }
        }

        if (keuangan_abs(npv) < KEUANGAN_TOLERANSI) break;
        if (keuangan_abs(dnpv) < KEUANGAN_TOLERANSI) break;

        irr = irr - npv / dnpv;

        /* Batasi nilai IRR agar tidak meledak */
        if (irr > 10.0) irr = 10.0;
        if (irr < -0.99) irr = -0.99;
    }

    *keluaran = irr;
    return 0;
}

/* MIRR: Modified Internal Rate of Return
 * MIRR(cashflows, finance_rate, reinvest_rate)
 */
int keuangan_mirr(const double *cashflows, int jumlah,
                  double finance_rate, double reinvest_rate,
                  double *keluaran) {
    double pv_negatif = 0.0;
    double fv_positif = 0.0;
    int i;

    if (!cashflows || !keluaran || jumlah <= 1) return -1;

    /* Hitung PV arus kas negatif */
    for (i = 0; i < jumlah; i++) {
        if (cashflows[i] < 0.0) {
            double faktor = keuangan_pow_int(1.0 + finance_rate, i);
            pv_negatif += cashflows[i] / faktor;
        }
    }

    /* Hitung FV arus kas positif */
    for (i = 0; i < jumlah; i++) {
        if (cashflows[i] > 0.0) {
            double faktor = keuangan_pow_int(1.0 + reinvest_rate, jumlah - 1 - i);
            fv_positif += cashflows[i] * faktor;
        }
    }

    if (keuangan_abs(pv_negatif) < KEUANGAN_TOLERANSI) return -1;

    /* MIRR = (FV_positif / |PV_negatif|)^(1/n) - 1 */
    *keluaran = keuangan_pow_int(fv_positif / keuangan_abs(pv_negatif),
                                  (int)(jumlah - 1) * 100) / 100.0 - 1.0;
    return 0;
}

/* ============================================================
 * FUNGSI DEPRESIASI
 * ============================================================ */

/* SLN: Straight-Line Depreciation (Penyusutan Garis Lurus)
 * SLN(cost, salvage, life)
 */
int keuangan_sln(double cost, double salvage, double life,
                 double *keluaran) {
    if (!keluaran) return -1;
    if (life <= 0.0) return -1;

    *keluaran = (cost - salvage) / life;
    return 0;
}

/* DDB: Double-Declining Balance Depreciation
 * DDB(cost, salvage, life, period, [factor])
 */
int keuangan_ddb(double cost, double salvage, double life,
                 double period, double factor, double *keluaran) {
    double book_value;
    double dep;
    int i;

    if (!keluaran) return -1;
    if (life <= 0.0 || period <= 0.0 || period > life) return -1;
    if (factor <= 0.0) factor = 2.0;

    book_value = cost;

    for (i = 1; i <= (int)period; i++) {
        dep = book_value * factor / life;
        if (book_value - dep < salvage) {
            dep = book_value - salvage;
        }
        book_value -= dep;

        if (i == (int)period) {
            *keluaran = dep;
            return 0;
        }
    }

    *keluaran = 0.0;
    return 0;
}

/* SYD: Sum-of-Years-Digits Depreciation
 * SYD(cost, salvage, life, period)
 */
int keuangan_syd(double cost, double salvage, double life,
                 double period, double *keluaran) {
    double syd;

    if (!keluaran) return -1;
    if (life <= 0.0 || period <= 0.0 || period > life) return -1;

    /* SYD = (cost - salvage) * (life - period + 1) * 2 / (life * (life + 1)) */
    syd = (cost - salvage) * (life - period + 1.0) * 2.0 /
          (life * (life + 1.0));

    *keluaran = syd;
    return 0;
}

/* DB: Declining Balance Depreciation
 * DB(cost, salvage, life, period, [month])
 */
int keuangan_db(double cost, double salvage, double life,
                double period, double month, double *keluaran) {
    double rate;
    double book_value;
    double dep;
    int i;

    if (!keluaran) return -1;
    if (life <= 0.0 || period <= 0.0) return -1;
    if (month <= 0.0) month = 12.0;

    /* Rate = 1 - (salvage / cost)^(1/life) */
    if (cost <= 0.0 || salvage >= cost) {
        *keluaran = 0.0;
        return 0;
    }

    rate = 1.0 - keuangan_pow_int(salvage / cost, (int)life * 100) / 100.0;
    rate = rate / 100.0; /* Normalisasi */

    book_value = cost;

    /* Tahun pertama dengan bulan parsial */
    if (period == 1.0) {
        dep = cost * rate * month / 12.0;
        *keluaran = dep;
        return 0;
    }

    /* Tahun-tahun berikutnya */
    dep = cost * rate * month / 12.0;
    book_value = cost - dep;

    for (i = 2; i <= (int)period && i <= (int)life; i++) {
        dep = book_value * rate;
        if (book_value - dep < salvage) {
            dep = book_value - salvage;
        }

        if (i == (int)period) {
            *keluaran = dep;
            return 0;
        }
        book_value -= dep;
    }

    *keluaran = 0.0;
    return 0;
}

/* VDB: Variable Declining Balance
 * VDB(cost, salvage, life, start_period, end_period, [factor], [no_switch])
 */
int keuangan_vdb(double cost, double salvage, double life,
                 double start_period, double end_period,
                 double factor, int no_switch, double *keluaran) {
    double total_dep = 0.0;
    double book_value;
    double dep;
    int i;

    if (!keluaran) return -1;
    if (life <= 0.0) return -1;
    if (start_period < 0.0 || end_period > life ||
        start_period >= end_period) return -1;
    if (factor <= 0.0) factor = 2.0;

    book_value = cost;

    for (i = 0; i <= (int)end_period; i++) {
        double start_frac = 0.0, end_frac = 1.0;

        if (i < (int)start_period) {
            /* Hitung depresiasi tapi jangan tambahkan */
            dep = book_value * factor / life;
            if (!no_switch && book_value - dep < salvage) {
                dep = book_value - salvage;
            }
            book_value -= dep;
            continue;
        }

        if (i == (int)start_period) {
            start_frac = start_period - (int)start_period;
        }
        if (i == (int)end_period) {
            end_frac = end_period - (int)end_period;
        }

        dep = book_value * factor / life;
        if (!no_switch && book_value - dep < salvage) {
            dep = book_value - salvage;
        }

        if (i == (int)start_period && start_frac > 0.0) {
            dep *= (1.0 - start_frac);
        }
        if (i == (int)end_period && end_frac > 0.0) {
            dep *= end_frac;
        }

        total_dep += dep;
        book_value -= dep;

        if (book_value <= salvage) break;
    }

    *keluaran = total_dep;
    return 0;
}

/* ============================================================
 * FUNGSI PEMBAYARAN IPMT DAN PPMT
 * ============================================================ */

/* IPMT: Interest Payment (Pembayaran Bunga)
 * IPMT(rate, per, nper, pv, [fv], [type])
 */
int keuangan_ipmt(double rate, double per, double nper, double pv,
                  double fv, int type, double *keluaran) {
    double pmt;
    double ipmt;
    double balance;
    int i;

    if (!keluaran) return -1;
    if (per <= 0.0 || per > nper || nper <= 0.0) return -1;

    /* Hitung PMT */
    if (keuangan_pmt(rate, nper, pv, fv, type, &pmt) != 0) return -1;

    /* Hitung saldo pada awal periode */
    balance = -pv;
    for (i = 1; i < (int)per; i++) {
        double interest = balance * rate;
        double principal = -pmt - interest;
        balance -= principal;
    }

    /* Bunga = saldo * rate */
    ipmt = balance * rate;

    *keluaran = ipmt;
    return 0;
}

/* PPMT: Principal Payment (Pembayaran Pokok)
 * PPMT(rate, per, nper, pv, [fv], [type])
 */
int keuangan_ppmt(double rate, double per, double nper, double pv,
                  double fv, int type, double *keluaran) {
    double pmt;
    double ipmt;

    if (!keluaran) return -1;
    if (per <= 0.0 || per > nper || nper <= 0.0) return -1;

    /* Hitung PMT */
    if (keuangan_pmt(rate, nper, pv, fv, type, &pmt) != 0) return -1;

    /* Hitung IPMT */
    if (keuangan_ipmt(rate, per, nper, pv, fv, type, &ipmt) != 0) return -1;

    /* PPMT = PMT - IPMT */
    *keluaran = -pmt - ipmt;
    return 0;
}

/* CUMIPMT: Cumulative Interest Payment
 * CUMIPMT(rate, nper, pv, start_period, end_period, type)
 */
int keuangan_cumipmt(double rate, double nper, double pv,
                     double start_period, double end_period,
                     int type, double *keluaran) {
    double total = 0.0;
    int i;

    if (!keluaran) return -1;
    if (start_period < 1.0 || end_period > nper ||
        start_period > end_period || nper <= 0.0) return -1;

    for (i = (int)start_period; i <= (int)end_period; i++) {
        double ipmt;
        if (keuangan_ipmt(rate, (double)i, nper, pv, 0.0, type, &ipmt) != 0) {
            return -1;
        }
        total += ipmt;
    }

    *keluaran = total;
    return 0;
}

/* CUMPRINC: Cumulative Principal Payment
 * CUMPRINC(rate, nper, pv, start_period, end_period, type)
 */
int keuangan_cumprinc(double rate, double nper, double pv,
                      double start_period, double end_period,
                      int type, double *keluaran) {
    double total = 0.0;
    int i;

    if (!keluaran) return -1;
    if (start_period < 1.0 || end_period > nper ||
        start_period > end_period || nper <= 0.0) return -1;

    for (i = (int)start_period; i <= (int)end_period; i++) {
        double ppmt;
        if (keuangan_ppmt(rate, (double)i, nper, pv, 0.0, type, &ppmt) != 0) {
            return -1;
        }
        total += ppmt;
    }

    *keluaran = total;
    return 0;
}

/* ============================================================
 * FUNGSI KONVERSI DAN HELPER TAMBAHAN
 * ============================================================ */

/* EFFECT: Effective Annual Interest Rate
 * EFFECT(nominal_rate, npery)
 */
int keuangan_effect(double nominal_rate, double npery,
                    double *keluaran) {
    if (!keluaran) return -1;
    if (npery <= 0.0) return -1;

    /* EFFECT = (1 + nominal_rate/npery)^npery - 1 */
    *keluaran = keuangan_pow_int(1.0 + nominal_rate / npery, (int)npery) - 1.0;
    return 0;
}

/* NOMINAL: Nominal Annual Interest Rate
 * NOMINAL(effect_rate, npery)
 */
int keuangan_nominal(double effect_rate, double npery,
                     double *keluaran) {
    if (!keluaran) return -1;
    if (npery <= 0.0) return -1;

    /* NOMINAL = npery * ((1 + effect_rate)^(1/npery) - 1) */
    *keluaran = npery * (keuangan_pow_int(1.0 + effect_rate, 100 / (int)npery) / 100.0 - 1.0);
    return 0;
}

/* FVSCHEDULE: Future Value with variable interest rate
 * FVSCHEDULE(principal, schedule)
 */
int keuangan_fvschedule(double principal, const double *rates,
                        int jumlah, double *keluaran) {
    double fv;
    int i;

    if (!keluaran || !rates) return -1;

    fv = principal;
    for (i = 0; i < jumlah; i++) {
        fv *= (1.0 + rates[i]);
    }

    *keluaran = fv;
    return 0;
}

/* ISPMT: Interest during a specific period (simple interest)
 * ISPMT(rate, per, nper, pv)
 */
int keuangan_ispmt(double rate, double per, double nper, double pv,
                   double *keluaran) {
    if (!keluaran) return -1;
    if (nper <= 0.0) return -1;

    /* ISPMT = pv * rate * (per - nper) / nper */
    *keluaran = pv * rate * (per - nper) / nper;
    return 0;
}

/* TBILLEQ: Treasury Bill Equivalent Yield
 * TBILLEQ(settlement, maturity, discount)
 */
int keuangan_tbilleq(double settlement, double maturity,
                     double discount, double *keluaran) {
    double days;
    double price;

    if (!keluaran) return -1;

    days = maturity - settlement;
    if (days <= 0.0 || days > 365.0) return -1;

    price = 100.0 * (1.0 - discount * days / 360.0);

    /* TBILLEQ = (100 - price) / price * 365 / days */
    *keluaran = (100.0 - price) / price * 365.0 / days;
    return 0;
}

/* TBILLPRICE: Treasury Bill Price
 * TBILLPRICE(settlement, maturity, discount)
 */
int keuangan_tbillprice(double settlement, double maturity,
                        double discount, double *keluaran) {
    double days;

    if (!keluaran) return -1;

    days = maturity - settlement;
    if (days <= 0.0 || days > 365.0) return -1;

    *keluaran = 100.0 * (1.0 - discount * days / 360.0);
    return 0;
}

/* TBILLYIELD: Treasury Bill Yield
 * TBILLYIELD(settlement, maturity, pr)
 */
int keuangan_tbillyield(double settlement, double maturity,
                        double pr, double *keluaran) {
    double days;

    if (!keluaran) return -1;

    days = maturity - settlement;
    if (days <= 0.0 || days > 365.0) return -1;

    /* YIELD = (100 - pr) / pr * 360 / days */
    *keluaran = (100.0 - pr) / pr * 360.0 / days;
    return 0;
}

/* ============================================================
 * DISPATCHER FUNGSI KEUANGAN
 * ============================================================ */

/* Helper untuk parsing range */
static int parse_range_keuangan(const char *s, int *x1, int *y1,
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

/* Kumpulkan cash flows dari range */
static int kumpulkan_cashflows(const struct buffer_tabel *buf,
                               int x1, int y1, int x2, int y2,
                               double *cashflows, int maks) {
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

            val = strtod(t, &endp);
            if (endp && endp != t && *endp == '\0') {
                cashflows[count++] = val;
            }
        }
    }

    return count;
}

/* Evaluasi fungsi keuangan berdasarkan nama */
int keuangan_eval(const struct buffer_tabel *buf, const char *nama,
                  const double *args, int nargs, double *keluaran) {
    if (!nama || !keluaran) return -1;

    /* PV */
    if (banding_str(nama, "PV") == 0) {
        if (nargs < 3) return -1;
        return keuangan_pv(args[0], args[1], args[2],
                          (nargs > 3) ? args[3] : 0.0,
                          (nargs > 4) ? (int)args[4] : 0,
                          keluaran);
    }

    /* FV */
    if (banding_str(nama, "FV") == 0) {
        if (nargs < 3) return -1;
        return keuangan_fv(args[0], args[1], args[2],
                          (nargs > 3) ? args[3] : 0.0,
                          (nargs > 4) ? (int)args[4] : 0,
                          keluaran);
    }

    /* PMT */
    if (banding_str(nama, "PMT") == 0) {
        if (nargs < 3) return -1;
        return keuangan_pmt(args[0], args[1], args[2],
                           (nargs > 3) ? args[3] : 0.0,
                           (nargs > 4) ? (int)args[4] : 0,
                           keluaran);
    }

    /* RATE */
    if (banding_str(nama, "RATE") == 0) {
        if (nargs < 3) return -1;
        return keuangan_rate(args[0], args[1], args[2],
                            (nargs > 3) ? args[3] : 0.0,
                            (nargs > 4) ? (int)args[4] : 0,
                            (nargs > 5) ? args[5] : 0.1,
                            keluaran);
    }

    /* NPER */
    if (banding_str(nama, "NPER") == 0) {
        if (nargs < 3) return -1;
        return keuangan_nper(args[0], args[1], args[2],
                            (nargs > 3) ? args[3] : 0.0,
                            (nargs > 4) ? (int)args[4] : 0,
                            keluaran);
    }

    /* SLN */
    if (banding_str(nama, "SLN") == 0) {
        if (nargs < 3) return -1;
        return keuangan_sln(args[0], args[1], args[2], keluaran);
    }

    /* DDB */
    if (banding_str(nama, "DDB") == 0) {
        if (nargs < 4) return -1;
        return keuangan_ddb(args[0], args[1], args[2], args[3],
                           (nargs > 4) ? args[4] : 2.0,
                           keluaran);
    }

    /* SYD */
    if (banding_str(nama, "SYD") == 0) {
        if (nargs < 4) return -1;
        return keuangan_syd(args[0], args[1], args[2], args[3], keluaran);
    }

    /* DB */
    if (banding_str(nama, "DB") == 0) {
        if (nargs < 4) return -1;
        return keuangan_db(args[0], args[1], args[2], args[3],
                          (nargs > 4) ? args[4] : 12.0,
                          keluaran);
    }

    /* VDB */
    if (banding_str(nama, "VDB") == 0) {
        if (nargs < 5) return -1;
        return keuangan_vdb(args[0], args[1], args[2], args[3], args[4],
                           (nargs > 5) ? args[5] : 2.0,
                           (nargs > 6) ? (int)args[6] : 0,
                           keluaran);
    }

    /* IPMT */
    if (banding_str(nama, "IPMT") == 0) {
        if (nargs < 4) return -1;
        return keuangan_ipmt(args[0], args[1], args[2], args[3],
                            (nargs > 4) ? args[4] : 0.0,
                            (nargs > 5) ? (int)args[5] : 0,
                            keluaran);
    }

    /* PPMT */
    if (banding_str(nama, "PPMT") == 0) {
        if (nargs < 4) return -1;
        return keuangan_ppmt(args[0], args[1], args[2], args[3],
                            (nargs > 4) ? args[4] : 0.0,
                            (nargs > 5) ? (int)args[5] : 0,
                            keluaran);
    }

    /* CUMIPMT */
    if (banding_str(nama, "CUMIPMT") == 0) {
        if (nargs < 6) return -1;
        return keuangan_cumipmt(args[0], args[1], args[2],
                               args[3], args[4], (int)args[5],
                               keluaran);
    }

    /* CUMPRINC */
    if (banding_str(nama, "CUMPRINC") == 0) {
        if (nargs < 6) return -1;
        return keuangan_cumprinc(args[0], args[1], args[2],
                                args[3], args[4], (int)args[5],
                                keluaran);
    }

    /* EFFECT */
    if (banding_str(nama, "EFFECT") == 0) {
        if (nargs < 2) return -1;
        return keuangan_effect(args[0], args[1], keluaran);
    }

    /* NOMINAL */
    if (banding_str(nama, "NOMINAL") == 0) {
        if (nargs < 2) return -1;
        return keuangan_nominal(args[0], args[1], keluaran);
    }

    /* ISPMT */
    if (banding_str(nama, "ISPMT") == 0) {
        if (nargs < 4) return -1;
        return keuangan_ispmt(args[0], args[1], args[2], args[3], keluaran);
    }

    /* TBILLEQ */
    if (banding_str(nama, "TBILLEQ") == 0) {
        if (nargs < 3) return -1;
        return keuangan_tbilleq(args[0], args[1], args[2], keluaran);
    }

    /* TBILLPRICE */
    if (banding_str(nama, "TBILLPRICE") == 0) {
        if (nargs < 3) return -1;
        return keuangan_tbillprice(args[0], args[1], args[2], keluaran);
    }

    /* TBILLYIELD */
    if (banding_str(nama, "TBILLYIELD") == 0) {
        if (nargs < 3) return -1;
        return keuangan_tbillyield(args[0], args[1], args[2], keluaran);
    }

    return -1;
}

/* Evaluasi fungsi keuangan dengan range (NPV, IRR, MIRR) */
int keuangan_eval_range(const struct buffer_tabel *buf, const char *nama,
                        const char *range, const double *extra_args,
                        int nargs, double *keluaran) {
    int x1, y1, x2, y2;
    double cashflows[256];
    int count;

    if (!buf || !nama || !range || !keluaran) return -1;

    if (parse_range_keuangan(range, &x1, &y1, &x2, &y2) != 0) return -1;

    count = kumpulkan_cashflows(buf, x1, y1, x2, y2, cashflows, 256);
    if (count <= 0) return -1;

    /* NPV */
    if (banding_str(nama, "NPV") == 0) {
        if (nargs < 1) return -1;
        return keuangan_npv(extra_args[0], cashflows, count, keluaran);
    }

    /* IRR */
    if (banding_str(nama, "IRR") == 0) {
        double guess = (nargs > 0) ? extra_args[0] : 0.1;
        return keuangan_irr(cashflows, count, guess, keluaran);
    }

    /* MIRR */
    if (banding_str(nama, "MIRR") == 0) {
        if (nargs < 2) return -1;
        return keuangan_mirr(cashflows, count,
                            extra_args[0], extra_args[1],
                            keluaran);
    }

    return -1;
}
