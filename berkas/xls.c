/* ============================================================
 * TABEL v3.0
 * Berkas: xls.c - Format Berkas Binary OLE2 Compound Document - Excel 97-2003
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA OLE2 & BIFF
 * ============================================================ */
#define OLE2_MAX_SECTORS        4096
#define OLE2_MAX_STREAMS        64
#define OLE2_SECTOR_SIZE        512
#define OLE2_MINI_SECTOR_SIZE   64
#define OLE2_MAX_PATH           64

/* OLE2 Signatures */
#define OLE2_SIGNATURE          0xE11AB1A1E011CFD0ULL

/* BIFF Record Types */
#define BIFF_BOF                0x0809
#define BIFF_EOF                0x000A
#define BIFF_DIMENSION          0x0200
#define BIFF_BLANK              0x0201
#define BIFF_INTEGER            0x0202
#define BIFF_NUMBER             0x0203
#define BIFF_LABEL              0x0204
#define BIFF_BOOLERR            0x0205
#define BIFF_FORMULA            0x0006
#define BIFF_STRING             0x0207
#define BIFF_ROW                0x0208
#define BIFF_BOF2               0x0009
#define BIFF_INDEX              0x020B
#define BIFF_CALCCOUNT          0x000C
#define BIFF_CALCMODE           0x000D
#define BIFF_PRECISION          0x000E
#define BIFF_REFMODE            0x000F
#define BIFF_DELTA              0x0010
#define BIFF_ITERATION          0x0011
#define BIFF_PROTECT            0x0012
#define BIFF_PASSWORD           0x0013
#define BIFF_HEADER             0x0014
#define BIFF_FOOTER             0x0015
#define BIFF_EXTERNCOUNT        0x0016
#define BIFF_EXTERNSHEET        0x0017
#define BIFF_NAME               0x0018
#define BIFF_WINDOWPROTECT      0x0019
#define BIFF_SELECTION          0x001D
#define BIFF_DATEMODE           0x0022
#define BIFF_EXTERNNAME         0x0023
#define BIFF_LEFTMARGIN         0x0026
#define BIFF_RIGHTMARGIN        0x0027
#define BIFF_TOPMARGIN          0x0028
#define BIFF_BOTTOMMARGIN       0x0029
#define BIFF_PRINTHEADERS       0x002A
#define BIFF_PRINTGRIDLINES     0x002B
#define BIFF_FILEPASS           0x002F
#define BIFF_FONT               0x0031
#define BIFF_PRINTSIZE          0x0033
#define BIFF_CONTINUE           0x003C
#define BIFF_WINDOW1            0x003D
#define BIFF_BACKUP             0x0040
#define BIFF_PANE               0x0041
#define BIFF_CODEPAGE           0x0042
#define BIFF_PLS                0x004D
#define BIFF_DSF                0x0051
#define BIFF_XF                 0x00E0
#define BIFF_MULBLANK           0x00BE
#define BIFF_MULRK              0x00BD
#define BIFF_RK                 0x027E
#define BIFF_RSTRING            0x00D6
#define BIFF_LABELSST           0x00FD
#define BIFF_SST                0x00FC
#define BIFF_EXTSST             0x00FF
#define BIFF_BOUNDSHEET         0x0085
#define BIFF_COUNTRY            0x008C
#define BIFF_HIDEOBJ            0x008D
#define BIFF_STANDARDWIDTH      0x0099
#define BIFF_FILTERMODE         0x009B
#define BIFF_FNGROUPCOUNT       0x009C
#define BIFF_AUTOFILTERINFO     0x009D
#define BIFF_AUTOFILTER         0x009E
#define BIFF_SCL                0x00A0
#define BIFF_SETUP              0x00A1
#define BIFF_SCENPROTECT        0x00DD
#define BIFF_SHAREDSTRING       0x00FC
#define BIFF_USESELFS           0x0160
#define BIFF_DCONREF            0x0051
#define BIFF_SUPBOOK            0x01AE
#define BIFF_TXO                0x01B6
#define BIFF_RECALCID           0x01C1

/* BIFF versions */
#define BIFF_VERSION_BIFF2      0x0200
#define BIFF_VERSION_BIFF3      0x0300
#define BIFF_VERSION_BIFF4      0x0400
#define BIFF_VERSION_BIFF5      0x0500
#define BIFF_VERSION_BIFF8      0x0600

/* ============================================================
 * STRUKTUR OLE2
 * ============================================================ */

/* OLE2 Header (512 bytes) */
struct ole2_header {
    unsigned char signature[8];         /* D0 CF 11 E0 A1 B1 1A E1 */
    unsigned char clsid[16];            /* CLSID (zeros) */
    unsigned short version_minor;       /* Minor version */
    unsigned short version_major;       /* Major version (3 or 4) */
    unsigned short byte_order;          /* 0xFFFE = little-endian */
    unsigned short sector_size_pow;     /* Sector size = 2^pow */
    unsigned short mini_sector_size_pow;/* Mini sector size = 2^pow */
    unsigned short reserved[6];
    unsigned int total_sectors;         /* Total sectors in file */
    unsigned int fat_sectors;           /* Number of FAT sectors */
    unsigned int first_dir_sector;      /* First directory sector */
    unsigned int transaction_sig;       /* Transaction signature */
    unsigned int mini_stream_cutoff;    /* Mini stream cutoff (4096) */
    unsigned int first_mini_fat_sector; /* First mini FAT sector */
    unsigned int mini_fat_sectors;      /* Number of mini FAT sectors */
    unsigned int first_difat_sector;    /* First DIFAT sector */
    unsigned int difat_sectors;         /* Number of DIFAT sectors */
    unsigned int difat[109];            /* DIFAT array */
};

