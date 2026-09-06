# Diagnostics, Encoding, Consortium and site meta pages

# ---- Diagnostics ----------------------------------------------------------

$Pages['diagnostics/index'] = @{
    path    = 'diagnostics/index.html'
    sec     = 'standard'
    title   = 'Diagnostics'
    desc    = 'How SuperUnicode reports and dispatches machine diagnostics.'
    crumbName = 'Diagnostics'
    crumbs  = @( @{ label = 'The Standard'; href = 'standard/index.html' } )
    h1      = 'Diagnostics'
    subtitle= 'How SuperUnicode reports and dispatches machine diagnostics.'
    body    = @(
        @{ t = 'p'; html = 'Diagnostics are first-class citizens of the encoding: they are codepoints, they are typed, and they flow through the same stream as text. The SCP registry defines the codes; traps deliver them; the kernel acts. The registry runs in <strong>System mode</strong> (fatal codes dispatch through Kernel Security Traps, the krnl path) or <strong>App mode</strong> (fatal codes crash the application through its own handler, bypassing the krnl dispatch table) &mdash; both modes share the identical codepoint registry.' }
        @{ t = 'grid'; cards = @(
            @{ title = 'The BANcode Registry'; href = 'diagnostics/bancode.html'; html = 'BANcode, WARNcode, COMcode and SOFTcode blocks, plus the System/App operating modes.' }
            @{ title = 'Traps'; href = 'diagnostics/traps.html'; html = 'How a diagnostic code becomes an interrupt &mdash; the System-mode (krnl) dispatch path.' }
        ) }
        @{ t = 'note'; html = 'The formal contract is <span class="mono">SUTR-0</span> and the data in <span class="mono">sudat/control/</span>.' }
    )
}

$Pages['diagnostics/bancode'] = @{
    path    = 'diagnostics/bancode.html'
    sec     = 'standard'
    title   = 'The BANcode Registry'
    desc    = 'BANcode, WARNcode, COMcode and SOFTcode diagnostic blocks.'
    crumbName = 'The BANcode Registry'
    crumbs  = @( @{ label = 'Diagnostics'; href = 'index.html' } )
    h1      = 'The BANcode <span class="grad">Registry</span>'
    subtitle= 'Four diagnostic blocks in the System Control Plane.'
    body    = @(
        @{ t = 'table'; head = @('Block', 'Range', 'Severity', 'Behavior'); rows = @(
            @('<span class="mono">BANcode (B+)</span>', '<span class="mono">0x0011A000&ndash;0x0011A7FF</span>', 'hard', 'halt or roll back the stream')
            @('<span class="mono">WARNcode (W+)</span>', '<span class="mono">0x0011A800&ndash;0x0011ABFF</span>', 'soft', 'warn and continue')
            @('<span class="mono">COMcode (C+)</span>', '<span class="mono">0x0011AC00&ndash;0x0011ADFF</span>', 'informational', 'communication and datagrams')
            @('<span class="mono">SOFTcode (S+)</span>', '<span class="mono">0x0011AE00&ndash;0x0011AEFF</span>', 'runtime', 'soft-fault handling')
        ) }
        @{ t = 'p'; html = 'Registry slots are assigned in <span class="mono">sudat/assignments.txt</span>. In 0.1.0 all slots are unassigned, pending the 0.2.0 registry work; the blocks themselves are frozen and named.' }
        @{ t = 'h2'; html = 'System and App modes' }
        @{ t = 'p'; html = 'The registry runs in one of two operating modes. Both modes share the <strong>identical codepoint registry</strong> &mdash; BANcode, WARNcode, COMcode and SOFTcode occupy the same blocks and carry the same meaning. The mode only controls how a <strong>fatal BANcode (B+)</strong> is handled when it is dispatched:' }
        @{ t = 'table'; head = @('Mode', 'Fatal B+ handling'); rows = @(
            @('<span class="mono">System mode</span> (default)', 'Dispatches through the Kernel Security Traps (<span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span>). Each of the 15 trap slots governs a cluster of 128 B+ BANcodes; the kernel&rsquo;s damage-control handler for the cluster runs. This is the krnl dispatch path.')
            @('<span class="mono">App mode</span>', 'Bypasses kernel dispatch entirely. A fatal B+ BANcode crashes the application through a registered App-level crash handler, keeping the kernel trap machinery reserved for the kernel.')
        ) }
        @{ t = 'p'; html = 'The mode is selected at runtime (<span class="mono">bancode_set_mode()</span>, <span class="mono">bancode_get_mode()</span>) and defaults to System unless an app-mode build is requested at compile time (<span class="mono">BANCODE_DEFAULT_MODE</span>). App handlers are registered with <span class="mono">bancode_register_app_crash_handler()</span>.' }
        @{ t = 'callout'; html = 'System mode is for <strong>krnl</strong>: fatal BANcodes route to Kernel Security Traps. App mode is for <strong>application code</strong>: fatal BANcodes crash the app via its own handler &mdash; never touching the krnl trap table.' }
        @{ t = 'callout'; html = 'See also <a href="../standard/control-plane.html">the System Control Plane</a> and <a href="traps.html">traps</a>.' }
    )
}

