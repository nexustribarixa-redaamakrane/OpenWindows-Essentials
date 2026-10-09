# SUTR report pages - generated from the markdown specifications
#
# The eight SUTR specifications live in docs/sutr/*.md and are the single
# source of truth. This file renders them into $Pages entries. The
# hand-authored SUTR-N pages that used to live in reports.ps1 have been
# deleted, so this is now the only definition of the SUTR page data. If
# docs/ is unavailable then no SUTR pages are produced and dispatch warns.
#
# The SUTR page headings and scope lines are metadata, parsed from each
# document's own bolded field block:
#
#   # SUTR-4 - ExtSUCS Transport
#
#   **Standard ID:** SUTR-4
#   **Status:** 0.1.0 - Published
#   **Scope:** Level 4 (Character Encoding Scheme / Transport) - ...
#   **Applies To:** ...
#   **Last Updated:** 2026-09-30
#
# Everything after the field block is body, converted to page blocks:
#   ## / ###      -> h2 / h3
#   | a | b |     -> table
#   ``` fenced    -> spec   (monospace block)
#   - item         -> ul    (consecutive)
#   1. item        -> ol    (consecutive)
#   > quote        -> callout
#   **END OF**     -> dropped
#   ---            -> dropped
#   otherwise      -> p     (consecutive lines joined)

# docs/ is not part of the deployed site, so a relative ".md" link inside a spec
# cannot resolve in the generated page. This table redirects the cross-document
# references onto the page that carries the same content, with hrefs relative to
# reports/. Anything not listed degrades to plain text rather than shipping a
# 404, so a new reference in a spec can never produce a broken link.
$SutrDocLinks = @{
    '../suts/SUTS-001-suca.md'     = 'SUTS-001.html'
    '../suas/SUAS-001-sdf.md'      = 'SUAS-001.html'
    '../suas/SUAS-002-sgw.md'      = 'SUAS-002.html'
    '../suas/SUAS-003-sbr.md'      = 'SUAS-003.html'
    '../suas/SUAS-004-sucf.md'     = 'SUAS-004.html'
    '../sutr/SUTR-0-sucs-core.md'  = 'SUTR-0.html'
    '../sutr/SUTR-1-sutf.md'       = 'SUTR-1.html'
    '../sutr/SUTR-2-suca.md'       = 'SUTR-2.html'
    '../sutr/SUTR-3-superunicode-storage.md' = 'SUTR-3.html'
    '../sutr/SUTR-4-extsucs-transport.md'    = 'SUTR-4.html'
    '../sutr/SUTR-5-plugin-lifecycle.md'      = 'SUTR-5.html'
    '../sutr/SUTR-6-unicode-bridge.md'        = 'SUTR-6.html'
    '../sutr/SUTR-7-sucem.md'                 = 'SUTR-7.html'
}

function Convert-SutrInline([string]$s) {
    if ($null -eq $s) { return '' }
    $h = $s
    # Escape bare angle brackets so literal generics survive, but leave the
    # ones we are about to emit alone (none are, at this point).
    $h = $h -replace '&', '&amp;'
    $h = $h -replace '<', '&lt;'
    $h = $h -replace '>', '&gt;'
    # Restore the entities that are meaningful in prose.
    $h = $h -replace '&amp;amp;', '&amp;'
    # Typographic: en dash for spaced hyphen, arrow for =>, times.
    # NOTE: the multiplication sign is only substituted for an explicit
    # "N x M" pair. A blanket /x/ -> &times; mangles ordinary words such as
    # "Extraction" or "index".
    $h = $h -replace ' -- ', ' &mdash; '
    $h = $h -replace ' - ', ' &mdash; '
    $h = $h -replace '--', '&ndash;'
    $h = $h -replace '=>', '&rarr;'
    $h = [regex]::Replace($h, '\b(\d+)\s*x\s*(\d+)\b', '&times;')
    # Literal Unicode punctuation in the specs becomes the site's entity form,
    # so the markup is pure ASCII and cannot be broken by encoding drift.
    $h = $h -replace ([char]0x2014), '&mdash;'   # em dash
    $h = $h -replace ([char]0x2013), '&ndash;'   # en dash
    $h = $h -replace ([char]0x2192), '&rarr;'    # rightwards arrow
    $h = $h -replace ([char]0x00D7), '&times;'   # multiplication sign
    $h = $h -replace ([char]0x00B7), '&middot;'  # middle dot
    # Inline code -> the site's mono span. Do this after escaping so the code
    # content keeps its own ampersands intact.
    $h = [regex]::Replace($h, '`([^`]+)`', { param($m) "<span class='mono'>" + $m.Groups[1].Value + "</span>" })
    # Bold / italic.
    $h = [regex]::Replace($h, '\*\*([^*]+)\*\*', '<strong>$1</strong>')
    $h = [regex]::Replace($h, '(?<![\w*])\*([^*\n]+)\*(?![\w*])', '<em>$1</em>')
    # Links: [text](href)
    $h = [regex]::Replace($h, '\[([^\]]+)\]\(([^)]+)\)', { param($m)
        $href = $m.Groups[2].Value
        $txt  = $m.Groups[1].Value
        if ($href -match '^(https?:|/)') { return "<a href='$href'>$txt</a>" }
        if ($href -notmatch '\.md$')      { return "<a href='$href'>$txt</a>" }
        # A relative link into docs/ - point it at the deployed page instead.
        $key = $href -replace '^\./', ''
        if ($SutrDocLinks.ContainsKey($key)) { return "<a href='$($SutrDocLinks[$key])'>$txt</a>" }
        if ($SutrDocLinks.ContainsKey("../$key")) { return "<a href='$($SutrDocLinks["../$key"])'>$txt</a>" }
        # Unmapped repository document: show the reference, do not link to a 404.
        return "<span class='mono'>$txt</span>"
    })
    return $h
}

