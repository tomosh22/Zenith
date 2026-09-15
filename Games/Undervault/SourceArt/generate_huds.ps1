# Original transparent decorative HUD layers. Geometry remains rendered by Zenith.
Add-Type -AssemblyName System.Drawing
$undervaultRoot=Split-Path $PSScriptRoot -Parent
$repoRoot=Split-Path (Split-Path $undervaultRoot -Parent) -Parent
$out=Join-Path $undervaultRoot 'Assets/Textures/UI'
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

function Icon($name,$x,$y,$size=58,$color='#cfdae3') {
 $state=$g.Save();$g.TranslateTransform($x,$y);$g.ScaleTransform($size/64.,$size/64.)
 switch($name) {
 'Dig' {Line 12 55 46 15 $color 8;Line 26 9 43 11 $color 9;Line 43 11 58 28 $color 7;Line 58 28 60 38 $color 3}
 'Build' {foreach($row in 0..2){foreach($col in 0..2){$xx=5+$col*19;$yy=8+$row*17;Line $xx $yy ($xx+14) $yy $color 12}}}
 'Carry' {Line 10 15 32 4 $color 5;Line 32 4 55 15 $color 5;Line 10 15 32 26 $color 5;Line 32 26 55 15 $color 5;Line 10 15 10 43 $color 5;Line 10 43 32 57 $color 5;Line 32 57 55 43 $color 5;Line 55 43 55 15 $color 5;Line 32 26 32 57 $color 5}
 'Repair' {Line 13 53 46 18 $color 10;Line 35 10 36 27 $color 8;Line 36 27 52 29 $color 8;Line 52 29 60 12 $color 8}
 'Cancel' {Line 13 13 52 52 $color 9;Line 52 13 13 52 $color 9}
 'Priority' {Line 32 57 32 10 $color 11;Line 12 29 32 9 $color 10;Line 32 9 52 29 $color 10}
 'Sweep' {Line 39 7 25 37 $color 6;Line 17 33 42 43 $color 9;foreach($i in 0..4){Line (15+$i*5) 36 (7+$i*8) 56 $color 4}}
 'Power' {Line 39 5 22 32 $color 10;Line 22 32 44 32 $color 9;Line 44 32 25 58 $color 10}
 'Water' {$p=[Drawing.Drawing2D.GraphicsPath]::new();$p.AddBezier(32,3,24,18,9,31,12,45);$p.AddBezier(12,45,14,64,52,64,53,43);$p.AddBezier(53,43,53,32,39,14,32,3);$b=Brush $color;$g.FillPath($b,$p);$b.Dispose();$p.Dispose();Line 20 41 23 50 '#eef9ff' 3}
 'Utilities' {Line 8 54 8 32 $color 12;Line 8 32 48 32 $color 12;Line 48 32 48 7 $color 12;Line 2 45 16 45 $color 4;Line 39 17 58 17 $color 4}
 'Overlays' {foreach($yy in @(15,28,41)){Line 8 $yy 32 ($yy+12) $color 5;Line 32 ($yy+12) 56 $yy $color 5}}
 'Menu' {foreach($yy in @(13,31,49)){Line 10 $yy 54 $yy $color 5}}
 };$g.Restore($state)
}
function Bar($x,$y,$w,$pct,$color) {Panel $x $y $w 12 '#34444e' 4;Panel $x $y ($w*$pct) 12 $color 4 255}
function Button($name,$x,$y,$w=120,$selected=$false,$label='') {
 $col=if($selected){'#f4bb42'}else{'#14212d'};$fg=if($selected){'#26313b'}else{'#cedde7'}
 Panel $x $y $w 126 $col 16;Icon $name ($x+($w-58)/2) ($y+14) 58 $fg
 if(!$label){$label=$name};Text $label ($x+15) ($y+92) 24 $fg
}
foreach($view in @('Colony','Planning','Flooding')) {
 $bitmap=[Drawing.Bitmap]::new(1920,1080,[Drawing.Imaging.PixelFormat]::Format32bppArgb)
 $g=[Drawing.Graphics]::FromImage($bitmap);$g.SmoothingMode='AntiAlias';$g.TextRenderingHint='AntiAliasGridFit';$g.Clear([Drawing.Color]::Transparent)
 if($view -eq 'Colony') {
  $xs=@(356,708,1010,1312);$ws=@(332,280,280,276);$labels=@('O₂','Food','Power','Cycle 3');$values=@('72%','68%','54%','Day 2');$cols=@('#55d7cf','#efc661','#f6a447','#b7c8dc')
  for($i=0;$i -lt 4;$i++){Panel $xs[$i] 14 $ws[$i] 80;Text $labels[$i] ($xs[$i]+20) 29 28 $cols[$i];if($i -lt 3){Bar ($xs[$i]+112) 53 ($ws[$i]-195) (.72-$i*.09) $cols[$i];Text $values[$i] ($xs[$i]+$ws[$i]-65) 43 25}else{Text 'Day 2' ($xs[$i]+155) 49 22}}
  Panel 1810 14 83 80;Icon Menu 1829 31 45
  Panel 32 871 526 174 '#101a23' 23;Button Dig 47 890 150;Button Carry 218 890 150 $false 'Move';Button Repair 389 890 150 $false 'Build'
  Panel 1410 884 477 155 '#13202a' 23;Icon Carry 1437 927 62 '#b89b66';Text 'Sandstone' 1530 909 29;Bar 1531 958 192 .54 '#55c7bc';Text 'Sturdy' 1532 983 22 '#aebdca';Button Overlays 1743 899 127 $false 'Select'
 } elseif($view -eq 'Planning') {
  Panel 22 14 297 83;Text 'Cycle 12' 44 26 28;Text '3 / 6  Vaulters' 45 62 22 '#a9c3d7'
  $labels=@('120 W','320 L','O₂  21%','18°C');$cols=@('#ffd058','#6fceff','#76d5ff','#f59180')
  for($i=0;$i -lt 4;$i++){$x=344+$i*210;Panel $x 14 195 83;Text $labels[$i] ($x+19) 30 27 $cols[$i];Bar ($x+19) 72 157 (.65-$i*.12) $cols[$i]}
  Panel 1545 14 347 83;Text 'Undervault' 1566 27 28;Text 'Deeper is a brighter tomorrow' 1566 63 18 '#a5becf'
  Text '-28°C' 64 135 31 '#96e6ff';Text 'Ice Biome' 64 178 24 '#96e6ff';Text '412°C' 1731 135 31 '#ffae70';Text 'Forge Biome' 1690 178 24 '#ffae70'
  Panel 25 833 485 59;Text 'Apply' 63 850 24 '#7ddb8c';Text 'Undo' 239 850 24;Text 'Done' 398 850 24
  Panel 24 904 569 149;Button Dig 38 916 125 $true;Button Sweep 177 916 125;Button Priority 316 916 125;Button Cancel 455 916 125
  Panel 1172 904 720 149;$names=@('Build','Utilities','Power','Water','Overlays');$labels=@('Build','Utilities','Power','Plumbing','Overlays')
  for($i=0;$i -lt 5;$i++){Button $names[$i] (1188+$i*139) 916 127 $false $labels[$i]}
  Panel 1662 609 230 272 '#0f1b25' 18;$labels=@('Oxygen','Carbon dioxide','Cold','Hot','Dig designation','Build ghost');$colors=@('#75beff','#68a376','#50cced','#ff6946','#f9c753','#7cc7ff')
  for($i=0;$i -lt 6;$i++){$yy=633+$i*38;Panel 1680 $yy 19 19 $colors[$i] 3;Text $labels[$i] 1713 ($yy-2) 19}
  Panel 399 741 267 68;Text 'Dig sandstone' 419 751 24;Text '4 cells selected' 419 783 19 '#9cb8cc'
 } else {
  Text 'U N D E R V A U L T' 28 20 28;Text 'PEOPLE . DEPTH . TOMORROW' 30 57 14 '#9bb3c3'
  $labels=@('12','86%','43%','O₂ 12%','CO₂ 4.1%','78%');$cols=@('#cbd9e5','#f9ab9d','#65c4f6','#8edf70','#ff6965','#fbd277')
  for($i=0;$i -lt 6;$i++){$x=450+$i*132;Panel $x 13 122 60;Text $labels[$i] ($x+12) 29 24 $cols[$i]}
  Panel 1454 12 171 63;Text 'DAY 27   14:32' 1466 21 20;Text 'DEPTH  -3' 1466 48 17 '#a8bfd0';Panel 1643 12 249 63;Text 'II     >>    3x' 1670 26 31 '#77c7f4'
  $titles=@('Water Breach','Low Oxygen','High CO₂','Evacuate');$sub=@('Lower Chamber','12% and falling','4.1% and rising','Move to upper levels')
  for($i=0;$i -lt 4;$i++){$yy=108+$i*88;$c=if($i -lt 3){'#421b22'}else{'#142733'};Panel 22 $yy 271 78 $c 12;Text $titles[$i] 43 ($yy+10) 27 $(if($i -lt 3){'#ff7778'}else{'#b6d1e4'});Text $sub[$i] 43 ($yy+47) 20 '#ceb8b7'}
  Line 30 584 30 859 '#69c6f1' 3;for($i=0;$i -lt 4;$i++){$yy=584+$i*90;Line 30 $yy 43 $yy '#69c6f1' 3;Text "$(-$i) m" 54 ($yy-12) 19 '#94c9e4'}
  Text 'WATER DEPTH' 230 783 23 '#57bcdf';Text '~ 2.1 m' 230 815 25 '#82dbf6'
  Panel 22 914 1870 144 '#172b36' 20;Panel 37 925 98 119 '#101c25' 14
  $b=Brush '#533726';$g.FillEllipse($b,50,946,70,85);$b.Dispose();$b=Brush '#d8a176';$g.FillEllipse($b,60,952,55,63);$b.Dispose()
  $b=Brush '#f1bd43';$g.FillEllipse($b,48,930,78,46);$g.FillRectangle($b,44,962,85,9);$g.FillRectangle($b,58,1011,64,25);$b.Dispose()
  $b=Brush '#192329';$g.FillEllipse($b,70,980,6,10);$g.FillEllipse($b,97,980,6,10);$b.Dispose();Line 79 1002 91 1003 '#794b36' 2;Line 70 1013 77 1034 '#76858c' 5;Line 109 1013 104 1034 '#76858c' 5
  Text 'Mara' 150 934 31;Text 'Builder' 151 976 22 '#a9c3d4';Bar 151 1019 170 .66 '#7ad778';Line 351 934 351 1038 '#5c7888' 2
  Icon Dig 383 946 62;Text 'Digging Escape Channel' 471 935 29;Text 'Clearing rock for a safe route...' 471 976 22 '#aec3d1';Bar 473 1021 502 .62 '#55baf1';Text '62%' 993 1005 25
  $names=@('Dig','Build','Carry','Repair','Cancel');for($i=0;$i -lt 5;$i++){Button $names[$i] (1190+$i*139) 925 124}
 }
 $bitmap.Save((Join-Path $out "HUD$view.png"),[Drawing.Imaging.ImageFormat]::Png);$g.Dispose();$bitmap.Dispose()
}
$fonts.Dispose()