$Pages['diagnostics/traps'] = @{
    path    = 'diagnostics/traps.html'
    sec     = 'standard'
    title   = 'Traps'
    desc    = 'How a diagnostic code becomes an interrupt at the top of the native space.'
    crumbName = 'Traps'
    crumbs  = @( @{ label = 'Diagnostics'; href = 'index.html' } )
    h1      = 'Traps'
    subtitle= 'Codepoints that raise the stream into the diagnostic subsystem.'
    body    = @(
        @{ t = 'p'; html = 'The trap range <span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span> sits at the top of the Native Space. Consuming a trap raises the current instruction stream into the diagnostic subsystem &mdash; the SuperUnicode equivalent of a hardware interrupt.' }
        @{ t = 'h2'; html = 'Dispatch mapping' }
        @{ t = 'p'; html = 'The registry functions (<span class="mono">sucs_bancode_to_trap()</span> and friends) map a registry codepoint to its trap. A hard BANcode halts or rolls back; a WARNcode warns and continues; the cursor behavior follows the instruction type.' }
        @{ t = 'spec'; html = 'BANcode  0x0011A0xx  &rarr;  trap 0x7FFFFFF0+  <span class="hl">// hard: halt/rollback</span><br>WARNcode 0x0011A8xx  &rarr;  warn and continue  <span class="hl">// cursor advances</span>' }
        @{ t = 'h2'; html = 'System vs. App mode' }
        @{ t = 'p'; html = 'Kernel Security Trap dispatch is the <strong>System mode</strong> path and is reserved for the krnl. Each of the 15 trap slots (<span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span>) governs a cluster of 128 B+ BANcodes, and the fitted damage-control handler runs in the crash context.' }
        @{ t = 'p'; html = 'In <strong>App mode</strong>, fatal BANcodes never reach the trap table. The application registers its own crash handler (<span class="mono">bancode_register_app_crash_handler()</span>), and a fatal B+ BANcode is delivered straight to it &mdash; the app crashes on its own terms, and the krnl dispatch machinery stays untouched.' }
        @{ t = 'note'; html = 'Terminal region data: <span class="mono">sudat/control/Traps.txt</span> and <span class="mono">Sentinel.txt</span>.' }
    )
}

# ---- Encoding -------------------------------------------------------------

$Pages['encoding/index'] = @{
    path    = 'encoding/index.html'
    sec     = 'standard'
    title   = 'Encoding'
    desc    = 'Framing and collation: SUTF and SUCA.'
    crumbName = 'Encoding'
    crumbs  = @( @{ label = 'The Standard'; href = 'standard/index.html' } )
    h1      = 'Encoding'
    subtitle= 'How SuperUnicode is framed, compared and normalized.'
    body    = @(
        @{ t = 'grid'; cards = @(
            @{ title = 'SUTF framing'; href = 'encoding/sutf.html'; html = 'SUCS UTF-8/16/32 and when to use each.' }
            @{ title = 'SUCA collation'; href = 'encoding/suca.html'; html = 'The ordering contract, default and tailored.' }
            @{ title = 'SGW width/grid'; href = 'encoding/sgw.html'; html = 'The monospace grid contract, width classes and zone dispatch.' }
            @{ title = 'SBR line breaking'; href = 'encoding/sbr.html'; html = 'System boundary &amp; line break rules over the 64-bit SUCS space.' }
            @{ title = 'SUCF canonical forms'; href = 'encoding/scf.html'; html = 'Dual-target canonical (de)composition over the 64-bit SUCS space.' }
        ) }
    )
}

