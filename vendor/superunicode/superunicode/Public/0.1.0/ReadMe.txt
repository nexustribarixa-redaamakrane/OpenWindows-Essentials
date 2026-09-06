SUCD 0.1.0 (Base SUCS)
======================

First versioned release of the Base SuperUnicode machine-instruction
database. Layout follows unicode.org/Public/<version>/:

  sucd/       SUCSData.txt, Blocks.txt, Props/, Derived/, auxiliary/
  sudat/      assignments.txt, control/ (SCP, Traps, Sentinel), bancodes/
  names/      NamesList.txt, NameAliases.txt
  sources/    Sources.txt (provenance)
  collation/  SUCA.txt (SuperUnicode Collation Algorithm ordering)
  sutf/       SUCS UTF-8/16/32 framing tables
  charts/     printable code charts
  mappings/   UNICODE (1:1 bridge), LEGACY, OBSOLETE
  partitions/ partition policy (SuperUnicode Partitions: OWFS or USFS)

The Base SUCS codepoint space is 0x00000000-0x7FFFFFFF (31-bit). It is
the FULL machine-instruction domain of the "website/default" runtime
(limit 0x7FFFFFFF). Codepoints beyond it require plugins (see the
Extended tree).

SUCD.zip is the single-file packed form of this release (see zipped/).
