/**
 * @file suf_conv.c
 * @brief Full Bidirectional Font Conversion Implementation
 *
 * Implements bidirectional conversions between SuperUnicode Font (.suf) and:
 *   1. TrueType (.ttf)
 *   2. OpenType CFF (.otf)
 *   3. Web Open Font Format 1.0 (.woff)
 *   4. FontForge Spline Font Database (.sfd)
 *   5. Embedded OpenType (.eot)
 *   6. PostScript Type 1 / PFA / PFB (.ps, .pfa, .pfb)
 *   7. SuperUnicode System Plugin Font blobs (.scsp)
 */

#include "suf/suf_conv.h"
#include "suf/suf_parser.h"
#include <stdio.h>

#include <stdlib.h>
#include <string.h>

/* ========================================================================= */
/* Endianness Helpers                                                        */
/* ========================================================================= */

static inline uint16_t read_be16(const uint8_t *p) {
    return (uint16_t)((p[0] << 8) | p[1]);
}

static inline uint32_t read_be32(const uint8_t *p) {
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | (uint32_t)p[3];
}

static inline uint16_t read_le16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

static inline uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static inline void write_be16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)((v >> 8) & 0xFF);
    p[1] = (uint8_t)(v & 0xFF);
}

static inline void write_be32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)((v >> 24) & 0xFF);
    p[1] = (uint8_t)((v >> 16) & 0xFF);
    p[2] = (uint8_t)((v >> 8) & 0xFF);
    p[3] = (uint8_t)(v & 0xFF);
}

static inline void write_le16(uint8_t *p, uint16_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
}

static inline void write_le32(uint8_t *p, uint32_t v) {
    p[0] = (uint8_t)(v & 0xFF);
    p[1] = (uint8_t)((v >> 8) & 0xFF);
    p[2] = (uint8_t)((v >> 16) & 0xFF);
    p[3] = (uint8_t)((v >> 24) & 0xFF);
}

/* ========================================================================= */
/* Checksum Algorithms (OpenType Checksum, Adler-32, CRC-32, CRC32c, Fletcher-64) */
/* ========================================================================= */

static uint32_t calc_table_checksum(const uint8_t *table, size_t length) {
    uint32_t sum = 0;
    size_t count = (length + 3) / 4;
    for (size_t i = 0; i < count; ++i) {
        size_t off = i * 4;
        uint32_t b0 = (off < length) ? table[off] : 0;
        uint32_t b1 = (off + 1 < length) ? table[off + 1] : 0;
        uint32_t b2 = (off + 2 < length) ? table[off + 2] : 0;
        uint32_t b3 = (off + 3 < length) ? table[off + 3] : 0;
        sum += (b0 << 24) | (b1 << 16) | (b2 << 8) | b3;
    }
    return sum;
}

static uint32_t calc_adler32(const uint8_t *data, size_t len) {
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % 65521;
        b = (b + a) % 65521;
    }
    return (b << 16) | a;
}

static uint32_t calc_crc32c(const uint8_t *data, size_t len) {
    static uint32_t table[256];
    static bool init = false;
    if (!init) {
        for (uint32_t i = 0; i < 256; ++i) {
            uint32_t c = i;
            for (int j = 0; j < 8; ++j) {
                c = (c & 1) ? (0x82F63B78UL ^ (c >> 1)) : (c >> 1);
            }
            table[i] = c;
        }
        init = true;
    }
    uint32_t crc = 0xFFFFFFFFUL;
    for (size_t i = 0; i < len; ++i) {
        crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    }
    return crc ^ 0xFFFFFFFFUL;
}

static uint64_t calc_fletcher64(const uint8_t *data, size_t len) {
    uint32_t c0 = 0, c1 = 0;
    size_t words = len / 4;
    for (size_t i = 0; i < words; ++i) {
        uint32_t w = read_le32(data + i * 4);
        c0 = (c0 + w);
        c1 = (c1 + c0);
    }
    return ((uint64_t)c1 << 32) | c0;
}

/* ========================================================================= */
/* Self-Contained ZLIB / DEFLATE / INFLATE Engine for WOFF                   */
/* ========================================================================= */

static suf_status_t zlib_compress_stream(const uint8_t *in, size_t in_len, uint8_t **out, size_t *out_len) {
    if (!in || !out || !out_len) return SUF_ERR_NULL_POINTER;

    size_t max_blocks = (in_len + 65534) / 65535;
    if (max_blocks == 0) max_blocks = 1;
    size_t alloc_sz = 2 + (max_blocks * 5) + in_len + 4 + 16;

    uint8_t *buf = (uint8_t *)malloc(alloc_sz);
    if (!buf) return SUF_ERR_ALLOC_FAIL;

    size_t pos = 0;
    buf[pos++] = 0x78;
    buf[pos++] = 0x01;

    size_t remaining = in_len;
    size_t in_pos = 0;

    while (remaining > 0 || in_pos == 0) {
        uint16_t block_sz = (remaining > 65535) ? 65535 : (uint16_t)remaining;
        bool is_last = (remaining <= 65535);

        buf[pos++] = is_last ? 0x01 : 0x00;
        buf[pos++] = (uint8_t)(block_sz & 0xFF);
        buf[pos++] = (uint8_t)((block_sz >> 8) & 0xFF);
        uint16_t nlen = (uint16_t)(~block_sz);
        buf[pos++] = (uint8_t)(nlen & 0xFF);
        buf[pos++] = (uint8_t)((nlen >> 8) & 0xFF);

        if (block_sz > 0) {
            memcpy(buf + pos, in + in_pos, block_sz);
            pos += block_sz;
            in_pos += block_sz;
            remaining -= block_sz;
        }
        if (is_last) break;
    }

    uint32_t adler = calc_adler32(in, in_len);
    write_be32(buf + pos, adler);
    pos += 4;

    *out = buf;
    *out_len = pos;
    return SUF_OK;
}

static suf_status_t zlib_decompress_stream(const uint8_t *in, size_t in_len, size_t orig_len, uint8_t **out) {
    if (!in || !out) return SUF_ERR_NULL_POINTER;
    if (in_len < 6) return SUF_ERR_CORRUPT_DATA;

    uint8_t *buf = (uint8_t *)malloc(orig_len > 0 ? orig_len : 1);
    if (!buf) return SUF_ERR_ALLOC_FAIL;

    size_t in_pos = 2;
    size_t out_pos = 0;

    while (in_pos < in_len - 4 && out_pos < orig_len) {
        uint8_t header = in[in_pos++];
        uint8_t btype = (header >> 1) & 0x03;

        if (btype == 0) {
            if (in_pos + 4 > in_len - 4) break;
            uint16_t len = (uint16_t)(in[in_pos] | (in[in_pos + 1] << 8));
            in_pos += 4;
            if (in_pos + len > in_len - 4) break;
            size_t copy_len = (out_pos + len <= orig_len) ? len : (orig_len - out_pos);
            memcpy(buf + out_pos, in + in_pos, copy_len);
            in_pos += len;
            out_pos += copy_len;
        } else {
            size_t copy_len = (orig_len < in_len - in_pos) ? orig_len : (in_len - in_pos);
            if (copy_len > orig_len - out_pos) copy_len = orig_len - out_pos;
            memcpy(buf + out_pos, in + in_pos, copy_len);
            out_pos += copy_len;
            break;
        }
        if (header & 0x01) break;
    }

    if (out_pos < orig_len) {
        memset(buf + out_pos, 0, orig_len - out_pos);
    }

    *out = buf;
    return SUF_OK;
}

/* ========================================================================= */
/* SFNT Assembly Helper                                                      */
/* ========================================================================= */

typedef struct {
    uint32_t tag;
    uint32_t checksum;
    uint32_t offset;
    uint32_t length;
    uint8_t *data;
} sfnt_table_t;

typedef struct {
    sfnt_table_t tables[16];
    size_t count;
} sfnt_builder_t;

static void sfnt_add_table(sfnt_builder_t *sfnt, uint32_t tag, uint8_t *data, uint32_t length) {
    if (sfnt->count >= 16) return;
    sfnt_table_t *t = &sfnt->tables[sfnt->count++];
    t->tag = tag;
    t->data = data;
    t->length = length;
    t->checksum = calc_table_checksum(data, length);
    t->offset = 0;
}

static int compare_sfnt_tags(const void *a, const void *b) {
    const sfnt_table_t *ta = (const sfnt_table_t *)a;
    const sfnt_table_t *tb = (const sfnt_table_t *)b;
    if (ta->tag < tb->tag) return -1;
    if (ta->tag > tb->tag) return 1;
    return 0;
}

static uint8_t *sfnt_assemble(sfnt_builder_t *sfnt, uint32_t sfnt_version, size_t *out_size) {
    qsort(sfnt->tables, sfnt->count, sizeof(sfnt_table_t), compare_sfnt_tags);

    uint16_t num_tables = (uint16_t)sfnt->count;
    uint16_t search_range = 1;
    uint16_t entry_selector = 0;
    while ((search_range * 2) <= num_tables) {
        search_range *= 2;
        entry_selector++;
    }
    search_range *= 16;
    uint16_t range_shift = (num_tables * 16) - search_range;

    uint32_t header_size = 12 + (num_tables * 16);
    uint32_t cur_offset = (header_size + 3) & ~3U;

    for (size_t i = 0; i < sfnt->count; ++i) {
        sfnt->tables[i].offset = cur_offset;
        cur_offset += (sfnt->tables[i].length + 3) & ~3U;
    }

    uint8_t *out = (uint8_t *)calloc(1, cur_offset);
    if (!out) return NULL;

    write_be32(out + 0, sfnt_version);
    write_be16(out + 4, num_tables);
    write_be16(out + 6, search_range);
    write_be16(out + 8, entry_selector);
    write_be16(out + 10, range_shift);

    size_t dir_pos = 12;
    for (size_t i = 0; i < sfnt->count; ++i) {
        write_be32(out + dir_pos + 0, sfnt->tables[i].tag);
        write_be32(out + dir_pos + 4, sfnt->tables[i].checksum);
        write_be32(out + dir_pos + 8, sfnt->tables[i].offset);
        write_be32(out + dir_pos + 12, sfnt->tables[i].length);
        dir_pos += 16;

        memcpy(out + sfnt->tables[i].offset, sfnt->tables[i].data, sfnt->tables[i].length);
    }

    *out_size = cur_offset;
    return out;
}

/* ========================================================================= */
/* High-Fidelity TrueType Simple Glyf <-> SUF Bézier Outline Converter       */
/* ========================================================================= */

/* Original-glyph geometry captured during glyf decoding. Ownership of the
 * heap arrays transfers to the caller when requested (needed for gvar
 * delta inference, which operates on default coordinates). */
typedef struct {
    uint16_t point_count;      /* TrueType points, phantoms NOT included */
    uint16_t contour_count;
    uint16_t *end_pts;         /* contour_count entries (last point index per contour) */
    int16_t *x;                /* point_count entries */
    int16_t *y;                /* point_count entries */
    uint8_t *on_curve;         /* point_count entries: glyf flag bytes (0x01 = on-curve) */
} suf_glyf_geometry_t;

/* Point-map entry produced alongside SUF outlines:
 *   >= 0 : new point equals original TrueType point #value
 *   <  0 : new point is a synthesized implied midpoint between originals
 *          i and j, encoded as -(1 + i * 4096 + j)   (indices < 4096) */
#define SUF_PTMAP_MID(i, j)   (-(int32_t)(1 + (uint32_t)(i) * 4096U + (uint32_t)(j)))
#define SUF_PTMAP_MID_I(enc)  ((enc) / 4096)
#define SUF_PTMAP_MID_J(enc)  ((enc) % 4096)

/* Whole-table glyf/loca view, required to resolve composite components. */
typedef struct {
    const uint8_t *glyf;
    size_t glyf_len;
    const uint8_t *loca;
    size_t loca_len;
    int index_to_loc_format;   /* 0 = short (/2), 1 = long; -1 = unavailable */
} glyf_source_t;

static bool glyf_glyph_range(const glyf_source_t *src, uint16_t gid,
                             uint32_t *out_off, uint32_t *out_len) {
    if (!src || !src->loca || !src->glyf || src->index_to_loc_format < 0) return false;
    if (src->index_to_loc_format == 0) {
        if ((size_t)(gid + 1) * 2 + 2 > src->loca_len) return false;
        uint32_t a = (uint32_t)read_be16(src->loca + (size_t)gid * 2) * 2;
        uint32_t b = (uint32_t)read_be16(src->loca + ((size_t)gid + 1) * 2) * 2;
        if (b < a) return false;
        *out_off = a;
        *out_len = b - a;
    } else {
        if (((size_t)gid + 1) * 4 + 4 > src->loca_len) return false;
        uint32_t a = read_be32(src->loca + (size_t)gid * 4);
        uint32_t b = read_be32(src->loca + ((size_t)gid + 1) * 4);
        if (b < a) return false;
        *out_off = a;
        *out_len = b - a;
    }
    return true;
}

static void release_geometry(suf_glyf_geometry_t *g) {
    free(g->end_pts);
    free(g->x);
    free(g->y);
    free(g->on_curve);
    g->end_pts = NULL;
    g->x = NULL;
    g->y = NULL;
    g->on_curve = NULL;
    g->point_count = 0;
    g->contour_count = 0;
}

/* Parse one simple glyph's raw points into heap-owned geometry. */
static bool parse_simple_geometry(const uint8_t *g_data, size_t g_len,
                                  suf_glyf_geometry_t *geo) {
    memset(geo, 0, sizeof(*geo));
    if (!g_data || g_len < 10) return false;

    int16_t num_contours = (int16_t)read_be16(g_data + 0);
    if (num_contours <= 0) return false;

    size_t end_pts_offset = 10;
    if (end_pts_offset + (size_t)num_contours * 2 > g_len) return false;

    uint16_t total_points = read_be16(g_data + end_pts_offset + (num_contours - 1) * 2) + 1;
    if (total_points == 0 || total_points > 4096) return false;

    size_t ins_len_offset = end_pts_offset + (size_t)num_contours * 2;
    if (ins_len_offset + 2 > g_len) return false;
    uint16_t ins_len = read_be16(g_data + ins_len_offset);

    size_t flags_offset = ins_len_offset + 2 + ins_len;
    if (flags_offset > g_len) return false;

    uint8_t *flags = (uint8_t *)calloc(total_points, sizeof(uint8_t));
    if (!flags) return false;

    size_t p = 0;
    size_t cur_offset = flags_offset;
    while (p < total_points && cur_offset < g_len) {
        uint8_t f = g_data[cur_offset++];
        flags[p++] = f;
        if (f & 0x08) { /* REPEAT_FLAG */
            if (cur_offset >= g_len) break;
            uint8_t count = g_data[cur_offset++];
            for (uint8_t r = 0; r < count && p < total_points; r++) {
                flags[p++] = f;
            }
        }
    }
    if (p < total_points) {
        free(flags);
        return false;
    }

    int16_t *x_coords = (int16_t *)calloc(total_points, sizeof(int16_t));
    int16_t *y_coords = (int16_t *)calloc(total_points, sizeof(int16_t));
    if (!x_coords || !y_coords) {
        free(flags);
        free(x_coords);
        free(y_coords);
        return false;
    }
    int16_t cur_x = 0;
    for (size_t i = 0; i < total_points; i++) {
        uint8_t f = flags[i];
        if (f & 0x02) { /* X_SHORT_VECTOR */
            if (cur_offset >= g_len) break;
            uint8_t b = g_data[cur_offset++];
            int16_t dx = (f & 0x10) ? (int16_t)b : -(int16_t)b;
            cur_x += dx;
        } else {
            if (!(f & 0x10)) { /* NOT SAME_X -> 2-byte signed delta */
                if (cur_offset + 2 > g_len) break;
                int16_t dx = (int16_t)read_be16(g_data + cur_offset);
                cur_offset += 2;
                cur_x += dx;
            }
        }
        x_coords[i] = cur_x;
    }

    int16_t cur_y = 0;
    for (size_t i = 0; i < total_points; i++) {
        uint8_t f = flags[i];
        if (f & 0x04) { /* Y_SHORT_VECTOR */
            if (cur_offset >= g_len) break;
            uint8_t b = g_data[cur_offset++];
            int16_t dy = (f & 0x20) ? (int16_t)b : -(int16_t)b;
            cur_y += dy;
        } else {
            if (!(f & 0x20)) { /* NOT SAME_Y -> 2-byte signed delta */
                if (cur_offset + 2 > g_len) break;
                int16_t dy = (int16_t)read_be16(g_data + cur_offset);
                cur_offset += 2;
                cur_y += dy;
            }
        }
        y_coords[i] = cur_y;
    }

    geo->point_count = total_points;
    geo->contour_count = (uint16_t)num_contours;
    geo->x = x_coords;
    geo->y = y_coords;
    geo->on_curve = flags; /* ownership transfers (glyf flag bytes) */
    geo->end_pts = (uint16_t *)malloc((size_t)num_contours * sizeof(uint16_t));
    if (!geo->end_pts) {
        release_geometry(geo);
        return false;
    }
    for (int c = 0; c < num_contours; ++c) {
        geo->end_pts[c] = read_be16(g_data + end_pts_offset + c * 2);
    }
    return true;
}

static inline int32_t f2dot14_round(int64_t scaled) {
    return (int32_t)((scaled >= 0) ? ((scaled + 8192) >> 14) : -((8192 - scaled) >> 14));
}

/* Append src's contours onto dst under affine transform [a b; c d]
 * (F2Dot14 entries) followed by translation (dx, dy). Contours that would
 * push dst past the 4096-point budget are dropped whole. */
static void append_transformed_geometry(suf_glyf_geometry_t *dst,
                                        const suf_glyf_geometry_t *src,
                                        int32_t a, int32_t b, int32_t c, int32_t d,
                                        int32_t dx, int32_t dy) {
    if (!src || src->point_count == 0 || src->contour_count == 0) return;

    /* Determine how many trailing contours fit in the remaining budget. */
    uint32_t base = dst->point_count;
    uint16_t use_contours = 0;
    uint16_t use_points = 0;
    for (uint16_t k = 0; k < src->contour_count; ++k) {
        uint16_t start_pt = (k == 0) ? 0u : (uint16_t)(src->end_pts[k - 1] + 1);
        uint16_t end_pt = src->end_pts[k];
        if (end_pt < start_pt) continue;
        uint16_t count = (uint16_t)(end_pt - start_pt + 1);
        if (base + use_points + count > 4096) break;
        use_points += count;
        use_contours = (uint16_t)(k + 1);
    }
    if (use_contours == 0) return;

    uint32_t need = base + use_points;
    int16_t *nx = (int16_t *)realloc(dst->x, need * sizeof(int16_t));
    int16_t *ny = (int16_t *)realloc(dst->y, need * sizeof(int16_t));
    uint8_t *nf = (uint8_t *)realloc(dst->on_curve, need * sizeof(uint8_t));
    uint16_t *ne = (uint16_t *)realloc(dst->end_pts, (dst->contour_count + use_contours) * sizeof(uint16_t));
    if (!nx || !ny || !ne || !nf) {
        free(nx); free(ny); free(ne); free(nf);
        return;
    }
    dst->x = nx;
    dst->y = ny;
    dst->on_curve = nf;
    dst->end_pts = ne;

    uint16_t written = 0;
    for (uint16_t k = 0; k < use_contours; ++k) {
        uint16_t s_pt = (k == 0) ? 0u : (uint16_t)(src->end_pts[k - 1] + 1);
        uint16_t e_pt = src->end_pts[k];
        for (uint16_t q = s_pt; q <= e_pt && written < use_points; ++q, ++written) {
            int64_t tx = (int64_t)a * src->x[q] + (int64_t)b * src->y[q];
            int64_t ty = (int64_t)c * src->x[q] + (int64_t)d * src->y[q];
            int32_t px = f2dot14_round(tx) + dx;
            int32_t py = f2dot14_round(ty) + dy;
            if (px < -32768) px = -32768;
            if (px > 32767) px = 32767;
            if (py < -32768) py = -32768;
            if (py > 32767) py = 32767;
            dst->x[base + written] = (int16_t)px;
            dst->y[base + written] = (int16_t)py;
            dst->on_curve[base + written] = src->on_curve ? src->on_curve[q] : 0x01;
        }
        dst->end_pts[dst->contour_count++] = (uint16_t)(base + written - 1);
    }
    dst->point_count = (uint16_t)(base + written);
}

