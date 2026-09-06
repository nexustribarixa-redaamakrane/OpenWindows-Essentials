# Technical Reports section

$Pages['reports/index'] = @{
    path    = 'reports/index.html'
    sec     = 'reports'
    title   = 'Technical Reports — SUTR'
    desc    = 'The SUTR-0 through SUTR-6 specifications of SuperUnicode.'
    crumbName = 'Technical Reports'
    h1      = 'Technical <span class="grad">Reports</span>'
    subtitle= 'The SUTR series &mdash; normative specifications, tracked per release.'
    body    = @(
        @{ t = 'p'; html = 'Technical Reports formalize each contract of the standard. They play the role of Unicode&rsquo;s UAX/UTR documents and are identified by the <span class="mono">SUTR-<em>n</em></span> scheme with per-version errata.' }
        @{ t = 'table'; head = @('Report', 'Status', 'Scope'); rows = @(
            @('<span class="mono">SUTS-001</span>', 'Draft &mdash; Ratified', '<a href="SUTS-001.html">SuperUnicode Collation Algorithm (SUCA)</a> &mdash; UCA-equivalent multilevel collation, 64-bit extSUCS compatible')
            @('<span class="mono">SUAS-001</span>', 'Draft &mdash; Ratified', '<a href="SUAS-001.html">Structural Directional Framing (SDF)</a> &mdash; core BiDi architecture, scope isolation and glyph mirroring')
            @('<span class="mono">SUAS-002</span>', 'Draft &mdash; Ratified', '<a href="SUAS-002.html">System Glyph Width &amp; Monospace Grid (SGW)</a> &mdash; fixed-cell terminal metrics, EAW-style width over 64-bit SUCS')
            @('<span class="mono">SUAS-003</span>', 'Draft &mdash; Ratified', '<a href="SUAS-003.html">System Boundary &amp; Line Break Rules (SBR)</a> &mdash; single-pass line breaking over 64-bit SUCS')
            @('<span class="mono">SUAS-004</span>', 'Draft &mdash; Ratified', '<a href="SUAS-004.html">SuperUnicode Canonical Forms (SUCF)</a> &mdash; dual-target canonical (de)composition, zero allocation')
            @('<span class="mono">SUTR-0</span>', '0.1.0', '<a href="SUTR-0.html">SUCS Core</a> &mdash; hierarchy, three-space layout, SCP, Traps, Sentinel')
            @('<span class="mono">SUTR-1</span>', '0.1.0', '<a href="SUTR-1.html">SUTF</a> &mdash; SUCS UTF-8/16/32 framing')
            @('<span class="mono">SUTR-2</span>', '0.1.0', '<a href="SUTR-2.html">SUCA</a> &mdash; SuperUnicode Collation Algorithm')
            @('<span class="mono">SUTR-3</span>', '0.1.0', '<a href="SUTR-3.html">SuperUnicode Storage</a> &mdash; OWFS/USFS partition policy')
            @('<span class="mono">SUTR-4</span>', '0.1.0', '<a href="SUTR-4.html">ExtSUCS Transport</a> &mdash; vector, vsutf, e-SUST')
            @('<span class="mono">SUTR-5</span>', '0.1.0', '<a href="SUTR-5.html">Plugin Lifecycle &amp; Blob Format</a> &mdash; stage, checksum gate, mount, registry')
            @('<span class="mono">SUTR-6</span>', '0.1.0', '<a href="SUTR-6.html">Unicode Compatibility Bridge</a> &mdash; the permanent 1:1 guarantee')
        ) }
        @{ t = 'h2'; html = 'Core invariants every report honors' }
        @{ t = 'ul'; items = @(
            '<span class="mono">SUCS_CP</span> is 31-bit; <span class="mono">sucs_ex_char_t</span> is 64-bit.'
            '<span class="mono">0x000000&ndash;0x10FFFF</span> is permanently 1:1 with Unicode.'
            '<span class="mono">0x7FFFFFFF</span> is the Sentinel; no allocation exists at or below it.'
            'Plugin codepoints start strictly above <span class="mono">0x7FFFFFFF</span> and mount only after the checksum gate.'
        ) }
    )
}