$Pages['encoding/sutf'] = @{
    path    = 'encoding/sutf.html'
    sec     = 'standard'
    title   = 'SUTF framing'
    desc    = 'SUCS UTF-8/16/32 framing.'
    crumbName = 'SUTF framing'
    crumbs  = @( @{ label = 'Encoding'; href = 'index.html' } )
    h1      = 'SUTF <span class="grad">framing</span>'
    subtitle= 'SUCS UTF-8, UTF-16 and UTF-32.'
    body    = @(
        @{ t = 'table'; head = @('Encoding', 'Unit', 'Covers', 'Notes'); rows = @(
            @('<span class="mono">SUTF-8</span>', '1&ndash;4 bytes', 'Full 31-bit space', 'Default stream encoding; standard UTF-8 leading-byte framing')
            @('<span class="mono">SUTF-16</span>', '2 or 4 bytes', 'Full 31-bit space', 'Word framing with a marker bit (1 word to <span class="mono">0x7FFF</span>, 2 words above); no surrogates &mdash; <span class="mono">0xD800</span>&ndash;<span class="mono">0xDFFF</span> are valid PUA. Byte order explicit: BE canonical (<span class="mono">sust16</span>) or LE')
            @('<span class="mono">SUTF-32</span>', '4 bytes (LE)', 'Full 31-bit space', 'Lossless fixed-width form for native allocations')
        ) }
        @{ t = 'p'; html = 'Reference implementations live in <a href="../modules/sutf/index.html">modules/sutf</a> (framing) and <a href="../modules/sust/index.html">modules/sust</a> (byte serialization, BE canonical/LE); the framing tables are published at <span class="mono">Public/0.1.0/sutf/</span>. Extended codepoints use <a href="../extended/transport.html">vector, vsutf and e-SUST</a>.' }
    )
}

$Pages['encoding/suca'] = @{
    path    = 'encoding/suca.html'
    sec     = 'standard'
    title   = 'SUCA collation'
    desc    = 'The SuperUnicode Collation Algorithm (SUTS-001).'
    crumbName = 'SUCA collation'
    crumbs  = @( @{ label = 'Encoding'; href = 'index.html' } )
    h1      = 'SUCA <span class="grad">collation</span>'
    subtitle= 'The UCA-equivalent multilevel ordering contract for SuperUnicode strings.'
    body    = @(
        @{ t = 'p'; html = 'SUCA (SUTS-001) is a full Unicode-UTS-#10-equivalent multilevel collation algorithm over the 64-bit SuperUnicode space. Strings are NFD-normalized, mapped to collation elements (L1&ndash;L4 + identical), and compared level by level.' }
        @{ t = 'table'; head = @('Level', 'Weights', 'Example difference'); rows = @(
            @('<span class="mono">L1</span>', 'Base letters', '<span class="mono">role &lt; roles</span>')
            @('<span class="mono">L2</span>', 'Accents', '<span class="mono">role &lt; r&ocirc;le</span>')
            @('<span class="mono">L3</span>', 'Case', '<span class="mono">role &lt; Role</span>')
            @('<span class="mono">L4</span>', 'Variable (punctuation) interleaving', '<span class="mono">shifted: de-luge &asymp; deluge</span>')
            @('<span class="mono">Identical</span>', 'NFD codepoint tie-break (native codepoint order)', 'deterministic total order')
        ) }
        @{ t = 'ul'; items = @(
            '<strong>Canonical equivalence</strong> &mdash; normalized input collates equal (e.g. precomposed vs decomposed accents).'
            '<strong>Contractions</strong> &mdash; e.g. Slovak <span class="mono">c h</span> as a single base letter; <strong>expansions</strong> &mdash; e.g. <span class="mono">&#339; &asymp; oe</span>.'
            '<strong>Variable weighting</strong> &mdash; shifted (default), blanked, non-ignorable, shift-trimmed; SCP controls are collation-ignorable at L1&ndash;L3.'
            '<strong>Backward secondary</strong> &mdash; French dictionary order.'
            '<strong>Implicit weights</strong> &mdash; unassigned, Han, Native and ExtSUCS plugin codepoints derive algorithmic primaries from the 64-bit codepoint.'
            '<strong>Tailoring</strong> &mdash; programmatic rules (<span class="mono">&amp; base &lt; x</span>, <span class="mono">&lt;&lt;</span>, <span class="mono">&lt;&lt;&lt;</span>, <span class="mono">=</span>, contractions).'
        ) }
        @{ t = 'note'; html = 'Formalized in <a href="../reports/SUTR-2.html">SUTR-2</a> and <a href="../reports/SUTS-001.html">SUTS-001</a>. Data: <span class="mono">collation/SUCA.txt</span> (Base) and <span class="mono">collation/ExtUCA.txt</span> (Extended) &mdash; both extSUCS-compatible.' }
    )
}