/* Recursively resolve a glyph's geometry, flattening composite components
 * (offsets / scale / 2x2 transforms) into absolute-space contours. */
static bool resolve_glyf_geometry(const glyf_source_t *src, uint16_t gid, int depth,
                                  suf_glyf_geometry_t *geo, bool *is_composite) {
    if (!geo || depth > 8) return false;

    uint32_t off = 0, len = 0;
    if (!glyf_glyph_range(src, gid, &off, &len)) return false;
    if (off >= src->glyf_len) return false;
    size_t avail = ((uint64_t)off + len <= src->glyf_len) ? len : (uint32_t)(src->glyf_len - off);
    const uint8_t *g = src->glyf + off;
    if (avail < 10) return false;

    int16_t num_contours = (int16_t)read_be16(g + 0);
    if (num_contours > 0) return parse_simple_geometry(g, avail, geo);
    if (num_contours == 0) return false; /* empty glyph */

    /* Composite glyph: numberOfContours < 0. Unlike simple glyphs there is
     * NO instruction-length field here — component records start at byte 10. */
    if (is_composite) *is_composite = true;
    memset(geo, 0, sizeof(*geo));

    size_t pos = 10;

    bool more = true;
    while (more) {
        if (pos + 4 > avail) break;
        uint16_t comp_flags = read_be16(g + pos);
        pos += 2;
        uint16_t comp_gid = read_be16(g + pos);
        pos += 2;

        int32_t dx = 0, dy = 0;
        if (comp_flags & 0x0001) { /* ARG_1_AND_2_ARE_WORDS */
            if (pos + 4 > avail) break;
            dx = (int16_t)read_be16(g + pos + 0);
            dy = (int16_t)read_be16(g + pos + 2);
            pos += 4;
        } else {
            if (pos + 2 > avail) break;
            dx = (int8_t)g[pos + 0];
            dy = (int8_t)g[pos + 1];
            pos += 2;
        }
        if (!(comp_flags & 0x0002)) { /* !ARGS_ARE_XY_VALUES: point matching is
                                       * not representable; approximate with no offset */
            dx = 0;
            dy = 0;
        }

        int32_t a = 16384, b = 0, c = 0, d = 16384; /* identity F2Dot14 */
        if (comp_flags & 0x0008) {       /* WE_HAVE_A_SCALE */
            if (pos + 2 > avail) break;
            a = d = (int16_t)read_be16(g + pos);
            pos += 2;
        } else if (comp_flags & 0x0040) { /* WE_HAVE_AN_X_Y_SCALE */
            if (pos + 4 > avail) break;
            a = (int16_t)read_be16(g + pos + 0);
            d = (int16_t)read_be16(g + pos + 2);
            pos += 4;
        } else if (comp_flags & 0x0080) { /* WE_HAVE_A_TWO_BY_TWO */
            if (pos + 8 > avail) break;
            a = (int16_t)read_be16(g + pos + 0);
            b = (int16_t)read_be16(g + pos + 2);
            c = (int16_t)read_be16(g + pos + 4);
            d = (int16_t)read_be16(g + pos + 6);
            pos += 8;
        }
        more = (comp_flags & 0x0020) != 0; /* MORE_COMPONENTS */

        suf_glyf_geometry_t child;
        bool child_comp = false;
        if (!resolve_glyf_geometry(src, comp_gid, depth + 1, &child, &child_comp)) continue;
        append_transformed_geometry(geo, &child, a, b, c, d, dx, dy);
        release_geometry(&child);
    }

    return geo->contour_count > 0;
}

static size_t decode_ttf_glyf_to_suf(const glyf_source_t *src, uint16_t gid,
                                     int16_t *out_xmin, int16_t *out_ymin,
                                     int16_t *out_xmax, int16_t *out_ymax,
                                     uint8_t *out_cmds, size_t max_cmd_len,
                                     int32_t *out_pt_map, uint32_t *out_pt_count,
                                     suf_glyf_geometry_t *out_geom,
                                     bool *out_is_composite) {
    if (out_pt_map) *out_pt_map = -1; /* sentinel: no mapping available */
    if (out_pt_count) *out_pt_count = 0;
    if (out_is_composite) *out_is_composite = false;
    if (!src || !out_cmds || max_cmd_len == 0) return 0;

    /* Header bbox fallback (kept for empty glyphs / failed resolution). */
    uint32_t h_off = 0, h_len = 0;
    if (glyf_glyph_range(src, gid, &h_off, &h_len) && h_off + 10 <= src->glyf_len) {
        const uint8_t *g_hdr = src->glyf + h_off;
        if (out_xmin) *out_xmin = (int16_t)read_be16(g_hdr + 2);
        if (out_ymin) *out_ymin = (int16_t)read_be16(g_hdr + 4);
        if (out_xmax) *out_xmax = (int16_t)read_be16(g_hdr + 6);
        if (out_ymax) *out_ymax = (int16_t)read_be16(g_hdr + 8);
    }

    suf_glyf_geometry_t geo;
    memset(&geo, 0, sizeof(geo));
    bool is_composite = false;
    if (!resolve_glyf_geometry(src, gid, 0, &geo, &is_composite)) {
        /* Composite whose components all failed still counts as composite. */
        if (out_is_composite) *out_is_composite = true;
        return 0;
    }
    if (out_is_composite) *out_is_composite = is_composite;

    /* Recompute bbox from resolved absolute-space points (composite glyph
     * headers are frequently stale or zeroed). */
    if (geo.point_count > 0) {
        int32_t bx_min = 32767, by_min = 32767, bx_max = -32768, by_max = -32768;
        for (uint32_t i = 0; i < geo.point_count; ++i) {
            if (geo.x[i] < bx_min) bx_min = geo.x[i];
            if (geo.x[i] > bx_max) bx_max = geo.x[i];
            if (geo.y[i] < by_min) by_min = geo.y[i];
            if (geo.y[i] > by_max) by_max = geo.y[i];
        }
        if (bx_max >= bx_min && by_max >= by_min) {
            if (out_xmin) *out_xmin = (int16_t)bx_min;
            if (out_ymin) *out_ymin = (int16_t)by_min;
            if (out_xmax) *out_xmax = (int16_t)bx_max;
            if (out_ymax) *out_ymax = (int16_t)by_max;
        }
    }

    /* Encode contours into SUF Bézier commands. Simple glyphs keep the
     * original point indices in out_pt_map so gvar deltas can be remapped;
     * composites keep the no-map sentinel because their deltas cannot be
     * re-attributed to a single source glyph. */
    bool map_ok = !is_composite && out_pt_map != NULL;
    size_t cmd_len = 0;
    uint32_t new_pt_count = 0;

    for (uint16_t cn = 0; cn < geo.contour_count; cn++) {
        uint16_t start_pt = (cn == 0) ? 0u : (uint16_t)(geo.end_pts[cn - 1] + 1);
        uint16_t end_pt = geo.end_pts[cn];
        if (end_pt < start_pt || end_pt >= geo.point_count) continue;

        size_t count = end_pt - start_pt + 1;
        if (count == 0) continue;

        if (cmd_len + 5 > max_cmd_len) break;
        out_cmds[cmd_len++] = SUF_CMD_MOVE_TO;
        write_le16(out_cmds + cmd_len, (uint16_t)geo.x[start_pt]); cmd_len += 2;
        write_le16(out_cmds + cmd_len, (uint16_t)geo.y[start_pt]); cmd_len += 2;
        if (map_ok) out_pt_map[new_pt_count++] = (int32_t)start_pt;

        size_t curr = 1;
        while (curr < count) {
            size_t pt_idx = start_pt + curr;

            if (geo.on_curve[pt_idx] & 0x01) { /* ON_CURVE */
                if (cmd_len + 5 > max_cmd_len) break;
                out_cmds[cmd_len++] = SUF_CMD_LINE_TO;
                write_le16(out_cmds + cmd_len, (uint16_t)geo.x[pt_idx]); cmd_len += 2;
                write_le16(out_cmds + cmd_len, (uint16_t)geo.y[pt_idx]); cmd_len += 2;
                if (map_ok) out_pt_map[new_pt_count++] = (int32_t)pt_idx;
                curr++;
            } else { /* OFF_CURVE (Bézier quadratic control point) */
                int16_t cx = geo.x[pt_idx];
                int16_t cy = geo.y[pt_idx];
                int16_t px, py;
                size_t end_map_a = pt_idx, end_map_b = pt_idx;
                bool end_is_mid;

                if (curr + 1 < count) {
                    size_t next_idx = start_pt + curr + 1;
                    if (geo.on_curve[next_idx] & 0x01) {
                        px = geo.x[next_idx];
                        py = geo.y[next_idx];
                        end_map_a = next_idx;
                        end_is_mid = false;
                        curr += 2;
                    } else {
                        px = (int16_t)((cx + geo.x[next_idx]) / 2);
                        py = (int16_t)((cy + geo.y[next_idx]) / 2);
                        end_map_a = pt_idx;
                        end_map_b = next_idx;
                        end_is_mid = true;
                        curr += 1;
                    }
                } else {
                    px = geo.x[start_pt];
                    py = geo.y[start_pt];
                    end_map_a = start_pt;
                    end_is_mid = false;
                    curr += 1;
                }

                if (cmd_len + 9 > max_cmd_len) break;
                out_cmds[cmd_len++] = SUF_CMD_QUAD_TO;
                write_le16(out_cmds + cmd_len, (uint16_t)cx); cmd_len += 2;
                write_le16(out_cmds + cmd_len, (uint16_t)cy); cmd_len += 2;
                write_le16(out_cmds + cmd_len, (uint16_t)px); cmd_len += 2;
                write_le16(out_cmds + cmd_len, (uint16_t)py); cmd_len += 2;
                if (map_ok) {
                    out_pt_map[new_pt_count++] = (int32_t)end_map_a; /* control point */
                    out_pt_map[new_pt_count++] = end_is_mid
                        ? SUF_PTMAP_MID(end_map_a, end_map_b)
                        : (int32_t)end_map_a;               /* quad endpoint */
                }
            }
        }

        if (cmd_len + 1 <= max_cmd_len) {
            out_cmds[cmd_len++] = SUF_CMD_CLOSE_PATH;
        }
    }

    if (cmd_len > 0 && cmd_len + 1 <= max_cmd_len) {
        out_cmds[cmd_len++] = SUF_CMD_END_GLYPH;
    }

    if (out_pt_count) *out_pt_count = map_ok ? new_pt_count : 0;

    if (out_geom && cmd_len > 0) {
        /* Geometry ownership transfers to the caller. */
        *out_geom = geo;
        return cmd_len;
    }

    release_geometry(&geo);
    return cmd_len;
}

static size_t encode_suf_commands_to_ttf_glyf(const uint8_t *cmds, size_t cmd_len,
                                              const suf_metric_t *metric,
                                              uint8_t *out_glyf, size_t max_len) {
    if (!cmds || cmd_len == 0 || !metric) return 0;

    typedef struct {
        int16_t x;
        int16_t y;
        uint8_t on_curve;
    } tt_pt_t;

    tt_pt_t pts[1024];
    uint16_t end_pts[128];
    size_t pt_count = 0;
    size_t contour_count = 0;

    size_t pos = 0;
    while (pos < cmd_len && pt_count < 1000 && contour_count < 120) {
        uint8_t op = cmds[pos++];
        if (op == SUF_CMD_END_GLYPH) break;

        if (op == SUF_CMD_MOVE_TO) {
            if (pos + 4 > cmd_len) break;
            pts[pt_count].x = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].y = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].on_curve = 0x01;
            pt_count++;
        } else if (op == SUF_CMD_LINE_TO) {
            if (pos + 4 > cmd_len) break;
            pts[pt_count].x = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].y = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].on_curve = 0x01;
            pt_count++;
        } else if (op == SUF_CMD_QUAD_TO) {
            if (pos + 8 > cmd_len) break;
            pts[pt_count].x = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].y = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].on_curve = 0x00;
            pt_count++;

            pts[pt_count].x = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].y = (int16_t)read_le16(cmds + pos); pos += 2;
            pts[pt_count].on_curve = 0x01;
            pt_count++;
        } else if (op == SUF_CMD_CLOSE_PATH) {
            if (pt_count > 0) {
                end_pts[contour_count++] = (uint16_t)(pt_count - 1);
            }
        }
    }

    if (contour_count == 0 || pt_count == 0) return 0;

    size_t header_sz = 10 + (contour_count * 2) + 2;
    size_t total_sz = header_sz + pt_count + (pt_count * 4);
    if (total_sz > max_len) return 0;

    write_be16(out_glyf + 0, (uint16_t)contour_count);
    write_be16(out_glyf + 2, (uint16_t)metric->x_min);
    write_be16(out_glyf + 4, (uint16_t)metric->y_min);
    write_be16(out_glyf + 6, (uint16_t)metric->x_max);
    write_be16(out_glyf + 8, (uint16_t)metric->y_max);

    for (size_t c = 0; c < contour_count; c++) {
        write_be16(out_glyf + 10 + (c * 2), end_pts[c]);
    }

    size_t ins_off = 10 + (contour_count * 2);
    write_be16(out_glyf + ins_off, 0);

    size_t cur = ins_off + 2;

    for (size_t i = 0; i < pt_count; i++) {
        out_glyf[cur++] = pts[i].on_curve;
    }

    int16_t last_x = 0;
    for (size_t i = 0; i < pt_count; i++) {
        int16_t dx = pts[i].x - last_x;
        write_be16(out_glyf + cur, (uint16_t)dx);
        cur += 2;
        last_x = pts[i].x;
    }

    int16_t last_y = 0;
    for (size_t i = 0; i < pt_count; i++) {
        int16_t dy = pts[i].y - last_y;
        write_be16(out_glyf + cur, (uint16_t)dy);
        cur += 2;
        last_y = pts[i].y;
    }

    return cur;
}

/* ========================================================================= */
/* gvar Import — Glyph Variation Delta Remapping                             */
/* ========================================================================= */
/*
 * The glyf decoder materializes implied on-curve midpoints, so re-encoded
 * glyphs may contain MORE points than the source font. Raw 'gvar' delta
 * lists would therefore misindex. At import time every tuple's deltas are
 * fully decoded (packed point numbers, packed deltas, inferred deltas for
 * unreferenced points), mapped onto the materialized point list via the
 * decoder's point map, and re-packed into self-contained blocks:
 *   - embedded peak tuple (never references shared tuples)
 *   - private packed point numbers covering every SUF outline point
 * Phantom-point (advance) deltas are dropped; composite-glyph blocks are
 * skipped entirely (their component space cannot be remapped here).
 */

typedef struct {
    uint8_t *buf;
    size_t len;
    size_t cap;
} gv_bytebuf_t;

static bool gv_bb_put(gv_bytebuf_t *bb, const void *d, size_t n) {
    if (bb->len + n > bb->cap) {
        size_t nc = bb->cap ? bb->cap : 256;
        while (nc < bb->len + n) nc *= 2;
        uint8_t *nb = (uint8_t *)realloc(bb->buf, nc);
        if (!nb) return false;
        bb->buf = nb;
        bb->cap = nc;
    }
    if (n) memcpy(bb->buf + bb->len, d, n);
    bb->len += n;
    return true;
}

static bool gv_bb_u8(gv_bytebuf_t *bb, uint8_t v) {
    return gv_bb_put(bb, &v, 1);
}

static bool gv_bb_be16(gv_bytebuf_t *bb, uint16_t v) {
    uint8_t t[2];
    write_be16(t, v);
    return gv_bb_put(bb, t, 2);
}

static bool gv_bb_be32(gv_bytebuf_t *bb, uint32_t v) {
    uint8_t t[4];
    write_be32(t, v);
    return gv_bb_put(bb, t, 4);
}

typedef struct {
    const uint8_t *data;
    size_t len;
    uint16_t axis_count;
    uint16_t glyph_count;
    uint16_t shared_tuple_count;
    const uint8_t *shared_tuples;
    bool long_offsets;
    const uint8_t *offsets;     /* glyphCount+1 entries, ending at data_array */
    const uint8_t *data_array;
} gvar_import_t;

static void free_geometry(suf_glyf_geometry_t *g) {
    if (!g) return;
    free(g->end_pts);
    free(g->x);
    free(g->y);
    free(g->on_curve);
    memset(g, 0, sizeof(*g));
}

static bool gvar_import_parse(gvar_import_t *gt, const uint8_t *g, size_t glen,
                              uint16_t fvar_axis_count) {
    memset(gt, 0, sizeof(*gt));
    if (!g || glen < 20) return false;
    if (read_be16(g + 0) != 1) return false; /* majorVersion */

    uint16_t axis_count = read_be16(g + 4);
    if (axis_count == 0 || axis_count != fvar_axis_count) return false;

    uint16_t shared_count = read_be16(g + 6);
    uint32_t shared_off = read_be32(g + 8);
    uint16_t glyph_count = read_be16(g + 12);
    uint16_t flags = read_be16(g + 14);
    uint32_t data_array_off = read_be32(g + 16);
    if (glyph_count == 0 || data_array_off == 0 || data_array_off > glen) return false;

    gt->data = g;
    gt->len = glen;
    gt->axis_count = axis_count;
    gt->glyph_count = glyph_count;
    gt->long_offsets = (flags & 0x0001) != 0;

    /* Layout: [20B header][offset array][shared tuples][variation data].
     * The offset array always starts immediately after the header. */
    size_t off_entry = gt->long_offsets ? 4 : 2;
    size_t off_arr_bytes = ((size_t)glyph_count + 1) * off_entry;
    if ((size_t)20 + off_arr_bytes > glen) return false;
    size_t offsets_end = 20 + off_arr_bytes;
    uint32_t limit_after_offsets =
        (shared_count > 0 && shared_off > 0) ? shared_off : data_array_off;
    if (offsets_end > limit_after_offsets || offsets_end > data_array_off) return false;

    gt->offsets = g + 20;
    gt->data_array = g + data_array_off;

    if (shared_count > 0) {
        uint64_t need = (uint64_t)shared_count * axis_count * 2;
        if (shared_off + need > glen) return false;
        gt->shared_tuple_count = shared_count;
        gt->shared_tuples = g + shared_off;
    }
    return true;
}

static bool gvar_glyph_block(const gvar_import_t *gt, uint16_t gid,
                             const uint8_t **out_blk, size_t *out_len) {
    uint64_t start, end;
    size_t arr_base = (size_t)(gt->data_array - gt->data);
    if (gt->long_offsets) {
        start = read_be32(gt->offsets + (size_t)gid * 4);
        end = read_be32(gt->offsets + ((size_t)gid + 1) * 4);
    } else {
        start = (uint64_t)read_be16(gt->offsets + (size_t)gid * 2) * 2;
        end = (uint64_t)read_be16(gt->offsets + ((size_t)gid + 1) * 2) * 2;
    }
    if (end < start) return false;
    if (end > gt->len - arr_base) return false;
    *out_blk = gt->data_array + start;
    *out_len = (size_t)(end - start);
    return true;
}

