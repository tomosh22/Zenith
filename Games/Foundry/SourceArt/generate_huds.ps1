# Original transparent decorative HUD layers. Geometry remains rendered by Zenith.
Add-Type -AssemblyName System.Drawing
$foundryRoot=Split-Path $PSScriptRoot -Parent
$repoRoot=Split-Path (Split-Path $foundryRoot -Parent) -Parent
$out=Join-Path $foundryRoot 'Assets/Textures/UI'
$fonts=[System.Drawing.Text.PrivateFontCollection]::new()
$fonts.AddFontFile((Join-Path $repoRoot 'Middleware/imgui-docking/misc/fonts/Roboto-Medium.ttf'))
$family=$fonts.Families[0]
$script:images=@{}
function Col($hex,$alpha=255) { $v=[Drawing.ColorTranslator]::FromHtml($hex); [Drawing.Color]::FromArgb($alpha,$v.R,$v.G,$v.B) }
function Brush($c) { [Drawing.SolidBrush]::new((Col $c)) }
function Panel($x,$y,$w,$h,$color='#1b242d',$radius=15,$alpha=242) {
    $p=[Drawing.Drawing2D.GraphicsPath]::new();$r=$radius*2
    $p.AddArc($x,$y,$r,$r,180,90);$p.AddArc($x+$w-$r,$y,$r,$r,270,90)
    $p.AddArc($x+$w-$r,$y+$h-$r,$r,$r,0,90);$p.AddArc($x,$y+$h-$r,$r,$r,90,90);$p.CloseFigure()
    $b=[Drawing.SolidBrush]::new((Col $color $alpha));$g.FillPath($b,$p);$b.Dispose()
    $pen=[Drawing.Pen]::new((Col '#65717a' 110),1.5);$g.DrawPath($pen,$p);$pen.Dispose();$p.Dispose()
}
function Text($text,$x,$y,$size=23,$color='#eef0f1') {
    $f=[Drawing.Font]::new($family,[float]$size,[Drawing.FontStyle]::Regular,[Drawing.GraphicsUnit]::Pixel)
    $b=Brush $color;$g.DrawString([string]$text,$f,$b,[float]$x,[float]$y,[Drawing.StringFormat]::GenericTypographic);$b.Dispose();$f.Dispose()
}
function Line($x,$y,$xx,$yy,$color='#d9e0e5',$width=3) {
    $p=[Drawing.Pen]::new((Col $color),[float]$width);$g.DrawLine($p,[float]$x,[float]$y,[float]$xx,[float]$yy);$p.Dispose()
}
function Icon($name,$x,$y,$size=42,$color='#e5eaf0') {
    if($name -in @('Rail','Splitter','Merger','Lift','Station','Radar','Attack','Ore','Foundry')) {
        $state=$g.Save();$g.TranslateTransform($x,$y);$g.ScaleTransform($size/64.,$size/64.)
        switch($name) {
            'Rail' {Line 18 4 18 60 $color 5;Line 46 4 46 60 $color 5;foreach($yy in @(12,22,32,42,52)){Line 10 $yy 54 $yy $color 4}}
            'Splitter' {Line 32 57 32 18 $color 12;Line 12 18 52 18 $color 12;Line 12 18 12 7 $color 12;Line 52 18 52 7 $color 12}
            'Merger' {Line 32 57 32 33 $color 12;Line 10 10 32 33 $color 12;Line 54 10 32 33 $color 12}
            'Lift' {Line 18 6 18 59 $color 4;Line 46 6 46 59 $color 4;Line 18 45 46 45 $color 12;Line 32 33 32 13 $color 4;Line 32 13 25 22 $color 4;Line 32 13 39 22 $color 4}
            'Station' {Line 8 18 56 18 $color 6;Line 12 20 12 55 $color 4;Line 52 20 52 55 $color 4;Line 9 53 55 53 $color 5;Line 23 30 41 30 $color 10;Line 23 42 41 42 $color 10}
            'Radar' {$p=[Drawing.Pen]::new((Col $color),3);$g.DrawEllipse($p,7,7,50,50);$g.DrawEllipse($p,18,18,28,28);$p.Dispose();Line 32 32 53 8 $color 5;Line 32 2 32 12 $color 3;Line 32 52 32 62 $color 3;Line 2 32 12 32 $color 3;Line 52 32 62 32 $color 3}
            'Attack' {$b=Brush $color;$g.FillEllipse($b,13,8,38,39);$g.FillRectangle($b,23,39,19,15);$b.Dispose();$b=Brush '#25272b';$g.FillEllipse($b,20,23,9,12);$g.FillEllipse($b,36,23,9,12);$b.Dispose();Line 12 48 6 57 $color 4;Line 52 48 58 57 $color 4}
            'Ore' {$pts=[Drawing.PointF[]]@([Drawing.PointF]::new(9,23),[Drawing.PointF]::new(27,7),[Drawing.PointF]::new(48,13),[Drawing.PointF]::new(57,40),[Drawing.PointF]::new(37,57),[Drawing.PointF]::new(12,49));$b=Brush $color;$g.FillPolygon($b,$pts);$b.Dispose();Line 27 7 31 32 '#71859b' 3;Line 31 32 12 49 '#71859b' 3;Line 31 32 57 40 '#71859b' 3}
            'Foundry' {$b=Brush $color;foreach($ox in @(3,23,43)){$pts=[Drawing.PointF[]]@([Drawing.PointF]::new($ox,54),[Drawing.PointF]::new(($ox+10),(12+[Math]::Abs($ox-23))),[Drawing.PointF]::new(($ox+20),54));$g.FillPolygon($b,$pts)};$b.Dispose()}
        }
        $g.Restore($state);return
    }
    if (!$script:images.ContainsKey($name)) {$script:images[$name]=[Drawing.Image]::FromFile((Join-Path $out "$name.png"))}
    $im=$script:images[$name];$c=Col $color;$matrix=[Drawing.Imaging.ColorMatrix]::new()
    $matrix.Matrix00=$c.R/242.;$matrix.Matrix11=$c.G/242.;$matrix.Matrix22=$c.B/242.
    $attr=[Drawing.Imaging.ImageAttributes]::new();$attr.SetColorMatrix($matrix)
    $g.DrawImage($im,[Drawing.Rectangle]::new($x,$y,$size,$size),0,0,$im.Width,$im.Height,[Drawing.GraphicsUnit]::Pixel,$attr);$attr.Dispose()
}
function Progress($x,$y,$w,$pct,$color) { Panel $x $y $w 9 '#414d53' 3;Panel $x $y ($w*$pct) 9 $color 3 255 }
function Badge($text,$x,$y,$color) {Panel $x $y 32 32 $color 16 255;Text $text ($x+10) ($y+4) 23}
foreach($view in @('Workshop','Logistics','MapDefense')) {
    $bitmap=[Drawing.Bitmap]::new(1920,1080,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
    $g=[Drawing.Graphics]::FromImage($bitmap);$g.SmoothingMode='AntiAlias';$g.TextRenderingHint='AntiAliasGridFit';$g.Clear([Drawing.Color]::Transparent)
    if($view -eq 'Workshop') {
        $labels=@('Low Power','Storage Full','Iron Ore Depleted');$icons=@('Alert','Logistics','Ore');$colors=@('#f56953','#ffcf4a','#a6b4c5')

        for($i=0;$i -lt 3;$i++) {
            $y=18+$i*70;Panel 18 $y 342 64;Icon $icons[$i] 36 ($y+12) 38 $colors[$i];Text $labels[$i] 91 ($y+20) 23
            Badge '1' 314 ($y+16) $(if($i -eq 1){'#ffc842'}else{'#e66050'})
        }
        Panel 694 15 527 81;Icon Science 712 31 48;Text 'Research: Logistics I' 782 29 23;Progress 782 69 360 .68 '#47c978';Text '68%' 1154 60 22
        Panel 1810 18 82 74;Line 1836 53 1870 53 '#e7edf1' 5;Line 1836 53 1850 39 '#e7edf1' 5;Line 1836 53 1850 67 '#e7edf1' 5
        Panel 110 887 1700 168 '#161e27' 25
        $labels=@('Machines','Belts','Power','Logistics','Production','Science','Decor','');$icons=@('Machines','Belts','Power','Logistics','Production','Science','Decor','More')
        for($i=0;$i -lt 8;$i++) {$x=126+$i*209;$selected=$i -eq 0;Panel $x 902 188 137 $(if($selected){'#ffcf45'}else{'#2b343e'}) 19 255
            Icon $icons[$i] ($x+61) 921 66 $(if($selected){'#24313c'}else{'#e5eaf0'});Text $labels[$i] ($x+28) 1006 23 $(if($selected){'#17222a'}else{'#f0f2f3'})}
    } elseif($view -eq 'Logistics') {
        Panel 27 18 330 82 '#1b222b' 12 188;Icon Foundry 43 31 48;Text 'Foundry' 107 27 35;Text 'Build  Automate  Expand' 107 68 16
        Panel 26 116 188 162 '#1b222b' 13 230
        $values=@('1,240','720 / 800','86 / 120','42 / 60');$icons=@('Production','Alert','Logistics','Science');$colors=@('#d1dae5','#ffd339','#70c9fb','#49c9ff')
        for($i=0;$i -lt 4;$i++) {Icon $icons[$i] 40 (130+$i*35) 27 $colors[$i];Text $values[$i] 82 (132+$i*35) 21}
        Panel 1520 16 371 49;Text 'Day 37   14:20      >  >>' 1540 30 20 '#edf1f5'
        Panel 1570 90 324 676 '#141d27' 14 250;Text 'Assembler Mk.2' 1590 115 26;Text 'Electronic Circuit' 1590 164 20
        $state=$g.Save();$g.TranslateTransform(1730,260);$g.RotateTransform(-25);Icon Circuit -74 -74 148;$g.Restore($state)
        Text '2 plates   +   1 coil' 1608 364 21;Text 'v' 1724 399 25 '#6ad9cc'
        $state=$g.Save();$g.TranslateTransform(1730,457);$g.RotateTransform(-25);Icon Circuit -31 -31 62;$g.Restore($state)
        Text '60 / min' 1690 509 23;Panel 1590 550 284 44 '#1b2a31' 9;Text 'Producing' 1639 561 21 '#89e88a'
        $script:images['Throughput']=[Drawing.Image]::FromFile((Join-Path $out 'Throughput.png'))
        # The chart image is deliberately wide rather than square.
        $g.DrawImage($script:images['Throughput'],[Drawing.Rectangle]::new(1590,609,270,55))
        Panel 1588 680 288 80 '#141d27' 4 255;Text 'Throughput (items/min)' 1592 680 17;Text '100%           25 MW' 1610 728 23
        $labels=@('Iron Ore','Copper Ore','Stone','Central Station');$rates=@('240/min','180/min','300/min','Inbound - Outbound');$xx=@(330,264,1320,1210);$yy=@(155,574,740,183)
        for($i=0;$i -lt 4;$i++) {Panel $xx[$i] $yy[$i] 190 64 '#1a2227' 8 230;Text $labels[$i] ($xx[$i]+12) ($yy[$i]+9) 21;Text $rates[$i] ($xx[$i]+12) ($yy[$i]+35) 17}
        Panel 145 920 445 128; $labels=@('Logistics','Production','Power','Rail');$icons=@('Belts','Machines','Alert','Rail')
        for($i=0;$i -lt 4;$i++) {$x=157+$i*106;if($i -eq 0){Panel $x 928 100 110 '#184371' 12};Icon $icons[$i] ($x+28) 939 45;Text $labels[$i] ($x+8) 1000 17}
        Panel 612 920 943 128;$labels=@('Conveyor Belt','Splitter','Merger','Lift','Storage','Rail','Train Station');$icons=@('Belts','Splitter','Merger','Lift','Logistics','Rail','Station')
        for($i=0;$i -lt 7;$i++) {$x=623+$i*132;Panel $x 932 121 104 $(if($i -eq 0){'#234574'}else{'#2b323c'}) 10;Icon $icons[$i] ($x+36) 943 48;Text $labels[$i] ($x+8) 1007 16}
        Panel 1320 837 210 58;Icon Belts 1330 846 36;Text 'Belt' 1380 844 20;Text 'Drag to place' 1380 870 16
    } else {
        Icon Logistics 22 18 47 '#57a4cf';Text 'FOUNDRY' 88 20 32;Text 'PLAN  -  AUTOMATE  -  DEFEND' 89 59 16
        $labels=@('Pollution','Power','Logistics','Radar');$values=@('72%','86%','94%','Scanning');$icons=@('Production','Alert','Logistics','Radar');$colors=@('#a2aeb5','#55ccbb','#64d99e','#63d5b1')
        for($i=0;$i -lt 4;$i++) {$x=735+$i*230;Panel $x 12 222 70 '#162327' 5;Icon $icons[$i] ($x+13) 31 36 $colors[$i];Text $labels[$i] ($x+62) 24 17;Text $values[$i] ($x+62) 47 17;Progress ($x+62) 70 140 (.72+$i*.06) $colors[$i]}
        Panel 1742 12 157 70 '#162327' 12;Text 'Day 238' 1763 25 20;Text '14:20' 1763 51 18
        Panel 13 105 360 94 '#292526' 12 241;Line 17 116 17 188 '#ee6055' 4;Icon Attack 35 134 39 '#ef665c';Text 'NATIVE ATTACK' 91 120 24 '#ff7765';Text 'Large group approaching' 91 151 18;Text 'East Wall' 91 175 17
        Panel 13 209 360 94 '#2b2923' 12 241;Line 17 220 17 292 '#f1c14b' 4;Icon Logistics 35 234 39 '#ffcc49';Text 'SUPPLY LOW' 91 223 24 '#ffcc49';Text '3 resources low' 91 254 18;Text 'Iron Plates, Circuits, Fuel' 91 277 17
        $labels=@('Copper','Iron','Uranium','Coal','Stone');$xx=@(500,298,590,1485,1570);$yy=@(158,518,885,166,865)
        for($i=0;$i -lt 5;$i++) {Panel $xx[$i] $yy[$i] 125 37 '#1b2725' 6 240;Text $labels[$i] ($xx[$i]+13) ($yy[$i]+8) 19}
        Panel 1725 468 170 76 '#60312b' 6 190;Text 'Hostiles' 1741 478 24 '#ff9a87';Text '~ 3 min' 1741 510 19 '#ffb1a0'
        Panel 16 819 290 244 '#142125' 15;$script:images['Minimap']=[Drawing.Image]::FromFile((Join-Path $out 'Minimap.png'));$g.DrawImage($script:images['Minimap'],[Drawing.Rectangle]::new(30,833,262,216))
        Panel 424 1005 1100 56 '#18262a' 15;Text 'Pollution (high)     Power Field       Logistics (rails)       Radar Range       Defensive Wall' 448 1024 18
        Panel 1590 940 309 115 '#19262a' 14;Text 'Selected' 1610 959 17;Text 'Nothing selected' 1610 1001 22 '#a2acb5'
        for($i=0;$i -lt 3;$i++) {Panel 1820 (658+$i*86) 76 73 '#17262b' 13;Text (@('+','-','o')[$i]) 1846 (669+$i*86) 39}
    }
    $g.Dispose();$bitmap.Save((Join-Path $out "HUD$view.png"),[Drawing.Imaging.ImageFormat]::Png);$bitmap.Dispose()
}
foreach($im in $script:images.Values){$im.Dispose()};$fonts.Dispose()
Write-Output 'FOUNDRY_HUDS_COMPLETE'
