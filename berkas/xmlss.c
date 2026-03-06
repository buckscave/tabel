/* ============================================================
 * TABEL v3.0
 * Berkas: xmlss.c - Format Berkas XML Spreadsheet (Excel 2002-2003)
 * Standar: C89 + POSIX.1-2008
 * Penulis: Chandra Lesmana
 * ============================================================ */

#include "tabel.h"

/* ============================================================
 * KONSTANTA XML SPREADSHEET
 * ============================================================ */
#define XMLSS_MAX_CELLS         100000
#define XMLSS_MAX_STYLES        256
#define XMLSS_BUFFER_SIZE       (64 * 1024)

/* XML Spreadsheet namespace */
#define XMLSS_NS_SS             "ss"
#define XMLSS_NS_O              "o"
#define XMLSS_NS_X              "x"
#define XMLSS_NS_HTML           "html"

/* ============================================================
 * STRUKTUR DATA
 * ============================================================ */

/* Cell data type */
typedef enum {
    XMLSS_TYPE_STRING = 0,
    XMLSS_TYPE_NUMBER,
    XMLSS_TYPE_BOOLEAN,
    XMLSS_TYPE_DATE,
    XMLSS_TYPE_ERROR
} xmlss_cell_type;

/* Cell structure */
struct xmlss_cell {
    int row;
    int col;
    char *value;
    xmlss_cell_type type;
    int merged;             /* Is this cell merged? */
    int merge_row;          /* Merge span rows */
    int merge_col;          /* Merge span columns */
};

/* Style structure */
struct xmlss_style {
    char id[32];
    char name[64];
    /* Add more style properties as needed */
};

/* Worksheet structure */
struct xmlss_worksheet {
    char name[128];
    struct xmlss_cell *cells;
    int num_cells;
    int max_cells;
    int max_row;
    int max_col;
};

/* Workbook structure */
struct xmlss_workbook {
    struct xmlss_worksheet *sheets;
    int num_sheets;
    struct xmlss_style *styles;
    int num_styles;
};

/* XML parser state */
struct xmlss_parser {
    const char *input;
    int input_len;
    int pos;
    int in_row;
    int in_cell;
    int current_row;
    int current_col;
    int row_index;
    int col_index;
};

/* ============================================================
 * VARIABEL GLOBAL
 * ============================================================ */
static char xmlss_error_msg[512] = "";

/* ============================================================
 * FUNGSI HELPER
 * ============================================================ */

static void xmlss_set_error(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vsnprintf(xmlss_error_msg, sizeof(xmlss_error_msg), fmt, args);
    va_end(args);
}

const char *xmlss_get_error(void) {
    return xmlss_error_msg;
}

/* XML escape */
static int xmlss_escape(const char *input, char *output, size_t output_size) {
    size_t i = 0, j = 0;

    while (input[i] && j < output_size - 8) {
        switch (input[i]) {
            case '&':
                strcpy(output + j, "&amp;");
                j += 5;
                break;
            case '<':
                strcpy(output + j, "&lt;");
                j += 4;
                break;
            case '>':
                strcpy(output + j, "&gt;");
                j += 4;
                break;
            case '"':
                strcpy(output + j, "&quot;");
                j += 6;
                break;
            case '\'':
                strcpy(output + j, "&apos;");
                j += 6;
                break;
            case '\n':
                strcpy(output + j, "&#10;");
                j += 5;
                break;
            case '\r':
                strcpy(output + j, "&#13;");
                j += 5;
                break;
            case '\t':
                strcpy(output + j, "&#9;");
                j += 4;
                break;
            default:
                output[j++] = input[i];
                break;
        }
        i++;
    }
    output[j] = '\0';
    return (int)j;
}