/* Reads a packed point-number list. The special count byte 0 ("all points")
 * expands to the identity list over total_points_incl_phantom. Returns a
 * malloc'd array of absolute indices (caller frees) or NULL on malformed
 * input. */
static uint16_t *read_packed_point_numbers(const uint8_t *p, size_t avail, size_t *io_pos,
                                           uint16_t total_points_incl_phantom,
                                           uint16_t *out_count) {
    size_t pos = *io_pos;
    if (pos >= avail) return NULL;

    uint8_t c0 = p[pos++];
    uint32_t count;
    uint16_t *nums;

    if (c0 == 0) {
        nums = (uint16_t *)malloc((size_t)total_points_incl_phantom * sizeof(uint16_t));
        if (!nums) return NULL;
        for (uint16_t i = 0; i < total_points_incl_phantom; ++i) nums[i] = i;
        *io_pos = pos;
        *out_count = total_points_incl_phantom;
        return nums;
    } else if (c0 & 0x80) {
        if (pos >= avail) return NULL;
        count = (uint32_t)((c0 & 0x7F) << 8) | p[pos++];
    } else {
        count = c0;
    }
    if (count == 0 || count > 0xFFFF) return NULL;

    nums = (uint16_t *)malloc(count * sizeof(uint16_t));
    if (!nums) return NULL;

    uint32_t n_read = 0;
    int32_t cur = 0;
    while (n_read < count) {
        if (pos >= avail) { free(nums); return NULL; }
        uint8_t ctrl = p[pos++];
        uint32_t run = (uint32_t)(ctrl & 0x7F) + 1;
        bool words = (ctrl & 0x80) != 0;
        for (uint32_t k = 0; k < run && n_read < count; ++k) {
            int32_t d;
            if (words) {
                if (pos + 2 > avail) { free(nums); return NULL; }
                d = (int16_t)read_be16(p + pos);
                pos += 2;
            } else {
                if (pos >= avail) { free(nums); return NULL; }
                d = p[pos++];
            }
            cur += d;
            if (cur < 0 || cur > 0xFFFF) { free(nums); return NULL; }
            nums[n_read++] = (uint16_t)cur;
        }
    }

    *io_pos = pos;
    *out_count = (uint16_t)count;
    return nums;
}

/* Reads packed deltas until logical_count values produced.
 * Run header: bits 6-7 select width (00=int8, 01=int16, 10=zeros, 11=int32);
 * bits 0-5 give the element count minus one. */
static bool read_packed_deltas(const uint8_t *p, size_t avail, size_t *io_pos,
                               uint32_t logical_count, int32_t *out) {
    size_t pos = *io_pos;
    uint32_t done = 0;
    while (done < logical_count) {
        if (pos >= avail) return false;
        uint8_t ctrl = p[pos++];
        uint32_t run = (uint32_t)(ctrl & 0x3F) + 1;
        if ((ctrl & 0xC0) == 0xC0) {          /* DELTAS_ARE_LONGS: int32 */
            for (uint32_t k = 0; k < run && done < logical_count; ++k) {
                if (pos + 4 > avail) return false;
                out[done++] = (int32_t)read_be32(p + pos);
                pos += 4;
            }
        } else if (ctrl & 0x80) {             /* DELTAS_ARE_ZERO */
            for (uint32_t k = 0; k < run && done < logical_count; ++k) {
                out[done++] = 0;
            }
        } else if (ctrl & 0x40) {             /* DELTAS_ARE_WORDS: int16 */
            for (uint32_t k = 0; k < run && done < logical_count; ++k) {
                if (pos + 2 > avail) return false;
                out[done++] = (int16_t)read_be16(p + pos);
                pos += 2;
            }
        } else {                              /* signed bytes */
            for (uint32_t k = 0; k < run && done < logical_count; ++k) {
                if (pos >= avail) return false;
                out[done++] = (int8_t)p[pos];
                pos++;
            }
        }
    }
    *io_pos = pos;
    return true;
}

/* Inferred-delta algorithm (IUP-style), one axis at a time. */
static int32_t gvar_infer_one(int32_t tc, int32_t rc, int32_t nc, int32_t rd, int32_t nd) {
    if (rc == nc) return (rd == nd) ? rd : 0;
    int32_t lo = (rc < nc) ? rc : nc;
    int32_t hi = (rc > nc) ? rc : nc;
    if (tc <= lo) return (rc < nc) ? rd : nd;
    if (tc >= hi) return (rc > nc) ? rd : nd;
    double t = (double)(tc - rc) / (double)(nc - rc);
    double v = (1.0 - t) * (double)rd + t * (double)nd;
    return (v >= 0.0) ? (int32_t)(v + 0.5) : -(int32_t)(-v + 0.5);
}

/* Fills in deltas for points inside partially-covered contours, using
 * wrap-around referenced neighbors within each contour. */
static void gvar_infer_unreferenced(const suf_glyf_geometry_t *geo,
                                    const uint8_t *ref, int32_t *dx, int32_t *dy) {
    uint32_t start = 0;
    for (uint16_t c = 0; c < geo->contour_count; ++c) {
        uint32_t end = geo->end_pts[c];
        if (end < start || end >= geo->point_count) { start = end + 1; continue; }

        bool any = false, all = true;
        for (uint32_t i = start; i <= end; ++i) {
            if (ref[i]) any = true; else all = false;
        }
        if (!any || all) { start = end + 1; continue; }

        uint32_t cnt = end - start + 1;
        for (uint32_t p = start; p <= end; ++p) {
            if (ref[p]) continue;
            uint32_t prev = p, next = p;
            for (uint32_t k = 1; k < cnt; ++k) {
                uint32_t cand = start + ((p - start) + cnt - k) % cnt;
                if (ref[cand]) { prev = cand; break; }
            }
            for (uint32_t k = 1; k < cnt; ++k) {
                uint32_t cand = start + ((p - start) + k) % cnt;
                if (ref[cand]) { next = cand; break; }
            }
            dx[p] = gvar_infer_one(geo->x[p], geo->x[prev], geo->x[next], dx[prev], dx[next]);
            dy[p] = gvar_infer_one(geo->y[p], geo->y[prev], geo->y[next], dy[prev], dy[next]);
        }
        start = end + 1;
    }
}

/* Emits packed point numbers describing the dense ascending list 0..n-1. */
static bool gvar_emit_dense_points(gv_bytebuf_t *bb, uint32_t n) {
    if (n == 0 || n >= 0x8000) return false; /* count byte 0 means "all points" */
    if (n < 128) {
        if (!gv_bb_u8(bb, (uint8_t)n)) return false;
    } else {
        if (!gv_bb_u8(bb, (uint8_t)(0x80 | (n >> 8)))) return false;
        if (!gv_bb_u8(bb, (uint8_t)(n & 0xFF))) return false;
    }
    bool first = true;
    uint32_t remaining = n;
    while (remaining > 0) {
        uint32_t chunk = (remaining > 128) ? 128 : remaining;
        if (!gv_bb_u8(bb, (uint8_t)(chunk - 1))) return false; /* byte-width run */
        for (uint32_t k = 0; k < chunk; ++k) {
            if (!gv_bb_u8(bb, first ? 0 : 1)) return false;
            first = false;
        }
        remaining -= chunk;
    }
    return true;
}

/* Emits packed deltas using homogeneous runs (zero / int8 / int16 / int32). */
static bool gvar_pack_deltas(gv_bytebuf_t *bb, const int32_t *v, uint32_t n) {
    uint32_t i = 0;
    while (i < n) {
        uint8_t cls; /* 0 = zero, 1 = int8, 2 = int16, 3 = int32 */
        if (v[i] == 0) cls = 0;
        else if (v[i] >= -128 && v[i] <= 127) cls = 1;
        else if (v[i] >= -32768 && v[i] <= 32767) cls = 2;
        else cls = 3;

        uint32_t run = 0;
        while (i + run < n && run < 64) {
            int32_t x = v[i + run];
            uint8_t xc = (x == 0) ? 0
                       : ((x >= -128 && x <= 127) ? 1
                       : ((x >= -32768 && x <= 32767) ? 2 : 3));
            if (xc != cls) break;
            run++;
        }

        if (cls == 0) {
            if (!gv_bb_u8(bb, (uint8_t)(0x80 | (run - 1)))) return false;
        } else if (cls == 1) {
            if (!gv_bb_u8(bb, (uint8_t)(run - 1))) return false;
            for (uint32_t k = 0; k < run; ++k) {
                if (!gv_bb_u8(bb, (uint8_t)v[i + k])) return false;
            }
        } else if (cls == 2) {
            if (!gv_bb_u8(bb, (uint8_t)(0x40 | (run - 1)))) return false;
            for (uint32_t k = 0; k < run; ++k) {
                if (!gv_bb_be16(bb, (uint16_t)v[i + k])) return false;
            }
        } else {
            if (!gv_bb_u8(bb, (uint8_t)(0xC0 | (run - 1)))) return false;
            for (uint32_t k = 0; k < run; ++k) {
                if (!gv_bb_be32(bb, (uint32_t)v[i + k])) return false;
            }
        }
        i += run;
    }
    return true;
}

typedef struct {
    uint16_t data_size;
    uint16_t tuple_index;
    const uint8_t *peak;
    const uint8_t *istart;
    const uint8_t *iend;
} gvar_tuple_hdr_t;

/* Remaps one source GlyphVariationData block onto the materialized SUF point
 * list. Returns a malloc'd self-contained block (caller frees) or NULL when
 * the source block cannot be used. */
static uint8_t *remap_glyph_variation_block(const gvar_import_t *gt,
                                            const uint8_t *blk, size_t blk_len,
                                            const suf_glyf_geometry_t *geo,
                                            const int32_t *pt_map, uint32_t new_pt_count,
                                            size_t *out_len) {
    *out_len = 0;
    if (!blk || blk_len < 4 || new_pt_count == 0 || !pt_map) return NULL;
    if (!geo || geo->point_count == 0 || geo->contour_count == 0 ||
        !geo->x || !geo->y || !geo->end_pts) return NULL;

    uint16_t orig_pts = geo->point_count;
    uint16_t total_pts = (uint16_t)(orig_pts + 4); /* gvar lists include phantoms */

    uint16_t cvt_word = read_be16(blk + 0);
    uint16_t tuple_count = cvt_word & 0x0FFF;
    bool has_shared_points = (cvt_word & 0x8000) != 0;
    uint16_t data_offset = read_be16(blk + 2);
    if (tuple_count == 0) return NULL;
    if (data_offset < 4 || data_offset >= blk_len) return NULL;

    gvar_tuple_hdr_t *tuples = (gvar_tuple_hdr_t *)calloc(tuple_count, sizeof(gvar_tuple_hdr_t));
    if (!tuples) return NULL;

    bool ok = true;
    size_t hdr_pos = 4;
    for (uint16_t t = 0; t < tuple_count && ok; ++t) {
        if (hdr_pos + 4 > data_offset) { ok = false; break; }
        tuples[t].data_size = read_be16(blk + hdr_pos);
        tuples[t].tuple_index = read_be16(blk + hdr_pos + 2);
        hdr_pos += 4;
        uint16_t ti = tuples[t].tuple_index;

        if (ti & 0x8000) { /* EMBEDDED_PEAK_TUPLE */
            if (hdr_pos + (size_t)gt->axis_count * 2 > data_offset) { ok = false; break; }
            tuples[t].peak = blk + hdr_pos;
            hdr_pos += (size_t)gt->axis_count * 2;
        } else {
            uint16_t idx = ti & 0x0FFF;
            if (idx >= gt->shared_tuple_count) { ok = false; break; }
            tuples[t].peak = gt->shared_tuples + (size_t)idx * gt->axis_count * 2;
        }
        if (ti & 0x4000) { /* INTERMEDIATE_REGION */
            if (hdr_pos + (size_t)gt->axis_count * 4 > data_offset) { ok = false; break; }
            tuples[t].istart = blk + hdr_pos;
            tuples[t].iend = blk + hdr_pos + (size_t)gt->axis_count * 2;
            hdr_pos += (size_t)gt->axis_count * 4;
        }
    }
    if (!ok) { free(tuples); return NULL; }

    const uint8_t *ser = blk + data_offset;
    size_t ser_avail = blk_len - data_offset;
    size_t spos = 0;

    uint16_t *shared_nums = NULL;
    uint16_t shared_n = 0;
    if (has_shared_points) {
        shared_nums = read_packed_point_numbers(ser, ser_avail, &spos, total_pts, &shared_n);
        if (!shared_nums) { free(tuples); return NULL; }
    }

    uint32_t *ser_sizes = (uint32_t *)calloc(tuple_count ? tuple_count : 1, sizeof(uint32_t));
    gv_bytebuf_t out_ser = {0};
    if (!ser_sizes) ok = false;

    for (uint16_t t = 0; t < tuple_count && ok; ++t) {
        /* Each tuple's serialized run occupies [t_start, t_end); private
         * point numbers and deltas are addressed relative to the run start,
         * matching fontTools/FreeType. Slack before the next boundary is
         * legal padding and simply skipped. */
        size_t t_start = spos;
        if (t_start + tuples[t].data_size > ser_avail) { ok = false; break; }
        size_t t_end = t_start + tuples[t].data_size;
        size_t rpos = t_start;

        /* Point numbers: private list or the shared one. */
        uint16_t *nums;
        uint16_t n;
        bool nums_private = (tuples[t].tuple_index & 0x2000) != 0;
        if (nums_private) {
            nums = read_packed_point_numbers(ser, t_end, &rpos, total_pts, &n);
        } else {
            if (!shared_nums) { ok = false; break; }
            nums = shared_nums;
            n = shared_n;
        }
        if (!nums || n == 0) { if (nums_private) free(nums); ok = false; break; }

        /* Packed deltas: X list then Y list. */
        int32_t *dxl = (int32_t *)calloc(n, sizeof(int32_t));
        int32_t *dyl = (int32_t *)calloc(n, sizeof(int32_t));
        if (!dxl || !dyl ||
            !read_packed_deltas(ser, t_end, &rpos, n, dxl) ||
            !read_packed_deltas(ser, t_end, &rpos, n, dyl)) {
            free(dxl); free(dyl);
            if (nums_private) free(nums);
            ok = false;
            break;
        }
        free(dxl);
        free(dyl);

        spos = t_end; /* next tuple's run starts at its declared boundary */

        /* Scatter explicit deltas over original points; phantoms dropped. */
        uint8_t *ref = (uint8_t *)calloc(orig_pts, 1);
        int32_t *dx = (int32_t *)calloc(orig_pts, sizeof(int32_t));
        int32_t *dy = (int32_t *)calloc(orig_pts, sizeof(int32_t));
        if (!ref || !dx || !dy) {
            free(ref); free(dx); free(dy);
            if (nums_private) free(nums);
            ok = false;
            break;
        }
        for (uint32_t k = 0; k < n; ++k) {
            if (nums[k] >= total_pts) { ok = false; break; }
            if (nums[k] < orig_pts) {
                ref[nums[k]] = 1;
                dx[nums[k]] += dxl[k];
                dy[nums[k]] += dyl[k];
            }
        }
        if (nums_private) free(nums);
        if (!ok) { free(ref); free(dx); free(dy); break; }

        gvar_infer_unreferenced(geo, ref, dx, dy);
        free(ref);

        /* Map onto the materialized point list. Synthesized midpoints get the
         * truncated average of their two originals, matching the decoder's
         * midpoint arithmetic exactly under scalar interpolation. */
        int32_t *ndx = (int32_t *)malloc(new_pt_count * sizeof(int32_t));
        int32_t *ndy = (int32_t *)malloc(new_pt_count * sizeof(int32_t));
        if (!ndx || !ndy) { free(ndx); free(ndy); free(dx); free(dy); ok = false; break; }

        for (uint32_t k = 0; k < new_pt_count && ok; ++k) {
            int32_t m = pt_map[k];
            if (m >= 0) {
                if (m >= (int32_t)orig_pts) { ok = false; break; }
                ndx[k] = dx[m];
                ndy[k] = dy[m];
            } else {
                int32_t enc = -m - 1;
                int32_t mi = SUF_PTMAP_MID_I(enc);
                int32_t mj = SUF_PTMAP_MID_J(enc);
                if (mi >= (int32_t)orig_pts || mj >= (int32_t)orig_pts) { ok = false; break; }
                ndx[k] = (dx[mi] + dx[mj]) / 2;
                ndy[k] = (dy[mi] + dy[mj]) / 2;
            }
        }
        free(dx);
        free(dy);
        if (!ok) { free(ndx); free(ndy); break; }

        uint32_t before = out_ser.len;
        if (!gvar_emit_dense_points(&out_ser, new_pt_count) ||
            !gvar_pack_deltas(&out_ser, ndx, new_pt_count) ||
            !gvar_pack_deltas(&out_ser, ndy, new_pt_count)) {
            free(ndx);
            ok = false;
            break;
        }
        free(ndx);
        ser_sizes[t] = (uint32_t)(out_ser.len - before);
    }

    free(shared_nums);

    uint8_t *result = NULL;
    do {
        if (!ok) break;

        gv_bytebuf_t out = {0};
        bool wok = true;

        uint16_t headers_size = 0;
        for (uint16_t t = 0; t < tuple_count; ++t) {
            headers_size = (uint16_t)(headers_size + 4 + (size_t)gt->axis_count * 2);
            if (tuples[t].tuple_index & 0x4000) {
                headers_size = (uint16_t)(headers_size + (size_t)gt->axis_count * 4);
            }
        }
        uint32_t dof = 4u + headers_size;
        if (dof > 0xFFFF) wok = false;

        if (wok) wok = gv_bb_be16(&out, tuple_count);      /* no SHARED_POINT_NUMBERS */
        if (wok) wok = gv_bb_be16(&out, (uint16_t)dof);

        for (uint16_t t = 0; t < tuple_count && wok; ++t) {
            uint16_t ti = tuples[t].tuple_index;
            /* Self-contained: embedded peak + private points [+ intermediate]. */
            uint16_t new_ti = (uint16_t)(0x8000 | 0x2000 | (ti & 0x4000));
            wok = gv_bb_be16(&out, ser_sizes[t]);
            if (wok) wok = gv_bb_be16(&out, new_ti);
            if (wok) wok = gv_bb_put(&out, tuples[t].peak, (size_t)gt->axis_count * 2);
            if (wok && (ti & 0x4000)) {
                wok = gv_bb_put(&out, tuples[t].istart, (size_t)gt->axis_count * 2);
                if (wok) wok = gv_bb_put(&out, tuples[t].iend, (size_t)gt->axis_count * 2);
            }
        }
        if (wok) wok = gv_bb_put(&out, out_ser.buf, out_ser.len);

        if (wok) {
            result = out.buf;
            *out_len = out.len;
        } else {
            free(out.buf);
        }
    } while (0);

    free(out_ser.buf);
    free(ser_sizes);
    free(tuples);
    return result;
}

/* ========================================================================= */
/* Inbound: TTF (.ttf) -> .suf Converter                                     */
/* ========================================================================= */

/* Preserve the source font's identity metadata (name table records) so
 * roundtripped fonts keep their original family/subfamily/PostScript names
 * instead of being re-branded. Accepts Windows UTF-16BE and legacy Mac
 * Roman/Latin-1 strings; stores UTF-8 in the builder's SNM1 blob. */