function Convert-SutrText([string]$s) {
    # For page metadata - <title>, the meta description and the breadcrumb
    # crumb. dispatch.ps1 interpolates those three fields into the markup
    # verbatim, so they must arrive pre-escaped and free of Markdown. Apostrophes
    # matter too: the description is emitted inside a single-quoted attribute.
    if ($null -eq $s) { return '' }
    $h = $s
    $h = $h -replace '&', '&amp;'
    $h = $h -replace '<', '&lt;'
    $h = $h -replace '>', '&gt;'
    $h = $h -replace "'", '&#39;'
    # Literal Unicode punctuation becomes the site's entity form, so the
    # generated markup stays ASCII and cannot be broken by encoding drift.
    $h = $h -replace ([char]0x2014), '&mdash;'   # em dash
    $h = $h -replace ([char]0x2013), '&ndash;'   # en dash
    $h = $h -replace ([char]0x00D7), '&times;'   # multiplication sign
    $h = $h -replace ([char]0x00B7), '&middot;'  # middle dot
    return $h
}

function Convert-SutrDoc([string]$mdPath) {
    $lines = [System.IO.File]::ReadAllLines($mdPath)
    $blocks = New-Object System.Collections.Generic.List[object]

    $title = $null; $status = $null; $scope = $null; $applies = $null; $updated = $null; $id = $null
    $i = 0

    # --- header: H1 + field block -----------------------------------------
    if ($i -lt $lines.Count -and $lines[$i] -match '^#\s+(.+?)\s*$') {
        $title = $Matches[1]
        $i++
    }
    for (; $i -lt $lines.Count; $i++) {
        $L = $lines[$i]
        if ($L -match '^\*\*Standard ID:\*\*\s*(.+?)\s*$')          { $id      = $Matches[1]; continue }
        if ($L -match '^\*\*Status:\*\*\s*(.+?)\s*$')              { $status  = $Matches[1]; continue }
        if ($L -match '^\*\*Scope:\*\*\s*(.+?)\s*$')               { $scope   = $Matches[1]; continue }
        if ($L -match '^\*\*Applies To:\*\*\s*(.+?)\s*$')         { $applies = $Matches[1]; continue }
        if ($L -match '^\*\*Last Updated:\*\*\s*(.+?)\s*$')        { $updated = $Matches[1]; continue }
        if ($L -match '^\*\*') { continue }   # any other bolded field
        if ([string]::IsNullOrWhiteSpace($L)) { continue }
        break
    }

    $desc = if ($scope) { $scope } else { $title }

    # --- body -------------------------------------------------------------
    $para = New-Object System.Collections.Generic.List[string]
    function Flush-Paragraph {
        if ($para.Count -gt 0) {
            $joined = ($para -join ' ')
            $blocks.Add(@{ t = 'p'; html = (Convert-SutrInline $joined) })
            $para.Clear()
        }
    }

    while ($i -lt $lines.Count) {
        $raw = $lines[$i]
        $L = $raw.Trim()

        if ($L -eq '') { Flush-Paragraph; $i++; continue }

        # trailing marker
        if ($L -match '^\*\*END OF') { Flush-Paragraph; break }
        if ($L -match '^---+$')       { Flush-Paragraph; $i++; continue }

        # fenced code -> spec
        if ($L -match '^```') {
            Flush-Paragraph
            $fence = $L
            $i++
            $code = New-Object System.Collections.Generic.List[string]
            while ($i -lt $lines.Count -and $lines[$i].Trim() -notmatch '^```') {
                $code.Add($lines[$i])
                $i++
            }
            $i++  # closing fence
            # Escape the code content, THEN join with real <br> separators, so
            # the line breaks survive as markup rather than being escaped.
            # Inline spans are deliberately NOT applied inside code: it must
            # stay literal. Unicode punctuation is still normalised so the
            # output is pure ASCII markup.
            $escLines = @($code | ForEach-Object {
                $l = $_ -replace '&', '&amp;'
                $l = $l -replace '<', '&lt;'
                $l = $l -replace '>', '&gt;'
                $l = $l -replace ([char]0x2014), '&mdash;'
                $l = $l -replace ([char]0x2013), '&ndash;'
                $l = $l -replace ([char]0x2192), '&rarr;'
                $l = $l -replace ([char]0x00D7), '&times;'
                $l = $l -replace ([char]0x00B7), '&middot;'
                $l
            })
            $body = ($escLines -join '<br>')
            $blocks.Add(@{ t = 'spec'; html = $body })
            continue
        }

        # headings
        if ($L -match '^###\s+(.+)$') { Flush-Paragraph; $blocks.Add(@{ t = 'h3'; html = (Convert-SutrInline $Matches[1]) }); $i++; continue }
        if ($L -match '^##\s+(.+)$')  { Flush-Paragraph; $blocks.Add(@{ t = 'h2'; html = (Convert-SutrInline $Matches[1]) }); $i++; continue }
        if ($L -match '^#\s+(.+)$')   { Flush-Paragraph; $i++; continue }

        # table
        if ($L -match '^\|') {
            Flush-Paragraph
            $rows = New-Object System.Collections.Generic.List[object]
            $head = $null
            while ($i -lt $lines.Count -and $lines[$i].Trim() -match '^\|') {
                $cells = $lines[$i].Trim().Trim('|') -split '\|'
                $cells = @($cells | ForEach-Object { $_.Trim() })
                # separator row |---|---|
                if (($cells | Where-Object { $_ -match '^:?-{2,}:?$' }).Count -eq $cells.Count) { $i++; continue }
                if ($null -eq $head) { $head = $cells; $i++; continue }
                $rows.Add(@($cells | ForEach-Object { Convert-SutrInline $_ }))
                $i++
            }
            if ($head) {
                $blocks.Add(@{ t = 'table'; head = @($head | ForEach-Object { Convert-SutrInline $_ }); rows = $rows.ToArray() })
            }
            continue
        }

        # blockquote -> callout
        if ($L -match '^>\s?(.*)$') {
            Flush-Paragraph
            $q = New-Object System.Collections.Generic.List[string]
            while ($i -lt $lines.Count -and $lines[$i].Trim() -match '^>') {
                $q.Add(($lines[$i].Trim() -replace '^>\s?', ''))
                $i++
            }
            $blocks.Add(@{ t = 'callout'; html = (Convert-SutrInline (($q -join ' '))) })
            continue
        }

        # ordered list
        if ($L -match '^\d+\.\s+(.*)$') {
            Flush-Paragraph
            $items = New-Object System.Collections.Generic.List[string]
            while ($i -lt $lines.Count -and $lines[$i].Trim() -match '^\d+\.\s+(.*)$') {
                $items.Add((Convert-SutrInline $Matches[1]))
                $i++
            }
            $blocks.Add(@{ t = 'ol'; items = $items.ToArray() })
            continue
        }

        # unordered list (allow one level of nesting, flattened with a marker)
        if ($L -match '^\s*[-*]\s+(.*)$') {
            Flush-Paragraph
            $items = New-Object System.Collections.Generic.List[string]
            while ($i -lt $lines.Count) {
                $t = $lines[$i]
                if ($t -match '^\s*[-*]\s+(.*)$') {
                    $depth = ($t.Length - $t.TrimStart().Length)
                    $txt = Convert-SutrInline $Matches[1]
                    if ($depth -ge 2) { $txt = "&mdash; $txt" }
                    $items.Add($txt)
                    $i++
                }
                elseif ($t.Trim() -eq '' -and $i + 1 -lt $lines.Count -and $lines[$i+1] -match '^\s+[-*]\s+') {
                    $i++   # blank line inside a list: keep going
                }
                else { break }
            }
            $blocks.Add(@{ t = 'ul'; items = $items.ToArray() })
            continue
        }

        # paragraph
        $para.Add($L)
        $i++
    }
    Flush-Paragraph

    return @{
        title   = $title
        id      = $id
        status  = $status
        scope   = $scope
        applies = $applies
        updated = $updated
        desc    = $desc
        blocks  = $blocks
    }
}

