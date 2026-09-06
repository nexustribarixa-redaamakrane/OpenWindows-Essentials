# /Public/zipped - Base SUCS release archives (0.1.0)
# All zips pack the corresponding 0.1.0/ sub-tree (Compress-Archive, UTF-8).

SUCD.zip          whole release (sucd + sudat + names + sources + collation
                  + sutf + charts + mappings + partitions)
sucd.zip          core codepoint inventory (SUCSData, Blocks, Props,
                  Derived, auxiliary)  [UCD.zip analog]
sudat.zip         unidata: SCP control plane, Traps, Sentinel, bancodes
                  [UNIDATA.zip analog]
names.zip         NamesList, NameAliases
sources.zip       provenance
collation.zip     SUCA ordering
sutf.zip          SUTF8/16/32 framing tables
mappings.zip      UNICODE, LEGACY, OBSOLETE
charts.zip        code charts
partitions.zip    partition policy (OWFS or USFS)

ExtSUCD.zip and Plugin-SDK.zip live in the Extended tree's zipped/.
