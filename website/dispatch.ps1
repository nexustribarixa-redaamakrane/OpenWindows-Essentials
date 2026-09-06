<# ============================================================================
   SuperUnicode website dispatcher — multi-page mirror of unicode.org
   ----------------------------------------------------------------------------
   Renders every page from the data files in pages/ (content) into static
   HTML, generates the repository directory-listing pages, assembles _site/,
   and optionally deploys to GitHub Pages.

   Usage:
     ./dispatch.ps1                 # render + build into website/_site/
     ./dispatch.ps1 -Deploy         # also push to the gh-pages branch

   Page data:  pages/*.ps1  ->  $Pages[slug] = @{ ... }
   Slugs used by modules listings and navigation are defined below.
   ==========================================================================#>
[CmdletBinding()]
param(
    [switch]$Deploy,
    [string]$Branch = 'gh-pages'
)

$ErrorActionPreference = 'Stop'
$ScriptDir = $PSScriptRoot
$SiteRoot  = $ScriptDir
$RepoRoot  = Split-Path $ScriptDir -Parent
$SiteOut   = Join-Path $ScriptDir '_site'

# ---------------------------------------------------------------------------
# Global navigation (site-relative hrefs; {p} prefix is injected per depth)
# ---------------------------------------------------------------------------
$Nav = @(
    @{ href = 'index.html';                    label = 'Home';                        sec = @('home', 'main') }
    @{ href = 'standard/index.html';           label = 'The Standard';                sec = @('standard') }
    @{ href = 'ucd/index.html';                label = 'Machine-Instruction Database'; sec = @('ucd') }
    @{ href = 'extended/index.html';           label = 'Extended';                    sec = @('extended') }
    @{ href = 'reports/index.html';            label = 'Technical Reports';            sec = @('reports') }
    @{ href = 'versions/index.html';           label = 'Versions';                    sec = @('versions') }
    @{ href = 'charts/index.html';             label = 'Charts';                      sec = @('charts') }
    @{ href = 'faq/index.html';                label = 'FAQ';                         sec = @('faq') }
    @{ href = 'modules/index.html';            label = 'Repositories';                sec = @('modules') }
)

# ---------------------------------------------------------------------------
# 1. Raster assets (regenerated only when missing)
# ---------------------------------------------------------------------------
Add-Type -AssemblyName System.Drawing -ErrorAction SilentlyContinue

function New-RoundRectPath([float]$w, [float]$h, [float]$r) {
    $p = New-Object System.Drawing.Drawing2D.GraphicsPath
    $d = $r * 2
    $p.AddArc(0, 0, $d, $d, 180, 90);  $p.AddArc($w - $d, 0, $d, $d, 270, 90)
    $p.AddArc($w - $d, $h - $d, $d, $d, 0, 90); $p.AddArc(0, $h - $d, $d, $d, 90, 90)
    $p.CloseFigure()
    return $p
}

function New-SucsTile([int]$size, [string]$path) {
    $bmp = New-Object System.Drawing.Bitmap($size, $size)
    $g   = [System.Drawing.Graphics]::FromImage($bmp)
    $g.SmoothingMode = 'AntiAlias'; $g.TextRenderingHint = 'AntiAliasGridFit'
    $rect = New-Object System.Drawing.RectangleF(0, 0, $size, $size)
    $brush = New-Object System.Drawing.Drawing2D.LinearGradientBrush($rect,
        [System.Drawing.Color]::FromArgb(255, 255, 122, 0),
        [System.Drawing.Color]::FromArgb(255, 168, 85, 247), 45.0)
    $g.FillPath($brush, (New-RoundRectPath $size $size ([float]($size * 0.22))))
    $font = New-Object System.Drawing.Font('Segoe UI', [float]($size * 0.46),
        [System.Drawing.FontStyle]::Bold, [System.Drawing.GraphicsUnit]::Pixel)
    $sb = New-Object System.Drawing.SolidBrush([System.Drawing.Color]::White)
    $sf = New-Object System.Drawing.StringFormat
    $sf.Alignment = 'Center'; $sf.LineAlignment = 'Center'
    $layout = New-Object System.Drawing.RectangleF(0, [float]($size * 0.04), $size, [float]($size * 0.88))
    $g.DrawString('SU', $font, $sb, $layout, $sf)
    $g.Dispose(); $bmp.Save($path, [System.Drawing.Imaging.ImageFormat]::Png); $bmp.Dispose()
}

function New-Ico([string[]]$pngPaths, [string]$outPath) {
    $pngs = $pngPaths | ForEach-Object { [System.IO.File]::ReadAllBytes($_) }
    $count = $pngs.Count; $offset = 6 + 16 * $count
    $ms = New-Object System.IO.MemoryStream
    $bw = New-Object System.IO.BinaryWriter($ms)
    $bw.Write([UInt16]0); $bw.Write([UInt16]1); $bw.Write([UInt16]$count)
    for ($i = 0; $i -lt $count; $i++) {
        $dim = switch ($i) { 0 { 16 } 1 { 32 } default { 48 } }
        $sz = $pngs[$i].Length
        $bw.Write([Byte]$dim); $bw.Write([Byte]$dim); $bw.Write([Byte]0); $bw.Write([Byte]0)
        $bw.Write([UInt16]1); $bw.Write([UInt16]32); $bw.Write([UInt32]$sz); $bw.Write([UInt32]$offset)
        $offset += $sz
    }
    foreach ($p in $pngs) { $bw.Write($p) }
    $bw.Flush()
    [System.IO.File]::WriteAllBytes($outPath, $ms.ToArray())
    $bw.Dispose(); $ms.Dispose()
}

New-Item -ItemType Directory -Force -Path (Join-Path $SiteRoot 'assets\img') | Out-Null
$logo = Join-Path $SiteRoot 'assets\img\logo.png'
$icon = Join-Path $SiteRoot 'favicon.ico'
if (-not (Test-Path $logo)) { New-SucsTile 256 $logo }
if (-not (Test-Path $icon)) {
    $t = Join-Path $env:TEMP 'sucs'; New-Item -ItemType Directory -Force -Path $t | Out-Null
    New-SucsTile 16 (Join-Path $t '16.png'); New-SucsTile 32 (Join-Path $t '32.png'); New-SucsTile 48 (Join-Path $t '48.png')
    New-Ico @((Join-Path $t '16.png'), (Join-Path $t '32.png'), (Join-Path $t '48.png')) $icon
}

# ---------------------------------------------------------------------------
# 2. Page renderer
# ---------------------------------------------------------------------------
$Pages = @{}
Get-ChildItem -Path (Join-Path $ScriptDir 'pages') -Filter '*.ps1' | Sort-Object Name | ForEach-Object {
    . $_.FullName
}

function Get-Prefix([string]$path) {
    $depth = ($path -split '/').Count - 1
    if ($depth -eq 0) { return '' }
    return ('../' * $depth)
}

function Resolve-Block([string]$html, [string]$p) {
    return ($html -replace '\{p\}', $p)
}

function Render-Block($b, [string]$p) {
    $sb = New-Object System.Text.StringBuilder
    switch ($b.t) {
        'hero' {
            [void]$sb.AppendLine('<section class="hero"><div class="hero-inner">')
            [void]$sb.AppendLine("<img class='hero-logo' src='{p}assets/img/logo.png' alt='SuperUnicode logo'>".Replace('{p}', $p))
            [void]$sb.AppendLine("<h1>$($b.h1)</h1>")
            [void]$sb.AppendLine("<p class='lede'>$($b.lede)</p>")
            if ($b.actions) {
                [void]$sb.AppendLine('<div class="hero-actions">')
                foreach ($a in $b.actions) {
                    [void]$sb.AppendLine("<a class='btn $($a.cls)' href='{p}$($a.href)'>$($a.label)</a>".Replace('{p}', $p))
                }
                [void]$sb.AppendLine('</div>')
            }
            [void]$sb.AppendLine('</div></section>')
        }
        'h2'  { [void]$sb.AppendLine("<h2>$($b.html)</h2>") }
        'h3'  { [void]$sb.AppendLine("<h3>$($b.html)</h3>") }
        'p'   { [void]$sb.AppendLine("<p>$(Resolve-Block $b.html $p)</p>") }
        'note'{ [void]$sb.AppendLine("<p class='note'>$(Resolve-Block $b.html $p)</p>") }
        'ul'  { [void]$sb.AppendLine('<ul>'); foreach ($i in $b.items) { [void]$sb.AppendLine("<li>$(Resolve-Block $i $p)</li>") }; [void]$sb.AppendLine('</ul>') }
        'ol'  { [void]$sb.AppendLine('<ol>'); foreach ($i in $b.items) { [void]$sb.AppendLine("<li>$(Resolve-Block $i $p)</li>") }; [void]$sb.AppendLine('</ol>') }
        'callout' {
            $cls = if ($b.cls) { $b.cls } else { '' }
            [void]$sb.AppendLine("<div class='callout $cls'><p>$(Resolve-Block $b.html $p)</p></div>")
        }
        'spec' { [void]$sb.AppendLine("<div class='spec'>$(Resolve-Block $b.html $p)</div>") }
        'table' {
            [void]$sb.AppendLine('<div class="table-wrap"><table class="plain"><thead><tr>')
            foreach ($h in $b.head) { [void]$sb.AppendLine("<th>$(Resolve-Block $h $p)</th>") }
            [void]$sb.AppendLine('</tr></thead><tbody>')

            # Rows are authored as one @(...) row literal per line. Because the
            # literals are separated by newlines (no comma), PowerShell flattens
            # them into a single flat cell sequence instead of an array of row
            # arrays. Group the flat list back into rows of head.Count cells;
            # genuinely nested arrays (one row per element) are rendered as-is.
            $colCount = @($b.head).Count
            $cells = @($b.rows)
            if (($cells | Where-Object { $_ -isnot [array] }).Count -eq $cells.Count) {
                $row = @()
                foreach ($cell in $cells) {
                    $row += $cell
                    if ($row.Count -eq $colCount) {
                        [void]$sb.AppendLine('<tr>')
                        foreach ($c in $row) { [void]$sb.AppendLine("<td>$(Resolve-Block $c $p)</td>") }
                        [void]$sb.AppendLine('</tr>')
                        $row = @()
                    }
                }
                if ($row.Count -gt 0) {
                    [void]$sb.AppendLine('<tr>')
                    foreach ($c in $row) { [void]$sb.AppendLine("<td>$(Resolve-Block $c $p)</td>") }
                    [void]$sb.AppendLine('</tr>')
                }
            } else {
                foreach ($r in $b.rows) {
                    [void]$sb.AppendLine('<tr>')
                    foreach ($c in $r) { [void]$sb.AppendLine("<td>$(Resolve-Block $c $p)</td>") }
                    [void]$sb.AppendLine('</tr>')
                }
            }
            [void]$sb.AppendLine('</tbody></table></div>')
        }
        'grid' {
            [void]$sb.AppendLine('<div class="grid">')
            foreach ($c in $b.cards) {
                if ($c.href) { [void]$sb.AppendLine("<div class='card'><h3><a href='{p}$($c.href)'>$($c.title)</a></h3><p>$(Resolve-Block $c.html $p)</p></div>".Replace('{p}', $p)) }
                else { [void]$sb.AppendLine("<div class='card'><h3>$($c.title)</h3><p>$(Resolve-Block $c.html $p)</p></div>") }
            }
            [void]$sb.AppendLine('</div>')
        }
        'spaces' {
            [void]$sb.AppendLine('<div class="spaces">')
            foreach ($s in $b.items) {
                [void]$sb.AppendLine("<div class='space $($s.cls)'><h3>$($s.title)</h3><span class='range mono'>$($s.range)</span><p>$(Resolve-Block $s.html $p)</p></div>")
            }
            [void]$sb.AppendLine('</div>')
        }
        'pyramid' {
            [void]$sb.AppendLine('<div class="pyramid"><ol>')
            foreach ($i in $b.items) { [void]$sb.AppendLine("<li class='$($i.cls)'>$($i.label)</li>") }
            [void]$sb.AppendLine('</ol></div>')
        }
    }
    return $sb.ToString()
}

function Render-Page($page) {
    $path = $page.path
    $p = Get-Prefix $path
    $active = $page.sec
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>")
    [void]$sb.AppendLine("<meta name='viewport' content='width=device-width, initial-scale=1'>")
    [void]$sb.AppendLine("<title>$($page.title)</title>")
    [void]$sb.AppendLine("<meta name='description' content='$($page.desc)'>")
    [void]$sb.AppendLine("<link rel='icon' href='{p}favicon.ico' sizes='16x16 32x32 48x48'>".Replace('{p}', $p))
    [void]$sb.AppendLine("<link rel='icon' href='{p}favicon.svg' type='image/svg+xml'>".Replace('{p}', $p))
    [void]$sb.AppendLine("<link rel='apple-touch-icon' href='{p}assets/img/logo.png'>".Replace('{p}', $p))
    [void]$sb.AppendLine("<link rel='manifest' href='{p}manifest.webmanifest'>".Replace('{p}', $p))
    [void]$sb.AppendLine("<link rel='preconnect' href='https://fonts.googleapis.com'>")
    [void]$sb.AppendLine("<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>")
    [void]$sb.AppendLine("<link href='https://fonts.googleapis.com/css2?family=Noto+Sans:ital,wght@0,100..900;1,100..900&family=Noto+Serif:ital,wght@0,100..900;1,100..900&family=Noto+Serif+Display:ital,wght@0,100..900;1,100..900&family=Noto+Sans+Mono:wght@100..900&display=swap' rel='stylesheet'>")
    [void]$sb.AppendLine("<link rel='stylesheet' href='{p}assets/css/main.css'>".Replace('{p}', $p))
    [void]$sb.AppendLine("</head><body>")
    [void]$sb.AppendLine("<div class='banner'>SuperUnicode 0.1.0 is out &mdash; multi-page site, machine-instruction database, plugin subsystem and SDK. <a href='{p}versions/index.html'>Release notes &raquo;</a></div>".Replace('{p}', $p))
    [void]$sb.AppendLine("<header class='site-header'><div class='masthead'>")
    [void]$sb.AppendLine("<a class='brand' href='{p}index.html'><img src='{p}assets/img/logo.png' alt='SuperUnicode logo'>".Replace('{p}', $p))
    [void]$sb.AppendLine("<span><span class='brand-name'>Super<em>Unicode</em></span><br><span class='brand-tag'>Native character encoding of the OpenWindows kernel</span></span></a>")
    [void]$sb.AppendLine('</div><nav class="site-nav">')
    foreach ($item in $Nav) {
        $cls = if ($active -in $item.sec) { " class='active'" } else { '' }
        [void]$sb.AppendLine("<a href='{p}$($item.href)'$cls>$($item.label)</a>".Replace('{p}', $p))
    }
    [void]$sb.AppendLine('</nav></header>')
    if ($page.layout -eq 'hero') {
        foreach ($b in $page.body) { [void]$sb.Append((Render-Block $b $p)) }
        [void]$sb.AppendLine('<main class="page">')
        if ($page.afterHero) { foreach ($b in $page.afterHero) { [void]$sb.Append((Render-Block $b $p)) } }
        [void]$sb.AppendLine('</main>')
    } else {
        [void]$sb.AppendLine("<main class='page'>")
        if ($page.crumbs) {
            [void]$sb.AppendLine("<nav class='breadcrumbs'><a href='{p}index.html'>Home</a>".Replace('{p}', $p))
            foreach ($c in $page.crumbs) {
                [void]$sb.AppendLine(" &raquo; <a href='{p}$($c.href)'>$($c.label)</a>".Replace('{p}', $p))
            }
            [void]$sb.AppendLine(" &raquo; $($page.crumbName)</nav>")
        }
        [void]$sb.AppendLine("<h1>$($page.h1)</h1>")
        if ($page.subtitle) { [void]$sb.AppendLine("<p class='subtitle'>$($page.subtitle)</p>") }
        foreach ($b in $page.body) { [void]$sb.Append((Render-Block $b $p)) }
        [void]$sb.AppendLine('</main>')
    }
    [void]$sb.AppendLine("<footer class='site-footer'><div class='footer-inner'>")
    [void]$sb.AppendLine('<span>SuperUnicode&reg; &mdash; native character encoding of the OpenWindows kernel.</span>')
    [void]$sb.AppendLine("<span><a href='{p}index.html'>Home</a> &middot; <a href='{p}standard/index.html'>Standard</a> &middot; <a href='{p}ucd/index.html'>SUCD</a> &middot; <a href='{p}extended/index.html'>Extended</a> &middot; <a href='{p}reports/index.html'>Reports</a> &middot; <a href='{p}faq/index.html'>FAQ</a> &middot; <a href='{p}sitemap.html'>Sitemap</a></span>".Replace('{p}', $p))
    [void]$sb.AppendLine('</div></footer>')
    [void]$sb.AppendLine('</body></html>')
    $out = Join-Path $SiteRoot $path
    $dir = Split-Path $out -Parent
    New-Item -ItemType Directory -Force -Path $dir | Out-Null
    [System.IO.File]::WriteAllText($out, $sb.ToString(), [System.Text.Encoding]::UTF8)
    Write-Host "rendered $path"
}

# ---------------------------------------------------------------------------
# 2b. Sitemap (computed from every loaded page)
# ---------------------------------------------------------------------------
$sitemapRows = foreach ($pg in ($Pages.Values | Sort-Object path)) {
    $top = ($pg.path -split '/')[0]
    if ($top -match '\.html$') { $top = 'Site' }
    $label = $pg.crumbName
    if (-not $label) { $label = ($pg.h1 -replace '<[^>]+>', '') }
    $label = $label -replace ' &mdash;.*$', '' -replace ' &ndash;.*$', ''
    @($top, "<a href='{p}$($pg.path)'>$label</a>", $pg.path)
}
$Pages['sitemap'] = @{
    path    = 'sitemap.html'
    sec     = 'home'
    title   = 'Sitemap'
    desc    = 'Every page of the SuperUnicode website.'
    crumbName = 'Sitemap'
    h1      = 'Sitemap'
    subtitle= 'Every page on this site, one table.'
    body    = @(
        @{ t = 'table'; head = @('Section', 'Page', 'File'); rows = $sitemapRows }
    )
}

foreach ($page in $Pages.Values) { Render-Page $page }

# ---------------------------------------------------------------------------
# 3. Module directory-listing pages
# ---------------------------------------------------------------------------
$Modules = @(
    @{ slug = 'unicode';          title = 'SuperUnicode (Base SUCS) module';        desc = 'Base SuperUnicode: core, framing, Public/ tree (SUCD, sudat, names, collation, charts, mappings, partitions).';            src = 'superunicode' }
    @{ slug = 'unicode-extended'; title = 'SuperUnicode Extended (ExtSUCS) module'; desc = 'Extended SuperUnicode: extSUTF, plugin subsystem, SDK, and the Extended Public/ tree (registry, plugins, transport).';        src = 'superunicode_extended' }
    @{ slug = 'sutf';             title = 'SUTF module';                        desc = 'SUCS UTF-8 / UTF-16 / UTF-32 transformation formats and tests.';                                        src = 'sutf' }
    @{ slug = 'extsutf';          title = 'extSUTF module';                     desc = 'Extended transformation: vsutf encoder over sucs_ex_char_t (64-bit).';                                  src = 'superunicode_extended'; filter = 'vsutf|conv' }
    @{ slug = 'sust';             title = 'SUST module';                        desc = 'Serialization transports: SUST-16/32/64/128/256/512/N fixed-width and e-SUST page-mapped IPC, over sucs_ex_char_t (64-bit).'; src = 'sust' }
    @{ slug = 'unified';          title = 'Unified module';                     desc = 'The one translation unit that compiles every public header of every module, plus its build wiring.';                        src = 'unified' }
)
$Ignore = @('build', 'bin', '.git', 'CMakeFiles', '_site', '.vs', '.vscode')

function Get-FilteredChildren([string]$abs, [string]$rel, [string]$filter) {
    $items = Get-ChildItem -LiteralPath $abs | Where-Object { $_.Name -notin $Ignore }
    $out = @()
    foreach ($item in $items) {
        $itemRel = "$rel/$($item.Name)"
        if ($item.PSIsContainer) {
            $sub = Get-FilteredChildren $item.FullName $itemRel $filter
            if (-not $filter -or $itemRel -match $filter -or $sub.Count -gt 0) { $out += @{ Item = $item; Sub = $sub } }
        } else {
            if (-not $filter -or $itemRel -match $filter) { $out += @{ Item = $item; Sub = @() } }
        }
    }
    return $out
}

function New-DirSection([string]$abs, [string]$rel, [int]$depth, [string]$filter, [System.Text.StringBuilder]$sb, [ref]$dirCount) {
    $anchor = ($rel -replace '[\\/]', '_')
    if ($depth -gt 0) { [void]$sb.AppendLine("        <h3 id='$anchor' class='mono'>$rel/</h3>") }
    [void]$sb.AppendLine('        <div class="table-wrap"><table class="dir"><thead><tr><th>Name</th><th>Last modified</th><th>Size</th></tr></thead><tbody>')
    if ($depth -gt 0) { [void]$sb.AppendLine("          <tr><td><a href='#' onclick='history.back();return false;'>Parent directory</a></td><td></td><td>-</td></tr>") }
    $children = Get-FilteredChildren $abs $rel $filter
    $dirs  = $children | Where-Object { $_.Item.PSIsContainer } | Sort-Object { $_.Item.Name }
    $files = $children | Where-Object { -not $_.Item.PSIsContainer } | Sort-Object { $_.Item.Name }
    foreach ($d in $dirs) {
        $subRel = "$rel/$($d.Item.Name)"
        $dAnchor = ($subRel -replace '[\\/]', '_')
        $dirCount.Value++
        [void]$sb.AppendLine("          <tr><td><a href='#$dAnchor'>$($d.Item.Name)/</a></td><td>$($d.Item.LastWriteTime.ToString('yyyy-MM-dd HH:mm'))</td><td>-</td></tr>")
    }
    foreach ($f in $files) {
        $size = if ($f.Item.Length -ge 1048576) { "{0:N1} MB" -f ($f.Item.Length / 1048576) } else { "$($f.Item.Length) B" }
        [void]$sb.AppendLine("          <tr><td class='mono'>$($f.Item.Name)</td><td>$($f.Item.LastWriteTime.ToString('yyyy-MM-dd HH:mm'))</td><td>$size</td></tr>")
    }
    [void]$sb.AppendLine('        </tbody></table></div>')
    foreach ($d in $dirs) { New-DirSection (Join-Path $abs $d.Item.Name) "$rel/$($d.Item.Name)" ($depth + 1) $filter $sb $dirCount }
}

function New-ModulePage($mod) {
    $slug = $mod.slug; $srcRel = $mod.src
    $srcAbs = Join-Path $RepoRoot $srcRel
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>")
    [void]$sb.AppendLine("<meta name='viewport' content='width=device-width, initial-scale=1'>")
    [void]$sb.AppendLine("<title>$($mod.title) — directory listing</title>")
    [void]$sb.AppendLine("<meta name='description' content='$($mod.desc)'>")
    [void]$sb.AppendLine("<link rel='icon' href='../../favicon.ico' sizes='16x16 32x32 48x48'>")
    [void]$sb.AppendLine("<link rel='icon' href='../../favicon.svg' type='image/svg+xml'>")
    [void]$sb.AppendLine("<link rel='apple-touch-icon' href='../../assets/img/logo.png'>")
    [void]$sb.AppendLine("<link rel='manifest' href='../../manifest.webmanifest'>")
    [void]$sb.AppendLine("<link rel='preconnect' href='https://fonts.googleapis.com'>")
    [void]$sb.AppendLine("<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>")
    [void]$sb.AppendLine("<link href='https://fonts.googleapis.com/css2?family=Noto+Sans:ital,wght@0,100..900;1,100..900&family=Noto+Serif:ital,wght@0,100..900;1,100..900&family=Noto+Serif+Display:ital,wght@0,100..900;1,100..900&family=Noto+Sans+Mono:wght@100..900&display=swap' rel='stylesheet'>")
    [void]$sb.AppendLine("<link rel='stylesheet' href='../../assets/css/main.css'>")
    [void]$sb.AppendLine("</head><body>")
    [void]$sb.AppendLine("<header class='site-header'><div class='masthead'>")
    [void]$sb.AppendLine("<a class='brand' href='../../index.html'><img src='../../assets/img/logo.png' alt='SuperUnicode logo'>")
    [void]$sb.AppendLine("<span><span class='brand-name'>Super<em>Unicode</em></span><br><span class='brand-tag'>Native character encoding of the OpenWindows kernel</span></span></a>")
    [void]$sb.AppendLine('</div><nav class="site-nav">')
    foreach ($item in $Nav) {
        $cls = if ($item.sec -contains 'modules') { " class='active'" } else { '' }
        [void]$sb.AppendLine("<a href='../../$($item.href)'$cls>$($item.label)</a>")
    }
    [void]$sb.AppendLine('</nav></header>')
    [void]$sb.AppendLine("<main class='page'>")
    [void]$sb.AppendLine("<nav class='breadcrumbs'><a href='../../index.html'>Home</a> &raquo; <a href='../index.html'>Repositories</a> &raquo; $($mod.title)</nav>")
    [void]$sb.AppendLine("<h1>Directory listing &mdash; <span class='grad'>$($mod.title)</span></h1>")
    [void]$sb.AppendLine("<p class='subtitle'>$($mod.desc)</p>")
    [void]$sb.AppendLine("<p class='note'>Physical source: <span class='mono'>$srcRel/</span> in the repository. Structure mirrors the versioned <span class='mono'>/Public</span> trees.</p>")
    $dc = 0; $ref = [ref]$dc
    New-DirSection $srcAbs '' 0 $mod.filter $sb $ref
    [void]$sb.AppendLine('</main>')
    [void]$sb.AppendLine("<footer class='site-footer'><div class='footer-inner'><span>SuperUnicode&reg; &mdash; native character encoding of the OpenWindows kernel.</span><span><a href='../../index.html'>Home</a> &middot; <a href='../../standard/index.html'>Standard</a> &middot; <a href='../../ucd/index.html'>SUCD</a> &middot; <a href='../../extended/index.html'>Extended</a></span></div></footer>")
    [void]$sb.AppendLine('</body></html>')
    $outDir = Join-Path $SiteRoot "modules\$slug"
    New-Item -ItemType Directory -Force -Path $outDir | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $outDir 'index.html'), $sb.ToString(), [System.Text.Encoding]::UTF8)
    Write-Host "generated modules/$slug/ ($($ref.Value) directories)"
}

function New-ModulesIndex {
    $sb = New-Object System.Text.StringBuilder
    [void]$sb.AppendLine("<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'>")
    [void]$sb.AppendLine("<meta name='viewport' content='width=device-width, initial-scale=1'>")
    [void]$sb.AppendLine('<title>Repositories — directory listings</title>')
    [void]$sb.AppendLine("<link rel='icon' href='../favicon.ico' sizes='16x16 32x32 48x48'>")
    [void]$sb.AppendLine("<link rel='icon' href='../favicon.svg' type='image/svg+xml'>")
    [void]$sb.AppendLine("<link rel='apple-touch-icon' href='../assets/img/logo.png'>")
    [void]$sb.AppendLine("<link rel='manifest' href='../manifest.webmanifest'>")
    [void]$sb.AppendLine("<link rel='preconnect' href='https://fonts.googleapis.com'>")
    [void]$sb.AppendLine("<link rel='preconnect' href='https://fonts.gstatic.com' crossorigin>")
    [void]$sb.AppendLine("<link href='https://fonts.googleapis.com/css2?family=Noto+Sans:ital,wght@0,100..900;1,100..900&family=Noto+Serif:ital,wght@0,100..900;1,100..900&family=Noto+Serif+Display:ital,wght@0,100..900;1,100..900&family=Noto+Sans+Mono:wght@100..900&display=swap' rel='stylesheet'>")
    [void]$sb.AppendLine("<link rel='stylesheet' href='../assets/css/main.css'>")
    [void]$sb.AppendLine('</head><body>')
    [void]$sb.AppendLine("<header class='site-header'><div class='masthead'>")
    [void]$sb.AppendLine("<a class='brand' href='../index.html'><img src='../assets/img/logo.png' alt='SuperUnicode logo'>")
    [void]$sb.AppendLine("<span><span class='brand-name'>Super<em>Unicode</em></span><br><span class='brand-tag'>Native character encoding of the OpenWindows kernel</span></span></a>")
    [void]$sb.AppendLine('</div><nav class="site-nav">')
    foreach ($item in $Nav) {
        $cls = if ($item.sec -contains 'modules') { " class='active'" } else { '' }
        [void]$sb.AppendLine("<a href='../$($item.href)'$cls>$($item.label)</a>")
    }
    [void]$sb.AppendLine('</nav></header>')
    [void]$sb.AppendLine("<main class='page'>")
    [void]$sb.AppendLine("<nav class='breadcrumbs'><a href='../index.html'>Home</a> &raquo; Repositories</nav>")
    [void]$sb.AppendLine("<h1>Repositories &mdash; <span class='grad'>directory listings</span></h1>")
    [void]$sb.AppendLine('<p class="subtitle">Live listings of every module in the SuperUnicode project.</p>')
    [void]$sb.AppendLine('<div class="grid">')
    foreach ($m in $Modules) { [void]$sb.AppendLine("<div class='card'><h3><a href='$($m.slug)/index.html'>$($m.title)</a></h3><p>$($m.desc)</p></div>") }
    [void]$sb.AppendLine('</div></main>')
    [void]$sb.AppendLine("<footer class='site-footer'><div class='footer-inner'><span>SuperUnicode&reg; &mdash; native character encoding of the OpenWindows kernel.</span><span><a href='../index.html'>Home</a> &middot; <a href='../standard/index.html'>Standard</a> &middot; <a href='../ucd/index.html'>SUCD</a></span></div></footer>")
    [void]$sb.AppendLine('</body></html>')
    [System.IO.File]::WriteAllText((Join-Path $SiteRoot 'modules\index.html'), $sb.ToString(), [System.Text.Encoding]::UTF8)
}

New-ModulesIndex
foreach ($m in $Modules) { New-ModulePage $m }

# ---------------------------------------------------------------------------
# 4. Assemble _site/
# ---------------------------------------------------------------------------
if (Test-Path $SiteOut) { Remove-Item -LiteralPath $SiteOut -Recurse -Force }
New-Item -ItemType Directory -Path $SiteOut | Out-Null
Get-ChildItem -LiteralPath $SiteRoot | Where-Object { $_.Name -notin @('_site', 'dispatch.ps1', 'pages') } | ForEach-Object {
    Copy-Item -LiteralPath $_.FullName -Destination $SiteOut -Recurse -Force
}
New-Item -ItemType File -Path (Join-Path $SiteOut '.nojekyll') -Force | Out-Null
$pageCount = $Pages.Count
Write-Host "site built: $pageCount pages + $(($Modules.Count)) module listings at $SiteOut"

# ---------------------------------------------------------------------------
# 5. Deploy to GitHub Pages (gh-pages branch)
# ---------------------------------------------------------------------------
if ($Deploy) {
    if (-not (Get-Command git -ErrorAction SilentlyContinue)) { throw 'git not found on PATH' }
    $work = Join-Path $env:TEMP 'sucs-ghpages'
    git -C $RepoRoot worktree remove --force $work 2>$null | Out-Null
    $branchExists = git -C $RepoRoot rev-parse --verify "$Branch" 2>$null
    if (-not $branchExists) { Write-Host "creating $Branch branch..."; git -C $RepoRoot branch $Branch }
    git -C $RepoRoot worktree add $work $Branch 2>&1 | Out-Null
    Get-ChildItem -LiteralPath $work | Where-Object { $_.Name -notin @('.git') } | Remove-Item -Recurse -Force
    Copy-Item -Path (Join-Path $SiteOut '*') -Destination $work -Recurse -Force
    git -C $work add -A
    git -C $work commit -m "deploy site $(Get-Date -Format 'yyyy-MM-dd HHmm')" 2>&1 | Out-Null
    git -C $work push origin $Branch 2>&1 | Out-Null
    git -C $RepoRoot worktree remove --force $work
    Write-Host "deployed to origin/$Branch — enable GitHub Pages with source = '$Branch' (branch root)"
}
