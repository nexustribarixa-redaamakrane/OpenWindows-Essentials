# The Standard section

$Pages['standard/index'] = @{
    path    = 'standard/index.html'
    sec     = 'standard'
    title   = 'The SuperUnicode Standard'
    desc    = 'The SuperUnicode Standard: codepoints, blocks, ranges, planes, districts, zones and territories, and the three spaces of Base SUCS.'
    crumbName = 'The Standard'
    h1      = 'The <span class="grad">SuperUnicode</span> Standard'
    subtitle= 'Base SUCS &mdash; a 31-bit character system of codepoints, machine instructions and a permanent Unicode bridge.'
    body    = @(
        @{ t = 'p'; html = 'The Base SuperUnicode Character System (SUCS) occupies the codepoint space <span class="mono">0x00000000&ndash;0x7FFFFFFF</span> (31 significant bits, held in a <span class="mono">uint32_t SUCS_CP</span>). Everything the standard defines is documented here and in the machine-instruction database.' }
        @{ t = 'h2'; html = 'The Standard, section by section' }
        @{ t = 'grid'; cards = @(
            @{ title = 'What is SuperUnicode?'; href = 'standard/what-is.html'; html = 'The one-paragraph answer, the origin in OpenWindows, and what &ldquo;native&rdquo; means.' }
            @{ title = 'The hierarchy'; href = 'standard/hierarchy.html'; html = 'Codepoints, blocks, ranges, planes, districts, zones and territories, with the addressing math.' }
            @{ title = 'The three spaces'; href = 'standard/spaces.html'; html = 'Unicode Compatibility Space, System Control Plane, Native Space &mdash; and why they never overlap.' }
            @{ title = 'The System Control Plane'; href = 'standard/control-plane.html'; html = 'Machine instructions, the BANcode Registry and how diagnostics are dispatched.' }
            @{ title = 'Machine instructions'; href = 'standard/instructions.html'; html = 'Printable vs. control codepoints, cursor advance, traps and the Sentinel.' }
            @{ title = 'Versioning'; href = 'standard/versioning.html'; html = 'How the standard, the database and plugins are versioned and stabilized.' }
            @{ title = 'Where is my character?'; href = 'standard/where-is.html'; html = 'How to find a codepoint: from a name, a range, a property or the charts.' }
            @{ title = 'Glossary'; href = 'glossary.html'; html = 'Every term used by the standard, defined in one place.' }
        ) }
        @{ t = 'h2'; html = 'Core invariants' }
        @{ t = 'ul'; items = @(
            '<span class="mono">SUCS_CP</span> is 31-bit; <span class="mono">sucs_ex_char_t</span> is 64-bit.'
            '<span class="mono">0x000000&ndash;0x10FFFF</span> is permanently 1:1 with Unicode.'
            '<span class="mono">0x7FFFFFFF</span> is the Sentinel; no allocation exists at or below it.'
            'Plugin codepoints start strictly above <span class="mono">0x7FFFFFFF</span> and mount only after the boot checksum gate.'
        ) }
        @{ t = 'callout'; html = 'See also: <a href="../ucd/index.html">the machine-instruction database</a> (the data behind these rules) and <a href="../reports/SUTR-0.html">SUTR-0 &mdash; SUCS Core</a> (the normative report).' }
    )
}