/* OLE2 Directory Entry (128 bytes) */
struct ole2_dir_entry {
    char name[64];                      /* Entry name (UTF-16LE) */
    unsigned short name_len;            /* Name length in bytes */
    unsigned char type;                 /* Entry type */
    unsigned char color;                /* Color flag (red/black tree) */
    unsigned int left;                  /* Left sibling */
    unsigned int right;                 /* Right sibling */
    unsigned int child;                 /* First child */
    unsigned char clsid[16];            /* CLSID */
    unsigned int state_bits;            /* State bits */
    unsigned long long create_time;     /* Creation time */
    unsigned long long modify_time;     /* Modification time */
    unsigned int start_sector;          /* Starting sector */
    unsigned long long size;            /* Size of stream */
};

/* Entry types */
#define OLE2_TYPE_EMPTY         0
#define OLE2_TYPE_STORAGE       1
#define OLE2_TYPE_STREAM        2
#define OLE2_TYPE_LOCKBYTES     3
#define OLE2_TYPE_PROPERTY      4
#define OLE2_TYPE_ROOT          5

/* OLE2 file structure */
struct ole2_file {
    FILE *fp;
    struct ole2_header header;
    unsigned int *fat;                  /* File Allocation Table */
    unsigned int fat_entries;
    unsigned int *mini_fat;             /* Mini FAT */
    unsigned int mini_fat_entries;
    struct ole2_dir_entry *entries;     /* Directory entries */
    int num_entries;
    unsigned int sector_size;
    unsigned int mini_sector_size;
};

/* ============================================================
 * STRUKTUR BIFF
 * ============================================================ */

/* BIFF record header */
struct biff_record {
    unsigned short type;
    unsigned short length;
    unsigned char *data;
};

/* Shared String Table entry */
struct xls_shared_string {
    char *text;
    int count;
};

/* XLS workbook structure */
struct xls_workbook {
    struct xls_shared_string *sst;
    int sst_count;
    int sst_capacity;
    int max_row;
    int max_col;
    int biff_version;
    int is_biff8;
};

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static char xls_error_msg[512] = "";

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

static void xls_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(xls_error_msg, sizeof(xls_error_msg), fmt, args);
    va_end(args);
}

const char *xls_get_error(void) {
    return xls_error_msg;
}

/* Read little-endian 16-bit */
static unsigned short xls_read_u16(const unsigned char *p) {
    return (unsigned short)(p[0] | (p[1] << 8));
}

