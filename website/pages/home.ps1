# Home pages (home.unicode.org + unicode.org/main.html mirrors)

$Pages['index'] = @{
    path    = 'index.html'
    sec     = 'home'
    layout  = 'hero'
    title   = 'SuperUnicode — Native Character Encoding of the OpenWindows Kernel'
    desc    = 'SuperUnicode is the native character encoding of the OpenWindows kernel: Unicode-compatible, plugin-extensible, storage-native.'
    body    = @(
        @{ t = 'hero'; h1 = 'One character system for the<br><span class="grad">OpenWindows kernel.</span>'
           lede = 'SuperUnicode is the <strong>native character encoding</strong> of OpenWindows. It is 1:1 compatible with Unicode, carries its own machine-instruction control plane, and grows without limit through plugins.'
           actions = @(
                @{ label = 'Explore the standard'; href = 'standard/index.html'; cls = 'btn-primary' }
                @{ label = 'Get SUCD 0.1.0';       href = 'ucd/index.html';       cls = 'btn-ghost' }
           ) }
    )
    afterHero = @(
        @{ t = 'h2'; html = 'What is SuperUnicode?' }
        @{ t = 'p';  html = 'SuperUnicode is the character encoding built into the OpenWindows kernel. Strings, filenames and data files are SuperUnicode streams by default &mdash; rendered directly by the kernel with no transcoding. It is organized into three spaces:' }
        @{ t = 'spaces'; items = @(
            @{ cls = 'u'; title = 'Unicode Compatibility Space'; range = '0x000000 &ndash; 0x10FFFF'; html = 'Identical to Unicode, codepoint-for-codepoint. If Unicode releases a new version or adds new codepoints, this space still matches &mdash; forever.' }
            @{ cls = 's'; title = 'System Control Plane'; range = '0x110000 &ndash; 0x11FFFF'; html = 'Where machine instructions live: the BANcode Registry for diagnostics, warnings, communication and soft-fault codes.' }
            @{ cls = 'g'; title = 'Native SuperUnicode Space'; range = '0x120000 &ndash; 0x7FFFFFFF'; html = 'The open field of OpenWindows&rsquo; own codepoints. Beyond it, the Extended system extends the space without any ceiling.' }
        ) }
        @{ t = 'h2'; html = 'Why SuperUnicode?' }
        @{ t = 'grid'; cards = @(
            @{ title = 'Native';       html = 'The kernel speaks one character system end-to-end. No code pages, no transcoding layers, no ambiguity.' }
            @{ title = 'Unicode-compatible'; html = 'The low range is a permanent 1:1 bridge. Every Unicode codepoint exists in SuperUnicode with the same value.' }
            @{ title = 'Instruction-grade';  html = 'Codepoints can be machine instructions. The SCP gives the kernel diagnostics, traps and control, not just text.' }
            @{ title = 'Unbounded';    html = 'ExtSUCS has no ceiling. Plugins declare ranges above <span class="mono">0x7FFFFFFF</span> and mount them securely at boot.' }
        ) }
        @{ t = 'callout'; cls = 'orange'; html = '<strong>SuperUnicode Extended</strong> extends the default runtime ceiling of <span class="mono">0x7FFFFFFF</span> with plugin-provided codepoints. Every plugin is verified by a checksum gate, mounted read-only as a SuperUnicode Plugin Partition (OWFS), and registered in the ExtSUCS registry. <a href="extended/index.html">Learn about Extended &raquo;</a>' }
        @{ t = 'h2'; html = 'Explore' }
        @{ t = 'grid'; cards = @(
            @{ title = 'The Standard';      href = 'standard/index.html'; html = 'Hierarchy, spaces, control plane, instructions.' }
            @{ title = 'Extended';          href = 'extended/index.html'; html = 'Plugins, SDK, registry, partitions, transport.' }
            @{ title = 'Machine-Instruction Database'; href = 'ucd/index.html'; html = 'SUCD, properties, blocks, /Public data files.' }
            @{ title = 'Technical Reports'; href = 'reports/index.html'; html = 'The SUTR-0 through SUTR-6 specifications.' }
            @{ title = 'Versions';          href = 'versions/index.html'; html = '0.1.0 release notes and roadmap.' }
            @{ title = 'Charts';            href = 'charts/index.html'; html = 'Block maps for Base and Extended spaces.' }
            @{ title = 'FAQ';               href = 'faq/index.html'; html = 'Compatibility, plugins, storage, encoding.' }
            @{ title = 'Glossary';          href = 'glossary.html'; html = 'Codepoint, plane, territory and every term between.' }
        ) }
        @{ t = 'h2'; html = 'News' }
        @{ t = 'ul'; items = @(
            '<strong>2026-08-14</strong> &mdash; The full multi-page website launches, mirroring the structure of unicode.org.'
            '<strong>2026-08-14</strong> &mdash; SuperUnicode 0.1.0 released: Base and Extended /Public trees, plugin subsystem with boot-time checksum gate, OWFS-only plugin partitions, SDK with template and hellocp example.'
            '<strong>Coming in 0.2.0</strong> &mdash; first native allocations, SCP registry assignments and additional SUTR reports.'
        ) }
    )
}

