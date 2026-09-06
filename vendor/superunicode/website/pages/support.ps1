# Versions, Charts and FAQ sections

$Pages['versions/index'] = @{
    path    = 'versions/index.html'
    sec     = 'versions'
    title   = 'Versions'
    desc    = 'Every release of SuperUnicode, its database and its reports.'
    crumbName = 'Versions'
    h1      = 'Versions'
    subtitle= 'Every release of SuperUnicode, its database and its reports.'
    body    = @(
        @{ t = 'p'; html = 'SuperUnicode follows the Unicode versioning philosophy: a single version number identifies a coherent release of the standard, the database and the reference implementation.' }
        @{ t = 'table'; head = @('Version', 'Date', 'Notes'); rows = @(
            @('<span class="mono">0.1.0</span>', '2026-08-14', 'First release. Base + Extended /Public trees, plugin subsystem, SDK, reference implementations. See <a href="0.1.0.html">release notes</a>.')
        ) }
        @{ t = 'h2'; html = 'Roadmap' }
        @{ t = 'table'; head = @('Version', 'Scope'); rows = @(
            @('<span class="mono">0.2.0</span>', 'First native allocations, SCP registry assignments, additional SUTR reports')
            @('<span class="mono">0.3.0</span>', 'SUCA full tailoring, charts generation, extended auxiliary data')
            @('<span class="mono">1.0.0</span>', 'Stable ABI: sucs_plugin_entry(), blob format and /Public schema frozen')
        ) }
        @{ t = 'p'; html = '<a href="latest.html">The latest version &raquo;</a> &middot; <a href="../standard/versioning.html">How versioning works &raquo;</a>' }
    )
}

$Pages['versions/0.1.0'] = @{
    path    = 'versions/0.1.0.html'
    sec     = 'versions'
    title   = 'SuperUnicode 0.1.0 — release notes'
    desc    = 'Release notes for SuperUnicode 0.1.0.'
    crumbName = '0.1.0 release notes'
    crumbs  = @( @{ label = 'Versions'; href = 'index.html' } )
    h1      = 'SuperUnicode <span class="grad">0.1.0</span>'
    subtitle= 'Release notes &mdash; 2026-08-14.'
    body    = @(
        @{ t = 'p'; html = 'The first release. Everything here is compiled, tested and attested with CRC32c + Fletcher-64:' }
        @{ t = 'ul'; items = @(
            '<strong>Base SUCS</strong> &mdash; 31-bit space, hierarchy, three-space layout, SCP/Traps/Sentinel.'
            '<strong>Machine-Instruction Database</strong> &mdash; the <span class="mono">Public/</span> trees for Base and Extended with <span class="mono">SUCD.zip</span> / <span class="mono">ExtSUCD.zip</span> and per-component archives.'
            '<strong>Plugin subsystem</strong> &mdash; stage &rarr; reboot &rarr; checksum gate &rarr; OWFS plugin partition &rarr; range registration, with quarantine.'
            '<strong>Plugin SDK</strong> &mdash; template, <span class="mono">hellocp</span> example, <span class="mono">plugin_pack</span> and <span class="mono">plugin_verify</span> tools.'
            '<strong>Reference implementations</strong> &mdash; SUTF8/16/32, extSUTF (vector/vsutf), SUST serialization transports (SUST-16 BE/LE, fixed, e-SUST), SUCA, SDF/BiDi, SGW width/grid, SBR line breaking, SUCF canonical (de)composition and the one-TU unified header test.'
            '<strong>The website</strong> &mdash; a multi-page mirror of unicode.org, plus repository directory listings.'
        ) }
        @{ t = 'h2'; html = 'Validation' }
        @{ t = 'p'; html = 'The 0.1.0 suite runs 15 tests (framing, planes, plugin lifecycle, serialization transports, SDF/BiDi, SUCA collation, SGW width/grid, SBR line breaking, SUCF canonical forms, unified headers) &mdash; all passing on the reference build.' }
        @{ t = 'callout'; html = 'Known limits: no native allocations yet, the SCP registry is empty pending assignments, and SUCA ships a curated default SUCET subset (full per-language tables pending native data).' }
    )
}