$Pages['standard/what-is'] = @{
    path    = 'standard/what-is.html'
    sec     = 'standard'
    title   = 'What is SuperUnicode?'
    desc    = 'SuperUnicode is the native character encoding of the OpenWindows kernel.'
    crumbName = 'What is SuperUnicode?'
    crumbs  = @( @{ label = 'The Standard'; href = 'index.html' } )
    h1      = 'What is <span class="grad">SuperUnicode</span>?'
    subtitle= 'The character system, the machine instructions and the storage framing of the OpenWindows kernel.'
    body    = @(
        @{ t = 'p'; html = '<strong>SuperUnicode</strong> is the native character encoding of the <strong>OpenWindows kernel</strong>. When OpenWindows reads a string, names a file or lays out a data file, it assumes SuperUnicode &mdash; no encoding negotiation, no code pages, no transcoding.' }
        @{ t = 'h2'; html = 'What it contains' }
        @{ t = 'ul'; items = @(
            '<strong>A character system.</strong> Codepoints in a 31-bit space, organized into a fixed hierarchy.'
            '<strong>A Unicode bridge.</strong> The low range is permanently 1:1 with Unicode.'
            '<strong>Machine instructions.</strong> Codepoints can be instructions that the kernel executes, not just glyphs.'
            '<strong>A database.</strong> Every codepoint, block, property, name and mapping is machine-readable (SUCD).'
            '<strong>Storage and transport.</strong> Framing (SUTF), serialization (SUST), storage (OWFS/USFS) and extended transport (vsutf/e-SUST).'
        ) }
        @{ t = 'h2'; html = 'Why &ldquo;native&rdquo;?' }
        @{ t = 'p'; html = 'An encoding is <em>native</em> when the system&rsquo;s lowest layers assume it. In OpenWindows the kernel itself reads and emits SuperUnicode streams: the compiler, the filesystem, the diagnostics and the boot loader all speak the same encoding, so nothing is ever reinterpreted.' }
        @{ t = 'callout'; html = 'Related: <a href="hierarchy.html">the hierarchy</a>, <a href="spaces.html">the three spaces</a>, and <a href="instructions.html">machine instructions</a>.' }
    )
}

$Pages['standard/hierarchy'] = @{
    path    = 'standard/hierarchy.html'
    sec     = 'standard'
    title   = 'The hierarchy — codepoints to territory'
    desc    = 'Codepoints, blocks, ranges, planes, districts, zones and territories, with the addressing math.'
    crumbName = 'The hierarchy'
    crumbs  = @( @{ label = 'The Standard'; href = 'index.html' } )
    h1      = 'The <span class="grad">hierarchy</span>'
    subtitle= 'Seven levels, from the atomic codepoint to the territory, all derived by bit-shifting.'
    body    = @(
        @{ t = 'pyramid'; items = @(
            @{ label = 'Territory &mdash; 16 zones (256 MiB)';  cls = 't1' }
            @{ label = 'Zone &mdash; 16 districts (16 MiB)';    cls = 't2' }
            @{ label = 'District &mdash; 16 planes (1 MiB)';   cls = 't3' }
            @{ label = 'Plane &mdash; 65,536 codepoints';      cls = 't4' }
            @{ label = 'Range &mdash; a declared allocation';  cls = 't5' }
            @{ label = 'Block &mdash; a named run';            cls = 't6' }
            @{ label = 'Codepoint &mdash; the atomic unit';    cls = 't7' }
        ) }
        @{ t = 'table'; head = @('Level', 'Size', 'Derived by', 'Notes'); rows = @(
            @('Codepoint', '1', '&mdash;', 'The atomic unit; e.g. <span class="mono">0x0011A000</span>')
            @('Block', 'variable', 'named run', 'Recorded in <span class="mono">Blocks.txt</span>')
            @('Range', 'variable', 'declared allocation', 'Plugins declare ranges; collisions are rejected at boot')
            @('Plane', '64 KiB', '<span class="mono">CP &gt;&gt; 16</span>', '8192 planes (0x00&ndash;0x7FF)')
            @('District', '16 planes', '<span class="mono">CP &gt;&gt; 20</span>', '128 districts (0x00&ndash;0x7F)')
            @('Zone', '16 districts', '<span class="mono">CP &gt;&gt; 24</span>', '8 zones (0x00&ndash;0x07)')
            @('Territory', '16 zones', '<span class="mono">CP &gt;&gt; 28</span>', '8 territories (0x00&ndash;0x07)')
        ) }
        @{ t = 'h2'; html = 'The math' }
        @{ t = 'p'; html = 'Address arithmetic makes the hierarchy total and cheap. Take the start of the BANcode registry, <span class="mono">0x0011A000</span>:' }
        @{ t = 'spec'; html = 'SUCS_CP 0x0011A000<br>plane     0x0011  <span class="hl">//</span> System Control Plane<br>district  0x0011 &gt;&gt; 4 = 0x01<br>zone      0x0011 &gt;&gt; 8 = 0x00<br>territory 0x0011 &gt;&gt; 12 = 0x00' }
        @{ t = 'callout'; html = 'Next: the three spaces built on this hierarchy &mdash; <a href="spaces.html">Unicode Compatibility Space, System Control Plane, Native Space &raquo;</a>' }
    )
}

