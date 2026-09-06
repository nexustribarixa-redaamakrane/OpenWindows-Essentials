# SuperUnicode Extended section

$Pages['extended/index'] = @{
    path    = 'extended/index.html'
    sec     = 'extended'
    title   = 'SuperUnicode Extended — ExtSUCS'
    desc    = 'ExtSUCS: an unbounded 64-bit space beyond the Base ceiling, built from plugin partitions.'
    crumbName = 'SuperUnicode Extended'
    h1      = 'SuperUnicode <span class="grad">Extended</span>'
    subtitle= 'ExtSUCS &mdash; codepoint territory without a ceiling, added by plugins.'
    body    = @(
        @{ t = 'p'; html = 'Base SUCS ends at the Sentinel value <span class="mono">0x7FFFFFFF</span> &mdash; the default runtime ceiling on websites and ordinary devices. <strong>SuperUnicode Extended (ExtSUCS)</strong> removes the ceiling: its codepoints are held in a 64-bit <span class="mono">sucs_ex_char_t</span>, and new codepoints are contributed by <strong>plugins</strong>.' }
        @{ t = 'h2'; html = 'Explore Extended' }
        @{ t = 'grid'; cards = @(
            @{ title = 'Plugins'; href = 'extended/plugins.html'; html = 'The lifecycle: stage, reboot, checksum gate, mount, register, quarantine.' }
            @{ title = 'Partitions'; href = 'extended/partitions.html'; html = 'SuperUnicode Plugin Partitions: OWFS only, always read-only.' }
            @{ title = 'The registry'; href = 'extended/registry.html'; html = 'Approved plugins, their ranges and states.' }
            @{ title = 'The SDK'; href = 'extended/sdk.html'; html = 'Template, hellocp example, plugin_pack and plugin_verify.' }
            @{ title = 'Transport'; href = 'extended/transport.html'; html = 'Vector, vsutf and e-SUST framing over 64-bit codepoints.' }
        ) }
        @{ t = 'h2'; html = 'How a plugin works' }
        @{ t = 'ol'; items = @(
            '<strong>Stage.</strong> The plugin blob is written to the staging table. The kernel answers <span class="mono">SUCS_PLUGIN_REBOOT_REQUIRED</span> &mdash; a restart is mandatory.'
            '<strong>Commit on boot.</strong> At early boot the kernel runs the <strong>checksum gate</strong>: the blob&rsquo;s CRC32c and Fletcher-64 must match. A valid plugin mounts as a <strong>SuperUnicode Plugin Partition</strong> and its ranges register into ExtSUCS.'
            '<strong>Quarantine.</strong> A blob that fails any gate is never mounted and never registers. It is quarantined and dispatched through the trap/BANcode machinery.'
        ) }
        @{ t = 'spec'; html = 'stage install  -&gt;  <span class="pv">SUCS_PLUGIN_REBOOT_REQUIRED</span><br>boot           -&gt;  crc32c + fletcher64 verify<br>             -&gt;  mount <span class="hl">SuperUnicode Plugin Partition</span> (OWFS, read-only)<br>             -&gt;  register ranges into ExtSUCS<br>gate failed    -&gt;  <span class="hl">quarantine</span> &mdash; never mounted' }
        @{ t = 'callout'; html = '<strong>Base and Extended are one system.</strong> ExtSUCS inherits the SCP, traps and Sentinel from Base SUCS verbatim. Base partitions may be OWFS or USFS; plugin partitions are always OWFS, always read-only, always checksum-gated. <a href="../standard/index.html">Back to the Base standard &raquo;</a>' }
    )
}

$Pages['extended/plugins'] = @{
    path    = 'extended/plugins.html'
    sec     = 'extended'
    title   = 'Plugins — SuperUnicode Extended'
    desc    = 'The plugin lifecycle: stage, reboot, checksum gate, mount, register, quarantine.'
    crumbName = 'Plugins'
    crumbs  = @( @{ label = 'Extended'; href = 'index.html' } )
    h1      = 'Plugins'
    subtitle= 'Self-contained packages of codepoints above the Base ceiling.'
    body    = @(
        @{ t = 'p'; html = 'A plugin is a self-contained package of codepoints: it declares ranges (all strictly above <span class="mono">0x7FFFFFFF</span>), a name table, properties, mappings and optional collation. A plugin blob is a single packed file validated before it is ever mounted.' }
        @{ t = 'h2'; html = 'Lifecycle' }
        @{ t = 'ol'; items = @(
            '<strong>Install &amp; stage.</strong> The blob lands in the staging table. The API returns <span class="mono">SUCS_PLUGIN_REBOOT_REQUIRED</span> (status 13).'
            '<strong>Reboot.</strong> Mandatory &mdash; mounting changes the active namespace, exactly like switching a runtime mode.'
            '<strong>Boot commit.</strong> <span class="mono">plugin_commit_on_boot()</span> verifies CRC32c + Fletcher-64, then validates ranges (sorted, non-overlapping, above the base limit, no collisions with mounted plugins).'
            '<strong>Mount.</strong> A valid plugin mounts as a SuperUnicode Plugin Partition &mdash; OWFS exclusively, read-only.'
            '<strong>Register.</strong> The plugin&rsquo;s ranges enter the active ExtSUCS namespace and codepoint lookups can resolve them.'
            '<strong>Quarantine.</strong> Any failed gate quarantines the plugin; it is never mounted and never registers.'
        ) }
        @{ t = 'h2'; html = 'Blob format' }
        @{ t = 'p'; html = 'The blob header (94 bytes, packed) carries the magic <span class="mono">SUCS</span>, blob version, the plugin version (<span class="mono">major.minor.patch</span>), the plugin id (64 bytes), range count, blob size, and the two integrity checksums computed over the whole blob with the checksum fields zeroed.' }
        @{ t = 'spec'; html = 'magic &quot;SUCS&quot; | blob_version 1 | ver major.minor.patch | id[64]<br>| range_count | blob_size | <span class="hl">crc32c</span> | <span class="hl">fletcher64</span><br>then ranges (16-byte LE pairs), then the payload' }
        @{ t = 'callout'; html = 'Practical guide: <a href="sdk.html">the plugin SDK</a>, and <a href="../modules/unicode-extended/index.html">the plugin sources</a> in the repository.' }
    )
}