$Pages['reports/SUTR-0'] = @{
    path    = 'reports/SUTR-0.html'
    sec     = 'reports'
    title   = 'SUTR-0 — SUCS Core'
    desc    = 'SUCS Core: hierarchy, three-space layout, SCP, Traps, Sentinel.'
    crumbName = 'SUTR-0 — SUCS Core'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTR-0 &mdash; SUCS <span class="grad">Core</span>'
    subtitle= 'The foundational layout of the Base SuperUnicode Character System.'
    body    = @(
        @{ t = 'p'; html = 'This report defines the Base SUCS codepoint space, its hierarchy, its three spaces, and the reserved terminal region.' }
        @{ t = 'h2'; html = 'Space' }
        @{ t = 'spec'; html = 'SUCS_CP        uint32_t, 31 significant bits<br>range          0x00000000 &ndash; 0x7FFFFFFF<br>hierarchy      codepoint &lt; block &lt; range &lt; plane &lt; district &lt; zone &lt; territory' }
        @{ t = 'h2'; html = 'Spaces' }
        @{ t = 'table'; head = @('Space', 'Range', 'Owner'); rows = @(
            @('Unicode Compatibility Space', '<span class="mono">0x000000&ndash;0x10FFFF</span>', '1:1 with Unicode, permanent')
            @('System Control Plane', '<span class="mono">0x110000&ndash;0x11FFFF</span>', 'BANcode Registry (B+, W+, C+, S+)')
            @('Native SuperUnicode Space', '<span class="mono">0x120000&ndash;0x7FFFFFFF</span>', 'OpenWindows allocations')
        ) }
        @{ t = 'h2'; html = 'Terminal region' }
        @{ t = 'ul'; items = @(
            'Traps: <span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span>'
            'Sentinel: <span class="mono">0x7FFFFFFF</span>'
            'No allocation exists at or below the Sentinel.'
        ) }
    )
}

$Pages['reports/SUTR-1'] = @{
    path    = 'reports/SUTR-1.html'
    sec     = 'reports'
    title   = 'SUTR-1 — SUTF'
    desc    = 'SUTF: SUCS UTF-8/16/32 framing.'
    crumbName = 'SUTR-1 — SUTF'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTR-1 &mdash; <span class="grad">SUTF</span>'
    subtitle= 'SUCS UTF-8, UTF-16 and UTF-32 framing.'
    body    = @(
        @{ t = 'p'; html = 'The SUTF family frames 31-bit <span class="mono">SUCS_CP</span> values into byte sequences. All three are lossless over the full Base space.' }
        @{ t = 'table'; head = @('Encoding', 'Unit', 'Covers', 'Notes'); rows = @(
            @('<span class="mono">SUTF-8</span>', '1&ndash;4 bytes', 'Full 31-bit space', 'Standard UTF-8 leading-byte framing; never emits a 4-byte value above <span class="mono">0x7FFFFFFF</span>')
            @('<span class="mono">SUTF-16</span>', '2 or 4 bytes', 'Full 31-bit space', 'Word framing with a marker bit (1 word to <span class="mono">0x7FFF</span>, 2 words above); no surrogates &mdash; <span class="mono">0xD800</span>&ndash;<span class="mono">0xDFFF</span> are valid PUA. Byte order explicit: BE canonical (<span class="mono">sust16</span>) or LE')
            @('<span class="mono">SUTF-32</span>', '4 bytes (LE)', 'Full 31-bit space', 'One codepoint per unit; the only lossless framing for the native space')
        ) }
        @{ t = 'callout'; html = 'Reference implementation: <a href="../modules/sutf/index.html">modules/sutf</a>. Data: <span class="mono">Public/0.1.0/sutf/</span>.' }
    )
}

$Pages['reports/SUTR-2'] = @{
    path    = 'reports/SUTR-2.html'
    sec     = 'reports'
    title   = 'SUTR-2 — SUCA'
    desc    = 'SUCA: the SuperUnicode Collation Algorithm (formalized by SUTS-001).'
    crumbName = 'SUTR-2 — SUCA'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTR-2 &mdash; <span class="grad">SUCA</span>'
    subtitle= 'The SuperUnicode Collation Algorithm.'
    body    = @(
        @{ t = 'p'; html = 'Collation is the ordering contract for SuperUnicode strings. SUCA is now a full UCA(UTS #10)-equivalent multilevel algorithm, formally specified by <a href="SUTS-001.html">SUTS-001</a> and implemented by the freestanding <span class="mono">suts_suca</span> reference.' }
        @{ t = 'ul'; items = @(
            '<strong>Multilevel</strong> &mdash; L1 (base), L2 (accent), L3 (case), L4 (variable), identical (NFD codepoint tie-break).'
            '<strong>Canonical equivalence</strong> &mdash; input is NFD-normalized before mapping (e.g. <span class="mono">role &lt; r&ocirc;le &lt; roles</span>, and precomposed &asymp; decomposed).'
            '<strong>Contractions &amp; expansions</strong> &mdash; e.g. <span class="mono">c h</span> as one letter; <span class="mono">&#339; &asymp; oe</span>.'
            '<strong>Variable weighting</strong> &mdash; shifted (default), blanked, non-ignorable, shift-trimmed; SCP controls ignorable at L1&ndash;L3.'
            '<strong>Backward secondary</strong> &mdash; French dictionary order.'
            '<strong>Implicit weights</strong> &mdash; Unassigned/Han/Native/plugin codepoints get algorithmic primaries from the 64-bit codepoint (monotonic; plugin space above Base).'
        ) }
        @{ t = 'callout'; html = 'The complete normative algorithm, weight scheme, data files and conformance matrix are in <a href="SUTS-001.html">SUTS-001 — SuperUnicode Collation Algorithm</a>.' }
        @{ t = 'note'; html = 'Data: <span class="mono">Public/0.1.0/collation/SUCA.txt</span> (Base) and <span class="mono">collation/ExtUCA.txt</span> (Extended) &mdash; both extSUCS-compatible.' }
    )
}