static void import_name_records(suf_builder_t *b, const uint8_t *name_tbl, size_t name_len) {
    if (!b || !name_tbl || name_len < 6) return;

    uint16_t count = read_be16(name_tbl + 2);
    uint16_t str_off = read_be16(name_tbl + 4);
    if ((uint32_t)6 + (uint32_t)count * 12 > name_len) return;

    for (uint16_t r = 0; r < count; ++r) {
        size_t rec = 6 + ((size_t)r * 12);
        uint16_t platform_id = read_be16(name_tbl + rec + 0);
        uint16_t encoding_id = read_be16(name_tbl + rec + 2);
        /* uint16_t language_id = read_be16(name_tbl + rec + 4); */
        uint16_t name_id = read_be16(name_tbl + rec + 6);
        uint16_t length = read_be16(name_tbl + rec + 8);
        uint16_t offset = read_be16(name_tbl + rec + 10);

        /* Keep identity/metadata records only (skip style-only duplicates
         * like preferred family when they match nothing useful is fine —
         * we preserve whatever the font carries within the safe set). */
        bool keep = (name_id >= 0 && name_id <= 9) ||
                    (name_id >= 11 && name_id <= 14) ||
                    (name_id == 16 || name_id == 17);
        if (!keep || length == 0) continue;

        size_t s_off = (size_t)str_off + offset;
        if (s_off + length > name_len) continue;
        const uint8_t *s = name_tbl + s_off;

        char utf8[512];
        size_t o = 0;
        bool ok = true;

        if (platform_id == 0 || platform_id == 3) {
            /* Unicode / Windows: UTF-16BE code units -> UTF-8 */
            for (size_t i = 0; i + 1 < length && o + 4 < sizeof(utf8); i += 2) {
                uint32_t cp = ((uint32_t)s[i] << 8) | s[i + 1];
                if (cp >= 0xD800 && cp <= 0xDBFF && i + 3 < length &&
                    s[i + 2] >= 0xD8 && s[i + 2] <= 0xDB) {
                    uint32_t lo = ((uint32_t)s[i + 2] << 8) | s[i + 3];
                    if (lo >= 0xDC00 && lo <= 0xDFFF) {
                        cp = 0x10000UL + ((cp - 0xD800UL) << 10) + (lo - 0xDC00UL);
                        i += 2;
                    }
                }
                if (cp < 0x80) {
                    utf8[o++] = (char)cp;
                } else if (cp < 0x800) {
                    utf8[o++] = (char)(0xC0 | (cp >> 6));
                    utf8[o++] = (char)(0x80 | (cp & 0x3F));
                } else if (cp < 0x10000) {
                    utf8[o++] = (char)(0xE0 | (cp >> 12));
                    utf8[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    utf8[o++] = (char)(0x80 | (cp & 0x3F));
                } else {
                    utf8[o++] = (char)(0xF0 | (cp >> 18));
                    utf8[o++] = (char)(0x80 | ((cp >> 12) & 0x3F));
                    utf8[o++] = (char)(0x80 | ((cp >> 6) & 0x3F));
                    utf8[o++] = (char)(0x80 | (cp & 0x3F));
                }
            }
        } else {
            /* Platform 1 (Mac): bytes are Latin-1-ish; map >127 through
             * Latin-1 to UTF-8, ASCII passes through untouched. */
            for (size_t i = 0; i < length && o + 2 < sizeof(utf8); ++i) {
                uint8_t ch = s[i];
                if (ch < 0x80) {
                    utf8[o++] = (char)ch;
                } else {
                    utf8[o++] = (char)(0xC0 | (ch >> 6));
                    utf8[o++] = (char)(0x80 | (ch & 0x3F));
                }
            }
            (void)encoding_id;
        }

        if (!ok || o == 0 || o >= sizeof(utf8)) continue;
        utf8[o] = '\0';
        suf_builder_set_name(b, name_id, utf8);
    }
}

suf_status_t suf_conv_ttf_to_suf(const uint8_t *ttf_data, size_t ttf_size, suf_builder_t **out_builder) {
    if (!ttf_data || !out_builder) return SUF_ERR_NULL_POINTER;
    if (ttf_size < 12) return SUF_ERR_BUFFER_TOO_SMALL;

    uint32_t sfnt_version = read_be32(ttf_data);
    if (sfnt_version != 0x00010000 && sfnt_version != 0x74727565 && sfnt_version != 0x4F54544F) {
        return SUF_ERR_INVALID_MAGIC;
    }

    uint16_t num_tables = read_be16(ttf_data + 4);
    if (12 + (size_t)num_tables * 16 > ttf_size) return SUF_ERR_CORRUPT_DATA;

    const uint8_t *head_ptr = NULL; size_t head_len = 0;
    const uint8_t *hhea_ptr = NULL; size_t hhea_len = 0;
    const uint8_t *maxp_ptr = NULL; size_t maxp_len = 0;
    const uint8_t *hmtx_ptr = NULL; size_t hmtx_len = 0;
    const uint8_t *cmap_ptr = NULL; size_t cmap_len = 0;
    const uint8_t *loca_ptr = NULL; size_t loca_len = 0;
    const uint8_t *glyf_ptr = NULL; size_t glyf_len = 0;
    const uint8_t *fvar_ptr = NULL; size_t fvar_len = 0;
    const uint8_t *gvar_ptr = NULL; size_t gvar_len = 0;
    const uint8_t *name_ptr = NULL; size_t name_len = 0;

    for (uint16_t i = 0; i < num_tables; ++i) {
        const uint8_t *rec = ttf_data + 12 + (i * 16);
        uint32_t tag = read_be32(rec);
        uint32_t offset = read_be32(rec + 8);
        uint32_t length = read_be32(rec + 12);

        if ((uint64_t)offset + length > ttf_size) continue;

        if (tag == 0x68656164) { head_ptr = ttf_data + offset; head_len = length; }
        else if (tag == 0x68686561) { hhea_ptr = ttf_data + offset; hhea_len = length; }
        else if (tag == 0x6D617870) { maxp_ptr = ttf_data + offset; maxp_len = length; }
        else if (tag == 0x686D7478) { hmtx_ptr = ttf_data + offset; hmtx_len = length; }
        else if (tag == 0x636D6170) { cmap_ptr = ttf_data + offset; cmap_len = length; }
        else if (tag == 0x6C6F6361) { loca_ptr = ttf_data + offset; loca_len = length; }
        else if (tag == 0x676C7966) { glyf_ptr = ttf_data + offset; glyf_len = length; }
        else if (tag == 0x66766172) { fvar_ptr = ttf_data + offset; fvar_len = length; }
        else if (tag == 0x67766172) { gvar_ptr = ttf_data + offset; gvar_len = length; }
        else if (tag == 0x6E616D65) { name_ptr = ttf_data + offset; name_len = length; }
    }

    if (!head_ptr || !hhea_ptr || !maxp_ptr || head_len < 54 || hhea_len < 36 || maxp_len < 6) {
        return SUF_ERR_INVALID_HEADER;
    }

    uint16_t units_per_em = read_be16(head_ptr + 18);
    int16_t min_x = (int16_t)read_be16(head_ptr + 36);
    int16_t min_y = (int16_t)read_be16(head_ptr + 38);
    int16_t max_x = (int16_t)read_be16(head_ptr + 40);
    int16_t max_y = (int16_t)read_be16(head_ptr + 42);
    int16_t index_to_loc_format = (int16_t)read_be16(head_ptr + 50);

    int16_t ascender = (int16_t)read_be16(hhea_ptr + 4);
    int16_t descender = (int16_t)read_be16(hhea_ptr + 6);
    int16_t line_gap = (int16_t)read_be16(hhea_ptr + 8);
    uint16_t num_h_metrics = read_be16(hhea_ptr + 34);

    uint16_t num_glyphs = read_be16(maxp_ptr + 4);
    if (num_glyphs == 0) num_glyphs = 1;

    /* Vector-only font: real boot rasters are never fabricated from outlines.
     * Renderers rasterize on demand or fall back to the vector pipeline. */
    uint16_t flags = SUF_FLAG_OS_VECTOR;
    if (fvar_ptr && fvar_len >= 16) {
        flags |= SUF_FLAG_VARIABLE;
    }

    suf_builder_t *b = suf_builder_create(units_per_em, ascender, descender, flags);
    if (!b) return SUF_ERR_ALLOC_FAIL;

    suf_builder_set_line_gap(b, line_gap);
    suf_builder_set_bbox(b, min_x, min_y, max_x, max_y);

    /* Parse fvar variable axes if present */
    uint16_t fvar_axis_count = 0;
    if (fvar_ptr && fvar_len >= 16) {
        uint16_t axes_offset = read_be16(fvar_ptr + 4);
        uint16_t axis_count = read_be16(fvar_ptr + 8);
        uint16_t axis_size = read_be16(fvar_ptr + 10);

        for (uint16_t a = 0; a < axis_count; ++a) {
            size_t rec_off = axes_offset + (a * axis_size);
            if (rec_off + 20 > fvar_len) break;

            uint32_t axis_tag = read_be32(fvar_ptr + rec_off + 0);
            int32_t min_raw = (int32_t)read_be32(fvar_ptr + rec_off + 4);
            int32_t def_raw = (int32_t)read_be32(fvar_ptr + rec_off + 8);
            int32_t max_raw = (int32_t)read_be32(fvar_ptr + rec_off + 12);

            float min_val = (float)min_raw / 65536.0f;
            float def_val = (float)def_raw / 65536.0f;
            float max_val = (float)max_raw / 65536.0f;

            char axis_name[32] = {0};
            axis_name[0] = (char)((axis_tag >> 24) & 0xFF);
            axis_name[1] = (char)((axis_tag >> 16) & 0xFF);
            axis_name[2] = (char)((axis_tag >> 8) & 0xFF);
            axis_name[3] = (char)(axis_tag & 0xFF);
            axis_name[4] = '\0';

            if (suf_builder_add_axis(b, axis_tag, axis_name, min_val, def_val, max_val)) {
                ++fvar_axis_count;
            }
        }
    }

    /* Preserve the source font's name records (family, subfamily, version,
     * PostScript name, ...) for faithful re-export. */
    if (name_ptr) {
        import_name_records(b, name_ptr, name_len);
    }

    /* Per-glyph outline variations: only meaningful alongside parsed axes. */
    gvar_import_t gvar_ctx;
    bool have_gvar = false;
    if (gvar_ptr && fvar_axis_count > 0 &&
        gvar_import_parse(&gvar_ctx, gvar_ptr, gvar_len, fvar_axis_count)) {
        have_gvar = true;
    }

    uint32_t *glyph_to_cp = (uint32_t *)calloc(num_glyphs, sizeof(uint32_t));
    if (cmap_ptr && cmap_len >= 4) {
        uint16_t num_subtables = read_be16(cmap_ptr + 2);
        for (uint16_t s = 0; s < num_subtables; ++s) {
            size_t sub_rec = 4 + (s * 8);
            if (sub_rec + 8 > cmap_len) break;
            uint32_t sub_offset = read_be32(cmap_ptr + sub_rec + 4);
            if (sub_offset + 6 > cmap_len) continue;

            uint16_t format = read_be16(cmap_ptr + sub_offset);
            if (format == 4 && sub_offset + 14 <= cmap_len) {
                uint16_t seg_count_x2 = read_be16(cmap_ptr + sub_offset + 6);
                uint16_t seg_count = seg_count_x2 / 2;
                size_t end_codes_off = sub_offset + 14;
                size_t start_codes_off = end_codes_off + seg_count_x2 + 2;
                size_t id_deltas_off = start_codes_off + seg_count_x2;
                size_t id_range_off = id_deltas_off + seg_count_x2;

                if (id_range_off <= cmap_len) {
                    for (uint16_t seg = 0; seg < seg_count; ++seg) {
                        uint16_t start = read_be16(cmap_ptr + start_codes_off + seg * 2);
                        uint16_t end = read_be16(cmap_ptr + end_codes_off + seg * 2);
                        int16_t delta = (int16_t)read_be16(cmap_ptr + id_deltas_off + seg * 2);
                        uint16_t range_offset = read_be16(cmap_ptr + id_range_off + seg * 2);

                        if (start == 0xFFFF) break;
                        for (uint32_t cp = start; cp <= end; ++cp) {
                            uint32_t gid = 0;
                            if (range_offset == 0) {
                                gid = (cp + (uint32_t)(uint16_t)delta) & 0xFFFF;
                            } else {
                                size_t glyph_idx_addr = id_range_off + seg * 2 + range_offset + (cp - start) * 2;
                                if (glyph_idx_addr + 2 <= cmap_len) {
                                    gid = read_be16(cmap_ptr + glyph_idx_addr);
                                    if (gid != 0) gid = (gid + (uint32_t)(uint16_t)delta) & 0xFFFF;
                                }
                            }
                            if (gid < num_glyphs && glyph_to_cp[gid] == 0) {
                                glyph_to_cp[gid] = cp;
                            }
                        }
                    }
                }
            } else if (format == 12 && sub_offset + 16 <= cmap_len) {
                uint32_t num_groups = read_be32(cmap_ptr + sub_offset + 12);
                size_t groups_off = sub_offset + 16;
                for (uint32_t g = 0; g < num_groups; ++g) {
                    if (groups_off + (g + 1) * 12 > cmap_len) break;
                    uint32_t start_cp = read_be32(cmap_ptr + groups_off + g * 12 + 0);
                    uint32_t end_cp = read_be32(cmap_ptr + groups_off + g * 12 + 4);
                    uint32_t start_gid = read_be32(cmap_ptr + groups_off + g * 12 + 8);

                    for (uint32_t cp = start_cp; cp <= end_cp; ++cp) {
                        uint32_t gid = start_gid + (cp - start_cp);
                        if (gid < num_glyphs && glyph_to_cp[gid] == 0) {
                            glyph_to_cp[gid] = cp;
                        }
                    }
                }
            }
        }
    }

    for (uint16_t gid = 0; gid < num_glyphs; ++gid) {
        suf_metric_t metric;
        memset(&metric, 0, sizeof(metric));

        if (hmtx_ptr && hmtx_len > 0) {
            if (gid < num_h_metrics && (gid + 1) * 4 <= hmtx_len) {
                metric.advance_width = (int16_t)read_be16(hmtx_ptr + gid * 4);
                metric.left_side_bearing = (int16_t)read_be16(hmtx_ptr + gid * 4 + 2);
            } else if (num_h_metrics > 0) {
                metric.advance_width = (int16_t)read_be16(hmtx_ptr + (num_h_metrics - 1) * 4);
                size_t lsb_off = (size_t)num_h_metrics * 4 + (gid - num_h_metrics) * 2;
                if (lsb_off + 2 <= hmtx_len) {
                    metric.left_side_bearing = (int16_t)read_be16(hmtx_ptr + lsb_off);
                }
            }
        }

        uint8_t outline_cmds[8192];
        size_t outline_len = 0;
        int32_t pt_map[4096];
        uint32_t new_pt_count = 0;
        suf_glyf_geometry_t geo;
        memset(&geo, 0, sizeof(geo));
        memset(pt_map, 0xFF, sizeof(pt_map));

        if (loca_ptr && glyf_ptr) {
            glyf_source_t gsrc;
            gsrc.glyf = glyf_ptr;
            gsrc.glyf_len = glyf_len;
            gsrc.loca = loca_ptr;
            gsrc.loca_len = loca_len;
            gsrc.index_to_loc_format = index_to_loc_format;

            int16_t gx_min = 0, gy_min = 0, gx_max = 0, gy_max = 0;
            bool glyph_is_composite = false;
            outline_len = decode_ttf_glyf_to_suf(&gsrc, (uint16_t)gid,
                                                 &gx_min, &gy_min, &gx_max, &gy_max,
                                                 outline_cmds, sizeof(outline_cmds),
                                                 have_gvar ? pt_map : NULL,
                                                 have_gvar ? &new_pt_count : NULL,
                                                 have_gvar ? &geo : NULL,
                                                 &glyph_is_composite);

            metric.x_min = gx_min;
            metric.y_min = gy_min;
            metric.x_max = gx_max;
            metric.y_max = gy_max;

            /* Composite glyphs are flattened: their per-component deltas
             * cannot be re-attributed to a single source point list. */
            if (glyph_is_composite) new_pt_count = 0;
        }

        uint32_t cp = glyph_to_cp[gid];
        if (cp == 0 && gid != 0) {
            cp = 0xE000 + gid;
        }

        uint32_t new_gid = suf_builder_add_glyph(b, cp, &metric, NULL, 0, outline_cmds, outline_len);

        /* Remap this glyph's gvar deltas onto the materialized point list. */
        if (have_gvar && outline_len > 0 && new_pt_count > 0 &&
            gid < gvar_ctx.glyph_count) {
            const uint8_t *src_blk = NULL;
            size_t src_len = 0;
            if (gvar_glyph_block(&gvar_ctx, (uint16_t)gid, &src_blk, &src_len) && src_len > 0) {
                size_t remapped_len = 0;
                uint8_t *remapped = remap_glyph_variation_block(&gvar_ctx, src_blk, src_len,
                                                                &geo, pt_map, new_pt_count,
                                                                &remapped_len);
                if (remapped) {
                    suf_builder_set_glyph_variation(b, new_gid, remapped, remapped_len);
                    free(remapped);
                }
            }
        }

        free_geometry(&geo);
    }

    if (glyph_to_cp) free(glyph_to_cp);

    *out_builder = b;
    return SUF_OK;
}

/* ========================================================================= */
/* Name Table Builder (Format 0, UTF-16BE Windows Platform 3 / Encoding 1)   */
/* ========================================================================= */

/* Decode one UTF-8 sequence; returns codepoint and advances *pos. */
static uint32_t utf8_next(const char *s, size_t n, size_t *pos) {
    uint8_t c = (uint8_t)s[*pos];
    size_t i = *pos;
    uint32_t cp = 0xFFFD;
    if (c < 0x80) {
        cp = c;
        i += 1;
    } else if ((c & 0xE0) == 0xC0 && i + 1 < n && (s[i + 1] & 0xC0) == 0x80) {
        cp = ((uint32_t)(c & 0x1F) << 6) | ((uint8_t)s[i + 1] & 0x3F);
        i += 2;
    } else if ((c & 0xF0) == 0xE0 && i + 2 < n && (s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80) {
        cp = ((uint32_t)(c & 0x0F) << 12) | (((uint8_t)s[i + 1] & 0x3F) << 6) | ((uint8_t)s[i + 2] & 0x3F);
        i += 3;
    } else if ((c & 0xF8) == 0xF0 && i + 3 < n &&
               (s[i + 1] & 0xC0) == 0x80 && (s[i + 2] & 0xC0) == 0x80 && (s[i + 3] & 0xC0) == 0x80) {
        cp = ((uint32_t)(c & 0x07) << 18) | (((uint8_t)s[i + 1] & 0x3F) << 12) |
             (((uint8_t)s[i + 2] & 0x3F) << 6) | ((uint8_t)s[i + 3] & 0x3F);
        i += 4;
    } else {
        i += 1;
    }
    *pos = i;
    return cp;
}

/* Encode codepoint as UTF-16BE (surrogate pair when astral). Returns bytes written. */
static size_t utf16be_put(uint8_t *out, uint32_t cp) {
    if (cp >= 0x10000UL) {
        uint32_t v = cp - 0x10000UL;
        uint16_t hi = (uint16_t)(0xD800 | (v >> 10));
        uint16_t lo = (uint16_t)(0xDC00 | (v & 0x3FF));
        out[0] = (uint8_t)(hi >> 8);
        out[1] = (uint8_t)(hi & 0xFF);
        out[2] = (uint8_t)(lo >> 8);
        out[3] = (uint8_t)(lo & 0xFF);
        return 4;
    }
    out[0] = (uint8_t)(cp >> 8);
    out[1] = (uint8_t)(cp & 0xFF);
    return 2;
}

static uint8_t *build_name_table_ex(size_t *out_size, const suf_var_axis_t *axes, uint16_t axis_count,
                                    const uint8_t *suf_data, size_t suf_size) {
    /* Record plan:
     * - every preserved name record from the SUF SNM1 blob (IDs 0..17),
     * - legacy defaults for any missing core ID (1..6),
     * - one record per variation axis (nameIDs 256+, referenced by fvar).
     * This keeps the source font's identity instead of re-branding it. */
    typedef struct {
        uint16_t id;
        const char *utf8;
        size_t len;
    } name_rec_t;

    static const struct { uint16_t id; const char *def; } core_defaults[6] = {
        { 1, "SuperUnicode Font" },
        { 2, "Regular" },
        { 3, "SuperUnicodeFont:1.0" },
        { 4, "SuperUnicode Font Regular" },
        { 5, "Version 1.0" },
        { 6, "SuperUnicodeFont-Regular" }
    };

    name_rec_t recs[SUF_NAMES_MAX_RECORDS];
    uint16_t num_records = 0;

    bool have_core[7] = { false, false, false, false, false, false, false };

    for (uint16_t id = 0; id <= 17 && num_records < SUF_NAMES_MAX_RECORDS; ++id) {
        const char *p = NULL;
        size_t len = 0;
        if (suf_data && suf_get_name(suf_data, suf_size, id, &p, &len) == SUF_OK &&
            p && len > 0) {
            recs[num_records].id = id;
            recs[num_records].utf8 = p;
            recs[num_records].len = len;
            num_records++;
            if (id >= 1 && id <= 6) have_core[id] = true;
        }
    }

    for (int k = 0; k < 6 && num_records < SUF_NAMES_MAX_RECORDS; ++k) {
        if (have_core[core_defaults[k].id]) continue;
        recs[num_records].id = core_defaults[k].id;
        recs[num_records].utf8 = core_defaults[k].def;
        recs[num_records].len = strlen(core_defaults[k].def);
        num_records++;
    }

    for (uint16_t a = 0; a < axis_count && num_records < SUF_NAMES_MAX_RECORDS; ++a) {
        recs[num_records].id = (uint16_t)(256 + a);
        recs[num_records].utf8 = axes[a].name;
        recs[num_records].len = 0;
        while (recs[num_records].len < sizeof(axes[a].name) &&
               axes[a].name[recs[num_records].len] != '\0') {
            recs[num_records].len++;
        }
        num_records++;
    }

    /* Measure UTF-16BE sizes. */
    uint16_t str_lens[SUF_NAMES_MAX_RECORDS];
    size_t total_str_bytes = 0;
    for (uint16_t i = 0; i < num_records; ++i) {
        size_t units = 0;
        size_t pos = 0;
        while (pos < recs[i].len) {
            uint32_t cp = utf8_next(recs[i].utf8, recs[i].len, &pos);
            units += (cp >= 0x10000UL) ? 2 : 1;
        }
        str_lens[i] = (uint16_t)(units * 2);
        total_str_bytes += str_lens[i];
    }

    uint16_t header_sz = (uint16_t)(6 + ((size_t)num_records * 12));
    size_t table_sz = header_sz + total_str_bytes;

    uint8_t *tbl = (uint8_t *)calloc(1, table_sz);
    if (!tbl) return NULL;

    write_be16(tbl + 0, 0);             /* format = 0 */
    write_be16(tbl + 2, num_records);   /* count */
    write_be16(tbl + 4, header_sz);     /* stringOffset */

    uint16_t cur_str_off = 0;
    for (uint16_t i = 0; i < num_records; ++i) {
        size_t rec_off = 6 + ((size_t)i * 12);
        write_be16(tbl + rec_off + 0, 3);               /* platformID = 3 (Windows) */
        write_be16(tbl + rec_off + 2, 1);               /* encodingID = 1 (Unicode BMP) */
        write_be16(tbl + rec_off + 4, 0x0409);          /* languageID = 0x0409 (English US) */
        write_be16(tbl + rec_off + 6, recs[i].id);      /* nameID */
        write_be16(tbl + rec_off + 8, str_lens[i]);     /* length */
        write_be16(tbl + rec_off + 10, cur_str_off);    /* offset */

        uint8_t *str_dest = tbl + header_sz + cur_str_off;
        size_t used = 0;
        size_t pos = 0;
        while (pos < recs[i].len) {
            uint32_t cp = utf8_next(recs[i].utf8, recs[i].len, &pos);
            size_t need = (cp >= 0x10000UL) ? 4 : 2;
            if (used + need > (size_t)str_lens[i]) break;
            used += utf16be_put(str_dest + used, cp);
        }
        cur_str_off += str_lens[i];
    }

    *out_size = table_sz;
    return tbl;
}

/* Convenience accessor for text-format exporters: fetch a preserved name as
 * a NUL-terminated string, falling back to a default when absent. */
static const char *suf_display_name(const uint8_t *suf_data, size_t suf_size,
                                    uint16_t name_id, char *buf, size_t bufsz,
                                    const char *fallback) {
    if (bufsz == 0) return fallback;
    buf[0] = '\0';
    const char *p = NULL;
    size_t len = 0;
    if (suf_data && suf_get_name(suf_data, suf_size, name_id, &p, &len) == SUF_OK &&
        p && len > 0 && len < bufsz) {
        memcpy(buf, p, len);
        buf[len] = '\0';
        return buf;
    }
    return fallback;
}

/* ========================================================================= */
/* FVAR Table Builder (OpenType Font Variations, axis metadata)              */
/* ========================================================================= */

static uint32_t float_to_fixed_1616(float v) {
    return (uint32_t)(int32_t)(v * 65536.0f + ((v >= 0.0f) ? 0.5f : -0.5f));
}

static uint8_t *build_fvar_table(const suf_var_axis_t *axes, uint16_t axis_count, size_t *out_size) {
    if (!axes || axis_count == 0 || !out_size) return NULL;

    size_t table_sz = 16 + ((size_t)axis_count * 20);
    uint8_t *tbl = (uint8_t *)calloc(1, table_sz);
    if (!tbl) return NULL;

    write_be16(tbl + 0, 1);                 /* majorVersion = 1 */
    write_be16(tbl + 2, 0);                 /* minorVersion = 0 */
    write_be16(tbl + 4, 16);                /* axesArrayOffset */
    write_be16(tbl + 6, 2);                 /* reserved */
    write_be16(tbl + 8, axis_count);        /* axisCount */
    write_be16(tbl + 10, 20);               /* axisSize */
    write_be16(tbl + 12, 0);                /* instanceCount = 0 */
    write_be16(tbl + 14, 4);                /* instanceSize (minimum legal value) */

    for (uint16_t i = 0; i < axis_count; ++i) {
        uint8_t *rec = tbl + 16 + ((size_t)i * 20);
        write_be32(rec + 0, axes[i].tag);
        write_be32(rec + 4, float_to_fixed_1616(axes[i].min_val));
        write_be32(rec + 8, float_to_fixed_1616(axes[i].def_val));
        write_be32(rec + 12, float_to_fixed_1616(axes[i].max_val));
        write_be16(rec + 16, (uint16_t)((axes[i].flags & 0x1U) ? 0x0001 : 0x0000)); /* HIDDEN */
        write_be16(rec + 18, (uint16_t)(256 + i)); /* axisNameID -> name table */
    }

    *out_size = table_sz;
    return tbl;
}

/* ========================================================================= */
/* Dynamic CMAP Table Builder (Format 4 for BMP + Format 12 for 32-bit UCS-4)*/
/* ========================================================================= */

static uint8_t *build_cmap_table(const uint8_t *suf_data, size_t suf_size,
                                 uint32_t num_glyphs, const uint32_t *gid_remap,
                                 size_t *out_size) {
    suf_header_t hdr;
    if (suf_validate_header(suf_data, suf_size, &hdr) != SUF_OK) return NULL;

    /* Collect sorted list of (codepoint, glyph_id) from SUF cmap.
     * Unicode-only output: codepoints beyond U+10FFFF cannot live in an
     * OpenType cmap and are dropped here (glyph slots were already stripped
     * by the export compaction pass). */
    typedef struct {
        uint32_t cp;
        uint16_t gid;
    } cp_gid_t;

    cp_gid_t *pairs = (cp_gid_t *)calloc(num_glyphs + 256, sizeof(cp_gid_t));
    if (!pairs) return NULL;
    size_t pair_count = 0;

    if (hdr.cmap_size > 0 && hdr.cmap_offset > 0) {
        if (hdr.flags & SUF_FLAG_EXTSUCS) {
            size_t entry_count = hdr.cmap_size / sizeof(suf_cmap_ext_entry_t);
            const suf_cmap_ext_entry_t *ext_entries =
                (const suf_cmap_ext_entry_t *)(suf_data + hdr.cmap_offset);
            for (size_t i = 0; i < entry_count; i++) {
                if (ext_entries[i].codepoint <= 0x10FFFFUL && ext_entries[i].glyph_id < num_glyphs) {
                    pairs[pair_count].cp = (uint32_t)ext_entries[i].codepoint;
                    pairs[pair_count].gid = (uint16_t)(gid_remap ? gid_remap[ext_entries[i].glyph_id] : ext_entries[i].glyph_id);
                    pair_count++;
                }
            }
        } else {
            size_t entry_count = hdr.cmap_size / sizeof(suf_cmap_entry_t);
            const suf_cmap_entry_t *base_entries =
                (const suf_cmap_entry_t *)(suf_data + hdr.cmap_offset);
            for (size_t i = 0; i < entry_count; i++) {
                if (base_entries[i].codepoint <= 0x10FFFFUL && base_entries[i].glyph_id < num_glyphs) {
                    pairs[pair_count].cp = base_entries[i].codepoint;
                    pairs[pair_count].gid = (uint16_t)(gid_remap ? gid_remap[base_entries[i].glyph_id] : base_entries[i].glyph_id);
                    pair_count++;
                }
            }
        }
    }

    /* If no cmap was present, synthesize simple identity mapping */
    if (pair_count == 0) {
        for (uint32_t i = 1; i < num_glyphs && i < 256; i++) {
            pairs[pair_count].cp = (i < 128) ? (0x20 + i) : (0xE000 + i);
            pairs[pair_count].gid = (uint16_t)i;
            pair_count++;
        }
    }

    /* Split BMP vs astral-plane mappings. Format 4 covers only the BMP;
     * supplementary-plane glyphs need a format 12 subtable or they would
     * silently vanish from exported fonts. */
    size_t bmp_count = 0;
    while (bmp_count < pair_count && pairs[bmp_count].cp <= 0xFFFE) bmp_count++;
    bool have_astral = bmp_count < pair_count;

    /* --- Build Format 4 subtable over the BMP range --- */
    typedef struct {
        uint16_t start;
        uint16_t end;
        int16_t  delta;
    } f4_seg_t;

    f4_seg_t *segs = (f4_seg_t *)calloc(bmp_count + 8, sizeof(f4_seg_t));
    if (!segs) {
        free(pairs);
        return NULL;
    }
    size_t seg_count = 0;

    size_t idx = 0;
    while (idx < bmp_count) {
        uint16_t s = (uint16_t)pairs[idx].cp;
        uint16_t e = s;
        int16_t d = (int16_t)(pairs[idx].gid - s);
        idx++;

        while (idx < bmp_count &&
               pairs[idx].cp == e + 1 && (int16_t)(pairs[idx].gid - pairs[idx].cp) == d) {
            e = (uint16_t)pairs[idx].cp;
            idx++;
        }

        segs[seg_count].start = s;
        segs[seg_count].end = e;
        segs[seg_count].delta = d;
        seg_count++;
    }

    /* Final terminating segment 0xFFFF */
    segs[seg_count].start = 0xFFFF;
    segs[seg_count].end   = 0xFFFF;
    segs[seg_count].delta = 1;
    seg_count++;

    uint16_t seg_count_x2 = (uint16_t)(seg_count * 2);
    uint16_t search_range = 1;
    uint16_t entry_sel = 0;
    while ((search_range * 2) <= seg_count) {
        search_range *= 2;
        entry_sel++;
    }
    search_range *= 2;
    uint16_t range_shift = seg_count_x2 - search_range;

    size_t f4_sub_sz = 16 + (seg_count * 8); /* 14 header + 2 pad + 4 parallel arrays */

    /* --- Build Format 12 groups over the astral range --- */
    typedef struct {
        uint32_t start;
        uint32_t end;
        uint32_t start_gid;
    } f12_group_t;

    f12_group_t *groups = NULL;
    size_t group_count = 0;
    if (have_astral) {
        groups = (f12_group_t *)calloc((pair_count - bmp_count) + 8, sizeof(f12_group_t));
        if (!groups) {
            free(pairs);
            free(segs);
            return NULL;
        }
        size_t idx12 = bmp_count;
        while (idx12 < pair_count) {
            uint32_t s = pairs[idx12].cp;
            uint32_t g0 = pairs[idx12].gid;
            idx12++;
            while (idx12 < pair_count &&
                   pairs[idx12].cp == pairs[idx12 - 1].cp + 1 &&
                   pairs[idx12].gid == pairs[idx12 - 1].gid + 1) {
                idx12++;
            }
            groups[group_count].start = s;
            groups[group_count].end = pairs[idx12 - 1].cp;
            groups[group_count].start_gid = g0;
            group_count++;
        }
    }

    size_t index_num_tables = have_astral ? 2 : 1;
    size_t total_cmap_sz = 12 + (index_num_tables * 8) + f4_sub_sz;
    size_t f12_off = total_cmap_sz;
    if (have_astral) {
        total_cmap_sz += 16 + (group_count * 12);
    }

    uint8_t *cmap_tbl = (uint8_t *)calloc(1, total_cmap_sz);
    if (!cmap_tbl) {
        free(pairs);
        free(segs);
        free(groups);
        return NULL;
    }

    /* CMAP Index Header */
    size_t f4_off = 12 + (index_num_tables * 8); /* subtable sits after records */
    write_be16(cmap_tbl + 0, 0);                       /* table version = 0 */
    write_be16(cmap_tbl + 2, (uint16_t)index_num_tables);
    write_be16(cmap_tbl + 4, 3);     /* platformID = 3 (Windows) */
    write_be16(cmap_tbl + 6, 1);     /* encodingID = 1 (Unicode BMP) */
    write_be32(cmap_tbl + 8, (uint32_t)f4_off);
    if (have_astral) {
        write_be16(cmap_tbl + 12, 3);  /* platformID = 3 */
        write_be16(cmap_tbl + 14, 10); /* encodingID = 10 (UCS-4) */
        write_be32(cmap_tbl + 16, (uint32_t)f12_off);
    }

    /* Format 4 Subtable Header */
    uint8_t *f4 = cmap_tbl + f4_off;
    write_be16(f4 + 0, 4);                          /* format = 4 */
    write_be16(f4 + 2, (uint16_t)f4_sub_sz);        /* length */
    write_be16(f4 + 4, 0);                          /* language = 0 */
    write_be16(f4 + 6, seg_count_x2);               /* segCountX2 */
    write_be16(f4 + 8, search_range);               /* searchRange */
    write_be16(f4 + 10, entry_sel);                 /* entrySelector */
    write_be16(f4 + 12, range_shift);               /* rangeShift */

    size_t end_code_off   = 14;
    size_t start_code_off = end_code_off + seg_count_x2 + 2; /* +2 reserved pad */
    size_t id_delta_off   = start_code_off + seg_count_x2;
    size_t id_range_off   = id_delta_off + seg_count_x2;

    for (size_t s = 0; s < seg_count; s++) {
        write_be16(f4 + end_code_off   + (s * 2), segs[s].end);
        write_be16(f4 + start_code_off + (s * 2), segs[s].start);
        write_be16(f4 + id_delta_off   + (s * 2), (uint16_t)segs[s].delta);
        write_be16(f4 + id_range_off   + (s * 2), 0);
    }

    /* Format 12 Subtable (segmented coverage for supplementary planes) */
    if (have_astral) {
        uint32_t f12_len = (uint32_t)(16 + (group_count * 12));
        uint8_t *f12 = cmap_tbl + f12_off;
        write_be16(f12 + 0, 12);                        /* format = 12 */
        write_be16(f12 + 2, 0);                         /* reserved */
        write_be32(f12 + 4, f12_len);                   /* length */
        write_be32(f12 + 8, 0);                         /* language = 0 */
        write_be32(f12 + 12, (uint32_t)group_count);    /* nGroups */
        for (size_t g = 0; g < group_count; ++g) {
            uint8_t *grp = f12 + 16 + (g * 12);
            write_be32(grp + 0, groups[g].start);
            write_be32(grp + 4, groups[g].end);
            write_be32(grp + 8, groups[g].start_gid);
        }
    }

    free(pairs);
    free(segs);
    free(groups);

    *out_size = total_cmap_sz;
    return cmap_tbl;
}

/* ========================================================================= */
/* Outbound: .suf -> TTF / OTF Exporter                                      */
/* ========================================================================= */

suf_status_t suf_conv_suf_to_ttf(const uint8_t *suf_data, size_t suf_size, uint8_t **out_ttf, size_t *out_ttf_size) {
    if (!suf_data || !out_ttf || !out_ttf_size) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(suf_data, suf_size, &hdr);
    if (st != SUF_OK) return st;

    uint32_t num_glyphs = hdr.glyph_count;
    if (num_glyphs == 0) num_glyphs = 1;

    /* --- Unicode-only export compaction ---
     * SUF natively stores SuperUnicode codepoints (32-bit SUCS or 64-bit
     * ExtSUCS), but TTF/OTF/WOFF/EOT outputs are Unicode fonts. Glyph slots
     * whose codepoint lies beyond U+10FFFF are stripped entirely: cmap
     * entries dropped, glyph streams and indices compacted. gid 0 (.notdef)
     * always survives. */
    uint32_t num_out = num_glyphs;
    uint8_t *u_keep = (uint8_t *)calloc(num_glyphs, sizeof(uint8_t));
    uint32_t *gid_remap = (uint32_t *)calloc(num_glyphs, sizeof(uint32_t));
    if (u_keep && gid_remap) {
        for (uint32_t i = 0; i < num_glyphs; ++i) u_keep[i] = 1;

        if (hdr.cmap_size > 0 && hdr.cmap_offset > 0) {
            uint32_t *g2cp = (uint32_t *)calloc(num_glyphs, sizeof(uint32_t));
            if (g2cp) {
                if (hdr.flags & SUF_FLAG_EXTSUCS) {
                    size_t entry_count = hdr.cmap_size / sizeof(suf_cmap_ext_entry_t);
                    const suf_cmap_ext_entry_t *e =
                        (const suf_cmap_ext_entry_t *)(suf_data + hdr.cmap_offset);
                    for (size_t k = 0; k < entry_count; ++k) {
                        if (e[k].glyph_id < num_glyphs) g2cp[e[k].glyph_id] = (uint32_t)e[k].codepoint;
                    }
                } else {
                    size_t entry_count = hdr.cmap_size / sizeof(suf_cmap_entry_t);
                    const suf_cmap_entry_t *e =
                        (const suf_cmap_entry_t *)(suf_data + hdr.cmap_offset);
                    for (size_t k = 0; k < entry_count; ++k) {
                        if (e[k].glyph_id < num_glyphs) g2cp[e[k].glyph_id] = e[k].codepoint;
                    }
                }
                uint32_t next = 0;
                for (uint32_t i = 0; i < num_glyphs; ++i) {
                    if (i != 0 && g2cp[i] > 0x10FFFFUL) u_keep[i] = 0;
                    gid_remap[i] = u_keep[i] ? next++ : 0;
                }
                num_out = next;
                free(g2cp);
            }
        } else {
            for (uint32_t i = 0; i < num_glyphs; ++i) gid_remap[i] = i;
        }
    } else {
        free(u_keep);
        free(gid_remap);
        u_keep = NULL;
        gid_remap = NULL;
    }

    /* Variable font metadata: exported via 'fvar' so the output declares
     * itself variable instead of baking a static default instance. */
    uint32_t n_axes = 0;
    suf_var_axis_t *axes = NULL;
    uint16_t axis_count = 0;
    if ((hdr.flags & SUF_FLAG_VARIABLE) &&
        suf_get_axis_count(suf_data, suf_size, &n_axes) == SUF_OK && n_axes > 0) {
        axes = (suf_var_axis_t *)calloc(n_axes, sizeof(suf_var_axis_t));
        if (axes) {
            for (uint32_t a = 0; a < n_axes; ++a) {
                if (suf_get_axis_info(suf_data, suf_size, a, &axes[axis_count]) == SUF_OK) {
                    axis_count++;
                }
            }
            if (axis_count == 0) {
                free(axes);
                axes = NULL;
            }
        }
    }

    /* Derive usWeightClass from the default 'wght' axis when present */
    uint16_t weight_class = 400;
    for (uint16_t a = 0; a < axis_count; ++a) {
        if (axes[a].tag == 0x77676874U /* 'wght' */) {
            float w = axes[a].def_val;
            if (w < 1.0f) w = 1.0f;
            if (w > 1000.0f) w = 1000.0f;
            weight_class = (uint16_t)(w + 0.5f);
        }
    }

    /* Style bits from the preserved subfamily name (preferred first). */
    char subfamily_buf[128];
    const char *subfamily = suf_display_name(suf_data, suf_size, 17,
                                             subfamily_buf, sizeof(subfamily_buf), NULL);
    if (!subfamily || !subfamily[0]) {
        subfamily = suf_display_name(suf_data, suf_size, 2,
                                     subfamily_buf, sizeof(subfamily_buf), "Regular");
    }
    bool style_bold = false;
    bool style_italic = false;
    for (const char *q = subfamily; *q; ++q) {
        char ch = (char)((*q >= 'A' && *q <= 'Z') ? (*q + 32) : *q);
        if (ch == 'b' && (q[1] | 0x20) == 'o' && (q[2] | 0x20) == 'l' && (q[3] | 0x20) == 'd') style_bold = true;
        if (ch == 'i' && (q[1] | 0x20) == 't' && (q[2] | 0x20) == 'a' && (q[3] | 0x20) == 'l' &&
            (q[4] | 0x20) == 'i' && (q[5] | 0x20) == 'c') style_italic = true;
    }
    uint16_t mac_style = (uint16_t)((style_bold ? 0x0001 : 0) | (style_italic ? 0x0002 : 0));
    uint16_t fs_selection = 0x0040; /* REGULAR */
    if (style_bold) fs_selection |= 0x0020;
    if (style_italic) { fs_selection |= 0x0001; fs_selection &= (uint16_t)~0x0040; }

    sfnt_builder_t sfnt;
    memset(&sfnt, 0, sizeof(sfnt));

    /* 1. 'head' table (54 bytes) */
    uint8_t *head_tbl = (uint8_t *)calloc(1, 54);
    write_be32(head_tbl + 0, 0x00010000);           /* version 1.0 */
    write_be32(head_tbl + 4, 0x00010000);           /* fontRevision 1.0 */
    write_be32(head_tbl + 8, 0x00000000);           /* checkSumAdjustment (computed below) */
    write_be32(head_tbl + 12, 0x5F0F3CF5);          /* magicNumber */
    write_be16(head_tbl + 16, 0x0001);              /* flags */
    write_be16(head_tbl + 18, hdr.units_per_em ? hdr.units_per_em : 1000); /* unitsPerEm */
    /* created / modified: fixed timestamp (seconds since 1904) */
    write_be32(head_tbl + 20, 0);
    write_be32(head_tbl + 24, 0x50000000);
    write_be32(head_tbl + 28, 0);
    write_be32(head_tbl + 32, 0x50000000);
    write_be16(head_tbl + 36, (uint16_t)hdr.bbox_min_x);
    write_be16(head_tbl + 38, (uint16_t)hdr.bbox_min_y);
    write_be16(head_tbl + 40, (uint16_t)hdr.bbox_max_x);
    write_be16(head_tbl + 42, (uint16_t)hdr.bbox_max_y);
    write_be16(head_tbl + 44, mac_style);           /* macStyle from subfamily */
    write_be16(head_tbl + 46, 6);                   /* lowestRecPPEM */
    write_be16(head_tbl + 48, 2);                   /* fontDirectionHint */
    write_be16(head_tbl + 50, 1);                   /* indexToLocFormat: 1 = long (32-bit offsets) */
    write_be16(head_tbl + 52, 0);                   /* glyphDataFormat */
    sfnt_add_table(&sfnt, 0x68656164, head_tbl, 54);

    /* 2. 'hhea' table (36 bytes) */
    uint8_t *hhea_tbl = (uint8_t *)calloc(1, 36);
    write_be32(hhea_tbl + 0, 0x00010000);           /* version 1.0 */
    write_be16(hhea_tbl + 4, (uint16_t)hdr.ascender);
    write_be16(hhea_tbl + 6, (uint16_t)hdr.descender);
    write_be16(hhea_tbl + 8, (uint16_t)hdr.line_gap);
    write_be16(hhea_tbl + 10, (uint16_t)(hdr.bbox_max_x > hdr.bbox_min_x ? (hdr.bbox_max_x - hdr.bbox_min_x) : 1000));
    write_be16(hhea_tbl + 12, (uint16_t)hdr.bbox_min_x); /* minLeftSideBearing */
    write_be16(hhea_tbl + 14, 0);                   /* minRightSideBearing */
    write_be16(hhea_tbl + 16, (uint16_t)hdr.bbox_max_x); /* xMaxExtent */
    write_be16(hhea_tbl + 18, 1);                   /* caretSlopeRise */
    write_be16(hhea_tbl + 20, 0);                   /* caretSlopeRun */
    write_be16(hhea_tbl + 22, 0);                   /* caretOffset */
    write_be16(hhea_tbl + 24, 0);
    write_be16(hhea_tbl + 26, 0);
    write_be16(hhea_tbl + 28, 0);
    write_be16(hhea_tbl + 30, 0);
    write_be16(hhea_tbl + 32, 0);                   /* metricDataFormat = 0 */
    write_be16(hhea_tbl + 34, (uint16_t)num_out);   /* numberOfHMetrics */
    sfnt_add_table(&sfnt, 0x68686561, hhea_tbl, 36);

    /* 3. 'maxp' table (32 bytes) */
    uint8_t *maxp_tbl = (uint8_t *)calloc(1, 32);
    write_be32(maxp_tbl + 0, 0x00010000);           /* version 1.0 */
    write_be16(maxp_tbl + 4, (uint16_t)num_out);    /* numGlyphs */
    write_be16(maxp_tbl + 6, 64);                   /* maxPoints */
    write_be16(maxp_tbl + 8, 4);                    /* maxContours */
    write_be16(maxp_tbl + 10, 0);                   /* maxCompositePoints */
    write_be16(maxp_tbl + 12, 0);                   /* maxCompositeContours */
    write_be16(maxp_tbl + 14, 1);                   /* maxZones */
    write_be16(maxp_tbl + 16, 0);                   /* maxTwilightPoints */
    write_be16(maxp_tbl + 18, 0);                   /* maxStorage */
    write_be16(maxp_tbl + 20, 0);                   /* maxFunctionDefs */
    write_be16(maxp_tbl + 22, 0);                   /* maxInstructionDefs */
    write_be16(maxp_tbl + 24, 0);                   /* maxStackElements */
    write_be16(maxp_tbl + 26, 0);                   /* maxSizeOfInstructions */
    write_be16(maxp_tbl + 28, 0);                   /* maxComponentElements */
    write_be16(maxp_tbl + 30, 0);                   /* maxComponentDepth */
    sfnt_add_table(&sfnt, 0x6D617870, maxp_tbl, 32);

    /* 4. 'OS/2' table (96 bytes, Version 4) */
    uint8_t *os2_tbl = (uint8_t *)calloc(1, 96);
    write_be16(os2_tbl + 0, 4);                     /* version 4 */
    write_be16(os2_tbl + 2, 500);                   /* xAvgCharWidth */
    write_be16(os2_tbl + 4, weight_class);          /* usWeightClass (default wght axis or 400) */
    write_be16(os2_tbl + 6, 5);                     /* usWidthClass = 5 (Medium) */
    write_be16(os2_tbl + 8, 0);                     /* fsType = 0 (Installable) */
    write_be16(os2_tbl + 10, 650);                  /* ySubscriptXSize */
    write_be16(os2_tbl + 12, 600);                  /* ySubscriptYSize */
    write_be16(os2_tbl + 14, 0);                    /* ySubscriptXOffset */
    write_be16(os2_tbl + 16, 75);                   /* ySubscriptYOffset */
    write_be16(os2_tbl + 18, 650);                  /* ySuperscriptXSize */
    write_be16(os2_tbl + 20, 600);                  /* ySuperscriptYSize */
    write_be16(os2_tbl + 22, 0);                    /* ySuperscriptXOffset */
    write_be16(os2_tbl + 24, 350);                  /* ySuperscriptYOffset */
    write_be16(os2_tbl + 26, 50);                   /* yStrikeoutSize */
    write_be16(os2_tbl + 28, 300);                  /* yStrikeoutPosition */
    write_be16(os2_tbl + 30, 0);                    /* sFamilyClass */
    /* panose[10] */
    os2_tbl[32] = 2; os2_tbl[33] = 0; os2_tbl[34] = 5; os2_tbl[35] = 3;
    write_be32(os2_tbl + 42, 0x80000001);          /* ulUnicodeRange1 */
    write_be32(os2_tbl + 46, 0x10000000);          /* ulUnicodeRange2 */
    /* achVendID */
    os2_tbl[58] = 'S'; os2_tbl[59] = 'U'; os2_tbl[60] = 'C'; os2_tbl[61] = 'S';
    write_be16(os2_tbl + 62, fs_selection);         /* fsSelection from subfamily */
    write_be16(os2_tbl + 64, 0x0020);               /* usFirstCharIndex */
    write_be16(os2_tbl + 66, 0xFFFF);               /* usLastCharIndex */
    write_be16(os2_tbl + 68, (uint16_t)hdr.ascender);   /* sTypoAscender */
    write_be16(os2_tbl + 70, (uint16_t)hdr.descender);  /* sTypoDescender */
    write_be16(os2_tbl + 72, (uint16_t)hdr.line_gap);   /* sTypoLineGap */
    write_be16(os2_tbl + 74, (uint16_t)(hdr.ascender > 0 ? hdr.ascender : 1000));  /* usWinAscent */
    write_be16(os2_tbl + 76, (uint16_t)(hdr.descender < 0 ? -hdr.descender : 200)); /* usWinDescent */
    write_be32(os2_tbl + 78, 0x00000001);          /* ulCodePageRange1 (Latin 1) */
    write_be16(os2_tbl + 86, (uint16_t)(hdr.ascender / 2));     /* sxHeight */
    write_be16(os2_tbl + 88, (uint16_t)(hdr.ascender * 3 / 4)); /* sCapHeight */
    write_be16(os2_tbl + 90, 0);                    /* usDefaultChar */
    write_be16(os2_tbl + 92, 0x0020);               /* usBreakChar */
    write_be16(os2_tbl + 94, 1);                    /* usMaxContext */
    sfnt_add_table(&sfnt, 0x4F532F32, os2_tbl, 96);

    /* 5. 'name' table (preserved records + legacy defaults + axis names) */
    size_t name_sz = 0;
    uint8_t *name_tbl = build_name_table_ex(&name_sz, axes, axis_count, suf_data, suf_size);
    if (name_tbl) {
        sfnt_add_table(&sfnt, 0x6E616D65, name_tbl, (uint32_t)name_sz);
    }

    /* 5b. 'fvar' table — declare the font as variable */
    if (axes && axis_count > 0) {
        size_t fvar_sz = 0;
        uint8_t *fvar_tbl = build_fvar_table(axes, axis_count, &fvar_sz);
        if (fvar_tbl) {
            sfnt_add_table(&sfnt, 0x66766172 /* 'fvar' */, fvar_tbl, (uint32_t)fvar_sz);
        }
    }

    /* 5c. 'gvar' table — verbatim per-glyph GlyphVariationData blocks that were
     * remapped onto the SUF point lists at import time. */
    if (axes && axis_count > 0 && (hdr.flags & SUF_FLAG_GLYPH_VARIATIONS)) {
        const uint8_t **blk_ptrs = (const uint8_t **)calloc(num_out, sizeof(const uint8_t *));
        uint32_t *blk_lens = (uint32_t *)calloc(num_out, sizeof(uint32_t));
        if (blk_ptrs && blk_lens) {
            uint64_t total_bytes = 0;
            bool any_block = false;
            for (uint32_t i = 0; i < num_glyphs; ++i) {
                if (gid_remap && !u_keep[i]) continue;
                const uint8_t *bp = NULL;
                size_t bl = 0;
                if (suf_get_glyph_variation(suf_data, suf_size, i, &bp, &bl) == SUF_OK && bl > 0) {
                    uint32_t dst_i = gid_remap ? gid_remap[i] : i;
                    blk_ptrs[dst_i] = bp;
                    blk_lens[dst_i] = (uint32_t)bl;
                    total_bytes += bl;
                    any_block = true;
                }
            }

            if (any_block) {
                /* Prefer 16-bit offsets (stored / 2) when every cumulative
                 * offset stays even and under 128 KiB; else 32-bit. */
                bool long_offsets = total_bytes > 65535ULL || total_bytes > 0xFFFEULL;
                if (!long_offsets) {
                    uint32_t acc_chk = 0;
                    for (uint32_t i = 0; i < num_out; ++i) {
                        acc_chk += blk_lens[i];
                        if (acc_chk & 1u) { long_offsets = true; break; }
                    }
                }

                size_t offs_size = ((size_t)num_out + 1) * (long_offsets ? 4 : 2);
                size_t tbl_size = 20 + offs_size + (size_t)total_bytes;
                uint8_t *gv_tbl = (uint8_t *)calloc(1, tbl_size);
                if (gv_tbl) {
                    write_be16(gv_tbl + 0, 1);              /* majorVersion */
                    write_be16(gv_tbl + 2, 0);              /* minorVersion */
                    write_be16(gv_tbl + 4, axis_count);
                    write_be16(gv_tbl + 6, 0);              /* sharedTupleCount */
                    write_be32(gv_tbl + 8, 20);             /* sharedTuplesOffset (unused) */
                    write_be16(gv_tbl + 12, (uint16_t)num_out);
                    write_be16(gv_tbl + 14, long_offsets ? 0x0001 : 0x0000);
                    write_be32(gv_tbl + 16, (uint32_t)(20 + offs_size));

                    uint32_t acc = 0;
                    uint8_t *dst = gv_tbl + 20 + offs_size;
                    for (uint32_t i = 0; i <= num_out; ++i) {
                        if (long_offsets) {
                            write_be32(gv_tbl + 20 + (size_t)i * 4, acc);
                        } else {
                            write_be16(gv_tbl + 20 + (size_t)i * 2, (uint16_t)(acc / 2));
                        }
                        if (i < num_out && blk_lens[i] > 0) {
                            memcpy(dst + acc, blk_ptrs[i], blk_lens[i]);
                            acc += blk_lens[i];
                        }
                    }
                    sfnt_add_table(&sfnt, 0x67766172 /* 'gvar' */, gv_tbl, (uint32_t)tbl_size);
                }
            }
        }
        free(blk_ptrs);
        free(blk_lens);
    }

    /* 6. 'post' table (32 bytes, Version 3.0) */
    uint8_t *post_tbl = (uint8_t *)calloc(1, 32);
    write_be32(post_tbl + 0, 0x00030000);           /* version 3.0 */
    write_be32(post_tbl + 4, 0);                    /* italicAngle = 0 */
    write_be16(post_tbl + 8, (uint16_t)(-100));     /* underlinePosition */
    write_be16(post_tbl + 10, 50);                  /* underlineThickness */
    write_be32(post_tbl + 12, 0);                   /* isFixedPitch = 0 */
    sfnt_add_table(&sfnt, 0x706F7374, post_tbl, 32);

    /* 7. 'hmtx' table */
    uint8_t *hmtx_tbl = (uint8_t *)calloc(1, num_out * 4);
    for (uint32_t i = 0; i < num_glyphs; ++i) {
        if (gid_remap && !u_keep[i]) continue;
        uint32_t dst_i = gid_remap ? gid_remap[i] : i;
        suf_metric_t m;
        if (suf_get_glyph_metric(suf_data, suf_size, i, &m) == SUF_OK) {
            write_be16(hmtx_tbl + (dst_i * 4), (uint16_t)(m.advance_width ? m.advance_width : 600));
            write_be16(hmtx_tbl + (dst_i * 4) + 2, (uint16_t)m.left_side_bearing);
        } else {
            write_be16(hmtx_tbl + (dst_i * 4), 600);
            write_be16(hmtx_tbl + (dst_i * 4) + 2, 0);
        }
    }
    sfnt_add_table(&sfnt, 0x686D7478, hmtx_tbl, num_out * 4);

    /* 8. 'cmap' table */
    size_t cmap_sz = 0;
    uint8_t *cmap_tbl = build_cmap_table(suf_data, suf_size, num_glyphs, gid_remap, &cmap_sz);
    if (cmap_tbl) {
        sfnt_add_table(&sfnt, 0x636D6170, cmap_tbl, (uint32_t)cmap_sz);
    }

    /* 9. 'glyf' and 'loca' tables (valid TrueType simple glyph contours) */
    uint8_t *loca_tbl = (uint8_t *)calloc(1, (num_out + 1) * 4);
    size_t glyf_capacity = num_out * 64 + 4096;
    uint8_t *glyf_tbl = (uint8_t *)calloc(1, glyf_capacity);
    size_t glyf_pos = 0;

    for (uint32_t i = 0; i < num_glyphs; ++i) {
        if (gid_remap && !u_keep[i]) continue;
        uint32_t dst_i = gid_remap ? gid_remap[i] : i;
        write_be32(loca_tbl + (dst_i * 4), (uint32_t)glyf_pos);

        suf_metric_t m;
        suf_get_glyph_metric(suf_data, suf_size, i, &m);

        const uint8_t *cmds = NULL;
        size_t cmd_len = 0;
        suf_get_glyph_outline(suf_data, suf_size, i, &cmds, &cmd_len);

        if (cmds && cmd_len > 0) {
            if (glyf_pos + 4096 > glyf_capacity) {
                glyf_capacity = glyf_capacity * 2 + 4096;
                glyf_tbl = (uint8_t *)realloc(glyf_tbl, glyf_capacity);
            }

            size_t written_glyf = encode_suf_commands_to_ttf_glyf(cmds, cmd_len, &m, glyf_tbl + glyf_pos, glyf_capacity - glyf_pos);
            if (written_glyf > 0) {
                glyf_pos += written_glyf;
                glyf_pos = (glyf_pos + 3) & ~3U;    /* 4-byte align */
            }
        }
    }
    write_be32(loca_tbl + (num_out * 4), (uint32_t)glyf_pos);

    sfnt_add_table(&sfnt, 0x6C6F6361, loca_tbl, (num_out + 1) * 4);
    sfnt_add_table(&sfnt, 0x676C7966, glyf_tbl, (uint32_t)glyf_pos);

    /* Assemble SFNT binary with TrueType version 0x00010000 */
    uint8_t *ttf = sfnt_assemble(&sfnt, 0x00010000, out_ttf_size);

    /* Calculate checkSumAdjustment across the assembled font */
    if (ttf && *out_ttf_size >= 12) {
        uint32_t total_csum = calc_table_checksum(ttf, *out_ttf_size);
        uint32_t adj = 0xB1B0AFBAUL - total_csum;

        /* Find 'head' table offset in SFNT directory and write adjustment at offset + 8 */
        uint16_t tbl_count = read_be16(ttf + 4);
        for (uint16_t t = 0; t < tbl_count; t++) {
            size_t dir = 12 + (t * 16);
            uint32_t tag = read_be32(ttf + dir);
            if (tag == 0x68656164) { /* 'head' */
                uint32_t head_off = read_be32(ttf + dir + 8);
                if (head_off + 12 <= *out_ttf_size) {
                    write_be32(ttf + head_off + 8, adj);
                }
                break;
            }
        }
    }

    for (size_t i = 0; i < sfnt.count; ++i) {
        free(sfnt.tables[i].data);
    }
    free(axes);
    free(u_keep);
    free(gid_remap);

    if (!ttf) return SUF_ERR_ALLOC_FAIL;
    *out_ttf = ttf;
    return SUF_OK;
}

/* ========================================================================= */
/* Inbound & Outbound: OTF CFF (.otf) <-> .suf Converter                     */
/* ========================================================================= */

suf_status_t suf_conv_otf_to_suf(const uint8_t *otf_data, size_t otf_size, suf_builder_t **out_builder) {
    return suf_conv_ttf_to_suf(otf_data, otf_size, out_builder);
}

suf_status_t suf_conv_suf_to_otf(const uint8_t *suf_data, size_t suf_size, uint8_t **out_otf, size_t *out_otf_size) {
    /* Generate OpenType TrueType/CFF sfnt container */
    return suf_conv_suf_to_ttf(suf_data, suf_size, out_otf, out_otf_size);
}

/* ========================================================================= */
/* Inbound & Outbound: WOFF 1.0 (.woff) <-> .suf Converter                   */
/* ========================================================================= */

#define WOFF_MAGIC 0x774F4646UL

suf_status_t suf_conv_woff_to_suf(const uint8_t *woff_data, size_t woff_size, suf_builder_t **out_builder) {
    if (!woff_data || !out_builder) return SUF_ERR_NULL_POINTER;
    if (woff_size < 44) return SUF_ERR_BUFFER_TOO_SMALL;

    uint32_t signature = read_be32(woff_data);
    if (signature != WOFF_MAGIC) return SUF_ERR_INVALID_MAGIC;

    uint32_t flavor = read_be32(woff_data + 4);
    uint16_t num_tables = read_be16(woff_data + 12);
    uint32_t total_sfnt_size = read_be32(woff_data + 16);

    if (44 + (size_t)num_tables * 20 > woff_size) return SUF_ERR_CORRUPT_DATA;

    uint8_t *sfnt_buf = (uint8_t *)calloc(1, total_sfnt_size > 0 ? total_sfnt_size : (woff_size * 4));
    if (!sfnt_buf) return SUF_ERR_ALLOC_FAIL;

    write_be32(sfnt_buf + 0, flavor);
    write_be16(sfnt_buf + 4, num_tables);

    uint32_t cur_sfnt_offset = 12 + (num_tables * 16);
    cur_sfnt_offset = (cur_sfnt_offset + 3) & ~3U;

    for (uint16_t i = 0; i < num_tables; ++i) {
        const uint8_t *rec = woff_data + 44 + (i * 20);
        uint32_t tag = read_be32(rec + 0);
        uint32_t offset = read_be32(rec + 4);
        uint32_t comp_len = read_be32(rec + 8);
        uint32_t orig_len = read_be32(rec + 12);
        uint32_t orig_chk = read_be32(rec + 16);

        if ((uint64_t)offset + comp_len > woff_size) continue;

        size_t sfnt_dir = 12 + (i * 16);
        write_be32(sfnt_buf + sfnt_dir + 0, tag);
        write_be32(sfnt_buf + sfnt_dir + 4, orig_chk);
        write_be32(sfnt_buf + sfnt_dir + 8, cur_sfnt_offset);
        write_be32(sfnt_buf + sfnt_dir + 12, orig_len);

        if (comp_len < orig_len) {
            uint8_t *decomp = NULL;
            if (zlib_decompress_stream(woff_data + offset, comp_len, orig_len, &decomp) == SUF_OK && decomp) {
                memcpy(sfnt_buf + cur_sfnt_offset, decomp, orig_len);
                free(decomp);
            }
        } else {
            memcpy(sfnt_buf + cur_sfnt_offset, woff_data + offset, orig_len);
        }

        cur_sfnt_offset += (orig_len + 3) & ~3U;
    }

    suf_status_t st = suf_conv_ttf_to_suf(sfnt_buf, cur_sfnt_offset, out_builder);
    free(sfnt_buf);
    return st;
}

suf_status_t suf_conv_suf_to_woff(const uint8_t *suf_data, size_t suf_size, uint8_t **out_woff, size_t *out_woff_size) {
    if (!suf_data || !out_woff || !out_woff_size) return SUF_ERR_NULL_POINTER;

    uint8_t *ttf = NULL;
    size_t ttf_size = 0;
    suf_status_t st = suf_conv_suf_to_ttf(suf_data, suf_size, &ttf, &ttf_size);
    if (st != SUF_OK) return st;

    uint32_t flavor = read_be32(ttf);
    uint16_t num_tables = read_be16(ttf + 4);

    size_t woff_alloc = 44 + (num_tables * 20) + (ttf_size * 2);
    uint8_t *woff = (uint8_t *)calloc(1, woff_alloc);
    if (!woff) {
        free(ttf);
        return SUF_ERR_ALLOC_FAIL;
    }

    uint32_t cur_woff_offset = 44 + (num_tables * 20);

    for (uint16_t i = 0; i < num_tables; ++i) {
        const uint8_t *rec = ttf + 12 + (i * 16);
        uint32_t tag = read_be32(rec + 0);
        uint32_t chk = read_be32(rec + 4);
        uint32_t offset = read_be32(rec + 8);
        uint32_t orig_len = read_be32(rec + 12);

        uint8_t *comp_data = NULL;
        size_t comp_len = 0;
        zlib_compress_stream(ttf + offset, orig_len, &comp_data, &comp_len);

        uint32_t final_len = (uint32_t)comp_len;
        const uint8_t *src_to_copy = comp_data;

        size_t dir_pos = 44 + (i * 20);
        write_be32(woff + dir_pos + 0, tag);
        write_be32(woff + dir_pos + 4, cur_woff_offset);
        write_be32(woff + dir_pos + 8, final_len);
        write_be32(woff + dir_pos + 12, orig_len);
        write_be32(woff + dir_pos + 16, chk);

        memcpy(woff + cur_woff_offset, src_to_copy, final_len);
        cur_woff_offset += (final_len + 3) & ~3U;

        if (comp_data) free(comp_data);
    }

    write_be32(woff + 0, WOFF_MAGIC);
    write_be32(woff + 4, flavor);
    write_be32(woff + 8, cur_woff_offset);
    write_be16(woff + 12, num_tables);
    write_be16(woff + 14, 0);
    write_be32(woff + 16, (uint32_t)ttf_size);
    write_be16(woff + 20, 1);
    write_be16(woff + 22, 0);
    write_be32(woff + 24, 0);
    write_be32(woff + 28, 0);
    write_be32(woff + 32, 0);
    write_be32(woff + 36, 0);
    write_be32(woff + 40, 0);

    free(ttf);
    *out_woff = woff;
    *out_woff_size = cur_woff_offset;
    return SUF_OK;
}

/* ========================================================================= */
/* Inbound & Outbound: FontForge Spline Font Database (.sfd) <-> .suf       */
/* ========================================================================= */

suf_status_t suf_conv_sfd_to_suf(const char *sfd_text, size_t sfd_len, suf_builder_t **out_builder) {
    if (!sfd_text || !out_builder) return SUF_ERR_NULL_POINTER;
    if (sfd_len == 0) return SUF_ERR_BUFFER_TOO_SMALL;

    int ascent = 800, descent = 200;
    const char *p = sfd_text;

    const char *asc_pos = strstr(p, "Ascent:");
    if (asc_pos) sscanf(asc_pos, "Ascent: %d", &ascent);
    const char *desc_pos = strstr(p, "Descent:");
    if (desc_pos) sscanf(desc_pos, "Descent: %d", &descent);

    uint16_t em = (uint16_t)(ascent + descent);
    if (em == 0) em = 1000;

    suf_builder_t *b = suf_builder_create(em, (int16_t)ascent, (int16_t)(-descent), SUF_FLAG_OS_VECTOR);
    if (!b) return SUF_ERR_ALLOC_FAIL;

    const char *cur = sfd_text;
    while ((cur = strstr(cur, "StartChar:")) != NULL) {
        cur += 10;
        char char_name[64] = {0};
        sscanf(cur, " %63s", char_name);

        int encoding = -1, width = 1000;
        const char *enc_pos = strstr(cur, "Encoding:");
        if (enc_pos) sscanf(enc_pos, "Encoding: %d", &encoding);
        const char *w_pos = strstr(cur, "Width:");
        if (w_pos) sscanf(w_pos, "Width: %d", &width);

        suf_metric_t metric;
        memset(&metric, 0, sizeof(metric));
        metric.advance_width = (int16_t)width;
        metric.x_min = 0;
        metric.y_min = (int16_t)(-descent);
        metric.x_max = (int16_t)width;
        metric.y_max = (int16_t)ascent;

        uint8_t outline_cmds[256];
        size_t cmd_len = 0;

        const char *spline_pos = strstr(cur, "SplineSet");
        const char *end_char = strstr(cur, "EndChar");

        if (spline_pos && end_char && spline_pos < end_char) {
            const char *sp = spline_pos + 9;
            while (sp < end_char) {
                while (sp < end_char && (*sp == ' ' || *sp == '\t' || *sp == '\r' || *sp == '\n')) sp++;
                if (sp >= end_char) break;

                if (*sp == 'm') {
                    int x = 0, y = 0;
                    if (sscanf(sp + 1, "%d %d", &x, &y) >= 2 && cmd_len + 5 <= sizeof(outline_cmds)) {
                        outline_cmds[cmd_len++] = SUF_CMD_MOVE_TO;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)x); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)y); cmd_len += 2;
                    }
                } else if (*sp == 'l') {
                    int x = 0, y = 0;
                    if (sscanf(sp + 1, "%d %d", &x, &y) >= 2 && cmd_len + 5 <= sizeof(outline_cmds)) {
                        outline_cmds[cmd_len++] = SUF_CMD_LINE_TO;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)x); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)y); cmd_len += 2;
                    }
                } else if (*sp == 'q') {
                    int cx = 0, cy = 0, x = 0, y = 0;
                    if (sscanf(sp + 1, "%d %d %d %d", &cx, &cy, &x, &y) >= 4 && cmd_len + 9 <= sizeof(outline_cmds)) {
                        outline_cmds[cmd_len++] = SUF_CMD_QUAD_TO;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)cx); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)cy); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)x); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)y); cmd_len += 2;
                    }
                } else if (*sp == 'c') {
                    int c1x = 0, c1y = 0, c2x = 0, c2y = 0, x = 0, y = 0;
                    if (sscanf(sp + 1, "%d %d %d %d %d %d", &c1x, &c1y, &c2x, &c2y, &x, &y) >= 6 && cmd_len + 13 <= sizeof(outline_cmds)) {
                        outline_cmds[cmd_len++] = SUF_CMD_CUBIC_TO;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)c1x); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)c1y); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)c2x); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)c2y); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)x); cmd_len += 2;
                        write_le16(outline_cmds + cmd_len, (uint16_t)(int16_t)y); cmd_len += 2;
                    }
                }
                while (sp < end_char && *sp != '\n') sp++;
            }
            if (cmd_len > 0 && cmd_len + 2 <= sizeof(outline_cmds)) {
                outline_cmds[cmd_len++] = SUF_CMD_CLOSE_PATH;
                outline_cmds[cmd_len++] = SUF_CMD_END_GLYPH;
            }
        }

        uint64_t cp = (encoding >= 0) ? (uint64_t)encoding : 0;
        suf_builder_add_glyph(b, cp, &metric, NULL, 0, outline_cmds, cmd_len);

        if (!end_char) break;
        cur = end_char + 7;
    }

    *out_builder = b;
    return SUF_OK;
}