$Pages['encoding/sgw'] = @{
    path    = 'encoding/sgw.html'
    sec     = 'standard'
    title   = 'SGW width/grid'
    desc    = 'The System Glyph Width & Monospace Grid (SUAS-002).'
    crumbName = 'SGW width/grid'
    crumbs  = @( @{ label = 'Encoding'; href = 'index.html' } )
    h1      = 'SGW <span class="grad">width/grid</span>'
    subtitle= 'The monospace grid contract, width classes and zone dispatch for terminal emulators.'
    body    = @(
        @{ t = 'p'; html = 'SGW (SUAS-002) is an East_Asian_Width-style (UAX #11) width classification over the full 64-bit SuperUnicode space (<span class="mono">sucs_ex_char_t</span>), folded into an O(1) monospace grid contract (0/1/2 cells + column cursor) for terminal emulators and CJK/ideographic framebuffer consoles.' }
        @{ t = 'table'; head = @('Width class', 'Grid cells', 'Examples'); rows = @(
            @('<span class="mono">F</span> (Fullwidth)', '2', 'CJK ideographs, fullwidth forms')
            @('<span class="mono">H</span> (Halfwidth)', '1', '<span class="mono">U+20A9 &#8361;</span>')
            @('<span class="mono">W</span> (Wide)', '2', 'Han ranges (incl. unassigned), Emoji_Presentation')
            @('<span class="mono">Na</span> (Narrow)', '1', '<span class="mono">U+00A5 &yen;</span>')
            @('<span class="mono">A</span> (Ambiguous)', '1 or 2 (context-dependent)', '<span class="mono">U+212B &#8491;</span>, <span class="mono">U+01D4</span>')
            @('<span class="mono">N</span> (Neutral)', '1', '<span class="mono">U+00C5 &Aring;</span>, <span class="mono">U+01D3</span>')
        ) }
        @{ t = 'ul'; items = @(
            '<strong>Zone dispatch</strong> &mdash; Unicode Bridge (curated EAW table), SCP (non-advancing, grid 0), Native SUCS (single cell), ExtSUCS (single cell), Trap/Sentinel (grid 0).'
            '<strong>Emoji_Presentation</strong> &mdash; mapped to Wide except Regional_Indicator (<span class="mono">U+1F1E6..1F1FF</span>).'
            '<strong>Tailoring</strong> &mdash; override hook for per-locale or per-terminal ambiguous resolution.'
            '<strong>Zero heap allocation</strong> &mdash; O(1) lookup, suitable for freestanding C99.'
        ) }
        @{ t = 'note'; html = 'Formalized in <a href="../reports/SUAS-002.html">SUAS-002</a>. Source of truth: <span class="mono">docs/suas/SUAS-002-sgw.md</span>.' }
    )
}