$Pages['reports/SUTS-001'] = @{
    path    = 'reports/SUTS-001.html'
    sec     = 'reports'
    title   = 'SUTS-001 — SuperUnicode Collation Algorithm'
    desc    = 'SUTS-001: the SuperUnicode Collation Algorithm (SUCA) — a full UCA-equivalent multilevel collation over the 64-bit SUCS space.'
    crumbName = 'SUTS-001 — SUCA'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTS-001 &mdash; SuperUnicode <span class="grad">Collation Algorithm</span>'
    subtitle= 'The first SuperUnicode Technical Specification: UCA (UTS #10) equivalence over 64-bit SUCS.'
    body    = @(
        @{ t = 'p'; html = 'SUTS-001 (SUCA) is the first SuperUnicode Technical Specification. It defines a full UTS #10 (UCA)-equivalent multilevel collation algorithm for the entire 64-bit SuperUnicode character space (<span class="mono">sucs_ex_char_t</span>), including the extSUCS plugin ranges above <span class="mono">0x7FFFFFFF</span>.' }
        @{ t = 'h2'; html = 'Algorithm' }
        @{ t = 'ol'; items = @(
            '<strong>Normalize (S1)</strong> &mdash; decompose input to NFD; algorithmic Hangul; canonical ordering.'
            '<strong>Map (S2)</strong> &mdash; walk the string, longest-match contractions and expansions, then simple/implicit CEs; apply variable weighting.'
            '<strong>Form sort key (S3)</strong> &mdash; L1&ndash;L4 with level separators, backward secondary support, optional identical level.'
            '<strong>Compare (S4)</strong> &mdash; binary compare of sort keys; identical level resolves by native codepoint order.'
        ) }
        @{ t = 'table'; head = @('Feature', 'Status'); rows = @(
            @('At least 3 collation levels + identical', 'Yes &mdash; L1&ndash;L4 + identical')
            @('Canonical equivalence (NFD)', 'Yes')
            @('Contractions &amp; expansions', 'Yes')
            @('Variable weighting (shifted/blanked/non-ignorable/shift-trimmed)', 'Yes')
            @('Backward secondary (French)', 'Yes')
            @('Implicit weights for unassigned/Han/native/plugin', 'Yes (algorithmic, 64-bit codepoint)')
            @('Tailoring rules (&amp; base &lt; x, &lt;&lt;, &lt;&lt;&lt;, =)', 'Yes (programmatic)')
            @('extSUCS 64-bit compatibility', 'Yes')
        ) }
        @{ t = 'callout'; html = 'Reference implementation: <span class="mono">include/suts/suts_suca.h</span> + <span class="mono">src/suts/suts_suca.c</span> (freestanding C99, no heap), registered as <span class="mono">suts_static</span>. Data: <span class="mono">collation/SUCA.txt</span>, <span class="mono">collation/ExtUCA.txt</span>.' }
        @{ t = 'note'; html = 'Source of truth: <span class="mono">docs/suts/SUTS-001-suca.md</span> in the repository. This Technical Report introduces SUCA; <a href="SUTR-2.html">SUTR-2</a> covers the same ground as the original report.' }
    )
}