$Pages['extended/partitions'] = @{
    path    = 'extended/partitions.html'
    sec     = 'extended'
    title   = 'Partitions — SuperUnicode Extended'
    desc    = 'SuperUnicode Plugin Partitions: OWFS only, always read-only, mounted after the checksum gate.'
    crumbName = 'Partitions'
    crumbs  = @( @{ label = 'Extended'; href = 'index.html' } )
    h1      = 'Partitions'
    subtitle= 'Storage governed by the OpenWindows Storage specification.'
    body    = @(
        @{ t = 'p'; html = 'A SuperUnicode Partition is a removable filesystem whose layout is declared by SuperUnicode machine instructions. Two formats exist, from the OpenWindows Storage specification (<span class="mono">libowfs.a</span> and <span class="mono">libusfs.a</span>):' }
        @{ t = 'table'; head = @('Format', 'Description'); rows = @(
            @('<span class="mono">OWFS</span>', 'Native drive filesystem. Data starts at drive offset <span class="mono">0x10000</span>; integrity is CRC32c + Fletcher-64; identity via <span class="mono">htl_device_t</span>/<span class="mono">ow_sec</span>; optional ChaCha20 data-at-rest encryption.')
            @('<span class="mono">USFS</span>', 'Portable external-media filesystem for removable drives.')
        ) }
        @{ t = 'h2'; html = 'Policy' }
        @{ t = 'table'; head = @('Partition type', 'Allowed formats', 'Used by', 'Mounted'); rows = @(
            @('SuperUnicode Partition', 'OWFS <em>or</em> USFS', 'Base SUCS &mdash; bugfix and rescue payloads only', 'read-only')
            @('SuperUnicode <strong>Plugin</strong> Partition', '<strong>OWFS exclusively</strong>', 'ExtSUCS plugins (Extended-only)', 'read-only, after the boot checksum gate')
        ) }
        @{ t = 'callout'; html = 'Plugin partitions are never USFS and never writable. Quarantined (gate-failed) plugins are never mounted and never register ranges. The full policy text is in both <span class="mono">Public</span> trees under <span class="mono">partitions/spec.txt</span>.' }
    )
}

$Pages['extended/registry'] = @{
    path    = 'extended/registry.html'
    sec     = 'extended'
    title   = 'The registry — SuperUnicode Extended'
    desc    = 'Approved plugins, their ranges and states, recorded in extsucd/registry.txt.'
    crumbName = 'The registry'
    crumbs  = @( @{ label = 'Extended'; href = 'index.html' } )
    h1      = 'The <span class="grad">registry</span>'
    subtitle= 'Every approved plugin, its ranges and its state.'
    body    = @(
        @{ t = 'p'; html = 'Approved plugins are recorded in <span class="mono">extsucd/registry.txt</span> inside the Extended /Public tree, and their full packages live under <span class="mono">extsucd/plugins/&lt;plugin-id&gt;/</span>. The registry is the authority for which plugin owns which ranges.' }
        @{ t = 'table'; head = @('Plugin id', 'Version', 'Ranges', 'Format', 'State'); rows = @(
            @('<span class="mono">org.openwindows.hellocp</span>', '1.0.0', '<span class="mono">0x80000000&ndash;0x80000FFF</span>', 'OWFS', 'approved')
        ) }
        @{ t = 'h2'; html = 'Registry rules' }
        @{ t = 'ul'; items = @(
            'Every plugin range is strictly above <span class="mono">0x7FFFFFFF</span>.'
            'Ranges may not overlap another plugin&rsquo;s ranges &mdash; collisions are rejected both offline (plugin_verify) and at boot.'
            'A plugin becomes authoritative for its ranges only after it mounts and registers.'
            'Registry rows mirror the plugin manifest; the checksums in <span class="mono">security/</span> attest each published blob.'
        ) }
        @{ t = 'note'; html = 'In the database: <span class="mono">extsucd/registry.txt</span> and <span class="mono">extsucd/plugins/org.openwindows.hellocp/</span>.' }
    )
}

