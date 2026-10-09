# Machine-Instruction Database (SUCD) section

$Pages['ucd/index'] = @{
    path    = 'ucd/index.html'
    sec     = 'ucd'
    title   = 'Machine-Instruction Database — SUCD'
    desc    = 'The SuperUnicode Data Collection: core inventory, properties, blocks, naming, and the /Public tree.'
    crumbName = 'Machine-Instruction Database'
    h1      = 'The Machine-Instruction <span class="grad">Database</span>'
    subtitle= 'SUCD 0.1.0 &mdash; the authoritative data behind Base SUCS and ExtSUCS.'
    body    = @(
        @{ t = 'p'; html = 'The <strong>SuperUnicode Data Collection (SUCD)</strong> plays the role the Unicode Character Database plays for Unicode: machine-readable files that define every codepoint, block, property, name and mapping. It is versioned exactly like <span class="mono">unicode.org/Public</span>.' }
        @{ t = 'spec'; html = '/Public/0.1.0/<br>  sucd/          <span class="hl">core inventory</span>: SUCSData, Blocks, Props, Derived, auxiliary<br>  sudat/         <span class="hl">unidata</span>: assignments, control/, bancodes/<br>  names/         NamesList, NameAliases<br>  sources/       provenance<br>  collation/     SUCA ordering<br>  sutf/          SUTF8/16/32 framing tables<br>  charts/        code charts<br>  mappings/      UNICODE, LEGACY, OBSOLETE<br>  partitions/    partition policy (OWFS or USFS)' }
        @{ t = 'h2'; html = 'Explore the database' }
        @{ t = 'grid'; cards = @(
            @{ title = 'Properties'; href = 'ucd/properties.html'; html = 'Printable, control, trap and diagnostic properties, plus the derived tables.' }
            @{ title = 'Blocks'; href = 'ucd/blocks.html'; html = 'The named runs of the Base space, from the Unicode Bridge to the Sentinel.' }
            @{ title = 'Naming'; href = 'ucd/naming.html'; html = 'NamesList, name aliases, and how names are assigned and stabilized.' }
            @{ title = 'The /Public tree'; href = 'ucd/public.html'; html = 'Versioned releases, zipped archives, drafts and security attestations.' }
        ) }
        @{ t = 'h2'; html = 'Zipped archives' }
        @{ t = 'table'; head = @('Archive', 'Contents'); rows = @(
            @('<span class="mono">SUCD.zip</span>', 'the whole 0.1.0 release')
            @('<span class="mono">sucd.zip</span>', 'core inventory (the UCD.zip analog)')
            @('<span class="mono">sudat.zip</span>', 'unidata: SCP, Traps, Sentinel, bancodes (the UNIDATA.zip analog)')
            @('<span class="mono">names / sources / collation / sutf / mappings / charts / partitions .zip</span>', 'individual components')
            @('<span class="mono">ExtSUCD.zip / extsucd.zip / Plugin-SDK.zip / transport.zip</span>', 'Extended-tree archives')
        ) }
        @{ t = 'h2'; html = 'Browse the data' }
        @{ t = 'p'; html = 'The full, versioned trees live in the repositories and are listed live:' }
        @{ t = 'ul'; items = @(
            '<a href="../modules/unicode/index.html">modules/unicode</a> &mdash; the Base SUCS module (core, <span class="mono">Public/</span>)'
            '<a href="../modules/unicode-extended/index.html">modules/unicode-extended</a> &mdash; ExtSUCS + plugin subsystem + SDK'
            '<a href="../modules/sutf/index.html">modules/sutf</a> &mdash; SUCS UTF-8/16/4/2 transformation formats'
            '<a href="../modules/extsutf/index.html">modules/extsutf</a> &mdash; Extended transformation (vsutf, over 64-bit ExtSUCS)'
            '<a href="../modules/sust/index.html">modules/sust</a> &mdash; Serialization transports (SUST-16/32/64/128/256/512/N, e-SUST)'
            '<a href="../modules/unified/index.html">modules/unified</a> &mdash; the one-TU test that compiles every public header'
        ) }
        @{ t = 'callout'; html = '<strong>OpenWindows Storage.</strong> The filesystem layer behind the partition policy is specified by OpenWindows Storage (OWFS via <span class="mono">libowfs.a</span>, USFS via <span class="mono">libusfs.a</span>): OWFS data begins at drive offset <span class="mono">0x10000</span>, integrity is CRC32c + Fletcher-64, and the identity layer is <span class="mono">htl_device_t</span>/<span class="mono">ow_sec</span> with optional ChaCha20 data-at-rest encryption.' }
    )
}