$Pages['versions/latest'] = @{
    path    = 'versions/latest.html'
    sec     = 'versions'
    title   = 'Latest — SuperUnicode'
    desc    = 'The latest release of SuperUnicode.'
    crumbName = 'Latest'
    crumbs  = @( @{ label = 'Versions'; href = 'index.html' } )
    h1      = 'Latest'
    subtitle= 'The current release.'
    body    = @(
        @{ t = 'callout'; html = '<strong>Latest release: SuperUnicode 0.1.0</strong> (2026-08-14). <a href="0.1.0.html">Release notes &raquo;</a>' }
        @{ t = 'p'; html = 'The machine-instruction database for the current release is published under <span class="mono">/Public/0.1.0/</span>, mirrored in the repositories and at <span class="mono">latest/</span> inside each /Public tree.' }
    )
}

$Pages['charts/index'] = @{
    path    = 'charts/index.html'
    sec     = 'charts'
    title   = 'Code Charts'
    desc    = 'Block maps and printable charts for the Base and Extended spaces.'
    crumbName = 'Code Charts'
    h1      = 'Code <span class="grad">Charts</span>'
    subtitle= 'Block maps for the Base and Extended spaces.'
    body    = @(
        @{ t = 'p'; html = 'Charts are generated from the machine-instruction database (<span class="mono">Blocks.txt</span>, <span class="mono">NamesList.txt</span>, <span class="mono">PropList.txt</span>). 0.1.0 publishes the block maps; per-codepoint printable charts ship as native allocations are assigned in 0.2.0.' }
        @{ t = 'h2'; html = 'Charts' }
        @{ t = 'grid'; cards = @(
            @{ title = 'Base SUCS blocks'; href = 'charts/blocks.html'; html = 'From the Unicode Bridge to the Sentinel.' }
            @{ title = 'Plugin ranges'; href = 'charts/plugins.html'; html = 'Charts for approved Extended plugins.' }
        ) }
        @{ t = 'note'; html = 'The normative tables live in the database: <span class="mono">sucd/Blocks.txt</span>, <span class="mono">names/NamesList.txt</span>, <span class="mono">sucd/Props/PropList.txt</span>.' }
    )
}

$Pages['charts/blocks'] = @{
    path    = 'charts/blocks.html'
    sec     = 'charts'
    title   = 'Base SUCS block chart'
    desc    = 'The block chart of the Base SUCS space.'
    crumbName = 'Base SUCS block chart'
    crumbs  = @( @{ label = 'Charts'; href = 'index.html' } )
    h1      = 'Base SUCS <span class="grad">block chart</span>'
    subtitle= 'Every named block of the 31-bit space.'
    body    = @(
        @{ t = 'table'; head = @('Block', 'Range', 'Space'); rows = @(
            @('Unicode Bridge', '<span class="mono">0000..0010FFFF</span>', 'Unicode Compatibility Space')
            @('SuperUnicode Control Plane', '<span class="mono">00110000..0011FFFF</span>', 'System Control Plane')
            @('BANcode', '<span class="mono">0011A000..0011A7FF</span>', 'System Control Plane')
            @('WARNcode', '<span class="mono">0011A800..0011ABFF</span>', 'System Control Plane')
            @('COMcode', '<span class="mono">0011AC00..0011ADFF</span>', 'System Control Plane')
            @('SOFTcode', '<span class="mono">0011AE00..0011AEFF</span>', 'System Control Plane')
            @('Native', '<span class="mono">00120000..7FFFFFFF</span>', 'Native Space')
            @('Traps', '<span class="mono">7FFFFFF0..7FFFFFFE</span>', 'Native Space (reserved)')
            @('Sentinel', '<span class="mono">7FFFFFFF</span>', 'Native Space (reserved)')
        ) }
    )
}

$Pages['charts/plugins'] = @{
    path    = 'charts/plugins.html'
    sec     = 'charts'
    title   = 'Plugin range chart'
    desc    = 'Charts for approved Extended plugins.'
    crumbName = 'Plugin range chart'
    crumbs  = @( @{ label = 'Charts'; href = 'index.html' } )
    h1      = 'Plugin <span class="grad">range chart</span>'
    subtitle= 'Approved Extended plugins and their ranges.'
    body    = @(
        @{ t = 'table'; head = @('Plugin', 'Range', 'Codepoints'); rows = @(
            @('<span class="mono">org.openwindows.hellocp</span>', '<span class="mono">80000000..80000FFF</span>', '4096')
        ) }
        @{ t = 'p'; html = 'Every plugin range begins strictly above <span class="mono">0x7FFFFFFF</span>. See the <a href="../extended/registry.html">ExtSUCS registry</a>.' }
    )
}