# --- emit the pages ------------------------------------------------------
$sutrDir = Join-Path $RepoRoot 'docs/sutr'
$sutrFiles = @(Get-ChildItem -LiteralPath $sutrDir -Filter 'SUTR-*.md' -ErrorAction SilentlyContinue |
               Sort-Object Name)

if ($sutrFiles.Count -gt 0) {
    # The specs separate the standard ID from the name with an em dash. Accept
    # an en dash and a space-hyphen-space as well, so a future edit to the
    # sources cannot silently collapse the heading into one mono run.
    $titleSep = '\s*(?:[' + [char]0x2014 + [char]0x2013 + ']|\s-\s)\s*'

    foreach ($f in $sutrFiles) {
        $doc = Convert-SutrDoc $f.FullName
        if (-not $doc.title) { continue }

        $slug = [regex]::Match($f.Name, '^(SUTR-\d+)').Groups[1].Value
        if (-not $slug) { continue }

        # Title: "SUTR-4 - ExtSUCS Transport" -> "SUTR-4 - <span>ExtSUCS Transport</span>"
        $tParts = $doc.title -split $titleSep, 2
        if ($tParts.Count -ge 2) {
            $tId   = $tParts[0].Trim()
            $tName = $tParts[1].Trim()
            $h1      = "<span class='mono'>$(Convert-SutrText $tId)</span> &mdash; <span class='grad'>$(Convert-SutrText $tName)</span>"
            $pageTtl = "$(Convert-SutrText $tId) &mdash; $(Convert-SutrText $tName)"
        } else {
            $h1      = "<span class='mono'>$(Convert-SutrText $doc.title)</span>"
            $pageTtl = Convert-SutrText $doc.title
        }

        # Provenance note: the page is generated, the spec governs.
        $prov = @()
        if ($doc.id)      { $prov += "<strong>Standard ID:</strong> <span class='mono'>$(Convert-SutrInline $doc.id)</span>" }
        if ($doc.status)  { $prov += "<strong>Status:</strong> $(Convert-SutrInline $doc.status)" }
        if ($doc.scope)   { $prov += "<strong>Scope:</strong> $(Convert-SutrInline $doc.scope)" }
        if ($doc.applies) { $prov += "<strong>Applies to:</strong> $(Convert-SutrInline $doc.applies)" }
        if ($doc.updated) { $prov += "<strong>Last updated:</strong> <span class='mono'>$(Convert-SutrInline $doc.updated)</span>" }
        $prov += "<strong>Source:</strong> <span class='mono'>docs/sutr/$($f.Name)</span> &mdash; this page is generated from it, and the repository copy governs where they differ."

        $body = New-Object System.Collections.Generic.List[object]
        $body.Add(@{ t = 'note'; html = ($prov -join '<br>') })
        foreach ($b in $doc.blocks) { $body.Add($b) }

        $Pages["reports/$slug"] = @{
            path     = "reports/$slug.html"
            sec      = 'reports'
            title    = $pageTtl
            desc     = Convert-SutrText $doc.desc
            crumbName= $pageTtl
            crumbs   = @( @{ label = 'Technical Reports'; href = 'index.html' } )
            h1       = $h1
            subtitle = if ($doc.scope) { Convert-SutrInline $doc.scope } else { '' }
            body     = $body.ToArray()
        }
    }
    Write-Host "  sutr.ps1: generated $($sutrFiles.Count) SUTR pages from docs/sutr/*.md"
} else {
    Write-Warning "sutr.ps1: docs/sutr/*.md not found - no SUTR pages will be generated"
}