$Pages['encoding/sbr'] = @{
    path    = 'encoding/sbr.html'
    sec     = 'standard'
    title   = 'SBR line breaking'
    desc    = 'The System Boundary & Line Break Rules (SUAS-003).'
    crumbName = 'SBR line breaking'
    crumbs  = @( @{ label = 'Encoding'; href = 'index.html' } )
    h1      = 'SBR <span class="grad">line breaking</span>'
    subtitle= 'The single-pass deterministic line-break contract, break statuses and zone dispatch.'
    body    = @(
        @{ t = 'p'; html = 'SBR (SUAS-003) is a single-pass deterministic line-breaking engine over the full 64-bit SuperUnicode space (<span class="mono">sucs_ex_char_t</span>). Each adjacent pair resolves to a break status via a transition matrix &mdash; mandatory, allowed, prohibited, or the CJK/numeric alphanumeric break &mdash; in constant time, with zero allocation.' }
        @{ t = 'table'; head = @('Status', 'Meaning'); rows = @(
            @('<span class="mono">MUST_BREAK</span>', 'Mandatory line boundary (e.g. after a control).')
            @('<span class="mono">CAN_BREAK</span>', 'Allowed / opportunistic boundary.')
            @('<span class="mono">NO_BREAK</span>', 'Prohibited boundary.')
            @('<span class="mono">ALPHANUM_BREAK</span>', 'Mandatory CJK/numeric alphabetic break.')
        ) }
        @{ t = 'ul'; items = @(
            '<strong>Zone dispatch</strong> &mdash; Unicode Bridge (curated UAX #14 break classes + transition matrix), SCP (Explicit Break Markers), Native SUCS (O(1) high-bit range bitmask word test), ExtSUCS (neutral gap).'
            '<strong>Explicit SCP Break Markers</strong> &mdash; <span class="mono">0x00110020 MANDATORY</span>, <span class="mono">0x00110021 PROHIBITED</span>, <span class="mono">0x00110022 OPPORTUNISTIC</span> &mdash; absolute overrides.'
            '<strong>Invisible formatting</strong> &mdash; <span class="mono">U+200B ZWSP</span> offers a break; <span class="mono">U+2060 WJ</span> prohibits on either side.'
            '<strong>Zero heap allocation</strong> &mdash; O(1) transition-matrix lookup, suitable for freestanding C99.'
        ) }
        @{ t = 'note'; html = 'Formalized in <a href="../reports/SUAS-003.html">SUAS-003</a>. Source of truth: <span class="mono">docs/suas/SUAS-003-sbr.md</span>.' }
    )
}

$Pages['encoding/scf'] = @{
    path    = 'encoding/scf.html'
    sec     = 'standard'
    title   = 'SUCF canonical forms'
    desc    = 'SuperUnicode Canonical Forms (SUCF): dual-target canonical (de)composition.'
    crumbName = 'SUCF canonical forms'
    crumbs  = @( @{ label = 'Encoding'; href = 'index.html' } )
    h1      = 'SUCF <span class="grad">canonical forms</span>'
    subtitle= 'Dual-target, zero-allocation canonical (de)composition over the full 64-bit SUCS space.'
    body    = @(
        @{ t = 'p'; html = 'SUCF (SUAS-004) is the canonical-equivalence engine of SuperUnicode: a single-pass, zero-allocation sliding-window (de)composition engine over <span class="mono">sucs_ex_char_t</span> (a UAX #15 analog). Within the Unicode Bridge it reproduces Unicode canonical equivalence; in the native space it makes canonical treatment of SCP, Trap, Sentinel and native codepoints explicit.' }
        @{ t = 'table'; head = @('Target', 'Behavior'); rows = @(
            @('<span class="mono">SUCF-C</span>', 'Canonical composition &mdash; the compact storage / equality form: decompose, reorder by CCC, recompose greedily.')
            @('<span class="mono">SUCF-D</span>', 'Canonical decomposition &mdash; the analysis / stripping / sorting form: decompose and reorder, never recompose.')
        ) }
        @{ t = 'ul'; items = @(
            '<strong>CCC reordering</strong> &mdash; marks sort ascending by Canonical Combining Class; swap iff <span class="mono">CCC_A &gt; CCC_B</span> and <span class="mono">CCC_B &ne; 0</span>; stable for equal classes; starters never move.'
            '<strong>Hangul</strong> &mdash; syllables <span class="mono">0xAC00&ndash;0xD7A3</span> decompose algorithmically (no table rows); SUCF-C composes L+V and LV+T arithmetically.'
            '<strong>Composition exclusions</strong> &mdash; singletons (e.g. <span class="mono">U+212B &Aring;</span>) and non-starter exclusions never recompose.'
            '<strong>SCP immunity</strong> &mdash; SCP <span class="mono">0x00110000&ndash;0x0011FFFF</span>, BANcodes, Trap and Sentinel pass through untouched and never disturb combining state.'
            '<strong>Stream-safe</strong> &mdash; 30 non-starters per run; <strong>quick check</strong> returns YES/NO/MAYBE; streaming equals bulk transform.'
        ) }
        @{ t = 'note'; html = 'Formalized in <a href="../reports/SUAS-004.html">SUAS-004</a>. Source of truth: <span class="mono">docs/suas/SUAS-004-sucf.md</span>.' }
    )
}