suf_status_t suf_conv_suf_to_sfd(const uint8_t *suf_data, size_t suf_size, char **out_sfd, size_t *out_sfd_len) {
    if (!suf_data || !out_sfd || !out_sfd_len) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(suf_data, suf_size, &hdr);
    if (st != SUF_OK) return st;

    size_t alloc_sz = 1024 + (hdr.glyph_count * 512);
    char *buf = (char *)malloc(alloc_sz);
    if (!buf) return SUF_ERR_ALLOC_FAIL;

    char fam_buf[128], full_buf[160], ps_buf[128], ver_buf[64];
    const char *fam = suf_display_name(suf_data, suf_size, 1, fam_buf, sizeof(fam_buf), "SuperUnicode");
    const char *full = suf_display_name(suf_data, suf_size, 4, full_buf, sizeof(full_buf), "SuperUnicode Font");
    const char *psn = suf_display_name(suf_data, suf_size, 6, ps_buf, sizeof(ps_buf), "SuperUnicodeFont-Regular");
    /* PostScript names may not contain spaces. */
    for (char *q = ps_buf; psn == ps_buf && q && *q; ++q) {
        if (*q == ' ') *q = '-';
    }
    char ver_def[64];
    snprintf(ver_def, sizeof(ver_def), "%s", suf_display_name(suf_data, suf_size, 5, ver_buf, sizeof(ver_buf), "1.0"));
    const char *version = (ver_buf[0] != '\0') ? ver_buf : "1.0";

    int pos = snprintf(buf, alloc_sz,
        "SplineFontDB: 3.0\n"
        "FontName: %s\n"
        "FullName: %s\n"
        "FamilyName: %s\n"
        "Weight: Regular\n"
        "Version: %s\n"
        "Ascent: %d\n"
        "Descent: %d\n"
        "UnitsPerEm: %u\n"
        "BeginChars: %u %u\n\n",
        psn,
        full,
        fam,
        version,
        (int)hdr.ascender,
        (int)(-hdr.descender),
        (unsigned int)hdr.units_per_em,
        (unsigned int)hdr.glyph_count,
        (unsigned int)hdr.glyph_count
    );

    for (uint32_t i = 0; i < hdr.glyph_count; ++i) {
        suf_metric_t m;
        suf_get_glyph_metric(suf_data, suf_size, i, &m);

        pos += snprintf(buf + pos, alloc_sz - pos,
            "StartChar: glyph%04u\n"
            "Encoding: %u %u %u\n"
            "Width: %d\n"
            "VWidth: %u\n"
            "Fore\n"
            "SplineSet\n"
            "m %d %d\n"
            "l %d %d\n"
            "l %d %d\n"
            "l %d %d\n"
            "EndSplineSet\n"
            "EndChar\n\n",
            i, i, i, i,
            (int)m.advance_width,
            (unsigned int)hdr.units_per_em,
            (int)m.x_min, (int)m.y_min,
            (int)m.x_max, (int)m.y_min,
            (int)m.x_max, (int)m.y_max,
            (int)m.x_min, (int)m.y_max
        );
    }

    pos += snprintf(buf + pos, alloc_sz - pos, "EndSplineFontDB\n");

    *out_sfd = buf;
    *out_sfd_len = (size_t)pos;
    return SUF_OK;
}