$Pages['extended/sdk'] = @{
    path    = 'extended/sdk.html'
    sec     = 'extended'
    title   = 'The SDK — SuperUnicode Extended'
    desc    = 'The plugin authoring kit: template, hellocp example, plugin_pack and plugin_verify.'
    crumbName = 'The SDK'
    crumbs  = @( @{ label = 'Extended'; href = 'index.html' } )
    h1      = 'The plugin <span class="grad">SDK</span>'
    subtitle= 'Everything a plugin author needs, in one download.'
    body    = @(
        @{ t = 'p'; html = 'Plugin authors build against the loader ABI (<span class="mono">plugin.h</span>). The SDK ships a copy-this template, a full worked example, and offline tooling. It is distributed as <span class="mono">Plugin-SDK.zip</span> in the Extended /Public tree.' }
        @{ t = 'h2'; html = 'Authoring a plugin' }
        @{ t = 'ol'; items = @(
            'Copy <span class="mono">template/</span> to your plugin id.'
            'Fill in <span class="mono">manifest.txt</span> and <span class="mono">ranges.txt</span> (all ranges above <span class="mono">0x7FFFFFFF</span>).'
            'Edit <span class="mono">src/plugin_entry.c</span> &mdash; the ONE required export is <span class="mono">sucs_plugin_entry()</span>.'
            'Add codepoint tables in <span class="mono">plugin_data.c</span>, plus optional mappings and collation.'
            'Pack and verify offline.'
        ) }
        @{ t = 'spec'; html = 'plugin_pack org.openwindows.hellocp 1 0 0 ranges.txt payload.bin hellocp.sucsplugin<br>plugin_verify hellocp.sucsplugin   <span class="hl"># checksum gate: PASS/FAIL</span>' }
        @{ t = 'h2'; html = 'hellocp &mdash; the worked example' }
        @{ t = 'p'; html = '<span class="mono">hellocp</span> declares 4096 codepoints at <span class="mono">0x80000000&ndash;0x80000FFF</span>, ships a two-entry data table, packs and verifies clean. Its blob passes the exact gate the boot loader runs.' }
        @{ t = 'callout'; html = 'Download: <span class="mono">Plugin-SDK.zip</span> from the <a href="../ucd/public.html">/Public tree</a>. Sources: <a href="../modules/unicode-extended/index.html">modules/unicode-extended</a> &rarr; <span class="mono">plugin/sdk/</span>.' }
    )
}

$Pages['extended/transport'] = @{
    path    = 'extended/transport.html'
    sec     = 'extended'
    title   = 'Transport — SuperUnicode Extended'
    desc    = 'Vector, vsutf and e-SUST framing over 64-bit codepoints.'
    crumbName = 'Transport'
    crumbs  = @( @{ label = 'Extended'; href = 'index.html' } )
    h1      = 'Transport'
    subtitle= 'Framing for codepoints that exceed 31 bits.'
    body    = @(
        @{ t = 'p'; html = 'Because plugin codepoints exceed 31 bits, Extended defines its own transport families over <span class="mono">sucs_ex_char_t</span> (64-bit). Base SUTF8/16/32 cap at <span class="mono">0x7FFFFFFF</span>; these do not.' }
        @{ t = 'table'; head = @('Transport', 'Description', 'Reference'); rows = @(
            @('<span class="mono">Vector</span>', 'Length-prefixed sequences of 64-bit codepoints; one codepoint per unit, little-endian.', 'vsutf.h / vector layout')
            @('<span class="mono">vsutf</span>', 'Variable-length integer framing (LEB128-style) covering the full 64-bit space.', '<span class="mono">vsutf.h</span>')
            @('<span class="mono">e-SUST</span>', 'The fixed-base Extended family complementing Base SUTF8/16/32, plus page-mapped IPC framing.', '<span class="mono">esust.h</span> (SUST module)')
        ) }
        @{ t = 'h2'; html = 'Why separate framing?' }
        @{ t = 'ul'; items = @(
            'Base SUTF is optimized for 31-bit codepoints and the Unicode Compatibility Space.'
            'Plugin codepoints can be any value up to <span class="mono">2^64-1</span>; variable-length framing keeps common values small.'
            'The vector layout gives zero-cost random access at the cost of 8 bytes per unit &mdash; the right trade for in-memory streams.'
        ) }
        @{ t = 'note'; html = 'Specified in <a href="../reports/SUTR-4.html">SUTR-4 &mdash; ExtSUCS Transport</a>; data files under <span class="mono">Public/0.1.0/transport/</span>.' }
    )
}