$Pages['main'] = @{
    path    = 'main.html'
    sec     = 'main'
    title   = 'SuperUnicode — The Machine-Instruction Standard'
    desc    = 'SuperUnicode is the native character encoding of the OpenWindows kernel. A 31-bit Base character system compatible with Unicode, a System Control Plane, and an unbounded plugin-extensible Extended space.'
    crumbName = 'Main'
    h1      = 'The <span class="grad">SuperUnicode</span>&reg; Standard'
    subtitle= 'Version 0.1.0 &mdash; the native character encoding of the <strong>OpenWindows</strong> kernel.'
    body    = @(
        @{ t = 'p'; html = '<strong>SuperUnicode</strong> defines the character system, machine instructions and storage framing that the OpenWindows kernel speaks natively. It is organized as two complementary systems:' }
        @{ t = 'ul'; items = @(
            '<strong>Base SUCS</strong> &mdash; a 31-bit space (<span class="mono">0x00000000&ndash;0x7FFFFFFF</span>) that is 1:1 compatible with Unicode in its low range, carries its own <em>System Control Plane</em> of machine instructions, and reserves a vast native space for OpenWindows codepoints.'
            '<strong>ExtSUCS (SuperUnicode Extended)</strong> &mdash; an unbounded, 64-bit space beyond the Base ceiling. New codepoints are added by <em>plugins</em> that mount as read-only OWFS partitions after a boot-time checksum gate.'
        ) }
        @{ t = 'callout'; html = '<strong>What does &ldquo;native&rdquo; mean?</strong> Every string, filename and data file in OpenWindows is a SuperUnicode stream by default. The kernel renders it directly &mdash; no transcoding step, no code-page table. Storage (OWFS/USFS), framing (SUTF) and diagnostics (the SCP) are all defined by the same standard.' }
        @{ t = 'h2'; html = 'Explore the standard' }
        @{ t = 'grid'; cards = @(
            @{ title = 'The SuperUnicode Standard'; href = 'standard/index.html'; html = 'Codepoints, blocks, ranges, planes, districts, zones and territories &mdash; and the three spaces that make up Base SUCS.' }
            @{ title = 'SuperUnicode Extended'; href = 'extended/index.html'; html = 'ExtSUCS, the plugin lifecycle, plugin partitions, the registry and the plugin SDK.' }
            @{ title = 'Machine-Instruction Database (SUCD)'; href = 'ucd/index.html'; html = 'The character database: core inventory, SCP, Traps, Sentinel, names, collation, mappings and the /Public tree.' }
            @{ title = 'Technical Reports'; href = 'reports/index.html'; html = 'SUTR specifications: framing, collation, storage, transport and the Unicode Compatibility Bridge.' }
            @{ title = 'Versions'; href = 'versions/index.html'; html = 'Version 0.1.0 release notes and the roadmap to 0.2.0.' }
            @{ title = 'Data Files (/Public)'; href = 'modules/unicode/index.html'; html = 'Versioned machine-instruction data, zipped archives, drafts and security attestations.' }
            @{ title = 'Code Charts'; href = 'charts/index.html'; html = 'Block maps and printable charts for the Base and Extended spaces.' }
            @{ title = 'FAQ'; href = 'faq/index.html'; html = 'Common questions about compatibility, scope and the relationship to Unicode.' }
        ) }
        @{ t = 'h2'; html = 'How the space is organized' }
        @{ t = 'spaces'; items = @(
            @{ cls = 'u'; title = 'Unicode Compatibility Space'; range = '0x000000 &ndash; 0x10FFFF'; html = '1:1 with Unicode, forever &mdash; even when Unicode publishes new versions or codepoints.' }
            @{ cls = 's'; title = 'System Control Plane'; range = '0x110000 &ndash; 0x11FFFF'; html = 'Machine instructions. Hosts the BANcode Registry (diagnostics, WARN/COM/SOFT codes).' }
            @{ cls = 'g'; title = 'Native SuperUnicode Space'; range = '0x120000 &ndash; 0x7FFFFFFF'; html = 'OpenWindows&rsquo; own allocations. Above it, plugins extend the space without limit.' }
        ) }
        @{ t = 'p'; html = '<a href="standard/index.html">Read the full hierarchy &raquo;</a>' }
        @{ t = 'h2'; html = 'News' }
        @{ t = 'ul'; items = @(
            '<strong>2026-08-14</strong> &mdash; SuperUnicode 0.1.0 released: Base and Extended /Public trees, plugin subsystem with boot-time checksum gate, OWFS-only plugin partitions, SDK with template and hellocp example, and this website.'
            '<strong>2026-08-14</strong> &mdash; The website goes live, mirroring the layout of unicode.org.'
            '<strong>Coming in 0.2.0</strong> &mdash; first native allocations, SCP registry assignments and additional SUTR reports.'
        ) }
    )
}