$Pages['standard/spaces'] = @{
    path    = 'standard/spaces.html'
    sec     = 'standard'
    title   = 'The three spaces'
    desc    = 'Unicode Compatibility Space (0x000000-0x10FFFF), System Control Plane (0x110000-0x11FFFF), Native Space (0x120000-0x7FFFFFFF).'
    crumbName = 'The three spaces'
    crumbs  = @( @{ label = 'The Standard'; href = 'index.html' } )
    h1      = 'The <span class="grad">three spaces</span>'
    subtitle= 'Exactly one space owns every codepoint in Base SUCS. They never overlap.'
    body    = @(
        @{ t = 'spaces'; items = @(
            @{ cls = 'u'; title = 'Unicode Compatibility Space'; range = '0x000000 &ndash; 0x10FFFF'; html = 'Planes 0 and 1. Every value is 1:1 with the same-numeric Unicode codepoint <strong>permanently</strong>. If Unicode publishes a new version or assigns new codepoints, this space still matches &mdash; compatibility is a frozen guarantee, not a snapshot.' }
            @{ cls = 's'; title = 'System Control Plane'; range = '0x110000 &ndash; 0x11FFFF'; html = 'Plane 0x11. Non-printable codepoints are <strong>machine instructions</strong> that do not advance the cursor. Hosts the BANcode Registry.' }
            @{ cls = 'g'; title = 'Native SuperUnicode Space'; range = '0x120000 &ndash; 0x7FFFFFFF'; html = 'Planes 0x12&ndash;0x7FF. The open field for OpenWindows&rsquo; own codepoints, with Traps and the Sentinel reserved at the top.' }
        ) }
        @{ t = 'h2'; html = 'Why three?' }
        @{ t = 'ul'; items = @(
            '<strong>Compatibility space</strong> buys interop with the entire Unicode ecosystem at zero cost &mdash; a Unicode string is a SuperUnicode string.'
            '<strong>The control plane</strong> keeps diagnostics out of the text. Instructions can never be mistaken for characters.'
            '<strong>The native space</strong> leaves room for OpenWindows to allocate codepoints for centuries, addressable in a single 32-bit word.'
        ) }
        @{ t = 'h2'; html = 'Reserved region at the top' }
        @{ t = 'table'; head = @('Codepoint', 'Purpose'); rows = @(
            @('<span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span>', 'Traps &mdash; raise the instruction stream into diagnostics')
            @('<span class="mono">0x7FFFFFFF</span>', 'Sentinel &mdash; end-of-stream; not an allocation')
        ) }
        @{ t = 'callout'; html = 'Plugins never land here: their codepoints begin strictly above <span class="mono">0x7FFFFFFF</span>. See <a href="../extended/index.html">SuperUnicode Extended &raquo;</a>' }
    )
}

