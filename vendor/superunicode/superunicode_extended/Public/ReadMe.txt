SUPERUNICODE EXTENDED /Public
=============================

Machine-instruction database for the Extended SuperUnicode Character System
(ExtSUCS). ExtSUCS is UNBOUNDED (64-bit sucs_ex_char_t). The default
runtime ceiling is the Base SUCS limit 0x7FFFFFFF; codepoints above it
are provided by PLUGINS, each mounting as a read-only OWFS partition.

Unlike the Base tree, this tree has NO fixed layout overlap with it:

  0.1.0/extsucd/    registry.txt, base/ (inherited snapshot), plugins/
  0.1.0/extdat/     extended per-codepoint data
  0.1.0/plugin-drafts/  plugin proposals for the NEXT release
  0.1.0/collation/  ExtUCA ordering
  0.1.0/transport/  vector layout, vsutf, esutf
  0.1.0/control/    inherited SCP/Traps/Sentinel (Base tree)
  0.1.0/charts/, mappings/, partitions/

A plugin is APPROVED by landing its package in
extsucd/plugins/<plugin-id>/ AND a row in extsucd/registry.txt.

See the SDK: superunicode_extended/plugin/sdk/README.md.