$Pages['reports/SUAS-001'] = @{
    path    = 'reports/SUAS-001.html'
    sec     = 'reports'
    title   = 'SUAS-001 — Structural Directional Framing'
    desc    = 'SUAS-001: Structural Directional Framing (SDF) — core BiDi architecture, scope isolation and glyph mirroring over the SUCS space.'
    crumbName = 'SUAS-001 — SDF'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUAS-001 &mdash; Structural Directional <span class="grad">Framing</span>'
    subtitle= 'The first SuperUnicode Architecture Standard: core BiDi architecture, scope isolation and glyph mirroring.'
    body    = @(
        @{ t = 'p'; html = 'SUAS-001 (SDF) is the first SuperUnicode Architecture Standard and the <strong>core architecture</strong> for bi-directional text layout, scope isolation and glyph mirroring. It is a <strong>non-negotiable</strong> invariant: every conforming renderer MUST resolve bi-directional text through the SDF engine.' }
        @{ t = 'h2'; html = 'Design' }
        @{ t = 'ul'; items = @(
            '<strong>Single-pass, zero-allocation</strong> &mdash; a state-machine pipeline carried in the SCP and the SUTF/SUST decoding streams.'
            '<strong>Dual-Mode Directional Resolver</strong> &mdash; Unicode Bridge classifies from SUCD BiDi; SCP directives are explicit; Native SUCS inherits.'
            '<strong>Fixed isolate stack</strong> &mdash; push/pop are O(1) on an embedded fixed array; overflow/underflow are structural errors.'
            '<strong>Framed output word</strong> &mdash; 64-bit: 31b codepoint, 3b dir_type, 1b mirrored, 29b reserved; renderers mirror instantly.'
        ) }
        @{ t = 'h2'; html = 'SCP directional directives' }
        @{ t = 'table'; head = @('Codepoint', 'Directive'); rows = @(
            @('<span class="mono">0x00110101</span>', '<span class="mono">SCP_DIR_LTR</span> — set resolved direction left-to-right')
            @('<span class="mono">0x00110102</span>', '<span class="mono">SCP_DIR_RTL</span> — set resolved direction right-to-left')
            @('<span class="mono">0x00110104</span>', '<span class="mono">SCP_DIR_ISOLATE_PUSH</span> — open an isolate')
            @('<span class="mono">0x00110108</span>', '<span class="mono">SCP_DIR_ISOLATE_POP</span> — close the active isolate')
        ) }
        @{ t = 'h2'; html = 'Full BiDi processing model (UAX #9)' }
        @{ t = 'p'; html = 'For renderers that require lexical bi-directional layout, SDF also implements the full Unicode Bidirectional Algorithm processing model: paragraph levels (P1&ndash;P3), explicit embedding/override/isolate levels (X1&ndash;X8), X9 removal, weak and neutral resolution (W1&ndash;W7, N0&ndash;N2), implicit levels (I1&ndash;I2), and reordering with mirroring (L1&ndash;L4).' }
        @{ t = 'table'; head = @('Feature', 'Status'); rows = @(
            @('Single-pass deterministic consumption', 'Yes')
            @('Zero heap allocation', 'Yes')
            @('O(1) isolate stack push/pop on fixed array', 'Yes')
            @('dir_type + mirrored metadata on every framed word', 'Yes')
            @('Dual-mode direction resolution', 'Yes')
            @('Full BiDi processing model (UAX #9)', 'Yes')
            @('Explicit-only scope changes (four SCP directives)', 'Yes')
        ) }
        @{ t = 'callout'; html = 'Reference implementation: <span class="mono">include/suas/suas_sdf.h</span> + <span class="mono">src/suas/suas_sdf.c</span> (freestanding C99, no heap), registered in <span class="mono">suas_static</span>.' }
        @{ t = 'note'; html = 'Source of truth: <span class="mono">docs/suas/SUAS-001-sdf.md</span> in the repository.' }
    )
}