$Pages['faq/index'] = @{
    path    = 'faq/index.html'
    sec     = 'faq'
    title   = 'FAQ'
    desc    = 'Frequently asked questions about SuperUnicode.'
    crumbName = 'FAQ'
    h1      = 'Frequently Asked <span class="grad">Questions</span>'
    subtitle= 'Everything about SuperUnicode and its relationship to Unicode.'
    body    = @(
        @{ t = 'h2'; html = 'Topics' }
        @{ t = 'grid'; cards = @(
            @{ title = 'Compatibility'; href = 'faq/compatibility.html'; html = 'Is SuperUnicode compatible with Unicode? What happens when Unicode grows?' }
            @{ title = 'Extended &amp; plugins'; href = 'faq/extended.html'; html = 'Why the restart? What stops a malicious plugin? Can Base use plugin partitions?' }
            @{ title = 'Encoding &amp; framing'; href = 'faq/encoding.html'; html = 'SUTF-8/16/32, the 31-bit width, and why 64-bit transport exists.' }
            @{ title = 'Storage'; href = 'faq/storage.html'; html = 'OWFS, USFS, partitions and the checksum gate.' }
        ) }
    )
}

$Pages['faq/compatibility'] = @{
    path    = 'faq/compatibility.html'
    sec     = 'faq'
    title   = 'FAQ — Compatibility'
    desc    = 'Is SuperUnicode compatible with Unicode? What happens when Unicode grows?'
    crumbName = 'Compatibility'
    crumbs  = @( @{ label = 'FAQ'; href = 'index.html' } )
    h1      = 'FAQ &mdash; <span class="grad">Compatibility</span>'
    body    = @(
        @{ t = 'p'; html = '<strong>Is SuperUnicode compatible with Unicode?</strong>' }
        @{ t = 'p'; html = 'Yes. Codepoints <span class="mono">0x000000&ndash;0x10FFFF</span> are 1:1 with Unicode &mdash; the <em>Unicode Compatibility Space</em>. The guarantee is permanent: even if Unicode releases a new version or assigns new codepoints, this range still matches Unicode exactly.' }
        @{ t = 'p'; html = '<strong>Will new Unicode codepoints break SuperUnicode?</strong>' }
        @{ t = 'p'; html = 'No. The bridge is defined by position, not by version. A codepoint&rsquo;s meaning in SuperUnicode is fixed at <span class="mono">0x000000&ndash;0x10FFFF</span>; when Unicode grows, SuperUnicode simply remains in sync with the same numeric values.' }
        @{ t = 'p'; html = '<strong>Can a SuperUnicode string be passed to a Unicode API?</strong>' }
        @{ t = 'p'; html = 'If it lies entirely within the compatibility space, yes &mdash; <span class="mono">sucs_downcast()</span> yields the identical Unicode value. Native and plugin codepoints have no Unicode equivalent; they are OpenWindows&rsquo; own.' }
        @{ t = 'p'; html = 'The formal guarantee is <a href="../reports/SUTR-6.html">SUTR-6 &mdash; Unicode Compatibility Bridge</a>.' }
    )
}

$Pages['faq/extended'] = @{
    path    = 'faq/extended.html'
    sec     = 'faq'
    title   = 'FAQ — Extended &amp; plugins'
    desc    = 'Why the restart? What stops a malicious plugin? Can Base use plugin partitions?'
    crumbName = 'Extended &amp; plugins'
    crumbs  = @( @{ label = 'FAQ'; href = 'index.html' } )
    h1      = 'FAQ &mdash; Extended &amp; <span class="grad">plugins</span>'
    body    = @(
        @{ t = 'p'; html = '<strong>Why do plugins require a restart?</strong>' }
        @{ t = 'p'; html = 'Mounting a plugin changes the active namespace, exactly like switching a runtime mode. The kernel stages the blob, returns <span class="mono">SUCS_PLUGIN_REBOOT_REQUIRED</span>, and only on the next boot runs the checksum gate before mounting.' }
        @{ t = 'p'; html = '<strong>What stops a malicious plugin?</strong>' }
        @{ t = 'p'; html = 'The boot checksum gate (CRC32c + Fletcher-64) rejects any altered blob. Plugin partitions are OWFS only, always read-only, and quarantined plugins never mount or register ranges.' }
        @{ t = 'p'; html = '<strong>Can Base SUCS use plugin partitions?</strong>' }
        @{ t = 'p'; html = 'No. Plugin Partitions are an Extended-only feature. Base SUCS uses its SuperUnicode Partition (OWFS or USFS) only for bugfix and rescue payloads.' }
        @{ t = 'p'; html = '<strong>Where do plugin codepoints live?</strong>' }
        @{ t = 'p'; html = 'Strictly above <span class="mono">0x7FFFFFFF</span>. The <a href="../extended/registry.html">registry</a> records each plugin&rsquo;s ranges and state.' }
    )
}