/* ========================================================================= */
/* Inbound & Outbound: Embedded OpenType (.eot) <-> .suf Converter           */
/* ========================================================================= */

#define EOT_MAGIC 0x504CUL

suf_status_t suf_conv_eot_to_suf(const uint8_t *eot_data, size_t eot_size, suf_builder_t **out_builder) {
    if (!eot_data || !out_builder) return SUF_ERR_NULL_POINTER;
    if (eot_size < 82) return SUF_ERR_BUFFER_TOO_SMALL;

    uint16_t magic = read_le16(eot_data + 34);
    if (magic != 0x504C && magic != 0x4C50) return SUF_ERR_INVALID_MAGIC;

    size_t font_offset = 82;
    for (size_t i = 82; i + 4 <= eot_size; ++i) {
        uint32_t tag = read_be32(eot_data + i);
        if (tag == 0x00010000 || tag == 0x74727565 || tag == 0x4F54544F) {
            font_offset = i;
            break;
        }
    }

    const uint8_t *font_ptr = eot_data + font_offset;
    size_t remaining_size = eot_size - font_offset;

    return suf_conv_ttf_to_suf(font_ptr, remaining_size, out_builder);
}

suf_status_t suf_conv_suf_to_eot(const uint8_t *suf_data, size_t suf_size, uint8_t **out_eot, size_t *out_eot_size) {
    if (!suf_data || !out_eot || !out_eot_size) return SUF_ERR_NULL_POINTER;

    uint8_t *ttf = NULL;
    size_t ttf_size = 0;
    suf_status_t st = suf_conv_suf_to_ttf(suf_data, suf_size, &ttf, &ttf_size);
    if (st != SUF_OK) return st;

    size_t header_size = 128;
    size_t total_size = header_size + ttf_size;
    uint8_t *eot = (uint8_t *)calloc(1, total_size);
    if (!eot) {
        free(ttf);
        return SUF_ERR_ALLOC_FAIL;
    }

    write_le32(eot + 0, (uint32_t)total_size);
    write_le32(eot + 4, (uint32_t)ttf_size);
    write_le32(eot + 8, 0x00020001);
    write_le32(eot + 12, 0);
    write_le16(eot + 34, 0x504C);

    memcpy(eot + header_size, ttf, ttf_size);

    free(ttf);
    *out_eot = eot;
    *out_eot_size = total_size;
    return SUF_OK;
}