# ---- Consortium -----------------------------------------------------------

$Pages['consortium/index'] = @{
    path    = 'consortium/index.html'
    sec     = 'standard'
    title   = 'The SuperUnicode Consortium'
    desc    = 'The organization behind the standard, the database and the plugin registry.'
    crumbName = 'The Consortium'
    h1      = 'The <span class="grad">SuperUnicode</span> Consortium'
    subtitle= 'The organization behind the standard, the database and the plugin registry.'
    body    = @(
        @{ t = 'p'; html = 'The SuperUnicode Consortium maintains the standard, publishes the machine-instruction database, reviews plugin proposals and approves registry rows. Members set policy; the technical committees write the SUTR reports.' }
        @{ t = 'grid'; cards = @(
            @{ title = 'Membership'; href = 'consortium/membership.html'; html = 'Who participates, the tiers, and what members decide.' }
            @{ title = 'Workshops'; href = 'consortium/workshops.html'; html = 'Where native allocations and registry slots are proposed.' }
        ) }
    )
}

$Pages['consortium/membership'] = @{
    path    = 'consortium/membership.html'
    sec     = 'standard'
    title   = 'Membership'
    desc    = 'Who participates in the SuperUnicode Consortium and how.'
    crumbName = 'Membership'
    crumbs  = @( @{ label = 'The Consortium'; href = 'index.html' } )
    h1      = 'Membership'
    body    = @(
        @{ t = 'ul'; items = @(
            '<strong>Founding members</strong> &mdash; the OpenWindows project and its kernel maintainers; set the roadmap.'
            '<strong>Supporting members</strong> &mdash; platform and toolchain vendors; participate in committees.'
            '<strong>Individual members</strong> &mdash; contribute to reports, plugins and the database.'
        ) }
        @{ t = 'p'; html = 'Members vote on native-space allocation plans, SCP registry assignments and plugin approval. Every decision is recorded in the /Public trees under <span class="mono">security/</span> and the registry.' }
    )
}

$Pages['consortium/workshops'] = @{
    path    = 'consortium/workshops.html'
    sec     = 'standard'
    title   = 'Workshops'
    desc    = 'Where native allocations and registry slots are proposed and reviewed.'
    crumbName = 'Workshops'
    crumbs  = @( @{ label = 'The Consortium'; href = 'index.html' } )
    h1      = 'Workshops'
    body    = @(
        @{ t = 'p'; html = 'Registry slots and native allocations are proposed at Consortium workshops, reviewed against the invariants (position-fixed bridge, Sentinel protection, range uniqueness), and assigned in the next release. Proposals land first in <span class="mono">drafts/</span> and become normative with the release that assigns them.' }
    )
}

# ---- Site meta ------------------------------------------------------------