$Pages['standard/control-plane'] = @{
    path    = 'standard/control-plane.html'
    sec     = 'standard'
    title   = 'The System Control Plane (SCP)'
    desc    = 'Plane 0x11: machine instructions and the BANcode Registry (BANcode, WARNcode, COMcode, SOFTcode).'
    crumbName = 'The System Control Plane'
    crumbs  = @( @{ label = 'The Standard'; href = 'index.html' } )
    h1      = 'The System <span class="grad">Control Plane</span>'
    subtitle= 'Plane 0x11 &mdash; where codepoints are instructions.'
    body    = @(
        @{ t = 'p'; html = 'The System Control Plane (SCP) occupies <span class="mono">0x110000&ndash;0x11FFFF</span>. Its codepoints are <strong>machine instructions</strong>: they do not print and they do not advance the cursor. Every instruction in the kernel&rsquo;s diagnostic machinery lives here.' }
        @{ t = 'h2'; html = 'The BANcode Registry' }
        @{ t = 'table'; head = @('Block', 'Range', 'Role'); rows = @(
            @('BANcode (B+)', '<span class="mono">0x0011A000&ndash;0x0011A7FF</span>', 'Hard diagnostics &mdash; halt or rollback the stream')
            @('WARNcode (W+)', '<span class="mono">0x0011A800&ndash;0x0011ABFF</span>', 'Soft diagnostics &mdash; warn and continue')
            @('COMcode (C+)', '<span class="mono">0x0011AC00&ndash;0x0011ADFF</span>', 'Communication and datagram instructions')
            @('SOFTcode (S+)', '<span class="mono">0x0011AE00&ndash;0x0011AEFF</span>', 'Runtime / soft-fault instructions')
        ) }
        @{ t = 'p'; html = 'The remaining SCP slots (<span class="mono">0x110000&ndash;0x11A000</span> and <span class="mono">0x11AF00&ndash;0x11FFFF</span>) are reserved for future registry blocks, exactly as the control plane of a processor reserves opcode space.' }
        @{ t = 'h2'; html = 'Dispatch' }
        @{ t = 'p'; html = 'Diagnostics produced anywhere in the kernel are dispatched through the SCP. The registry functions <span class="mono">sucs_bancode_to_trap()</span> and friends map a registry codepoint to the corresponding trap so a hard BANcode halts the stream while a WARNcode lets it continue.' }
        @{ t = 'spec'; html = 'BANcode  0x0011A0xx  &rarr;  trap 0x7FFFFFF0+  <span class="hl">// hard: halt/rollback</span><br>WARNcode 0x0011A8xx  &rarr;  warn and continue  <span class="hl">// cursor advances</span>' }
        @{ t = 'h2'; html = 'System and App modes' }
        @{ t = 'p'; html = 'The dispatch path for a fatal B+ BANcode depends on the mode the registry runs in. Both modes share the identical codepoint registry.' }
        @{ t = 'ul'; items = @(
            '<strong>System mode</strong> &mdash; the krnl path. Fatal BANcodes route to the Kernel Security Trap governing their 128-codepoint cluster (<span class="mono">0x7FFFFFF0+slot</span>); the fitted damage-control handler executes in the crash context.'
            '<strong>App mode</strong> &mdash; application code. Fatal BANcodes bypass the traps entirely and are delivered to the App-level crash handler registered by the application (<span class="mono">bancode_register_app_crash_handler()</span>), so the krnl dispatch table is never entered.'
        ) }
        @{ t = 'callout'; html = 'Also see <a href="instructions.html">machine instructions</a>, <a href="../diagnostics/index.html">diagnostics</a>, <a href="../diagnostics/bancode.html">the BANcode Registry</a>, and <span class="mono">sudat/control/SCP.txt</span> in the database.' }
    )
}

$Pages['standard/instructions'] = @{
    path    = 'standard/instructions.html'
    sec     = 'standard'
    title   = 'Machine instructions'
    desc    = 'Printable vs. control codepoints, cursor advance, traps and the Sentinel.'
    crumbName = 'Machine instructions'
    crumbs  = @( @{ label = 'The Standard'; href = 'index.html' } )
    h1      = 'Machine <span class="grad">instructions</span>'
    subtitle= 'A codepoint can print a character, or it can run the kernel.'
    body    = @(
        @{ t = 'p'; html = 'Every codepoint in SUCSData carries a <em>type</em>. The type decides what the rendering engine does when the codepoint is consumed:' }
        @{ t = 'table'; head = @('Type', 'Behavior', 'Examples'); rows = @(
            @('<span class="mono">0</span> &mdash; printable native allocation', 'Advances the cursor; produces a glyph', 'Unicode Bridge, Native Space codepoints')
            @('<span class="mono">1</span> &mdash; control / machine instruction', 'Does not advance the cursor; invokes kernel machinery', 'SCP registry, Traps')
        ) }
        @{ t = 'h2'; html = 'Cursor advance' }
        @{ t = 'p'; html = 'The <em>cursor</em> is the position in the instruction stream. Printable codepoints consume one cell and advance. Control codepoints act on the machine &mdash; they may set mode, emit diagnostics, or change how the rest of the stream is interpreted &mdash; but they leave the cursor where it is, so a control code can prefix a printable codepoint without consuming a cell.' }
        @{ t = 'h2'; html = 'Traps and the Sentinel' }
        @{ t = 'ul'; items = @(
            '<strong>Traps</strong> (<span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span>) raise the current instruction stream into the diagnostic subsystem. A trap is the hardware-style &ldquo;interrupt&rdquo; of the SuperUnicode machine.'
            '<strong>The Sentinel</strong> (<span class="mono">0x7FFFFFFF</span>) marks end-of-stream. It stops cursor advance and terminates the stream. It is not an allocation and no plugin may claim it.'
        ) }
        @{ t = 'callout'; html = 'Diagnostics in depth: <a href="../diagnostics/index.html">the diagnostics section &raquo;</a>' }
    )
}