/* ========================================================================= */
/* Inbound & Outbound: PostScript Type 1 / PFA / PFB (.ps, .pfa, .pfb)       */
/* ========================================================================= */

suf_status_t suf_conv_ps_to_suf(const char *ps_text, size_t ps_len, suf_builder_t **out_builder) {
    if (!ps_text || !out_builder) return SUF_ERR_NULL_POINTER;
    if (ps_len == 0) return SUF_ERR_BUFFER_TOO_SMALL;

    /* Parse FontBBox and Matrix */
    int min_x = 0, min_y = -200, max_x = 1000, max_y = 800;
    const char *bbox_pos = strstr(ps_text, "/FontBBox");
    if (bbox_pos) {
        const char *b = strchr(bbox_pos, '[');
        if (b) {
            sscanf(b + 1, "%d %d %d %d", &min_x, &min_y, &max_x, &max_y);
        }
    }

    int ascent = (max_y > 0) ? max_y : 800;
    int descent = (min_y < 0) ? min_y : -200;
    uint16_t em = (uint16_t)(ascent - descent);
    if (em == 0) em = 1000;

    suf_builder_t *b = suf_builder_create(em, (int16_t)ascent, (int16_t)descent, SUF_FLAG_OS_VECTOR);
    if (!b) return SUF_ERR_ALLOC_FAIL;
    suf_builder_set_bbox(b, (int16_t)min_x, (int16_t)min_y, (int16_t)max_x, (int16_t)max_y);

    /* Parse /CharStrings */
    const char *cs_pos = strstr(ps_text, "/CharStrings");
    if (cs_pos) {
        const char *cur = cs_pos;
        while ((cur = strstr(cur, "/")) != NULL) {
            if (cur == cs_pos) { cur++; continue; }
            if (strncmp(cur, "/CharStrings", 12) == 0) { cur += 12; continue; }
            if (strstr(cur, "end") == cur || strstr(cur, "def") == cur) break;

            char name[64] = {0};
            if (sscanf(cur, "/%63s", name) >= 1) {
                uint32_t cp = 0;
                if (strcmp(name, ".notdef") == 0) cp = 0;
                else if (strlen(name) == 1) cp = (uint8_t)name[0];
                else if (name[0] == 'u' && name[1] == 'n' && name[2] == 'i') {
                    unsigned int hex = 0;
                    if (sscanf(name + 3, "%x", &hex) >= 1) cp = hex;
                } else {
                    cp = 0xE000 + (uint32_t)(cur - cs_pos);
                }

                suf_metric_t m = { .advance_width = (int16_t)(max_x - min_x), .left_side_bearing = (int16_t)min_x,
                                   .x_min = (int16_t)min_x, .y_min = (int16_t)min_y, .x_max = (int16_t)max_x, .y_max = (int16_t)max_y, .data_offset = 0 };

                uint8_t outline[64];
                size_t olen = 0;
                outline[olen++] = SUF_CMD_MOVE_TO;
                write_le16(outline + olen, (uint16_t)m.x_min); olen += 2;
                write_le16(outline + olen, (uint16_t)m.y_min); olen += 2;
                outline[olen++] = SUF_CMD_LINE_TO;
                write_le16(outline + olen, (uint16_t)m.x_max); olen += 2;
                write_le16(outline + olen, (uint16_t)m.y_max); olen += 2;
                outline[olen++] = SUF_CMD_CLOSE_PATH;
                outline[olen++] = SUF_CMD_END_GLYPH;

                suf_builder_add_glyph(b, cp, &m, NULL, 0, outline, olen);
            }
            cur = strchr(cur, '\n');
            if (!cur) break;
        }
    } else {
        /* Default glyph */
        suf_metric_t m = { .advance_width = 1000, .left_side_bearing = 0, .x_min = 0, .y_min = (int16_t)descent, .x_max = 1000, .y_max = (int16_t)ascent, .data_offset = 0 };
        suf_builder_add_glyph(b, 0, &m, NULL, 0, NULL, 0);
    }

    *out_builder = b;
    return SUF_OK;
}

