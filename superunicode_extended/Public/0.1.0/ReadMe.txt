ExtSUCD 0.1.0 (Extended)
========================

First versioned release of the Extended machine-instruction database.

  extsucd/         registry.txt + base/ snapshot + plugins/<id>/
  extdat/          extended per-codepoint data
  plugin-drafts/   draft plugin packages
  collation/       ExtUCA
  transport/       vector layout, vsutf.txt, esutf.txt
  control/         inherited Base control plane (SCP/Traps/Sentinel)
  charts/          extended code charts
  mappings/        UNICODE bridge + per-plugin mappings
  partitions/      Plugin Partition policy (OWFS-only)

Base codepoints 0x00000000-0x7FFFFFFF remain authoritative from the Base
tree; this release only ADDS plugin territory above 0x7FFFFFFF. ExtSUCD.zip
packs this release (see zipped/).