$Pages['faq/encoding'] = @{
    path    = 'faq/encoding.html'
    sec     = 'faq'
    title   = 'FAQ — Encoding &amp; framing'
    desc    = 'SUTF-8/16/32, the 31-bit width, and why 64-bit transport exists.'
    crumbName = 'Encoding &amp; framing'
    crumbs  = @( @{ label = 'FAQ'; href = 'index.html' } )
    h1      = 'FAQ &mdash; Encoding &amp; <span class="grad">framing</span>'
    body    = @(
        @{ t = 'p'; html = '<strong>Why does SuperUnicode need a 31-bit space?</strong>' }
        @{ t = 'p'; html = 'Unicode covers about 1.1 million codepoints. OpenWindows uses codepoints as machine instructions too, so it reserves the <em>System Control Plane</em> for diagnostics and a large <em>Native Space</em> for its own allocations &mdash; all while keeping the entire space addressable in a single <span class="mono">uint32_t</span>.' }
        @{ t = 'p'; html = '<strong>Which encoding should I use?</strong>' }
        @{ t = 'p'; html = 'SUTF-8 is the general-purpose stream encoding. SUTF-32 is a lossless fixed-width form. SUTF-16 covers the full 31-bit space with word framing &mdash; 1 word up to <span class="mono">0x7FFF</span>, 2 words above &mdash; in explicit big-endian (canonical) or little-endian byte order (<span class="mono">sust16</span>). There are no surrogates: <span class="mono">0xD800</span>&ndash;<span class="mono">0xDFFF</span> are ordinary valid PUA codepoints.' }
        @{ t = 'p'; html = '<strong>What about codepoints above 0x7FFFFFFF?</strong>' }
        @{ t = 'p'; html = 'They cannot be framed by Base SUTF. Use the Extended transports &mdash; vector, vsutf or e-SUST &mdash; over the 64-bit <span class="mono">sucs_ex_char_t</span>. See <a href="../extended/transport.html">transport &raquo;</a>' }
        @{ t = 'p'; html = 'The framing contracts are <a href="../reports/SUTR-1.html">SUTR-1</a> and <a href="../reports/SUTR-4.html">SUTR-4</a>.' }
    )
}

$Pages['faq/storage'] = @{
    path    = 'faq/storage.html'
    sec     = 'faq'
    title   = 'FAQ — Storage'
    desc    = 'OWFS, USFS, partitions and the checksum gate.'
    crumbName = 'Storage'
    crumbs  = @( @{ label = 'FAQ'; href = 'index.html' } )
    h1      = 'FAQ &mdash; <span class="grad">Storage</span>'
    body    = @(
        @{ t = 'p'; html = '<strong>What is OWFS?</strong>' }
        @{ t = 'p'; html = 'The OpenWindows native drive filesystem. Data begins at drive offset <span class="mono">0x10000</span>, integrity is CRC32c + Fletcher-64, and identity/encryption come from the <span class="mono">ow_sec</span> layer (ChaCha20 data-at-rest).' }
        @{ t = 'p'; html = '<strong>What is USFS?</strong>' }
        @{ t = 'p'; html = 'The portable external-media variant of the storage spec, for removable drives that must be readable outside OpenWindows.' }
        @{ t = 'p'; html = '<strong>Why is the checksum gate a boot-time thing?</strong>' }
        @{ t = 'p'; html = 'A plugin changes the active namespace; the kernel refuses to do that mid-flight. The gate runs at the earliest safe moment &mdash; early boot &mdash; so every decision about the namespace is made before user code runs.' }
        @{ t = 'p'; html = 'The storage contract is <a href="../reports/SUTR-3.html">SUTR-3 &mdash; SuperUnicode Storage</a>.' }
    )
}