$Pages['reports/SUAS-002'] = @{
    path    = 'reports/SUAS-002.html'
    sec     = 'reports'
    title   = 'SUAS-002 — System Glyph Width & Monospace Grid'
    desc    = 'SUAS-002: System Glyph Width & Monospace Grid (SGW) — East_Asian_Width-style width classification and O(1) fixed-cell grid over the 64-bit SUCS space.'
    crumbName = 'SUAS-002 — SGW'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUAS-002 &mdash; System Glyph Width <span class="grad">&amp; Monospace Grid</span>'
    subtitle= 'The second SuperUnicode Architecture Standard: East_Asian_Width-style width classification and O(1) fixed-cell grid metric.'
    body    = @(
        @{ t = 'p'; html = 'SUAS-002 (SGW) is the second SuperUnicode Architecture Standard. It defines an East_Asian_Width-style (UAX #11) width classification over the full 64-bit SUCS space (<span class="mono">sucs_ex_char_t</span>), folded into an O(1) monospace grid contract (0/1/2 cells + column cursor) for terminal emulators and CJK/ideographic framebuffer consoles.' }
        @{ t = 'h2'; html = 'Width classes' }
        @{ t = 'p'; html = 'Six width classes are defined, mirroring the UAX #11 categories:' }
        @{ t = 'ul'; items = @(
            '<strong>F</strong> (Fullwidth) &mdash; characters that are always wide.'
            '<strong>H</strong> (Halfwidth) &mdash; characters that are always narrow (e.g. <span class="mono">U+20A9 &#8361;</span>).'
            '<strong>W</strong> (Wide) &mdash; characters that are wide in East Asian context (e.g. Han ideographs, Emoji_Presentation).'
            '<strong>Na</strong> (Narrow) &mdash; characters that are always narrow (e.g. <span class="mono">U+00A5 &yen;</span>).'
            '<strong>A</strong> (Ambiguous) &mdash; characters whose width depends on context (e.g. <span class="mono">U+212B &#8491;</span>, <span class="mono">U+01D4</span>).'
            '<strong>N</strong> (Neutral) &mdash; characters that do not occur in East Asian text (e.g. <span class="mono">U+00C5 &Aring;</span>, <span class="mono">U+01D3</span>).'
        ) }
        @{ t = 'h2'; html = 'Zone dispatch' }
        @{ t = 'ol'; items = @(
            '<strong>Unicode Bridge</strong> <span class="mono">0x00000000&ndash;0x0010FFFF</span> &mdash; curated EAW table.'
            '<strong>SCP</strong> <span class="mono">0x00110000&ndash;0x0011FFFF</span> &mdash; non-advancing control, grid 0.'
            '<strong>Native SUCS</strong> <span class="mono">0x00120000&ndash;0x7FFFFFFE</span> &mdash; single cell.'
            '<strong>ExtSUCS</strong> <span class="mono">&gt;0x7FFFFFFF</span> &mdash; single cell.'
            '<strong>Trap</strong> <span class="mono">0x7FFFFFF0&ndash;0x7FFFFFFE</span> / <strong>Sentinel</strong> <span class="mono">0x7FFFFFFF</span> &mdash; grid 0.'
        ) }
        @{ t = 'h2'; html = 'Key rules' }
        @{ t = 'ul'; items = @(
            '<span class="mono">U+20A9 &#8361;</span> = H, <span class="mono">U+00A5 &yen;</span> = Na, <span class="mono">U+00C5 &Aring;</span> = N, <span class="mono">U+212B &#8491;</span> = A.'
            '<span class="mono">U+01D4</span> = A, <span class="mono">U+01D3</span> = N.'
            'Han ranges = Wide (including unassigned).'
            'Emoji_Presentation = Wide except Regional_Indicator (<span class="mono">U+1F1E6..1F1FF</span>).'
        ) }
        @{ t = 'table'; head = @('Feature', 'Status'); rows = @(
            @('Six width classes F/H/W/Na/A/N', 'Yes')
            @('O(1) grid cell + column advance', 'Yes')
            @('Zero heap allocation', 'Yes')
            @('64-bit extSUCS zone dispatch', 'Yes')
            @('Ambiguous contextual resolution', 'Yes')
            @('Emoji_Presentation&rarr;Wide except Regional_Indicator', 'Yes')
            @('Tailoring override hook', 'Yes')
        ) }
        @{ t = 'callout'; html = 'Reference implementation: <span class="mono">include/suas/suas_sgw.h</span> + <span class="mono">src/suas/suas_sgw.c</span> (freestanding C99, no heap), registered in <span class="mono">suas_static</span>.' }
        @{ t = 'note'; html = 'Source of truth: <span class="mono">docs/suas/SUAS-002-sgw.md</span> in the repository.' }
    )
}