/* Read little-endian 32-bit */
static unsigned int xls_read_u32(const unsigned char *p) {
    return (unsigned int)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* Write little-endian 16-bit */
static void xls_write_u16(unsigned char *p, unsigned short v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
}

/* Write little-endian 32-bit */
static void xls_write_u32(unsigned char *p, unsigned int v) {
    p[0] = v & 0xFF;
    p[1] = (v >> 8) & 0xFF;
    p[2] = (v >> 16) & 0xFF;
    p[3] = (v >> 24) & 0xFF;
}

/* Write little-endian 64-bit (double) */
static void xls_write_double(unsigned char *p, double v) {
    memcpy(p, &v, 8);
}

/* Read IEEE 754 double */
static double xls_read_double(const unsigned char *p) {
    double v;
    memcpy(&v, p, 8);
    return v;
}

/* ============================================================
 * KONVERSI UTF-16LE KE UTF-8
 * ============================================================ */

/* Konversi UTF-16LE ke UTF-8 */
static int utf16le_to_utf8(const unsigned char *utf16, int utf16_len,
                           char *utf8, int utf8_max) {
    int i = 0, j = 0;
    unsigned int cp;

    while (i + 1 < utf16_len && j < utf8_max - 4) {
        /* Baca code point (little-endian) */
        cp = utf16[i] | (utf16[i + 1] << 8);
        i += 2;

        /* Handle surrogate pairs */
        if (cp >= 0xD800 && cp <= 0xDBFF) {
            unsigned int low;
            if (i + 1 >= utf16_len) break;
            low = utf16[i] | (utf16[i + 1] << 8);
            if (low >= 0xDC00 && low <= 0xDFFF) {
                i += 2;
                cp = 0x10000 + ((cp - 0xD800) << 10) + (low - 0xDC00);
            }
        }

        /* Konversi ke UTF-8 */
        if (cp < 0x80) {
            utf8[j++] = (char)cp;
        } else if (cp < 0x800) {
            utf8[j++] = (char)(0xC0 | (cp >> 6));
            utf8[j++] = (char)(0x80 | (cp & 0x3F));
        } else if (cp < 0x10000) {
            utf8[j++] = (char)(0xE0 | (cp >> 12));
            utf8[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8[j++] = (char)(0x80 | (cp & 0x3F));
        } else {
            utf8[j++] = (char)(0xF0 | (cp >> 18));
            utf8[j++] = (char)(0x80 | ((cp >> 12) & 0x3F));
            utf8[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
            utf8[j++] = (char)(0x80 | (cp & 0x3F));
        }
    }

    utf8[j] = '\0';
    return j;
}

/* Konversi UTF-8 ke UTF-16LE */
static int utf8_to_utf16le(const char *utf8, unsigned char *utf16, int utf16_max) {
    int i = 0, j = 0;
    const unsigned char *p = (const unsigned char *)utf8;

    while (p[i] && j < utf16_max - 4) {
        unsigned int cp = 0;
        int len = 0;

        if ((p[i] & 0x80) == 0) {
            cp = p[i++];
        } else if ((p[i] & 0xE0) == 0xC0) {
            cp = (p[i] & 0x1F);
            cp = (cp << 6) | (p[i + 1] & 0x3F);
            i += 2;
        } else if ((p[i] & 0xF0) == 0xE0) {
            cp = (p[i] & 0x0F);
            cp = (cp << 6) | (p[i + 1] & 0x3F);
            cp = (cp << 6) | (p[i + 2] & 0x3F);
            i += 3;
        } else if ((p[i] & 0xF8) == 0xF0) {
            cp = (p[i] & 0x07);
            cp = (cp << 6) | (p[i + 1] & 0x3F);
            cp = (cp << 6) | (p[i + 2] & 0x3F);
            cp = (cp << 6) | (p[i + 3] & 0x3F);
            i += 4;
        } else {
            i++;
            continue;
        }

        /* Handle surrogate pairs untuk code points > 0xFFFF */
        if (cp > 0xFFFF) {
            unsigned int high = 0xD800 + ((cp - 0x10000) >> 10);
            unsigned int low = 0xDC00 + ((cp - 0x10000) & 0x3FF);
            utf16[j++] = high & 0xFF;
            utf16[j++] = (high >> 8) & 0xFF;
            utf16[j++] = low & 0xFF;
            utf16[j++] = (low >> 8) & 0xFF;
        } else {
            utf16[j++] = cp & 0xFF;
            utf16[j++] = (cp >> 8) & 0xFF;
        }
    }

    return j;
}

/* ============================================================
 * FUNGSI RK NUMBER
 * ============================================================
 * RK adalah format number compressed 4-byte
 * Bit 0: 0 = encoded IEEE float, 1 = integer * 100
 * Bit 1: 0 = tidak ada *100, 1 = ada *100
 * Bit 2-31: 30-bit signed integer atau 30-bit IEEE fraction
 * ============================================================ */

static double decode_rk(const unsigned char *p) {
    unsigned int rk = xls_read_u32(p);
    double v;

    if (rk & 0x02) {
        /* Integer value */
        int val = (int)(rk >> 2);
        if (rk & 0x01) {
            v = (double)val / 100.0;
        } else {
            v = (double)val;
        }
    } else {
        /* IEEE float (encoded) */
        /* 30 bits shifted into positions 2-31 of 64-bit double */
        unsigned long long ieee = ((unsigned long long)rk & 0xFFFFFFFC) << 32;
        memcpy(&v, &ieee, 8);
        if (rk & 0x01) {
            v /= 100.0;
        }
    }

    return v;
}

/* ============================================================
 * FUNGSI OLE2 - READING
 * ============================================================ */

/* Baca OLE2 header */
static int ole2_read_header(struct ole2_file *of) {
    unsigned char buf[512];

    if (fread(buf, 1, 512, of->fp) != 512) {
        xls_set_error("Cannot read OLE2 header");
        return -1;
    }

    /* Verify signature */
    if (memcmp(buf, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) != 0) {
        xls_set_error("Invalid OLE2 signature");
        return -1;
    }

    /* Parse header */
    of->header.version_minor = xls_read_u16(buf + 24);
    of->header.version_major = xls_read_u16(buf + 26);
    of->header.byte_order = xls_read_u16(buf + 28);
    of->header.sector_size_pow = xls_read_u16(buf + 30);
    of->header.mini_sector_size_pow = xls_read_u16(buf + 32);
    of->header.total_sectors = xls_read_u32(buf + 44);
    of->header.fat_sectors = xls_read_u32(buf + 48);
    of->header.first_dir_sector = xls_read_u32(buf + 52);
    of->header.mini_stream_cutoff = xls_read_u32(buf + 60);
    of->header.first_mini_fat_sector = xls_read_u32(buf + 64);
    of->header.mini_fat_sectors = xls_read_u32(buf + 68);
    of->header.first_difat_sector = xls_read_u32(buf + 72);
    of->header.difat_sectors = xls_read_u32(buf + 76);

    /* Calculate sector sizes */
    of->sector_size = 1 << of->header.sector_size_pow;
    of->mini_sector_size = 1 << of->header.mini_sector_size_pow;

    /* Read DIFAT (initial 109 entries) */
    for (int i = 0; i < 109; i++) {
        of->header.difat[i] = xls_read_u32(buf + 76 + i * 4);
    }

    return 0;
}

/* Read sector */
static unsigned char *ole2_read_sector(struct ole2_file *of, unsigned int sector) {
    unsigned char *buf;
    long offset;

    buf = malloc(of->sector_size);
    if (!buf) return NULL;

    offset = (sector + 1) * of->sector_size;
    fseek(of->fp, offset, SEEK_SET);

    if (fread(buf, 1, of->sector_size, of->fp) != of->sector_size) {
        free(buf);
        return NULL;
    }

    return buf;
}

/* Read FAT */
static int ole2_read_fat(struct ole2_file *of) {
    unsigned int i, j, k;
    unsigned char *sector;

    /* Allocate FAT */
    of->fat_entries = of->header.fat_sectors * (of->sector_size / 4);
    of->fat = calloc(of->fat_entries, sizeof(unsigned int));
    if (!of->fat) return -1;

    k = 0;
    for (i = 0; i < of->header.fat_sectors && i < 109; i++) {
        sector = ole2_read_sector(of, of->header.difat[i]);
        if (!sector) continue;

        for (j = 0; j < of->sector_size / 4 && k < of->fat_entries; j++) {
            of->fat[k++] = xls_read_u32(sector + j * 4);
        }

        free(sector);
    }

    return 0;
}

/* Read directory entries */
static int ole2_read_directory(struct ole2_file *of) {
    unsigned char *sector;
    unsigned int sec = of->header.first_dir_sector;
    int count = 0;
    int i;

    /* Allocate initial entries */
    of->entries = calloc(64, sizeof(struct ole2_dir_entry));
    if (!of->entries) return -1;
    of->num_entries = 0;

    /* Read all directory sectors */
    while (sec != 0xFFFFFFFE && sec != 0xFFFFFFFF) {
        sector = ole2_read_sector(of, sec);
        if (!sector) break;

        /* Each sector contains sector_size/128 entries */
        for (i = 0; i < (int)(of->sector_size / 128); i++) {
            unsigned char *entry = sector + i * 128;
            struct ole2_dir_entry *de = &of->entries[of->num_entries];

            /* Read name (UTF-16LE) */
            de->name_len = xls_read_u16(entry + 64);
            if (de->name_len == 0) continue;

            /* Convert name to ASCII (simplified) */
            int ni;
            for (ni = 0; ni < de->name_len / 2 && ni < 63; ni++) {
                de->name[ni] = entry[ni * 2];
            }
            de->name[ni] = '\0';

            de->type = entry[66];
            de->color = entry[67];
            de->left = xls_read_u32(entry + 68);
            de->right = xls_read_u32(entry + 72);
            de->child = xls_read_u32(entry + 76);
            de->start_sector = xls_read_u32(entry + 116);
            de->size = xls_read_u32(entry + 120); /* Only low 32 bits */

            of->num_entries++;
            if (of->num_entries >= 64) break;
        }

        free(sector);

        if (of->num_entries >= 64) break;

        /* Get next sector from FAT */
        if (sec < of->fat_entries) {
            sec = of->fat[sec];
        } else {
            break;
        }
    }

    return 0;
}

/* Find stream by name */
static struct ole2_dir_entry *ole2_find_stream(struct ole2_file *of,
                                                const char *name) {
    int i;

    for (i = 0; i < of->num_entries; i++) {
        if (of->entries[i].type == OLE2_TYPE_STREAM) {
            if (strcmp(of->entries[i].name, name) == 0) {
                return &of->entries[i];
            }
        }
    }

    return NULL;
}

/* Read stream data */
static unsigned char *ole2_read_stream(struct ole2_file *of,
                                        struct ole2_dir_entry *entry,
                                        size_t *size) {
    unsigned char *data;
    unsigned char *sector;
    unsigned int sec;
    size_t total_size;
    size_t offset;

    if (!entry || entry->size == 0) {
        *size = 0;
        return NULL;
    }

    total_size = (size_t)entry->size;
    data = malloc(total_size + 1);
    if (!data) return NULL;

    sec = entry->start_sector;
    offset = 0;

    while (sec != 0xFFFFFFFE && sec != 0xFFFFFFFF && offset < total_size) {
        sector = ole2_read_sector(of, sec);
        if (!sector) break;

        size_t chunk = of->sector_size;
        if (offset + chunk > total_size) {
            chunk = total_size - offset;
        }

        memcpy(data + offset, sector, chunk);
        offset += chunk;
        free(sector);

        if (sec < of->fat_entries) {
            sec = of->fat[sec];
        } else {
            break;
        }
    }

    data[total_size] = '\0';
    *size = total_size;
    return data;
}

/* Free OLE2 file */
static void ole2_free(struct ole2_file *of) {
    if (!of) return;
    if (of->fat) free(of->fat);
    if (of->mini_fat) free(of->mini_fat);
    if (of->entries) free(of->entries);
    if (of->fp) fclose(of->fp);
    free(of);
}

/* ============================================================
 * FUNGSI BIFF - PARSING
 * ============================================================ */

/* Parse BIFF record */
static int biff_parse_record(const unsigned char *data, size_t data_len,
                             size_t *pos, struct biff_record *rec) {
    if (*pos + 4 > data_len) return -1;

    rec->type = xls_read_u16(data + *pos);
    rec->length = xls_read_u16(data + *pos + 2);
    *pos += 4;

    if (*pos + rec->length > data_len) return -1;

    rec->data = (unsigned char *)(data + *pos);
    *pos += rec->length;

    return 0;
}

/* Parse SST (Shared String Table) */
static int biff_parse_sst(const unsigned char *data, unsigned short len,
                          struct xls_workbook *wb) {
    unsigned int total;
    unsigned int unique;
    size_t pos = 0;
    int i;

    if (len < 8) return -1;

    total = xls_read_u32(data);
    unique = xls_read_u32(data + 4);
    pos = 8;

    /* Allocate SST */
    wb->sst = calloc(unique, sizeof(struct xls_shared_string));
    if (!wb->sst) return -1;
    wb->sst_capacity = unique;
    wb->sst_count = 0;

    /* Read strings */
    for (i = 0; i < (int)unique && pos < len; i++) {
        unsigned short str_len;
        unsigned char flags;
        int is_unicode = 0;
        int has_rich = 0;
        int has_ext = 0;
        size_t str_size;

        if (pos + 3 > len) break;

        str_len = xls_read_u16(data + pos);
        flags = data[pos + 2];
        pos += 3;

        is_unicode = (flags & 0x01) ? 1 : 0;
        has_rich = (flags & 0x08) ? 1 : 0;
        has_ext = (flags & 0x04) ? 1 : 0;

        /* Skip rich text formatting runs */
        if (has_rich) {
            unsigned short runs = xls_read_u16(data + pos);
            pos += 2;
            (void)runs; /* unused */
        }

        /* Skip extended string data */
        if (has_ext) {
            unsigned int ext_size = xls_read_u32(data + pos);
            pos += 4;
            if (pos + ext_size > len) break;
            pos += ext_size;
        }

        /* Calculate string size in bytes */
        str_size = is_unicode ? str_len * 2 : str_len;
        if (pos + str_size > len) break;

        /* Convert to UTF-8 */
        wb->sst[i].text = malloc(MAKS_TEKS);
        if (wb->sst[i].text) {
            if (is_unicode) {
                utf16le_to_utf8(data + pos, str_size, wb->sst[i].text, MAKS_TEKS);
            } else {
                /* ASCII/Latin-1 */
                int j;
                for (j = 0; j < str_len && j < MAKS_TEKS - 1; j++) {
                    wb->sst[i].text[j] = data[pos + j];
                }
                wb->sst[i].text[j] = '\0';
            }
        }

        pos += str_size;
        wb->sst_count++;
    }

    return 0;
}

/* Parse BIFF8 worksheet stream */
static int biff_parse_worksheet(struct buffer_tabel *buf,
                                 const unsigned char *data,
                                 size_t data_len,
                                 struct xls_workbook *wb) {
    size_t pos = 0;
    struct biff_record rec;
    int bof_found = 0;
    int eof_found = 0;

    while (!eof_found && pos < data_len) {
        if (biff_parse_record(data, data_len, &pos, &rec) != 0) break;

        switch (rec.type) {
            case BIFF_BOF:
            case BIFF_BOF2:
                {
                    unsigned short version = xls_read_u16(rec.data);
                    if (version == BIFF_VERSION_BIFF8) {
                        wb->is_biff8 = 1;
                        wb->biff_version = 8;
                    } else if (version == BIFF_VERSION_BIFF5) {
                        wb->is_biff8 = 0;
                        wb->biff_version = 5;
                    }
                    bof_found = 1;
                }
                break;

            case BIFF_EOF:
                eof_found = 1;
                break;

            case BIFF_DIMENSION:
                if (rec.length >= 14) {
                    unsigned int row_first = xls_read_u32(rec.data);
                    unsigned int row_last = xls_read_u32(rec.data + 4);
                    unsigned short col_first = xls_read_u16(rec.data + 8);
                    unsigned short col_last = xls_read_u16(rec.data + 10);

                    /* Adjust buffer if needed */
                    int rows = row_last - row_first;
                    int cols = col_last - col_first + 1;

                    if (rows > buf->cfg.baris || cols > buf->cfg.kolom) {
                        /* Extend buffer */
                        int new_rows = rows > buf->cfg.baris ? rows : buf->cfg.baris;
                        int new_cols = cols > buf->cfg.kolom ? cols : buf->cfg.kolom;

                        /* Simple extension */
                        if (new_cols > buf->cfg.kolom) {
                            for (int r = 0; r < buf->cfg.baris; r++) {
                                buf->isi[r] = realloc(buf->isi[r], new_cols * sizeof(char *));
                                buf->perataan_sel[r] = realloc(buf->perataan_sel[r], new_cols * sizeof(perataan_teks));
                                for (int c = buf->cfg.kolom; c < new_cols; c++) {
                                    buf->isi[r][c] = calloc(1, MAKS_TEKS);
                                    buf->perataan_sel[r][c] = RATA_KIRI;
                                }
                            }
                            buf->lebar_kolom = realloc(buf->lebar_kolom, new_cols * sizeof(int));
                            buf->tergabung = realloc(buf->tergabung, new_cols * sizeof(struct info_gabung *));
                            for (int c = buf->cfg.kolom; c < new_cols; c++) {
                                buf->lebar_kolom[c] = LEBAR_KOLOM_DEFAULT;
                            }
                            buf->cfg.kolom = new_cols;
                        }
                    }

                    wb->max_row = row_last;
                    wb->max_col = col_last;
                }
                break;

            case BIFF_NUMBER:
                {
                    unsigned short row = xls_read_u16(rec.data);
                    unsigned short col = xls_read_u16(rec.data + 2);
                    double value = xls_read_double(rec.data + 6);

                    if (row < buf->cfg.baris && col < buf->cfg.kolom) {
                        snprintf(buf->isi[row][col], MAKS_TEKS, "%.15g", value);
                    }
                }
                break;

            case BIFF_RK:
                {
                    unsigned short row = xls_read_u16(rec.data);
                    unsigned short col = xls_read_u16(rec.data + 2);
                    double value = decode_rk(rec.data + 6);

                    if (row < buf->cfg.baris && col < buf->cfg.kolom) {
                        snprintf(buf->isi[row][col], MAKS_TEKS, "%.15g", value);
                    }
                }
                break;

            case BIFF_MULRK:
                {
                    unsigned short row = xls_read_u16(rec.data);
                    unsigned short col_first = xls_read_u16(rec.data + 2);
                    unsigned short col_last = xls_read_u16(rec.data + rec.length - 2);
                    int num_rk = (col_last - col_first + 1);
                    size_t offset = 6;

                    for (int i = 0; i < num_rk && offset + 6 <= rec.length; i++) {
                        unsigned short col = col_first + i;
                        /* Skip XF index */
                        offset += 2;
                        double value = decode_rk(rec.data + offset);
                        offset += 4;

                        if (row < buf->cfg.baris && col < buf->cfg.kolom) {
                            snprintf(buf->isi[row][col], MAKS_TEKS, "%.15g", value);
                        }
                    }
                }
                break;

            case BIFF_LABELSST:
                if (wb->is_biff8) {
                    unsigned short row = xls_read_u16(rec.data);
                    unsigned short col = xls_read_u16(rec.data + 2);
                    unsigned int sst_idx = xls_read_u32(rec.data + 6);

                    if (row < buf->cfg.baris && col < buf->cfg.kolom &&
                        sst_idx < (unsigned int)wb->sst_count && wb->sst) {
                        salin_str_aman(buf->isi[row][col], wb->sst[sst_idx].text, MAKS_TEKS);
                    }
                }
                break;

            case BIFF_LABEL:
                /* BIFF2-7 label format */
                {
                    unsigned short row = xls_read_u16(rec.data);
                    unsigned short col = xls_read_u16(rec.data + 2);
                    unsigned char str_len = rec.data[6];
                    const unsigned char *str = rec.data + 7;

                    if (row < buf->cfg.baris && col < buf->cfg.kolom) {
                        int copy_len = str_len < MAKS_TEKS - 1 ? str_len : MAKS_TEKS - 1;
                        memcpy(buf->isi[row][col], str, copy_len);
                        buf->isi[row][col][copy_len] = '\0';
                    }
                }
                break;

            case BIFF_SST:
                biff_parse_sst(rec.data, rec.length, wb);
                break;

            case BIFF_BLANK:
            case BIFF_MULBLANK:
                /* Empty cells - nothing to do */
                break;

            case BIFF_FORMULA:
                /* Formula result - read the value */
                {
                    unsigned short row = xls_read_u16(rec.data);
                    unsigned short col = xls_read_u16(rec.data + 2);

                    /* Check if result is string (recalculated) or number */
                    unsigned char result_flags = rec.data[12];

                    if (!(result_flags & 0x01)) {
                        /* Number result */
                        double value = xls_read_double(rec.data + 6);
                        if (row < buf->cfg.baris && col < buf->cfg.kolom) {
                            snprintf(buf->isi[row][col], MAKS_TEKS, "%.15g", value);
                        }
                    }
                    /* String results are in a separate STRING record */
                }
                break;

            case BIFF_STRING:
                /* String result of formula */
                /* Note: This comes after a FORMULA record */
                break;

            case BIFF_BOOLERR:
                {
                    unsigned short row = xls_read_u16(rec.data);
                    unsigned short col = xls_read_u16(rec.data + 2);
                    unsigned char value = rec.data[6];
                    unsigned char is_error = rec.data[8];

                    if (row < buf->cfg.baris && col < buf->cfg.kolom) {
                        if (is_error) {
                            const char *errors[] = {"#NULL!", "#DIV/0!", "#VALUE!",
                                                    "#REF!", "#NAME?", "#NUM!",
                                                    "#N/A"};
                            if (value < 7) {
                                salin_str_aman(buf->isi[row][col], errors[value], MAKS_TEKS);
                            }
                        } else {
                            snprintf(buf->isi[row][col], MAKS_TEKS, "%s",
                                     value ? "TRUE" : "FALSE");
                        }
                    }
                }
                break;
        }
    }

    return 0;
}

/* ============================================================
 * FUNGSI BIFF - WRITING
 * ============================================================ */

/* Write BIFF record */
static void biff_write_record(FILE *fp, unsigned short type,
                               const unsigned char *data, unsigned short len) {
    unsigned char header[4];

    xls_write_u16(header, type);
    xls_write_u16(header + 2, len);

    fwrite(header, 1, 4, fp);
    if (data && len > 0) {
        fwrite(data, 1, len, fp);
    }
}

/* Write BIFF BOF */
static void biff_write_bof(FILE *fp, int version) {
    unsigned char data[16];

    memset(data, 0, sizeof(data));
    xls_write_u16(data, BIFF_VERSION_BIFF8);  /* BIFF8 */
    xls_write_u16(data + 2, 0x0010);          /* Worksheet */
    xls_write_u16(data + 4, 0);               /* Build */
    xls_write_u16(data + 6, 0);               /* Year */

    biff_write_record(fp, BIFF_BOF, data, 16);
}

/* Write dimension record */
static void biff_write_dimension(FILE *fp, int rows, int cols) {
    unsigned char data[14];

    memset(data, 0, sizeof(data));
    xls_write_u32(data, 0);                /* First row */
    xls_write_u32(data + 4, rows > 0 ? rows - 1 : 0); /* Last row */
    xls_write_u16(data + 8, 0);            /* First col */
    xls_write_u16(data + 10, cols > 0 ? cols - 1 : 0); /* Last col */

    biff_write_record(fp, BIFF_DIMENSION, data, 14);
}

/* Write number cell */
static void biff_write_number(FILE *fp, int row, int col, double value) {
    unsigned char data[14];

    xls_write_u16(data, row);
    xls_write_u16(data + 2, col);
    xls_write_u16(data + 4, 0);            /* XF index */
    xls_write_double(data + 6, value);

    biff_write_record(fp, BIFF_NUMBER, data, 14);
}

/* Write label cell */
static void biff_write_label(FILE *fp, int row, int col, const char *text) {
    unsigned char data[MAKS_TEKS + 10];
    int text_len = strlen(text);
    int utf16_len;
    unsigned short total_len;

    xls_write_u16(data, row);
    xls_write_u16(data + 2, col);
    xls_write_u16(data + 4, 0);            /* XF index */

    /* Write string in UTF-16LE for BIFF8 */
    utf16_len = utf8_to_utf16le(text, data + 7, MAKS_TEKS * 2);
    xls_write_u16(data + 5, utf16_len / 2); /* Character count */
    data[7 + utf16_len] = 0x00;            /* Flags: Unicode */

    total_len = 8 + utf16_len;
    biff_write_record(fp, BIFF_LABEL, data, total_len);
}

/* Write EOF */
static void biff_write_eof(FILE *fp) {
    biff_write_record(fp, BIFF_EOF, NULL, 0);
}

/* ============================================================
 * FUNGSI OLE2 - WRITING
 * ============================================================
 * Untuk simplicitas, kita menulis XLS tanpa OLE2 container
 * (format BIFF stream langsung yang masih bisa dibaca)
 * ============================================================ */

/* Simple XLS writer - BIFF stream only */
static int write_simple_xls(const struct buffer_tabel *buf, const char *fn) {
    FILE *fp;
    int r, c;

    fp = fopen(fn, "wb");
    if (!fp) return -1;

    /* Write BOF */
    biff_write_bof(fp, 8);

    /* Write codepage */
    {
        unsigned char cp_data[2];
        xls_write_u16(cp_data, 0x04E4);  /* UTF-16 */
        biff_write_record(fp, BIFF_CODEPAGE, cp_data, 2);
    }

    /* Write dimension */
    biff_write_dimension(fp, buf->cfg.baris, buf->cfg.kolom);

    /* Write cells */
    for (r = 0; r < buf->cfg.baris; r++) {
        for (c = 0; c < buf->cfg.kolom; c++) {
            const char *val = buf->isi[r][c];
            if (val && strlen(val) > 0) {
                /* Try to parse as number */
                char *endptr;
                double num = strtod(val, &endptr);

                if (*endptr == '\0' && endptr != val) {
                    /* Valid number */
                    biff_write_number(fp, r, c, num);
                } else {
                    /* Text */
                    biff_write_label(fp, r, c, val);
                }
            }
        }
    }

    /* Write EOF */
    biff_write_eof(fp);

    fclose(fp);
    return 0;
}

/* ============================================================
 * OLE2 Container Writer
 * ============================================================ */

/* Create proper OLE2 file with Workbook stream */
static int write_ole2_xls(const struct buffer_tabel *buf, const char *fn) {
    FILE *fp;
    unsigned char header[512];
    unsigned char sector[512];
    unsigned char *stream_data;
    size_t stream_size;
    size_t stream_sectors;
    int i;

    /* First, create the BIFF stream in memory */
    stream_data = malloc(buf->cfg.baris * buf->cfg.kolom * 32 + 1024);
    if (!stream_data) return -1;

    /* Build BIFF stream */
    size_t pos = 0;
    unsigned char *p = stream_data;

    /* BOF */
    xls_write_u16(p + pos, BIFF_BOF); pos += 2;
    xls_write_u16(p + pos, 16); pos += 2;
    xls_write_u16(p + pos, BIFF_VERSION_BIFF8); pos += 2;
    xls_write_u16(p + pos, 0x0010); pos += 2;
    memset(p + pos, 0, 12); pos += 12;

    /* Codepage */
    xls_write_u16(p + pos, BIFF_CODEPAGE); pos += 2;
    xls_write_u16(p + pos, 2); pos += 2;
    xls_write_u16(p + pos, 0x04E4); pos += 2;

    /* Dimension */
    xls_write_u16(p + pos, BIFF_DIMENSION); pos += 2;
    xls_write_u16(p + pos, 14); pos += 2;
    xls_write_u32(p + pos, 0); pos += 4;
    xls_write_u32(p + pos, buf->cfg.baris > 0 ? buf->cfg.baris - 1 : 0); pos += 4;
    xls_write_u16(p + pos, 0); pos += 2;
    xls_write_u16(p + pos, buf->cfg.kolom > 0 ? buf->cfg.kolom - 1 : 0); pos += 2;

    /* Write cells */
    for (int r = 0; r < buf->cfg.baris; r++) {
        for (int c = 0; c < buf->cfg.kolom; c++) {
            const char *val = buf->isi[r][c];
            if (val && strlen(val) > 0) {
                char *endptr;
                double num = strtod(val, &endptr);

                if (*endptr == '\0' && endptr != val) {
                    /* Number */
                    xls_write_u16(p + pos, BIFF_NUMBER); pos += 2;
                    xls_write_u16(p + pos, 14); pos += 2;
                    xls_write_u16(p + pos, r); pos += 2;
                    xls_write_u16(p + pos, c); pos += 2;
                    xls_write_u16(p + pos, 0); pos += 2;  /* XF */
                    xls_write_double(p + pos, num); pos += 8;
                } else {
                    /* Label */
                    int utf16_len = utf8_to_utf16le(val, p + pos + 11, MAKS_TEKS * 2);
                    xls_write_u16(p + pos, BIFF_LABEL); pos += 2;
                    xls_write_u16(p + pos, 8 + utf16_len); pos += 2;
                    xls_write_u16(p + pos, r); pos += 2;
                    xls_write_u16(p + pos, c); pos += 2;
                    xls_write_u16(p + pos, 0); pos += 2;  /* XF */
                    xls_write_u16(p + pos, utf16_len / 2); pos += 2;
                    p[pos++] = 0x01;  /* Unicode flag */
                    pos += utf16_len;
                }
            }
        }
    }

    /* EOF */
    xls_write_u16(p + pos, BIFF_EOF); pos += 2;
    xls_write_u16(p + pos, 0); pos += 2;

    stream_size = pos;
    stream_sectors = (stream_size + 511) / 512;

    /* Now create the OLE2 file */
    fp = fopen(fn, "wb");
    if (!fp) {
        free(stream_data);
        return -1;
    }

    /* Write OLE2 header */
    memset(header, 0, 512);
    memcpy(header, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8);  /* Signature */
    xls_write_u16(header + 24, 0x0003);   /* Minor version */
    xls_write_u16(header + 26, 0x0003);   /* Major version */
    xls_write_u16(header + 28, 0xFFFE);   /* Little-endian */
    xls_write_u16(header + 30, 0x0009);   /* Sector size = 512 */
    xls_write_u16(header + 32, 0x0006);   /* Mini sector size = 64 */
    xls_write_u32(header + 44, 0);        /* Total sectors (unknown) */
    xls_write_u32(header + 48, 1);        /* 1 FAT sector */
    xls_write_u32(header + 52, 1);        /* First dir at sector 1 */
    xls_write_u32(header + 60, 4096);     /* Mini stream cutoff */
    xls_write_u32(header + 64, 0xFFFFFFFE); /* No mini FAT */
    xls_write_u32(header + 72, 0xFFFFFFFE); /* No DIFAT */
    xls_write_u32(header + 76, 0);        /* First FAT at sector 0 */

    fwrite(header, 1, 512, fp);

    /* Write FAT sector (sector 0) */
    memset(sector, 0, 512);
    xls_write_u32(sector, 0xFFFFFFFD);    /* FAT sector marker */
    xls_write_u32(sector + 4, 0xFFFFFFFE); /* End of chain for dir */
    for (i = 0; i < (int)stream_sectors - 1; i++) {
        xls_write_u32(sector + 8 + i * 4, 3 + i + 1);
    }
    xls_write_u32(sector + 8 + (stream_sectors - 1) * 4, 0xFFFFFFFE);
    fwrite(sector, 1, 512, fp);

    /* Write directory sector (sector 1) */
    memset(sector, 0, 512);

    /* Root entry */
    unsigned char *root = sector;
    memcpy(root, "R\0o\0o\0t\0 \0E\0n\0t\0r\0y", 22);
    xls_write_u16(root + 64, 22);
    root[66] = OLE2_TYPE_ROOT;
    xls_write_u32(root + 116, 0xFFFFFFFE); /* No mini stream */
    xls_write_u32(root + 120, 0);

    /* Workbook entry */
    unsigned char *wb_entry = sector + 128;
    memcpy(wb_entry, "W\0o\0r\0k\0b\0o\0o\0k", 16);
    xls_write_u16(wb_entry + 64, 16);
    wb_entry[66] = OLE2_TYPE_STREAM;
    xls_write_u32(wb_entry + 116, 2);     /* Start at sector 2 */
    xls_write_u32(wb_entry + 120, stream_size); /* Size */

    fwrite(sector, 1, 512, fp);

    /* Write workbook stream data (starting at sector 2) */
    /* Pad to sector boundary */
    fwrite(stream_data, 1, stream_size, fp);
    size_t padding = stream_sectors * 512 - stream_size;
    if (padding > 0) {
        memset(sector, 0, 512);
        fwrite(sector, 1, padding, fp);
    }

    free(stream_data);
    fclose(fp);
    return 0;
}

/* ============================================================
 * FUNGSI PUBLIK
 * ============================================================ */

/* Buka file XLS */
int buka_xls(struct buffer_tabel **pbuf, const char *fn) {
    struct ole2_file *of;
    struct ole2_dir_entry *stream;
    struct xls_workbook wb;
    unsigned char *data;
    size_t data_size;
    int result;

    if (!pbuf || !fn) return -1;

    /* Initialize OLE2 reader */
    of = calloc(1, sizeof(*of));
    if (!of) return -1;

    of->fp = fopen(fn, "rb");
    if (!of->fp) {
        xls_set_error("Cannot open file: %s", fn);
        free(of);
        return -1;
    }

    /* Read OLE2 structure */
    if (ole2_read_header(of) != 0) {
        ole2_free(of);
        return -1;
    }

    if (ole2_read_fat(of) != 0) {
        ole2_free(of);
        return -1;
    }

    if (ole2_read_directory(of) != 0) {
        ole2_free(of);
        return -1;
    }

    /* Find Workbook stream */
    stream = ole2_find_stream(of, "Workbook");
    if (!stream) {
        /* Try "Book" for older formats */
        stream = ole2_find_stream(of, "Book");
    }

    if (!stream) {
        xls_set_error("No Workbook stream found");
        ole2_free(of);
        return -1;
    }

    /* Read stream data */
    data = ole2_read_stream(of, stream, &data_size);
    ole2_free(of);

    if (!data) {
        xls_set_error("Cannot read workbook stream");
        return -1;
    }

    /* Initialize workbook structure */
    memset(&wb, 0, sizeof(wb));

    /* Create initial buffer */
    bebas_buffer(*pbuf);
    *pbuf = buat_buffer(100, 26);

    /* Parse BIFF records */
    result = biff_parse_worksheet(*pbuf, data, data_size, &wb);

    /* Free SST */
    if (wb.sst) {
        for (int i = 0; i < wb.sst_count; i++) {
            if (wb.sst[i].text) free(wb.sst[i].text);
        }
        free(wb.sst);
    }

    free(data);

    return result;
}

/* Simpan file XLS */
int simpan_xls(const struct buffer_tabel *buf, const char *fn) {
    if (!buf || !fn) return -1;

    /* Use OLE2 container format */
    return write_ole2_xls(buf, fn);
}

/* Cek validitas file XLS */
int xls_apakah_valid(const char *fn) {
    FILE *fp;
    unsigned char sig[8];

    fp = fopen(fn, "rb");
    if (!fp) return 0;

    if (fread(sig, 1, 8, fp) != 8) {
        fclose(fp);
        return 0;
    }

    fclose(fp);

    /* Check OLE2 signature */
    return memcmp(sig, "\xD0\xCF\x11\xE0\xA1\xB1\x1A\xE1", 8) == 0;
}