$Pages['news'] = @{
    path    = 'news.html'
    sec     = 'home'
    title   = 'News'
    desc    = 'News from SuperUnicode.'
    crumbName = 'News'
    h1      = 'News'
    body    = @(
        @{ t = 'ul'; items = @(
            '<strong>2026-08-14</strong> &mdash; The full multi-page website launches, mirroring the structure of unicode.org, with repository directory listings for every module.'
            '<strong>2026-08-14</strong> &mdash; SuperUnicode 0.1.0 released: Base and Extended /Public trees, plugin subsystem with boot-time checksum gate, OWFS-only plugin partitions, SDK with template and hellocp example.'
            '<strong>2026-08-14</strong> &mdash; The plugin pipeline is verified end-to-end: <span class="mono">plugin_pack</span> &rarr; <span class="mono">plugin_verify</span> passes the hellocp blob through the exact checksum gate the boot loader runs.'
            '<strong>2026-08-26</strong> &mdash; The SUST serialization module lands: SUTF-16 gains explicit big-endian (canonical) and little-endian byte-order transports (<span class="mono">sust16</span>), the SUST fixed-width and e-SUST IPC families complete the byte layer, and SUTF-16 is documented surrogate-free. Framing (<span class="mono">sutf</span>) and serialization (<span class="mono">sust</span>) are now separate modules.'
            '<strong>Coming in 0.2.0</strong> &mdash; first native allocations, SCP registry assignments, and additional SUTR reports.'
        ) }
    )
}

$Pages['about'] = @{
    path    = 'about.html'
    sec     = 'home'
    title   = 'About SuperUnicode'
    desc    = 'About SuperUnicode and the OpenWindows kernel.'
    crumbName = 'About'
    h1      = 'About SuperUnicode'
    body    = @(
        @{ t = 'p'; html = '<strong>SuperUnicode</strong> is the native character encoding of the <strong>OpenWindows kernel</strong>. It was designed because OpenWindows treats codepoints as machine instructions, not just glyphs: text, diagnostics, storage and transport are all one system, defined by one standard.' }
        @{ t = 'p'; html = 'The project ships three things: the <strong>standard</strong> (SUTR reports), the <strong>machine-instruction database</strong> (SUCD, versioned under /Public), and the <strong>reference implementation</strong> (framing, transport, collation and the plugin subsystem, all in this repository and open for inspection).' }
        @{ t = 'h2'; html = 'Guiding principles' }
        @{ t = 'ul'; items = @(
            'Unicode compatibility is a permanent guarantee, not a snapshot.'
            'Machine instructions are codepoints &mdash; one namespace, one set of rules.'
            'Plugins extend the space, never the other way around &mdash; and only through the checksum gate.'
            'Everything normative is data, and every release is attested.'
        ) }
    )
}

$Pages['copyright'] = @{
    path    = 'copyright.html'
    sec     = 'home'
    title   = 'Copyright &amp; licenses'
    desc    = 'Copyright and license terms for SuperUnicode material.'
    crumbName = 'Copyright'
    h1      = 'Copyright &amp; <span class="grad">licenses</span>'
    body    = @(
        @{ t = 'p'; html = 'SuperUnicode&reg; is a registered trademark. The machine-instruction database, the SUTR reports and the reference implementation are published for inspection and use under the project license terms.' }
        @{ t = 'ul'; items = @(
            'The standard text and the SUCD data files may be reproduced and redistributed, provided the SuperUnicode name and the release version are retained.'
            'The reference implementation is provided as-is; the checksum gate protects mount-time integrity, not data ownership.'
            'Unicode data incorporated by the Compatibility Bridge remains subject to the Unicode terms of use.'
        ) }
    )
}

$Pages['privacy'] = @{
    path    = 'privacy.html'
    sec     = 'home'
    title   = 'Privacy'
    desc    = 'Privacy statement for the SuperUnicode website.'
    crumbName = 'Privacy'
    h1      = 'Privacy'
    body    = @(
        @{ t = 'p'; html = 'This site is a static snapshot. It sets no cookies, runs no analytics and performs no tracking. The fonts are served from Google Fonts, which may log standard request metadata in line with its own policy. The repository itself is public by design.' }
    )
}

