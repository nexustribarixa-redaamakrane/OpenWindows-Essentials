/*
 * udf_format.h - Universal Disk Format (UDF) Optical Media Specification
 * C99 freestanding. Zero dynamic heap allocation.
 */
#ifndef UDF_FORMAT_H
#define UDF_FORMAT_H

#include <stdint.h>

#define UDF_BEA01_MAGIC 0x3130414542ULL /* "BEA01" */
#define UDF_NSR02_MAGIC 0x3230534EULL   /* "NSR02" */
#define UDF_NSR03_MAGIC 0x3330534EULL   /* "NSR03" */

#pragma pack(push, 1)
typedef struct {
    uint8_t  structure_type;
    char     standard_identifier[5]; /* "BEA01", "NSR02", etc. */
    uint8_t  structure_version;
    uint8_t  structure_data[2041];
} udf_anchor_descriptor_t;
#pragma pack(pop)

#endif /* UDF_FORMAT_H */
