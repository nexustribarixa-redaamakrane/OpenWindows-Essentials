SUPERUNICODE /Public
====================

The machine-instruction database for the Base SuperUnicode Character System
(SUCS), mirroring the conventions of unicode.org/Public (versioned
releases under /Public/<version>, plus latest/, zipped/, draft/).

Contents of this tree (case-sensitive, like unicode.org/Public):

  0.1.0/     versioned release: sucd, sudat, names, sources, collation,
             sutf, charts, mappings, partitions
  latest/    pointer to the current versioned release
  drafts/    working drafts for the NEXT release (0.2.0)
  zipped/    SUCD.zip etc. ready to download
  programs/  official machine-instruction tooling
  security/  attestation data for published releases

A plugin's ranges live ABOVE this tree's domain: the Base SUCS ceiling is
0x7FFFFFFF. Plugins belong to the Extended tree:
superunicode_extended/Public/extsucd/plugins/.