$Pages['ucd/properties'] = @{
    path    = 'ucd/properties.html'
    sec     = 'ucd'
    title   = 'Properties — Machine-Instruction Database'
    desc    = 'Printable, control, trap and diagnostic properties, and the derived tables.'
    crumbName = 'Properties'
    crumbs  = @( @{ label = 'Machine-Instruction Database'; href = 'index.html' } )
    h1      = 'Properties'
    subtitle= 'Every codepoint&rsquo;s machine-readable attributes.'
    body    = @(
        @{ t = 'p'; html = 'Properties are stored in <span class="mono">Props/PropList.txt</span> and the derived tables under <span class="mono">Derived/</span>. The starter set for 0.1.0:' }
        @{ t = 'table'; head = @('Property', 'Scope', 'Meaning'); rows = @(
            @('<span class="mono">Unicode_Bridged</span>', '<span class="mono">0x000000&ndash;0x10FFFF</span>', 'Value is 1:1 with Unicode')
            @('<span class="mono">Control_Plane</span>', '<span class="mono">0x110000&ndash;0x11FFFF</span>', 'Member of the SCP')
            @('<span class="mono">Diagnostic_BAN / WARN / COM / SOFT</span>', 'BANcode registry blocks', 'Member of a diagnostic registry block')
            @('<span class="mono">Printable</span>', '<span class="mono">0x120000&ndash;0x7FFFFFEF</span>', 'Advances the cursor')
            @('<span class="mono">Trap_Instruction</span>', '<span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span>', 'Raises the stream into diagnostics')
            @('<span class="mono">Sentinel_Value</span>', '<span class="mono">0x7FFFFFFF</span>', 'End-of-stream marker')
        ) }
        @{ t = 'h2'; html = 'Derived properties' }
        @{ t = 'p'; html = 'Derived tables close the property set over rules, mirroring Unicode&rsquo;s DerivedCoreProperties. 0.1.0 derives <span class="mono">Alphabetic</span> over the compatibility and native spaces and <span class="mono">Control</span> over the SCP and traps.' }
        @{ t = 'note'; html = 'In the database: <span class="mono">sucd/Props/PropList.txt</span> and <span class="mono">sucd/Derived/DerivedCoreProperties.txt</span>.' }
    )
}

$Pages['ucd/blocks'] = @{
    path    = 'ucd/blocks.html'
    sec     = 'ucd'
    title   = 'Blocks — Machine-Instruction Database'
    desc    = 'The named runs of the Base space, from the Unicode Bridge to the Sentinel.'
    crumbName = 'Blocks'
    crumbs  = @( @{ label = 'Machine-Instruction Database'; href = 'index.html' } )
    h1      = 'Blocks'
    subtitle= 'The named runs of the Base space.'
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
        @{ t = 'note'; html = 'In the database: <span class="mono">sucd/Blocks.txt</span>. Plugin blocks are declared per plugin and recorded in the ExtSUCS registry.' }
    )
}

$Pages['ucd/naming'] = @{
    path    = 'ucd/naming.html'
    sec     = 'ucd'
    title   = 'Naming — Machine-Instruction Database'
    desc    = 'NamesList, name aliases, and how names are assigned and stabilized.'
    crumbName = 'Naming'
    crumbs  = @( @{ label = 'Machine-Instruction Database'; href = 'index.html' } )
    h1      = 'Naming'
    subtitle= 'How codepoints get their names, and why those names never change.'
    body    = @(
        @{ t = 'p'; html = 'Names live in <span class="mono">names/NamesList.txt</span>, with alternates in <span class="mono">names/NameAliases.txt</span>. In 0.1.0 the registry blocks and the Sentinel carry names; per-codepoint names arrive with native allocations in 0.2.0.' }
        @{ t = 'table'; head = @('Name', 'Codepoint'); rows = @(
            @('<span class="mono">BLOCK BANCODE</span>', '<span class="mono">0x0011A000</span>')
            @('<span class="mono">BLOCK WARNCODE</span>', '<span class="mono">0x0011A800</span>')
            @('<span class="mono">BLOCK COMCODE</span>', '<span class="mono">0x0011AC00</span>')
            @('<span class="mono">BLOCK SOFTCODE</span>', '<span class="mono">0x0011AE00</span>')
            @('<span class="mono">BLOCK TRAPS</span>', '<span class="mono">0x7FFFFFF0</span>')
            @('<span class="mono">SENTINEL</span>', '<span class="mono">0x7FFFFFFF</span>')
        ) }
        @{ t = 'h2'; html = 'Stability' }
        @{ t = 'ul'; items = @(
            'Names, once assigned, are permanent and unique within the space.'
            'Names are stable across versions: a codepoint never changes name, and a name is never reused.'
            'Aliases may be added but never removed.'
        ) }
    )
}

$Pages['ucd/public'] = @{
    path    = 'ucd/public.html'
    sec     = 'ucd'
    title   = 'The /Public tree — Machine-Instruction Database'
    desc    = 'Versioned releases, zipped archives, drafts and security attestations under /Public.'
    crumbName = 'The /Public tree'
    crumbs  = @( @{ label = 'Machine-Instruction Database'; href = 'index.html' } )
    h1      = 'The <span class="grad">/Public</span> tree'
    subtitle= 'Versioned machine-instruction data, mirroring unicode.org/Public.'
    body    = @(
        @{ t = 'p'; html = 'Data is published under a case-sensitive <span class="mono">/Public</span> layout, one directory per release, exactly as unicode.org does. The Base tree ships <span class="mono">SUCD.zip</span> and per-component archives; the Extended tree ships <span class="mono">ExtSUCD.zip</span>, the <span class="mono">extsucd.zip</span> registry package and <span class="mono">Plugin-SDK.zip</span>.' }
        @{ t = 'table'; head = @('Directory', 'Purpose'); rows = @(
            @('<span class="mono">0.1.0/</span>', 'Versioned release data (sucd, sudat, names, sources, collation, sutf, charts, mappings, partitions)')
            @('<span class="mono">latest/</span>', 'Pointer to the current release')
            @('<span class="mono">drafts/</span>', 'Working drafts for the next release')
            @('<span class="mono">zipped/</span>', 'Download-ready archives (SUCD.zip, component zips, ExtSUCD.zip, Plugin-SDK.zip)')
            @('<span class="mono">programs/</span>', 'Official tooling (plugin_pack, plugin_verify, inspectors)')
            @('<span class="mono">security/</span>', 'CRC32c + Fletcher-64 attestations for every published release')
        ) }
        @{ t = 'callout'; html = 'Browse live: <a href="../modules/unicode/index.html">modules/unicode</a> (Base) and <a href="../modules/unicode-extended/index.html">modules/unicode-extended</a> (Extended).' }
    )
}