$Pages['reports/SUAS-003'] = @{
    path    = 'reports/SUAS-003.html'
    sec     = 'reports'
    title   = 'SUAS-003 — System Boundary & Line Break Rules'
    desc    = 'SUAS-003: System Boundary & Line Break Rules (SBR) — single-pass deterministic line breaking over the 64-bit SUCS space.'
    crumbName = 'SUAS-003 — SBR'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUAS-003 &mdash; System Boundary <span class="grad">&amp; Line Break Rules</span>'
    subtitle= 'The third SuperUnicode Architecture Standard: single-pass deterministic line breaking over 64-bit SUCS.'
    body    = @(
        @{ t = 'p'; html = 'SUAS-003 (SBR) is the third SuperUnicode Architecture Standard. It defines where a line may, must, or must not be broken over the full 64-bit SUCS space (<span class="mono">sucs_ex_char_t</span>) via a <strong>single-pass deterministic state transition table</strong> (a UAX #14 analog) &mdash; no multi-pass buffer algorithm, zero allocation, O(1) pair resolution.' }
        @{ t = 'h2'; html = 'Break statuses' }
        @{ t = 'ul'; items = @(
            '<strong>MUST_BREAK</strong> &mdash; a line boundary is mandatory (e.g. after a control).'
            '<strong>CAN_BREAK</strong> &mdash; an allowed / opportunistic boundary.'
            '<strong>NO_BREAK</strong> &mdash; a prohibited boundary.'
            '<strong>ALPHANUM_BREAK</strong> &mdash; mandatory CJK/numeric alphabetic break.'
        ) }
        @{ t = 'h2'; html = 'Explicit SCP Break Markers' }
        @{ t = 'table'; head = @('Codepoint', 'Directive'); rows = @(
            @('<span class="mono">0x00110020</span>', '<span class="mono">SCP_BRK_MANDATORY</span> — force a line break here')
            @('<span class="mono">0x00110021</span>', '<span class="mono">SCP_BRK_PROHIBITED</span> — force no line break here')
            @('<span class="mono">0x00110022</span>', '<span class="mono">SCP_BRK_OPPORTUNISTIC</span> — allow a break here')
        ) }
        @{ t = 'h2'; html = 'Zone dispatch' }
        @{ t = 'ol'; items = @(
            '<strong>Unicode Bridge</strong> <span class="mono">0x00000000&ndash;0x0010FFFF</span> &mdash; curated UAX #14 break-class table + transition matrix.'
            '<strong>SCP</strong> <span class="mono">0x00110000&ndash;0x0011FFFF</span> &mdash; Explicit Break Markers override; other controls are mandatory.'
            '<strong>Native SUCS</strong> <span class="mono">0x00120000&ndash;0x7FFFFFFE</span> &mdash; O(1) high-bit range bitmask word test.'
            '<strong>ExtSUCS</strong> <span class="mono">&gt;0x7FFFFFFF</span> &mdash; neutral gap default.'
        ) }
        @{ t = 'table'; head = @('Feature', 'Status'); rows = @(
            @('Four break statuses (MUST/CAN/NO/ALPHANUM)', 'Yes')
            @('Single-pass O(1) transition-matrix pair resolution', 'Yes')
            @('Zero heap allocation', 'Yes')
            @('64-bit extSUCS zone dispatch', 'Yes')
            @('Explicit SCP Break Markers override', 'Yes')
            @('Invisible formatting (WJ, ZWSP)', 'Yes')
            @('Native SUCS high-bit bitmask word test', 'Yes')
            @('Alphanumeric (CJK numeric) break', 'Yes')
            @('Tailoring override hook', 'Yes')
        ) }
        @{ t = 'callout'; html = 'Reference implementation: <span class="mono">include/suas/suas_sbr.h</span> + <span class="mono">src/suas/suas_sbr.c</span> (freestanding C99, no heap), registered in <span class="mono">suas_static</span>.' }
        @{ t = 'note'; html = 'Source of truth: <span class="mono">docs/suas/SUAS-003-sbr.md</span> in the repository.' }
    )
}

$Pages['reports/SUAS-004'] = @{
    path    = 'reports/SUAS-004.html'
    sec     = 'reports'
    title   = 'SUAS-004 — SuperUnicode Canonical Forms (SUCF)'
    desc    = 'SUAS-004: SuperUnicode Canonical Forms (SUCF) — dual-target canonical decomposition/composition over the 64-bit SUCS space.'
    crumbName = 'SUAS-004 — SUCF'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUAS-004 &mdash; SuperUnicode <span class="grad">Canonical Forms</span>'
    subtitle= 'The fourth SuperUnicode Architecture Standard: dual-target, zero-allocation canonical (de)composition over 64-bit SUCS.'
    body    = @(
        @{ t = 'p'; html = 'SUAS-004 (SUCF) is the fourth SuperUnicode Architecture Standard. It defines canonical equivalence over the full 64-bit SUCS space (<span class="mono">sucs_ex_char_t</span>) via a <strong>single-pass sliding-window engine</strong> (a UAX #15 analog) with two targets: <strong>SUCF-C</strong> (canonical composition &mdash; the compact storage / equality form) and <strong>SUCF-D</strong> (canonical decomposition &mdash; the analysis / stripping / sorting form). Zero allocation: all reordering lives in a small stack-allocated window.' }
        @{ t = 'h2'; html = 'Dual targets' }
        @{ t = 'table'; head = @('Target', 'Purpose', 'Behavior'); rows = @(
            @('<span class="mono">SUCF-C</span>', 'Compact storage &amp; equality', 'decompose, reorder by CCC, then recompose greedily (Hangul algorithmic)')
            @('<span class="mono">SUCF-D</span>', 'Analysis, stripping &amp; sorting', 'decompose and reorder by CCC; never recompose')
        ) }
        @{ t = 'h2'; html = 'CCC reordering' }
        @{ t = 'p'; html = 'Every combining mark carries a Canonical Combining Class (CCC). Marks sort ascending by CCC; a pair swaps iff <span class="mono">CCC_A &gt; CCC_B</span> and <span class="mono">CCC_B &ne; 0</span> &mdash; stable for equal classes, and a starter (CCC 0) never moves. Stream-safe bound: 30 non-starters per run.' }
        @{ t = 'h2'; html = 'Hangul' }
        @{ t = 'p'; html = 'No table rows: syllables <span class="mono">0xAC00&ndash;0xD7A3</span> decompose algorithmically to L+V(+T); SUCF-C composes explicit L+V and LV+T sequences arithmetically.' }
        @{ t = 'h2'; html = 'SCP immunity' }
        @{ t = 'p'; html = 'SCP <span class="mono">0x00110000&ndash;0x0011FFFF</span>, BANcodes, Trap and Sentinel are canonically invariant: they pass through untouched and never disturb combining state, while preserved in their original stream position.' }
        @{ t = 'table'; head = @('Feature', 'Status'); rows = @(
            @('Dual targets (SUCF-C composition / SUCF-D decomposition)', 'Yes')
            @('Single-pass sliding-window engine', 'Yes')
            @('Zero heap allocation', 'Yes')
            @('Curated CCC + decomposition tables (Unicode Bridge)', 'Yes')
            @('Algorithmic Hangul (de)composition', 'Yes')
            @('Composition exclusions (singletons, non-starters)', 'Yes')
            @('30 non-starter stream-safe bound', 'Yes')
            @('SCP / Trap / Sentinel invariance', 'Yes')
            @('Quick check YES/NO/MAYBE', 'Yes')
            @('Streaming &equiv; bulk transform', 'Yes')
        ) }
        @{ t = 'callout'; html = 'Reference implementation: <span class="mono">include/suas/suas_sucf.h</span> + <span class="mono">src/suas/suas_sucf.c</span> (freestanding C99, no heap), registered in <span class="mono">suas_static</span>.' }
        @{ t = 'note'; html = 'Source of truth: <span class="mono">docs/suas/SUAS-004-sucf.md</span> in the repository.' }
    )
}