$Pages['standard/versioning'] = @{
    path    = 'standard/versioning.html'
    sec     = 'standard'
    title   = 'Versioning'
    desc    = 'How the standard, the database and plugins are versioned and stabilized.'
    crumbName = 'Versioning'
    crumbs  = @( @{ label = 'The Standard'; href = 'index.html' } )
    h1      = 'Versioning'
    subtitle= 'One version number for the standard and its database, plus per-plugin versions.'
    body    = @(
        @{ t = 'p'; html = 'SuperUnicode follows the Unicode versioning philosophy: a single version number identifies a coherent release of the standard, the database and the reference implementation. Version 0.1.0 is the first such release.' }
        @{ t = 'table'; head = @('Artifact', 'Versioning'); rows = @(
            @('The standard (SUTR-0..6)', 'Named by the database version; normative text is versioned with it')
            @('Machine-Instruction Database', '<span class="mono">SUCD &lt;n&gt;.&lt;m&gt;.&lt;p&gt;</span>, laid out under <span class="mono">/Public/&lt;version&gt;/</span>')
            @('Reference implementation', 'Same number as the database it implements')
            @('Plugins', '<span class="mono">major.minor.patch</span> carried in the plugin blob header')
        ) }
        @{ t = 'h2'; html = 'Stability promises' }
        @{ t = 'ul'; items = @(
            'The Unicode Compatibility Space never changes: codepoint meanings are frozen at release 0.1.0 and persist even when Unicode grows.'
            'Assigned codepoints are never re-used. The SCP registry only ever grows.'
            'Plugin blobs are self-describing: version, ranges and checksums live in the blob header, so loaders can reject incompatible versions.'
            'An ABI freeze (plugin entry, blob format, /Public schema) is targeted for 1.0.0.'
        ) }
        @{ t = 'p'; html = '<a href="../versions/index.html">Current and past versions &raquo;</a>' }
    )
}

$Pages['standard/where-is'] = @{
    path    = 'standard/where-is.html'
    sec     = 'standard'
    title   = 'Where is my character?'
    desc    = 'How to find a codepoint: from a name, a range, a property or the charts.'
    crumbName = 'Where is my character?'
    crumbs  = @( @{ label = 'The Standard'; href = 'index.html' } )
    h1      = 'Where is my <span class="grad">character</span>?'
    subtitle= 'Four ways to locate a codepoint.'
    body    = @(
        @{ t = 'h2'; html = '1. You know the name' }
        @{ t = 'p'; html = 'Look it up in the <span class="mono">NamesList.txt</span> from the database. In 0.1.0 the registry blocks are named; per-codepoint names arrive with native allocations in 0.2.0.' }
        @{ t = 'h2'; html = '2. You know the range or block' }
        @{ t = 'p'; html = 'Browse <span class="mono">Blocks.txt</span> or the <a href="../charts/index.html">charts</a>. Blocks are named runs; every block belongs to exactly one of the three spaces.' }
        @{ t = 'h2'; html = '3. You know a property' }
        @{ t = 'p'; html = 'Query <span class="mono">Props/PropList.txt</span> and the derived tables for properties such as <span class="mono">Printable</span>, <span class="mono">Trap_Instruction</span> or <span class="mono">Sentinel_Value</span>. See <a href="../ucd/properties.html">properties &raquo;</a>' }
        @{ t = 'h2'; html = '4. It is a plugin codepoint' }
        @{ t = 'p'; html = 'Codepoints above <span class="mono">0x7FFFFFFF</span> belong to plugins. Check the <a href="../extended/registry.html">ExtSUCS registry</a>, which maps plugins to their ranges, then the plugin&rsquo;s own names and properties tables.' }
        @{ t = 'callout'; html = 'Start searching: <a href="../ucd/index.html">the machine-instruction database &raquo;</a>' }
    )
}