suf_status_t suf_conv_pfb_to_suf(const uint8_t *pfb_data, size_t pfb_size, suf_builder_t **out_builder) {
    if (!pfb_data || !out_builder) return SUF_ERR_NULL_POINTER;
    if (pfb_size < 18) return SUF_ERR_BUFFER_TOO_SMALL;

    /* Peel PFB records: 0x80 [type 0x01/0x02] [4-byte LE length] */
    char *text_buf = (char *)malloc(pfb_size + 1);
    if (!text_buf) return SUF_ERR_ALLOC_FAIL;

    size_t in_pos = 0;
    size_t out_pos = 0;

    while (in_pos + 6 <= pfb_size && pfb_data[in_pos] == 0x80) {
        uint8_t type = pfb_data[in_pos + 1];
        if (type == 3) break; /* End of font */

        uint32_t len = read_le32(pfb_data + in_pos + 2);
        in_pos += 6;

        if (type == 1) { /* ASCII chunk */
            if (in_pos + len > pfb_size) len = (uint32_t)(pfb_size - in_pos);
            memcpy(text_buf + out_pos, pfb_data + in_pos, len);
            out_pos += len;
        }
        in_pos += len;
    }
    text_buf[out_pos] = '\0';

    suf_status_t st = suf_conv_ps_to_suf(text_buf, out_pos, out_builder);
    free(text_buf);
    return st;
}

suf_status_t suf_conv_suf_to_ps(const uint8_t *suf_data, size_t suf_size, char **out_ps, size_t *out_ps_len) {
    if (!suf_data || !out_ps || !out_ps_len) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(suf_data, suf_size, &hdr);
    if (st != SUF_OK) return st;

    size_t alloc_sz = 2048 + (hdr.glyph_count * 256);
    char *buf = (char *)malloc(alloc_sz);
    if (!buf) return SUF_ERR_ALLOC_FAIL;

    char ps_buf[128], full_buf[160], fam_buf[128];
    const char *psn = suf_display_name(suf_data, suf_size, 6, ps_buf, sizeof(ps_buf), "SuperUnicodeFont");
    for (char *q = ps_buf; psn == ps_buf && q && *q; ++q) {
        if (*q == ' ') *q = '-';
    }
    /* PostScript strings must not carry parentheses. */
    const char *full = suf_display_name(suf_data, suf_size, 4, full_buf, sizeof(full_buf), "SuperUnicode Font");
    for (char *q = full_buf; full == full_buf && q && *q; ++q) {
        if (*q == '(') *q = '[';
        if (*q == ')') *q = ']';
    }
    const char *fam = suf_display_name(suf_data, suf_size, 1, fam_buf, sizeof(fam_buf), "SuperUnicode");
    for (char *q = fam_buf; fam == fam_buf && q && *q; ++q) {
        if (*q == '(') *q = '[';
        if (*q == ')') *q = ']';
    }

    int pos = snprintf(buf, alloc_sz,
        "%%!PS-AdobeFont-1.0: %s 1.0\n"
        "%%Title: %s\n"
        "11 dict begin\n"
        "/FontInfo 5 dict dup begin\n"
        "  /version (1.0) readonly def\n"
        "  /FullName (%s) readonly def\n"
        "  /FamilyName (%s) readonly def\n"
        "  /Weight (Regular) readonly def\n"
        "end readonly def\n"
        "/FontName /%s def\n"
        "/PaintType 0 def\n"
        "/FontType 1 def\n"
        "/FontMatrix [0.001 0 0 0.001 0 0] readonly def\n"
        "/FontBBox [%d %d %d %d] readonly def\n"
        "/Encoding StandardEncoding def\n"
        "/CharStrings %u dict dup begin\n",
        psn, psn, full, fam, psn,
        hdr.bbox_min_x, hdr.bbox_min_y, hdr.bbox_max_x, hdr.bbox_max_y,
        (unsigned int)hdr.glyph_count + 1
    );

    pos += snprintf(buf + pos, alloc_sz - pos, "  /.notdef { 0 0 %d %d rectpath fill } def\n", hdr.ascender, hdr.descender);

    for (uint32_t i = 0; i < hdr.glyph_count; ++i) {
        suf_metric_t m;
        suf_get_glyph_metric(suf_data, suf_size, i, &m);

        pos += snprintf(buf + pos, alloc_sz - pos,
            "  /glyph%04u { %d %d %d %d rectpath fill } def\n",
            i, m.x_min, m.y_min, m.x_max - m.x_min, m.y_max - m.y_min
        );
    }

    pos += snprintf(buf + pos, alloc_sz - pos,
        "end readonly def\n"
        "currentdict end\n"
        "/SuperUnicodeFont exch definefont pop\n"
    );

    *out_ps = buf;
    *out_ps_len = (size_t)pos;
    return SUF_OK;
}

suf_status_t suf_conv_suf_to_pfb(const uint8_t *suf_data, size_t suf_size, uint8_t **out_pfb, size_t *out_pfb_size) {
    if (!suf_data || !out_pfb || !out_pfb_size) return SUF_ERR_NULL_POINTER;

    char *ps_text = NULL;
    size_t ps_len = 0;
    suf_status_t st = suf_conv_suf_to_ps(suf_data, suf_size, &ps_text, &ps_len);
    if (st != SUF_OK) return st;

    /* PFB format: 0x80 0x01 [4-byte LE length] <ASCII> 0x80 0x03 */
    size_t total_sz = 6 + ps_len + 2;
    uint8_t *pfb = (uint8_t *)malloc(total_sz);
    if (!pfb) {
        free(ps_text);
        return SUF_ERR_ALLOC_FAIL;
    }

    pfb[0] = 0x80;
    pfb[1] = 0x01;
    write_le32(pfb + 2, (uint32_t)ps_len);
    memcpy(pfb + 6, ps_text, ps_len);

    pfb[6 + ps_len] = 0x80;
    pfb[6 + ps_len + 1] = 0x03; /* EOF */

    free(ps_text);
    *out_pfb = pfb;
    *out_pfb_size = total_sz;
    return SUF_OK;
}

/* ========================================================================= */
/* Inbound & Outbound: Unified Font Object (.ufo) <-> .suf Converter        */
/* ========================================================================= */

/**
 * @brief Extracts an integer value from a plist XML string for a given key.
 * Scans for <key>keyname</key> followed by <integer>value</integer>.
 */
static int plist_extract_int(const char *xml, const char *key, int default_val) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "<key>%s</key>", key);
    const char *pos = strstr(xml, pattern);
    if (!pos) return default_val;
    pos += strlen(pattern);
    /* Skip whitespace and find <integer> or <real> */
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    if (strncmp(pos, "<integer>", 9) == 0) {
        int val = 0;
        if (sscanf(pos + 9, "%d", &val) >= 1) return val;
    } else if (strncmp(pos, "<real>", 6) == 0) {
        double val = 0.0;
        if (sscanf(pos + 6, "%lf", &val) >= 1) return (int)val;
    }
    return default_val;
}

/**
 * @brief Extracts a string value from a plist XML string for a given key.
 */
static const char *plist_extract_string(const char *xml, const char *key, char *out, size_t out_sz) {
    char pattern[128];
    snprintf(pattern, sizeof(pattern), "<key>%s</key>", key);
    const char *pos = strstr(xml, pattern);
    if (!pos) return NULL;
    pos += strlen(pattern);
    while (*pos == ' ' || *pos == '\t' || *pos == '\r' || *pos == '\n') pos++;
    if (strncmp(pos, "<string>", 8) == 0) {
        pos += 8;
        const char *end = strstr(pos, "</string>");
        if (end) {
            size_t len = (size_t)(end - pos);
            if (len >= out_sz) len = out_sz - 1;
            memcpy(out, pos, len);
            out[len] = '\0';
            return out;
        }
    }
    return NULL;
}

suf_status_t suf_conv_ufo_to_suf(const char *ufo_xml, size_t ufo_len, suf_builder_t **out_builder) {
    if (!ufo_xml || !out_builder) return SUF_ERR_NULL_POINTER;
    if (ufo_len == 0) return SUF_ERR_BUFFER_TOO_SMALL;

    /* Extract font metrics from fontinfo.plist XML */
    int units_per_em = plist_extract_int(ufo_xml, "unitsPerEm", 1000);
    int ascender     = plist_extract_int(ufo_xml, "ascender", 800);
    int descender    = plist_extract_int(ufo_xml, "descender", -200);
    int x_height     = plist_extract_int(ufo_xml, "xHeight", 500);
    int cap_height   = plist_extract_int(ufo_xml, "capHeight", 700);

    if (units_per_em <= 0) units_per_em = 1000;

    suf_builder_t *b = suf_builder_create(
        (uint16_t)units_per_em,
        (int16_t)ascender,
        (int16_t)descender,
        SUF_FLAG_OS_VECTOR
    );
    if (!b) return SUF_ERR_ALLOC_FAIL;

    suf_builder_set_bbox(b, 0, (int16_t)descender, (int16_t)units_per_em, (int16_t)ascender);

    /* Generate a default .notdef glyph */
    suf_metric_t metric;
    memset(&metric, 0, sizeof(metric));
    metric.advance_width = (int16_t)(units_per_em / 2);
    metric.x_min = 0;
    metric.y_min = (int16_t)descender;
    metric.x_max = (int16_t)(units_per_em / 2);
    metric.y_max = (int16_t)ascender;

    uint8_t outline_cmds[64];
    size_t olen = 0;
    outline_cmds[olen++] = SUF_CMD_MOVE_TO;
    write_le16(outline_cmds + olen, (uint16_t)metric.x_min); olen += 2;
    write_le16(outline_cmds + olen, (uint16_t)metric.y_min); olen += 2;
    outline_cmds[olen++] = SUF_CMD_LINE_TO;
    write_le16(outline_cmds + olen, (uint16_t)metric.x_max); olen += 2;
    write_le16(outline_cmds + olen, (uint16_t)metric.y_min); olen += 2;
    outline_cmds[olen++] = SUF_CMD_LINE_TO;
    write_le16(outline_cmds + olen, (uint16_t)metric.x_max); olen += 2;
    write_le16(outline_cmds + olen, (uint16_t)metric.y_max); olen += 2;
    outline_cmds[olen++] = SUF_CMD_LINE_TO;
    write_le16(outline_cmds + olen, (uint16_t)metric.x_min); olen += 2;
    write_le16(outline_cmds + olen, (uint16_t)metric.y_max); olen += 2;
    outline_cmds[olen++] = SUF_CMD_CLOSE_PATH;
    outline_cmds[olen++] = SUF_CMD_END_GLYPH;

    suf_builder_add_glyph(b, 0, &metric, NULL, 0, outline_cmds, olen);

    /* Add a space glyph (U+0020) */
    suf_metric_t space_metric;
    memset(&space_metric, 0, sizeof(space_metric));
    space_metric.advance_width = (int16_t)(units_per_em / 4);
    suf_builder_add_glyph(b, 0x0020, &space_metric, NULL, 0, NULL, 0);

    (void)x_height;
    (void)cap_height;

    *out_builder = b;
    return SUF_OK;
}

suf_status_t suf_conv_suf_to_ufo(const uint8_t *suf_data, size_t suf_size, char **out_ufo, size_t *out_ufo_len) {
    if (!suf_data || !out_ufo || !out_ufo_len) return SUF_ERR_NULL_POINTER;

    suf_header_t hdr;
    suf_status_t st = suf_validate_header(suf_data, suf_size, &hdr);
    if (st != SUF_OK) return st;

    size_t alloc_sz = 4096;
    char *buf = (char *)malloc(alloc_sz);
    if (!buf) return SUF_ERR_ALLOC_FAIL;

    char fam_buf[128];
    const char *fam = suf_display_name(suf_data, suf_size, 1, fam_buf, sizeof(fam_buf), "SuperUnicode Font");
    /* XML text payload: escape & < > minimally. */
    char xml_fam[384];
    size_t xo = 0;
    for (const char *q = fam; *q && xo + 7 < sizeof(xml_fam); ++q) {
        if (*q == '&') { memcpy(xml_fam + xo, "&amp;", 5); xo += 5; }
        else if (*q == '<') { memcpy(xml_fam + xo, "&lt;", 4); xo += 4; }
        else if (*q == '>') { memcpy(xml_fam + xo, "&gt;", 4); xo += 4; }
        else { xml_fam[xo++] = *q; }
    }
    xml_fam[xo] = '\0';

    int pos = snprintf(buf, alloc_sz,
        "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
        "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
        "<plist version=\"1.0\">\n"
        "<dict>\n"
        "\t<key>familyName</key>\n"
        "\t<string>%s</string>\n"
        "\t<key>unitsPerEm</key>\n"
        "\t<integer>%u</integer>\n"
        "\t<key>ascender</key>\n"
        "\t<integer>%d</integer>\n"
        "\t<key>descender</key>\n"
        "\t<integer>%d</integer>\n"
        "\t<key>xHeight</key>\n"
        "\t<integer>%d</integer>\n"
        "\t<key>capHeight</key>\n"
        "\t<integer>%d</integer>\n"
        "\t<key>openTypeHeadCreated</key>\n"
        "\t<string>2025/01/01 00:00:00</string>\n"
        "\t<key>openTypeHheaAscender</key>\n"
        "\t<integer>%d</integer>\n"
        "\t<key>openTypeHheaDescender</key>\n"
        "\t<integer>%d</integer>\n"
        "\t<key>openTypeHheaLineGap</key>\n"
        "\t<integer>%d</integer>\n"
        "</dict>\n"
        "</plist>\n",
        xml_fam,
        (unsigned int)hdr.units_per_em,
        (int)hdr.ascender,
        (int)hdr.descender,
        (int)(hdr.ascender * 2 / 3),   /* Estimated xHeight */
        (int)(hdr.ascender * 9 / 10),  /* Estimated capHeight */
        (int)hdr.ascender,
        (int)hdr.descender,
        (int)hdr.line_gap
    );

    *out_ufo = buf;
    *out_ufo_len = (size_t)pos;
    return SUF_OK;
}

/* ========================================================================= */
/* Modded SuperUnicode Plugin Font Packaging (Extended Mode Only)            */
/* ========================================================================= */

#define SCSP_MAGIC 0x53435343UL /* 'SUCS' */

#pragma pack(push, 1)
typedef struct {
    uint32_t magic;
    uint16_t blob_version;
    uint8_t  ver_major;
    uint8_t  ver_minor;
    uint8_t  ver_patch;
    uint8_t  reserved;
    char     id[64];
    uint32_t range_count;
    uint32_t blob_size;
    uint32_t crc32c;
    uint64_t fletcher64;
} scsp_blob_hdr_t;
#pragma pack(pop)

suf_status_t suf_conv_pack_plugin_font(const uint8_t *suf_data, size_t suf_size,
                                      const char *plugin_id,
                                      uint8_t ver_major, uint8_t ver_minor, uint8_t ver_patch,
                                      uint8_t **out_blob, size_t *out_blob_size) {
    if (!suf_data || !plugin_id || !out_blob || !out_blob_size) return SUF_ERR_NULL_POINTER;

    suf_header_t suf_hdr;
    suf_status_t st = suf_validate_header(suf_data, suf_size, &suf_hdr);
    if (st != SUF_OK) return st;

    size_t hdr_sz = sizeof(scsp_blob_hdr_t);
    size_t total_sz = hdr_sz + suf_size;

    uint8_t *blob = (uint8_t *)calloc(1, total_sz);
    if (!blob) return SUF_ERR_ALLOC_FAIL;

    scsp_blob_hdr_t *hdr = (scsp_blob_hdr_t *)blob;
    hdr->magic = SCSP_MAGIC;
    hdr->blob_version = 1;
    hdr->ver_major = ver_major;
    hdr->ver_minor = ver_minor;
    hdr->ver_patch = ver_patch;
    hdr->reserved = 0;
    strncpy(hdr->id, plugin_id, sizeof(hdr->id) - 1);
    hdr->range_count = 1;
    hdr->blob_size = (uint32_t)total_sz;
    hdr->crc32c = 0;
    hdr->fletcher64 = 0;

    memcpy(blob + hdr_sz, suf_data, suf_size);

    /* Compute CRC32c and Fletcher-64 with checksum fields zeroed */
    hdr->crc32c = calc_crc32c(blob, total_sz);
    hdr->fletcher64 = calc_fletcher64(blob, total_sz);

    *out_blob = blob;
    *out_blob_size = total_sz;
    return SUF_OK;
}

suf_status_t suf_conv_unpack_plugin_font(const uint8_t *blob_data, size_t blob_size,
                                        uint8_t **out_suf, size_t *out_suf_size) {
    if (!blob_data || !out_suf || !out_suf_size) return SUF_ERR_NULL_POINTER;
    if (blob_size < sizeof(scsp_blob_hdr_t) + sizeof(suf_header_t)) return SUF_ERR_BUFFER_TOO_SMALL;

    const scsp_blob_hdr_t *hdr = (const scsp_blob_hdr_t *)blob_data;
    if (hdr->magic != SCSP_MAGIC) return SUF_ERR_INVALID_MAGIC;

    size_t font_sz = blob_size - sizeof(scsp_blob_hdr_t);
    const uint8_t *font_src = blob_data + sizeof(scsp_blob_hdr_t);

    suf_header_t check_hdr;
    suf_status_t st = suf_validate_header(font_src, font_sz, &check_hdr);
    if (st != SUF_OK) return st;

    uint8_t *suf_out = (uint8_t *)malloc(font_sz);
    if (!suf_out) return SUF_ERR_ALLOC_FAIL;
    memcpy(suf_out, font_src, font_sz);

    *out_suf = suf_out;
    *out_suf_size = font_sz;
    return SUF_OK;
}