$Pages['reports/SUTR-3'] = @{
    path    = 'reports/SUTR-3.html'
    sec     = 'reports'
    title   = 'SUTR-3 — SuperUnicode Storage'
    desc    = 'SuperUnicode Storage: the OWFS/USFS partition policy.'
    crumbName = 'SUTR-3 — SuperUnicode Storage'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTR-3 &mdash; SuperUnicode <span class="grad">Storage</span>'
    subtitle= 'Partitions, formats and the mount policy.'
    body    = @(
        @{ t = 'p'; html = 'Storage is governed by the OpenWindows Storage specification. A <strong>SuperUnicode Partition</strong> is a removable filesystem whose layout is declared by SuperUnicode machine instructions.' }
        @{ t = 'table'; head = @('Format', 'Description'); rows = @(
            @('<span class="mono">OWFS</span>', 'Native drive filesystem; data starts at offset <span class="mono">0x10000</span>; CRC32c + Fletcher-64 integrity; <span class="mono">ow_sec</span> identity; optional ChaCha20 at-rest encryption.')
            @('<span class="mono">USFS</span>', 'Portable external-media filesystem.')
        ) }
        @{ t = 'h2'; html = 'Policy summary' }
        @{ t = 'ul'; items = @(
            'Base SuperUnicode Partitions may be OWFS or USFS, and are used for bugfix and rescue payloads only.'
            'Plugin Partitions are Extended-only and MUST be OWFS &mdash; never USFS.'
            'All partitions mount read-only; plugin partitions mount only after the boot checksum gate.'
        ) }
        @{ t = 'note'; html = 'Full policy text: <span class="mono">partitions/spec.txt</span> in both /Public trees.' }
    )
}

$Pages['reports/SUTR-4'] = @{
    path    = 'reports/SUTR-4.html'
    sec     = 'reports'
    title   = 'SUTR-4 — ExtSUCS Transport'
    desc    = 'ExtSUCS Transport: vector, vsutf and e-SUST framing.'
    crumbName = 'SUTR-4 — ExtSUCS Transport'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTR-4 &mdash; ExtSUCS <span class="grad">Transport</span>'
    subtitle= 'Framing for 64-bit codepoints.'
    body    = @(
        @{ t = 'table'; head = @('Transport', 'Description'); rows = @(
            @('<span class="mono">Vector</span>', 'Length-prefixed sequences of 64-bit <span class="mono">sucs_ex_char_t</span>; one codepoint per little-endian unit.')
            @('<span class="mono">vsutf</span>', 'Variable-length integer framing (LEB128-style) covering the full 64-bit space.')
            @('<span class="mono">e-SUST</span>', 'The fixed-base Extended family, complementing Base SUTF8/16/32, with page-mapped IPC framing (<span class="mono">esust.h</span>).')
        ) }
        @{ t = 'h2'; html = 'Requirements' }
        @{ t = 'ul'; items = @(
            'Every transport must represent any value from <span class="mono">0</span> to <span class="mono">0xFFFFFFFFFFFFFFFF</span>.'
            'Base SUTF8/16/32 values must round-trip unchanged into the Extended transports.'
            'Sentinel semantics carry over: plugin streams terminate with the inherited Sentinel value.'
        ) }
        @{ t = 'note'; html = 'Reference: <a href="../modules/sust/index.html">modules/sust</a>. Data: <span class="mono">Public/0.1.0/transport/</span>.' }
    )
}