$Pages['glossary'] = @{
    path    = 'glossary.html'
    sec     = 'home'
    title   = 'Glossary'
    desc    = 'The glossary of SuperUnicode terms.'
    crumbName = 'Glossary'
    h1      = 'Glossary'
    subtitle= 'Every term, defined in one place.'
    body    = @(
        @{ t = 'table'; head = @('Term', 'Definition'); rows = @(
            @('<strong>Codepoint</strong>', 'The atomic unit of SuperUnicode; a 31-bit <span class="mono">SUCS_CP</span> value in Base, or a 64-bit <span class="mono">sucs_ex_char_t</span> in Extended.')
            @('<strong>Block</strong>', 'A named run of codepoints, recorded in <span class="mono">Blocks.txt</span>.')
            @('<strong>Range</strong>', 'A declared contiguous allocation, e.g. a plugin&rsquo;s codepoint range.')
            @('<strong>Plane</strong>', '64,536 codepoints; plane = <span class="mono">CP &gt;&gt; 16</span>.')
            @('<strong>District</strong>', '16 planes (1 MiB); district = <span class="mono">CP &gt;&gt; 20</span>.')
            @('<strong>Zone</strong>', '16 districts (16 MiB); zone = <span class="mono">CP &gt;&gt; 24</span>.')
            @('<strong>Territory</strong>', '16 zones (256 MiB); territory = <span class="mono">CP &gt;&gt; 28</span>.')
            @('<strong>Unicode Compatibility Space</strong>', '<span class="mono">0x000000&ndash;0x10FFFF</span>, permanently 1:1 with Unicode.')
            @('<strong>System Control Plane (SCP)</strong>', '<span class="mono">0x110000&ndash;0x11FFFF</span>; machine instructions, host of the BANcode Registry.')
            @('<strong>BANcode Registry</strong>', 'The BANcode, WARNcode, COMcode and SOFTcode diagnostic blocks.')
            @('<strong>Trap</strong>', '<span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span>; raises the stream into diagnostics.')
            @('<strong>Sentinel</strong>', '<span class="mono">0x7FFFFFFF</span>; end-of-stream marker, never an allocation.')
            @('<strong>SUCD</strong>', 'The SuperUnicode machine-instruction database, versioned under /Public.')
            @('<strong>SUTR</strong>', 'A SuperUnicode Technical Report (SUTR-0 through SUTR-6).')
            @('<strong>ExtSUCS</strong>', 'SuperUnicode Extended; the unbounded 64-bit space above the Base ceiling.')
            @('<strong>Base limit</strong>', '<span class="mono">0x7FFFFFFF</span>; the default runtime ceiling and the floor for plugin ranges.')
            @('<strong>Plugin</strong>', 'A package of codepoints above the base limit, mounted after the checksum gate.')
            @('<strong>Checksum gate</strong>', 'CRC32c + Fletcher-64 verification of a plugin blob at boot.')
            @('<strong>OWFS</strong>', 'The OpenWindows native drive filesystem (<span class="mono">libowfs.a</span>).')
            @('<strong>USFS</strong>', 'The portable external-media filesystem (<span class="mono">libusfs.a</span>).')
            @('<strong>SUTF</strong>', 'SUCS UTF-8/16/32 framing &mdash; the endian-neutral codepoint &harr; word-sequence transformations (see <a href="encoding/sutf.html">SUTF framing</a>).')
            @('<strong>SUST</strong>', 'SuperUnicode Serialization Transports &mdash; the byte-packing layer: SUST-16 (SUTF-16 words in explicit big-endian canonical or little-endian order, no surrogates), SUST-32/64/128/256/512/N fixed-width, and e-SUST page-mapped IPC (<a href="modules/sust/index.html">modules/sust</a>).')
            @('<strong>SUCA</strong>', 'The SuperUnicode Collation Algorithm.')
            @('<strong>SGW</strong>', 'The System Glyph Width &amp; Monospace Grid standard &mdash; East_Asian_Width-style width classification and the O(1) fixed-cell grid metric.')
            @('<strong>SBR</strong>', 'The System Boundary &amp; Line Break Rules standard &mdash; single-pass deterministic line breaking (a UAX #14 analog) over the full 64-bit SUCS space, with explicit SCP break markers.')
            @('<strong>SUCF</strong>', 'SuperUnicode Canonical Forms &mdash; the dual-target canonical (de)composition engine (a UAX #15 analog): SUCF-C composes for compact storage/equality, SUCF-D decomposes for analysis/sorting; zero allocation over the 64-bit space (see <a href="encoding/scf.html">SUCF canonical forms</a>).')
            @('<strong>Cursor</strong>', 'Position in the instruction stream; printable codepoints advance it, control codepoints do not.')
        ) }
    )
}