/* XML unescape */
static int xmlss_unescape(const char *input, char *output, size_t output_size) {
    size_t i = 0, j = 0;

    while (input[i] && j < output_size - 1) {
        if (input[i] == '&') {
            if (strncmp(input + i, "&amp;", 5) == 0) {
                output[j++] = '&';
                i += 5;
            } else if (strncmp(input + i, "&lt;", 4) == 0) {
                output[j++] = '<';
                i += 4;
            } else if (strncmp(input + i, "&gt;", 4) == 0) {
                output[j++] = '>';
                i += 4;
            } else if (strncmp(input + i, "&quot;", 6) == 0) {
                output[j++] = '"';
                i += 6;
            } else if (strncmp(input + i, "&apos;", 6) == 0) {
                output[j++] = '\'';
                i += 6;
            } else if (strncmp(input + i, "&#10;", 5) == 0) {
                output[j++] = '\n';
                i += 5;
            } else if (strncmp(input + i, "&#13;", 5) == 0) {
                output[j++] = '\r';
                i += 5;
            } else if (strncmp(input + i, "&#9;", 4) == 0) {
                output[j++] = '\t';
                i += 4;
            } else if (strncmp(input + i, "&#x", 3) == 0) {
                /* Hex entity */
                unsigned int cp = 0;
                i += 3;
                while (isxdigit((unsigned char)input[i])) {
                    cp = cp * 16;
                    if (input[i] >= '0' && input[i] <= '9') cp += input[i] - '0';
                    else if (input[i] >= 'A' && input[i] <= 'F') cp += input[i] - 'A' + 10;
                    else if (input[i] >= 'a' && input[i] <= 'f') cp += input[i] - 'a' + 10;
                    i++;
                }
                if (input[i] == ';') i++;
                if (cp < 0x80) output[j++] = (char)cp;
                else if (cp < 0x800) {
                    output[j++] = (char)(0xC0 | (cp >> 6));
                    output[j++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    output[j++] = (char)(0xE0 | (cp >> 12));
                    output[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    output[j++] = (char)(0x80 | (cp & 0x3F));
                }
            } else if (strncmp(input + i, "&#", 2) == 0) {
                /* Decimal entity */
                unsigned int cp = 0;
                i += 2;
                while (isdigit((unsigned char)input[i])) {
                    cp = cp * 10 + (input[i] - '0');
                    i++;
                }
                if (input[i] == ';') i++;
                if (cp < 0x80) output[j++] = (char)cp;
                else if (cp < 0x800) {
                    output[j++] = (char)(0xC0 | (cp >> 6));
                    output[j++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    output[j++] = (char)(0xE0 | (cp >> 12));
                    output[j++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    output[j++] = (char)(0x80 | (cp & 0x3F));
                }
            } else {
                output[j++] = input[i++];
            }
        } else {
            output[j++] = input[i++];
        }
    }
    output[j] = '\0';
    return (int)j;
}

/* ============================================================
 * XML PARSING FUNCTIONS
 * ============================================================ */

/* Skip whitespace */
static void xmlss_skip_ws(struct xmlss_parser *xp) {
    while (xp->pos < xp->input_len && 
           isspace((unsigned char)xp->input[xp->pos])) {
        xp->pos++;
    }
}

/* Check if at specific string */
static int xmlss_at_string(struct xmlss_parser *xp, const char *s) {
    size_t len = strlen(s);
    if (xp->pos + len > xp->input_len) return 0;
    return strncmp(xp->input + xp->pos, s, len) == 0;
}

/* Skip until specific string */
static void xmlss_skip_until(struct xmlss_parser *xp, const char *s) {
    size_t len = strlen(s);
    while (xp->pos + len <= xp->input_len) {
        if (strncmp(xp->input + xp->pos, s, len) == 0) {
            return;
        }
        xp->pos++;
    }
}

/* Skip XML declaration or comment */
static void xmlss_skip_pi(struct xmlss_parser *xp) {
    if (xp->input[xp->pos] == '<') {
        if (xp->pos + 1 < xp->input_len) {
            if (xp->input[xp->pos + 1] == '?') {
                /* Processing instruction */
                xp->pos += 2;
                while (xp->pos < xp->input_len - 1) {
                    if (xp->input[xp->pos] == '?' && xp->input[xp->pos + 1] == '>') {
                        xp->pos += 2;
                        break;
                    }
                    xp->pos++;
                }
            } else if (xp->pos + 3 < xp->input_len &&
                       strncmp(xp->input + xp->pos, "<!--", 4) == 0) {
                /* Comment */
                xp->pos += 4;
                while (xp->pos < xp->input_len - 2) {
                    if (strncmp(xp->input + xp->pos, "-->", 3) == 0) {
                        xp->pos += 3;
                        break;
                    }
                    xp->pos++;
                }
            }
        }
    }
}

/* Parse identifier */
static int xmlss_parse_ident(struct xmlss_parser *xp, char *buf, size_t buf_size) {
    size_t i = 0;

    while (xp->pos < xp->input_len && i < buf_size - 1) {
        char c = xp->input[xp->pos];
        if (isalnum((unsigned char)c) || c == '_' || c == '-' || c == ':' || c == '.') {
            buf[i++] = c;
            xp->pos++;
        } else {
            break;
        }
    }
    buf[i] = '\0';
    return (int)i;
}

/* Parse quoted string */
static int xmlss_parse_quoted(struct xmlss_parser *xp, char *buf, size_t buf_size) {
    char quote;
    size_t i = 0;

    if (xp->pos >= xp->input_len) return -1;

    quote = xp->input[xp->pos];
    if (quote != '"' && quote != '\'') return -1;
    xp->pos++;

    while (xp->pos < xp->input_len && i < buf_size - 1) {
        char c = xp->input[xp->pos];
        if (c == quote) {
            xp->pos++;
            buf[i] = '\0';
            return (int)i;
        }
        buf[i++] = c;
        xp->pos++;
    }

    buf[i] = '\0';
    return -1;
}

/* Get local name from qualified name */
static const char *xmlss_local_name(const char *qname) {
    const char *colon = strchr(qname, ':');
    return colon ? colon + 1 : qname;
}

/* Find element end and extract content */
static char *xmlss_extract_content(struct xmlss_parser *xp, const char *tag_name) {
    size_t start = xp->pos;
    size_t len;
    char *content;
    char end_tag[256];

    snprintf(end_tag, sizeof(end_tag), "</%s>", tag_name);

    /* Find end tag */
    while (xp->pos < xp->input_len) {
        if (xp->input[xp->pos] == '<') {
            if (xmlss_at_string(xp, end_tag)) {
                break;
            }
        }
        xp->pos++;
    }

    len = xp->pos - start;
    content = malloc(len + 1);
    if (!content) return NULL;

    memcpy(content, xp->input + start, len);
    content[len] = '\0';

    /* Unescape */
    {
        char *unescaped = malloc(len + 1);
        if (unescaped) {
            xmlss_unescape(content, unescaped, len + 1);
            free(content);
            return unescaped;
        }
    }

    return content;
}

/* ============================================================
 * PARSE XML SPREADSHEET
 * ============================================================ */

/* Parse cell attributes */
static void parse_cell_attrs(struct xmlss_parser *xp, struct buffer_tabel *buf,
                             int row_idx, int *col_idx) {
    char attr_name[128];
    char attr_value[MAKS_TEKS];
    int merge_down = 0;
    int merge_across = 0;
    int index = -1;

    xmlss_skip_ws(xp);

    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>' &&
           xp->input[xp->pos] != '/') {

        /* Parse attribute name */
        xmlss_skip_ws(xp);
        if (xp->input[xp->pos] == '>' || xp->input[xp->pos] == '/') break;

        if (xmlss_parse_ident(xp, attr_name, sizeof(attr_name)) == 0) break;

        xmlss_skip_ws(xp);
        if (xp->input[xp->pos] != '=') break;
        xp->pos++;
        xmlss_skip_ws(xp);

        if (xmlss_parse_quoted(xp, attr_value, sizeof(attr_value)) < 0) break;

        /* Check for relevant attributes */
        const char *local = xmlss_local_name(attr_name);

        if (strcmp(local, "Index") == 0) {
            index = atoi(attr_value);
        } else if (strcmp(local, "MergeAcross") == 0) {
            merge_across = atoi(attr_value);
        } else if (strcmp(local, "MergeDown") == 0) {
            merge_down = atoi(attr_value);
        }
    }

    /* Calculate column position */
    if (index > 0) {
        *col_idx = index - 1;
    }

    /* Handle merge */
    if (merge_down > 0 || merge_across > 0) {
        if (*col_idx < buf->cfg.kolom && row_idx < buf->cfg.baris) {
            atur_blok_gabung_buf(buf, *col_idx, row_idx, merge_across + 1, merge_down + 1);
        }
    }
}

/* Parse Data element */
static void parse_data_element(struct xmlss_parser *xp, struct buffer_tabel *buf,
                               int row_idx, int col_idx) {
    char attr_name[128];
    char attr_value[MAKS_TEKS];
    char *content;
    char data_type[32] = "String";

    xmlss_skip_ws(xp);

    /* Parse attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>' &&
           xp->input[xp->pos] != '/') {

        xmlss_skip_ws(xp);
        if (xp->input[xp->pos] == '>' || xp->input[xp->pos] == '/') break;

        if (xmlss_parse_ident(xp, attr_name, sizeof(attr_name)) == 0) break;

        xmlss_skip_ws(xp);
        if (xp->input[xp->pos] != '=') break;
        xp->pos++;
        xmlss_skip_ws(xp);

        if (xmlss_parse_quoted(xp, attr_value, sizeof(attr_value)) < 0) break;

        const char *local = xmlss_local_name(attr_name);
        if (strcmp(local, "Type") == 0) {
            salin_str_aman(data_type, attr_value, sizeof(data_type));
        }
    }

    /* Skip to content */
    if (xp->input[xp->pos] == '>') xp->pos++;

    /* Extract content */
    content = xmlss_extract_content(xp, "Data");

    if (content && row_idx < buf->cfg.baris && col_idx < buf->cfg.kolom) {
        /* Handle different data types */
        const char *local_type = xmlss_local_name(data_type);

        if (strcmp(local_type, "Number") == 0) {
            /* Keep as number string */
            salin_str_aman(buf->isi[row_idx][col_idx], content, MAKS_TEKS);
        } else if (strcmp(local_type, "Boolean") == 0) {
            salin_str_aman(buf->isi[row_idx][col_idx],
                          strcmp(content, "1") == 0 ? "TRUE" : "FALSE", MAKS_TEKS);
        } else if (strcmp(local_type, "DateTime") == 0) {
            salin_str_aman(buf->isi[row_idx][col_idx], content, MAKS_TEKS);
        } else {
            /* String or other */
            salin_str_aman(buf->isi[row_idx][col_idx], content, MAKS_TEKS);
        }
    }

    if (content) free(content);

    /* Skip end tag */
    if (xmlss_at_string(xp, "</Data>")) {
        xp->pos += 7;
    } else if (xmlss_at_string(xp, "</ss:Data>")) {
        xp->pos += 10;
    }
}

/* Parse Cell element */
static void parse_cell_element(struct xmlss_parser *xp, struct buffer_tabel *buf,
                               int row_idx, int *col_idx) {
    xmlss_skip_ws(xp);

    /* Parse cell attributes */
    parse_cell_attrs(xp, buf, row_idx, col_idx);

    /* Skip to content */
    while (xp->pos < xp->input_len) {
        if (xp->input[xp->pos] == '>') {
            xp->pos++;
            break;
        } else if (xp->input[xp->pos] == '/' && xp->pos + 1 < xp->input_len &&
                   xp->input[xp->pos + 1] == '>') {
            /* Self-closing */
            xp->pos += 2;
            (*col_idx)++;
            return;
        }
        xp->pos++;
    }

    /* Parse cell content */
    while (xp->pos < xp->input_len) {
        xmlss_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            if (xmlss_at_string(xp, "</Cell") || xmlss_at_string(xp, "</ss:Cell")) {
                break;
            } else if (xmlss_at_string(xp, "<Data") || xmlss_at_string(xp, "<ss:Data")) {
                /* Skip to Data */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;

                parse_data_element(xp, buf, row_idx, *col_idx);
            } else {
                /* Skip other elements */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }

    /* Skip end tag */
    xmlss_skip_until(xp, ">");
    if (xp->input[xp->pos] == '>') xp->pos++;

    (*col_idx)++;
}

/* Parse Row element */
static void parse_row_element(struct xmlss_parser *xp, struct buffer_tabel *buf) {
    char attr_name[128];
    char attr_value[MAKS_TEKS];
    int row_index = -1;
    int col_idx = 0;

    xmlss_skip_ws(xp);

    /* Parse row attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>' &&
           xp->input[xp->pos] != '/') {

        xmlss_skip_ws(xp);
        if (xp->input[xp->pos] == '>' || xp->input[xp->pos] == '/') break;

        if (xmlss_parse_ident(xp, attr_name, sizeof(attr_name)) == 0) break;

        xmlss_skip_ws(xp);
        if (xp->input[xp->pos] != '=') break;
        xp->pos++;
        xmlss_skip_ws(xp);

        if (xmlss_parse_quoted(xp, attr_value, sizeof(attr_value)) < 0) break;

        const char *local = xmlss_local_name(attr_name);
        if (strcmp(local, "Index") == 0) {
            row_index = atoi(attr_value) - 1;
        }
    }

    /* Determine row number */
    if (row_index < 0) {
        row_index = xp->row_index;
    }
    xp->row_index = row_index + 1;

    /* Skip to content */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    /* Parse cells */
    col_idx = 0;
    while (xp->pos < xp->input_len) {
        xmlss_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            if (xmlss_at_string(xp, "</Row") || xmlss_at_string(xp, "</ss:Row")) {
                break;
            } else if (xmlss_at_string(xp, "<Cell") || xmlss_at_string(xp, "<ss:Cell")) {
                /* Skip to element start */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;

                /* Back up to parse properly */
                /* Actually, let's find the Cell start */
                const char *cell_start = xp->input + xp->pos;

                /* Find <Cell or <ss:Cell */
                while (xp->pos > 0) {
                    if (xp->input[xp->pos] == '<') {
                        const char *tag = xp->input + xp->pos;
                        if (strncmp(tag, "<Cell", 5) == 0 || strncmp(tag, "<ss:Cell", 8) == 0) {
                            break;
                        }
                    }
                    xp->pos--;
                }

                parse_cell_element(xp, buf, row_index, &col_idx);
            } else {
                /* Skip other elements */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }

    /* Skip end tag */
    xmlss_skip_until(xp, ">");
    if (xp->input[xp->pos] == '>') xp->pos++;
}

/* Parse Table element */
static void parse_table_element(struct xmlss_parser *xp, struct buffer_tabel *buf) {
    xmlss_skip_ws(xp);

    /* Skip attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    /* Parse rows */
    xp->row_index = 0;
    while (xp->pos < xp->input_len) {
        xmlss_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            if (xmlss_at_string(xp, "</Table") || xmlss_at_string(xp, "</ss:Table") ||
                xmlss_at_string(xp, "</Worksheet") || xmlss_at_string(xp, "</ss:Worksheet")) {
                break;
            } else if (xmlss_at_string(xp, "<Row") || xmlss_at_string(xp, "<ss:Row")) {
                /* Find proper start */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                /* Backtrack */
                while (xp->pos > 0) {
                    if (xp->input[xp->pos] == '<') {
                        const char *tag = xp->input + xp->pos;
                        if (strncmp(tag, "<Row", 4) == 0 || strncmp(tag, "<ss:Row", 7) == 0) {
                            break;
                        }
                    }
                    xp->pos--;
                }
                parse_row_element(xp, buf);
            } else {
                /* Skip other elements */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }
}

/* Parse Worksheet element */
static void parse_worksheet_element(struct xmlss_parser *xp, struct buffer_tabel *buf) {
    xmlss_skip_ws(xp);

    /* Skip attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    /* Parse content */
    while (xp->pos < xp->input_len) {
        xmlss_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            if (xmlss_at_string(xp, "</Worksheet") || xmlss_at_string(xp, "</ss:Worksheet")) {
                break;
            } else if (xmlss_at_string(xp, "<Table") || xmlss_at_string(xp, "<ss:Table")) {
                parse_table_element(xp, buf);
                break;  /* Only parse first table */
            } else if (xp->input[xp->pos + 1] == '/') {
                /* End tag */
                break;
            } else {
                /* Skip other elements */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }
}

/* Parse Workbook element */
static void parse_workbook_element(struct xmlss_parser *xp, struct buffer_tabel *buf) {
    xmlss_skip_ws(xp);

    /* Skip attributes */
    while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
        xp->pos++;
    }
    if (xp->input[xp->pos] == '>') xp->pos++;

    /* Parse content */
    while (xp->pos < xp->input_len) {
        xmlss_skip_ws(xp);

        if (xp->input[xp->pos] == '<') {
            xmlss_skip_pi(xp);

            if (xmlss_at_string(xp, "</Workbook")) {
                break;
            } else if (xmlss_at_string(xp, "<Worksheet") || xmlss_at_string(xp, "<ss:Worksheet")) {
                parse_worksheet_element(xp, buf);
                break;  /* Only parse first worksheet */
            } else if (xp->input[xp->pos + 1] == '/') {
                break;
            } else {
                /* Skip other elements */
                while (xp->pos < xp->input_len && xp->input[xp->pos] != '>') {
                    xp->pos++;
                }
                if (xp->input[xp->pos] == '>') xp->pos++;
            }
        } else {
            xp->pos++;
        }
    }
}

/* Main parse function */
static int xmlss_parse(const char *input, int input_len, struct buffer_tabel *buf) {
    struct xmlss_parser xp;

    memset(&xp, 0, sizeof(xp));
    xp.input = input;
    xp.input_len = input_len;
    xp.pos = 0;

    /* Skip XML declaration */
    xmlss_skip_ws(&xp);
    xmlss_skip_pi(&xp);

    /* Find root element */
    while (xp.pos < xp.input_len) {
        xmlss_skip_ws(&xp);

        if (xp.input[xp.pos] == '<') {
            xmlss_skip_pi(&xp);

            if (xmlss_at_string(&xp, "<Workbook")) {
                parse_workbook_element(&xp, buf);
                break;
            } else if (xmlss_at_string(&xp, "<Table")) {
                parse_table_element(&xp, buf);
                break;
            } else {
                /* Skip unknown element */
                while (xp.pos < xp.input_len && xp.input[xp.pos] != '>') {
                    xp.pos++;
                }
                if (xp.input[xp.pos] == '>') xp.pos++;
            }
        } else {
            xp.pos++;
        }
    }

    return 0;
}

/* ============================================================
 * WRITE XML SPREADSHEET
 * ============================================================ */

/* Write XML Spreadsheet */
int write_xml_spreadsheet(const struct buffer_tabel *buf, const char *fn) {
    FILE *fp;
    char escaped[MAKS_TEKS * 2];
    int r, c;

    if (!buf || !fn) return -1;

    fp = fopen(fn, "w");
    if (!fp) {
        xmlss_set_error("Cannot create file: %s", fn);
        return -1;
    }

    /* XML declaration */
    fprintf(fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");

    /* Processing instructions for Excel */
    fprintf(fp, "<?mso-application progid=\"Excel.Sheet\"?>\n");

    /* Workbook root */
    fprintf(fp, "<Workbook xmlns=\"urn:schemas-microsoft-com:office:spreadsheet\"\n");
    fprintf(fp, " xmlns:ss=\"urn:schemas-microsoft-com:office:spreadsheet\"\n");
    fprintf(fp, " xmlns:o=\"urn:schemas-microsoft-com:office:office\"\n");
    fprintf(fp, " xmlns:x=\"urn:schemas-microsoft-com:office:excel\"\n");
    fprintf(fp, " xmlns:html=\"http://www.w3.org/TR/REC-html40\">\n");

    /* Document properties */
    fprintf(fp, " <DocumentProperties xmlns=\"urn:schemas-microsoft-com:office:office\">\n");
    fprintf(fp, "  <Author>TABEL 3.0</Author>\n");
    fprintf(fp, "  <Created>%s</Created>\n", "2024-01-01T00:00:00Z");
    fprintf(fp, " </DocumentProperties>\n");

    /* ExcelWorkbook */
    fprintf(fp, " <ExcelWorkbook xmlns=\"urn:schemas-microsoft-com:office:excel\">\n");
    fprintf(fp, "  <WindowHeight>10000</WindowHeight>\n");
    fprintf(fp, "  <WindowWidth>20000</WindowWidth>\n");
    fprintf(fp, " </ExcelWorkbook>\n");

    /* Styles */
    fprintf(fp, " <Styles>\n");
    fprintf(fp, "  <Style ss:ID=\"Default\">\n");
    fprintf(fp, "   <Alignment ss:Vertical=\"Bottom\"/>\n");
    fprintf(fp, "  </Style>\n");
    fprintf(fp, " </Styles>\n");

    /* Worksheet */
    fprintf(fp, " <Worksheet ss:Name=\"Sheet1\">\n");

    /* Table */
    fprintf(fp, "  <Table ss:ExpandedColumnCount=\"%d\" ss:ExpandedRowCount=\"%d\">\n",
            buf->cfg.kolom, buf->cfg.baris);

    /* Column widths */
    for (c = 0; c < buf->cfg.kolom; c++) {
        fprintf(fp, "   <Column ss:Index=\"%d\" ss:Width=\"%d\"/>\n",
                c + 1, buf->lebar_kolom[c] * 7);
    }

    /* Rows and cells */
    for (r = 0; r < buf->cfg.baris; r++) {
        int has_data = 0;

        /* Check if row has any data */
        for (c = 0; c < buf->cfg.kolom; c++) {
            if (buf->isi[r][c] && strlen(buf->isi[r][c]) > 0) {
                has_data = 1;
                break;
            }
        }

        if (!has_data) {
            /* Write empty row */
            fprintf(fp, "   <Row ss:Index=\"%d\"/>\n", r + 1);
            continue;
        }

        fprintf(fp, "   <Row ss:Index=\"%d\">\n", r + 1);

        for (c = 0; c < buf->cfg.kolom; c++) {
            const char *val = buf->isi[r][c];

            if (val && strlen(val) > 0) {
                /* Check for merge */
                if (apakah_sel_anchor_buf(buf, c, r)) {
                    struct info_gabung *g = &buf->tergabung[r][c];
                    fprintf(fp, "    <Cell ss:Index=\"%d\"", c + 1);
                    if (g->lebar > 1) {
                        fprintf(fp, " ss:MergeAcross=\"%d\"", g->lebar - 1);
                    }
                    if (g->tinggi > 1) {
                        fprintf(fp, " ss:MergeDown=\"%d\"", g->tinggi - 1);
                    }
                    fprintf(fp, ">\n");
                } else if (apakah_ada_gabung_buf(buf, c, r)) {
                    /* Skip merged cells that are not anchor */
                    continue;
                } else {
                    fprintf(fp, "    <Cell ss:Index=\"%d\">\n", c + 1);
                }

                /* Determine data type */
                char *endptr;
                double num = strtod(val, &endptr);

                if (*endptr == '\0' && endptr != val) {
                    /* Number */
                    fprintf(fp, "     <Data ss:Type=\"Number\">%s</Data>\n", val);
                } else {
                    /* String */
                    xmlss_escape(val, escaped, sizeof(escaped));
                    fprintf(fp, "     <Data ss:Type=\"String\">%s</Data>\n", escaped);
                }

                fprintf(fp, "    </Cell>\n");
            }
        }

        fprintf(fp, "   </Row>\n");
    }

    fprintf(fp, "  </Table>\n");

    /* Worksheet options */
    fprintf(fp, "  <WorksheetOptions xmlns=\"urn:schemas-microsoft-com:office:excel\">\n");
    fprintf(fp, "   <Selected/>\n");
    fprintf(fp, "   <ProtectObjects>False</ProtectObjects>\n");
    fprintf(fp, "   <ProtectScenarios>False</ProtectScenarios>\n");
    fprintf(fp, "  </WorksheetOptions>\n");

    fprintf(fp, " </Worksheet>\n");
    fprintf(fp, "</Workbook>\n");

    fclose(fp);
    return 0;
}

/* ============================================================
 * FUNGSI PUBLIK
 * ============================================================ */

/* Buka file XML Spreadsheet */
int buka_xml_spreadsheet(struct buffer_tabel **pbuf, const char *fn) {
    FILE *fp;
    char *content;
    size_t file_size;
    size_t bytes_read;

    if (!pbuf || !fn) return -1;

    fp = fopen(fn, "r");
    if (!fp) {
        xmlss_set_error("Cannot open file: %s", fn);
        return -1;
    }

    /* Get file size */
    fseek(fp, 0, SEEK_END);
    file_size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    /* Allocate and read content */
    content = malloc(file_size + 1);
    if (!content) {
        fclose(fp);
        return -1;
    }

    bytes_read = fread(content, 1, file_size, fp);
    fclose(fp);

    content[bytes_read] = '\0';

    /* Create buffer */
    bebas_buffer(*pbuf);
    *pbuf = buat_buffer(100, 26);

    /* Parse XML Spreadsheet */
    xmlss_parse(content, bytes_read, *pbuf);

    free(content);
    return 0;
}

/* Simpan file XML Spreadsheet */
int simpan_xml_spreadsheet(const struct buffer_tabel *buf, const char *fn) {
    return write_xml_spreadsheet(buf, fn);
}

/* Cek validitas file XML Spreadsheet */
int xml_spreadsheet_apakah_valid(const char *fn) {
    FILE *fp;
    char buf[256];
    size_t bytes_read;

    fp = fopen(fn, "r");
    if (!fp) return 0;

    bytes_read = fread(buf, 1, sizeof(buf) - 1, fp);
    fclose(fp);

    buf[bytes_read] = '\0';

    /* Check for XML declaration and Spreadsheet namespace */
    return (strstr(buf, "<?xml") != NULL &&
            strstr(buf, "schemas-microsoft-com:office:spreadsheet") != NULL);
}

/* Alias functions */
int buka_xml(struct buffer_tabel **pbuf, const char *fn) {
    return buka_xml_spreadsheet(pbuf, fn);
}

int simpan_xml(const struct buffer_tabel *buf, const char *fn) {
    return simpan_xml_spreadsheet(buf, fn);
}