$Pages['reports/SUTR-5'] = @{
    path    = 'reports/SUTR-5.html'
    sec     = 'reports'
    title   = 'SUTR-5 — Plugin Lifecycle &amp; Blob Format'
    desc    = 'Plugin Lifecycle and Blob Format: stage, checksum gate, mount, registry, quarantine.'
    crumbName = 'SUTR-5 — Plugin Lifecycle'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTR-5 &mdash; Plugin <span class="grad">Lifecycle</span>'
    subtitle= 'How a plugin goes from blob to registered codepoints.'
    body    = @(
        @{ t = 'h2'; html = 'Blob format' }
        @{ t = 'p'; html = 'A plugin blob is one packed file: a 94-byte header (magic <span class="mono">SUCS</span>, blob version, plugin version, 64-byte id, range count, blob size, CRC32c, Fletcher-64), followed by 16-byte little-endian range pairs and the payload.' }
        @{ t = 'h2'; html = 'Gates' }
        @{ t = 'ol'; items = @(
            '<strong>Checksum gate</strong> &mdash; CRC32c and Fletcher-64 over the whole blob (with the checksum fields zeroed).'
            '<strong>Range gate</strong> &mdash; ranges sorted, non-overlapping, and strictly above the base limit.'
            '<strong>Collision gate</strong> &mdash; no overlap with any mounted plugin&rsquo;s ranges.'
            '<strong>Mount gate</strong> &mdash; OWFS only, read-only.'
            '<strong>Register</strong> &mdash; ranges enter ExtSUCS and lookups resolve.'
        ) }
        @{ t = 'h2'; html = 'Status codes' }
        @{ t = 'p'; html = 'The plugin ABI exposes a 14-value status enum. <span class="mono">SUCS_PLUGIN_REBOOT_REQUIRED</span> (13) is returned by staging and signals the mandatory restart. Gate failures return the specific diagnostic state.' }
        @{ t = 'callout'; html = 'Tooling: <a href="../extended/sdk.html">plugin_pack and plugin_verify</a> reproduce the checksum gate offline.' }
    )
}

$Pages['reports/SUTR-6'] = @{
    path    = 'reports/SUTR-6.html'
    sec     = 'reports'
    title   = 'SUTR-6 — Unicode Compatibility Bridge'
    desc    = 'The Unicode Compatibility Bridge: the permanent 1:1 guarantee across 0x000000-0x10FFFF.'
    crumbName = 'SUTR-6 — Unicode Compatibility Bridge'
    crumbs  = @( @{ label = 'Technical Reports'; href = 'index.html' } )
    h1      = 'SUTR-6 &mdash; Unicode <span class="grad">Compatibility Bridge</span>'
    subtitle= 'The permanent 1:1 guarantee.'
    body    = @(
        @{ t = 'p'; html = 'Codepoints <span class="mono">0x000000&ndash;0x10FFFF</span> correspond 1:1 with Unicode codepoints of the same numeric value. This is the <em>Unicode Compatibility Space</em>.' }
        @{ t = 'h2'; html = 'The guarantee' }
        @{ t = 'ul'; items = @(
            'Bidirectional and lossless: <span class="mono">sucs_downcast()</span> of a bridged codepoint yields the identical Unicode value, and every Unicode codepoint has a SuperUnicode identity.'
            'Permanent by position, not by version: the mapping is fixed at release 0.1.0 and does not change when Unicode publishes new versions or new codepoints.'
            'New Unicode codepoints appear at the same numeric values &mdash; the bridge tracks Unicode growth without any remapping.'
        ) }
        @{ t = 'spec'; html = 'SUCS 0x000000 &harr; U+000000   <span class="hl">// bi-directional, lossless</span><br>SUCS 0x10FFFF &harr; U+10FFFF   <span class="hl">// the ceiling of the bridge</span>' }
        @{ t = 'callout'; html = 'The bridge stops where the SCP begins: above <span class="mono">0x10FFFF</span>, SuperUnicode is fully its own system. <a href="../standard/spaces.html">The three spaces &raquo;</a>' }
    )
}
