param([switch]$SelfTest, [switch]$CheckGui, [string]$ReviewFolder)
$ErrorActionPreference = 'Stop'

$RxHeader = 'seq_enlace,seq_rx,tecnica,residuo,decisao,status,payload_confere,verificacao_us,intervalo_inicio_us,recuperacao_ms,corpo_rx_hex,corpo_tx_hex,motivo'
$NoiseHeader = 'seq,modo,byte_payload,mascara_alvo_hex,timestamp_us,antes_hex,depois_amostrado_hex,mascara_mudou_hex,mudou,sincronismo_ok,atraso_amostra_max_us,amostras_tardias_hex,pulsos_tardios_hex,duracao_byte_us'

function New-RunState([string]$Technique, [int]$Mode, [int]$Count) {
    [pscustomobject]@{
        Technique=$Technique; Mode=$Mode; Count=$Count; Prepared=$false; NoiseAck=$false
        RxRows=[Collections.Generic.List[string]]::new(); NoiseRows=[Collections.Generic.List[string]]::new()
        Diagnostics=[Collections.Generic.List[string]]::new(); Summary=$null; Validation=$null
        RawRx=[Collections.Generic.List[string]]::new(); RawNoise=[Collections.Generic.List[string]]::new()
    }
}

function Add-RunLine($State, [string]$Source, [string]$Line) {
    $Line = $Line.TrimEnd([char]13)
    if ($Source -eq 'RX') {
        $State.RawRx.Add($Line)
        if ($Line -match '^# PREPARADO_RX_COMPLETO,([^,]+),(\d+),') {
            if ($State.Prepared) { $State.Diagnostics.Add('RX reiniciou a preparacao durante a coleta.') }
            if ($Matches[1] -eq $State.Technique -and [int]$Matches[2] -eq $State.Count) { $State.Prepared=$true }
            else { $State.Diagnostics.Add('Preparacao RX diferente da configuracao escolhida.') }
        }
        if (-not $State.Prepared) { return }
        if ($Line -match '^\d+,') { $State.RxRows.Add($Line) }
        if ($Line -match '^# RESUMO,') { $State.Summary=$Line }
        if ($Line -match '^# VALIDACAO,') { $State.Validation=$Line }
        if ($Line -match '^# (REFERENCIA_INVALIDA|SEQUENCIA_NAO_CONFIAVEL|RODADA_INCOMPLETA_OU_INVALIDA)') {
            $State.Diagnostics.Add($Line)
        }
    } else {
        $State.RawNoise.Add($Line)
        if ($Line -match '^# MODO_PROXIMO_QUADRO,(\d+)$') {
            if ([int]$Matches[1] -eq $State.Mode) { $State.NoiseAck=$true }
            else { $State.Diagnostics.Add('Injetor confirmou outro modo.') }
        }
        if ($Line -match '^\d+,') { $State.NoiseRows.Add($Line) }
        if ($Line -match '^# (SINCRONISMO_INVALIDO|DIAG|TIMEOUT)') { $State.Diagnostics.Add($Line) }
    }
}


function Read-HexBody([string]$Hex) {
    if ($Hex -notmatch '^[0-9A-Fa-f]{16}$') { return $null }
    $v=[byte[]]::new(8)
    for ($i=0; $i -lt 8; $i++) { $v[$i]=[Convert]::ToByte($Hex.Substring(2*$i,2),16) }
    return ,$v
}
function Get-BodyResidue([byte[]]$Bytes,[string]$Technique) {
    $r=0
    foreach ($b in $Bytes) {
        if ($Technique -eq 'SUM8') { $r=($r+$b)-band 255 }
        else {
            $r=$r-bxor $b
            for ($i=0;$i -lt 8;$i++) { if ($r -band 128) { $r=(($r-shl 1)-bxor 7)-band 255 } else { $r=($r-shl 1)-band 255 } }
        }
    }
    return $r
}
function Get-CrossIssues($State) {
    $problems=[Collections.Generic.List[string]]::new(); $records=@{}; $targets=@{}
    foreach ($line in $State.RxRows) {
        $f=$line.Split(',')
        if ($f.Count -ne 13 -or $f[0] -notmatch '^\d+$' -or $f[5] -eq 'PERDIDO') { continue }
        $seq=[int]$f[0]; $rx=Read-HexBody $f[10]; $tx=Read-HexBody $f[11]
        if ($null -eq $rx -or $null -eq $tx) { $problems.Add("RX: corpos ausentes/invalidos na sequencia $seq."); continue }
        if ($rx[0] -ne 7 -or $tx[0] -ne 7 -or ($rx[1]+256*$rx[2]) -ne $seq -or ($tx[1]+256*$tx[2]) -ne $seq -or $f[1] -ne [string]$seq) {
            $problems.Add("RX: corpo LEN/sequencia divergente em $seq.")
        }
        $r=Get-BodyResidue $rx $State.Technique
        if ($f[3] -ne [string]$r -or (Get-BodyResidue $tx $State.Technique) -ne 0) { $problems.Add("RX: residuo/copia diverge do recalculo em $seq.") }
        $same=$true
        for ($i=3;$i -le 6;$i++) { if ($rx[$i] -ne $tx[$i]) { $same=$false } }
        $expected=if ($r -ne 0) { 'CORROMPIDO' } elseif ($same) { 'OK' } else { 'NAO_DETECTADO' }
        $decision=if ($r -eq 0) { 'ACEITO' } else { 'DESCARTADO' }
        $conf=if ($same) { 'SIM' } else { 'NAO' }
        if ($f[5] -ne $expected -or $f[4] -ne $decision -or $f[6] -ne $conf) { $problems.Add("RX: decisao/status/comparacao nao confere com os corpos em $seq.") }
        $records[$seq]=[pscustomobject]@{RX=$rx; TX=$tx}
        $targets[$seq]=[int[]]::new(4)
    }
    foreach ($line in $State.NoiseRows) {
        $n=$line.Split(',')
        if ($n.Count -ne 14 -or $n[0] -notmatch '^\d+$' -or $n[2] -notmatch '^[0-3]$') { continue }
        $seq=[int]$n[0]; $byte=[int]$n[2]
        if (-not $records.ContainsKey($seq)) { continue } # perdas nao tem corpo checado para comparar
        try {
            $before=[Convert]::ToInt32($n[5],16); $after=[Convert]::ToInt32($n[6],16); $mask=[Convert]::ToInt32($n[3],16)
            if ($records[$seq].TX[3+$byte] -ne $before -or $records[$seq].RX[3+$byte] -ne $after) {
                $problems.Add("CRUZAMENTO: amostra do RUIDO difere dos corpos RX/copia em seq=$seq,byte_payload=$byte,mesmo_se_sincronismo_ok=$($n[9]).")
            }
            $targets[$seq][$byte]=$targets[$seq][$byte]-bor $mask
        } catch { $problems.Add("CRUZAMENTO: campo hexadecimal invalido em seq=$seq.") }
    }
    foreach ($seq in $records.Keys) {
        $r=$records[$seq]
        for ($i=0;$i -lt 8;$i++) {
            $mask=if ($i -ge 3 -and $i -le 6) { $targets[$seq][$i-3] } else { 0 }
            $delta=$r.TX[$i]-bxor $r.RX[$i]
            if (($delta-band (255-bxor $mask)) -ne 0 -or (($r.RX[$i]-band $delta) -ne 0)) {
                $problems.Add("CRUZAMENTO: alteracao fora dos alvos ou diferente de 1->0 em seq=$seq,byte_corpo=$i.")
            }
        }
    }
    return ,$problems
}

function Get-RunReview($State, [bool]$Aborted=$false) {
    $issues = [Collections.Generic.List[string]]::new()
    foreach ($msg in $State.Diagnostics) { $issues.Add($msg) }
    if ($Aborted) { $issues.Add('Coleta interrompida; arquivos parciais preservados.') }
    if (-not $State.Prepared) { $issues.Add('Sem confirmacao do RX completo preparado.') }
    if (-not $State.NoiseAck) { $issues.Add('Sem confirmacao do modo do injetor.') }
    if (-not $State.Validation -or $State.Validation -notmatch 'rodada_valida=SIM(?:,|$)') { $issues.Add('RX nao confirmou rodada_valida=SIM.') }
    if ($State.RxRows.Count -ne $State.Count) { $issues.Add("RX: $($State.RxRows.Count) linhas; esperado $($State.Count).") }
    $seen = @{}; $noiseSeen = @{}; $noiseBits = @{}; $noiseBytes = @{}
    $counts = @{OK=0; CORROMPIDO=0; PERDIDO=0; NAO_DETECTADO=0}
    foreach ($line in $State.RxRows) {
        $f = $line.Split(',')
        if ($f.Count -ne 13 -or $f[0] -notmatch '^\d+$') { $issues.Add('Linha RX com formato inesperado.'); continue }
        $seq = [int]$f[0]
        if ($seq -ge $State.Count -or $seen.ContainsKey($seq)) { $issues.Add("RX: sequencia fora da janela ou repetida: $seq.") }
        $seen[$seq] = $true
        if ($f[2] -ne $State.Technique) { $issues.Add("RX: tecnica divergente em $seq.") }
        if ($counts.ContainsKey($f[5])) { $counts[$f[5]]++ } else { $issues.Add("RX: status sem classificacao em $seq.") }
    }
    foreach ($line in $State.NoiseRows) {
        $f = $line.Split(',')
        if ($f.Count -ne 14 -or $f[0] -notmatch '^\d+$' -or $f[1] -notmatch '^\d+$' -or $f[2] -notmatch '^[0-3]$' -or $f[3] -notmatch '^[0-9A-Fa-f]{2}$') {
            $issues.Add('Linha RUIDO com formato inesperado.'); continue
        }
        $seq = [int]$f[0]; $byte = [int]$f[2]; $key = "$seq/$byte"
        if ($seq -ge $State.Count -or $noiseBytes.ContainsKey($key)) { $issues.Add("RUIDO: sequencia/byte fora da janela ou repetido: $key.") }
        $noiseBytes[$key]=$true; $noiseSeen[$seq]=$true
        if ([int]$f[1] -ne $State.Mode) { $issues.Add("RUIDO: modo divergente em $seq.") }
        if ($f[9] -ne 'SIM') { $issues.Add("RUIDO: sincronismo nao confirmado em $seq.") }
        try {
            $mask=[Convert]::ToInt32($f[3],16); $a=[Convert]::ToInt32($f[5],16); $b=[Convert]::ToInt32($f[6],16); $changed=[Convert]::ToInt32($f[7],16)
            if (($a -bxor $b) -ne $changed -or ($a -band (255 -bxor $mask)) -ne $b) { $issues.Add("RUIDO: amostra/efeito inconsistentes em $key.") }
            if (($changed -eq 0 -and $f[8] -ne 'NAO') -or ($changed -ne 0 -and $f[8] -ne 'SIM')) { $issues.Add("RUIDO: indicador mudou inconsistente em $key.") }
            $bits=0; for ($j=0; $j -lt 8; $j++) { if ($mask -band (1 -shl $j)) { $bits++ } }
            if (-not $noiseBits.ContainsKey($seq)) { $noiseBits[$seq]=0 }
            $noiseBits[$seq]+=$bits
            if ($State.Mode -eq 3) {
                $shifted=$mask
                if ($shifted) { while (($shifted -band 1) -eq 0) { $shifted=$shifted -shr 1 } }
                if ($bits -lt 3 -or $bits -gt 8 -or ($shifted -band ($shifted+1)) -ne 0) { $issues.Add("RUIDO: rajada alvo nao contigua de 3..8 bits em $key.") }
            }
            if ($State.Mode -eq 4 -and ($byte -gt 1 -or $mask -ne 128)) { $issues.Add("RUIDO: padrao extra 4 divergente em $key.") }
        } catch { $issues.Add("RUIDO: campos hexadecimais invalidos em $key.") }
    }
    if ($seen.Count -ne $State.Count) { $issues.Add('RX: faltam sequencias unicas da janela.') }
    if ($noiseSeen.Count -ne $State.Count) { $issues.Add('RUIDO: faltam logs de quadros da janela.') }
    for ($seq=0; $seq -lt $State.Count; $seq++) {
        $n=0; for ($byte=0; $byte -lt 4; $byte++) { if ($noiseBytes.ContainsKey("$seq/$byte")) { $n++ } }
        $bits=0; if ($noiseBits.ContainsKey($seq)) { $bits=$noiseBits[$seq] }
        $ok = switch ($State.Mode) {
            0 { $n -eq 4 -and $bits -eq 0 }
            1 { $n -eq 1 -and $bits -eq 1 }
            2 { $n -ge 1 -and $n -le 2 -and $bits -eq 2 }
            3 { $n -eq 1 -and $bits -ge 3 -and $bits -le 8 }
            4 { $n -eq 2 -and $bits -eq 2 }
        }
        if (-not $ok) { $issues.Add("RUIDO: cobertura de alvos incorreta/ausente no quadro $seq.") }
    }
    if ($State.Summary) {
        if ($State.Summary -notmatch ('^# RESUMO,'+[regex]::Escape($State.Technique)+',total='+$State.Count+',')) { $issues.Add('Resumo RX com tecnica/total divergente.') }
        foreach ($name in $counts.Keys) {
            if ($State.Summary -notmatch (','+$name+'=')) { $issues.Add("Resumo sem $name."); continue }
            $m=[regex]::Match($State.Summary, (','+$name+'=(\d+)(?:,|$)'))
            if (-not $m.Success -or [int]$m.Groups[1].Value -ne $counts[$name]) { $issues.Add("Resumo diverge das linhas: $name.") }
        }
    } else { $issues.Add('Resumo RX ausente.') }
    foreach ($crossIssue in (Get-CrossIssues $State)) { $issues.Add($crossIssue) }
    [pscustomobject]@{ estrutura_ok=($issues.Count -eq 0); contagens=$counts; rx_linhas=$State.RxRows.Count; ruido_linhas=$State.NoiseRows.Count; problemas=@($issues.ToArray()) }
}

if ($SelfTest) {
    function Assert($condition,$message) { if (-not $condition) { throw "TESTE FALHOU: $message" } }
    function Make-TestRun([string]$tech='SUM8',[int]$mode=0,[int]$count=1) {
        $s=New-RunState $tech $mode $count
        Add-RunLine $s 'RX' "# PREPARADO_RX_COMPLETO,$tech,$count,primeira_seq=0,pausa_automatica=NAO"
        Add-RunLine $s 'RUIDO' "# MODO_PROXIMO_QUADRO,$mode"
        $stats=@{OK=0;CORROMPIDO=0;PERDIDO=0;NAO_DETECTADO=0}
        for ($seq=0;$seq -lt $count;$seq++) {
            $tx=[byte[]]@(7,($seq-band 255),(($seq-shr 8)-band 255),240,196,65,46,0)
            $prefix=[byte[]]$tx[0..6]; $check=Get-BodyResidue $prefix $tech
            $tx[7]=if ($tech -eq 'SUM8') { (256-$check)-band 255 } else { $check }
            $rx=[byte[]]$tx.Clone()
            $masks=[int[]]@(0,0,0,0)
            switch ($mode) { 1 {$masks[0]=128} 2 {$masks[0]=129} 3 {$masks[1]=28} 4 {$masks[0]=128;$masks[1]=128} }
            for ($b=0;$b -lt 4;$b++) { $rx[3+$b]=$rx[3+$b]-band (255-bxor $masks[$b]) }
            $r=Get-BodyResidue $rx $tech
            $same=([Convert]::ToBase64String($rx)-eq [Convert]::ToBase64String($tx))
            $st=if ($r) {'CORROMPIDO'} elseif ($same) {'OK'} else {'NAO_DETECTADO'}
            $decision=if ($r) {'DESCARTADO'} else {'ACEITO'}; $conf=if ($same) {'SIM'} else {'NAO'}
            $rxHex=($rx|ForEach-Object {$_.ToString('X2')})-join ''; $txHex=($tx|ForEach-Object {$_.ToString('X2')})-join ''
            Add-RunLine $s 'RX' "$seq,$seq,$tech,$r,$decision,$st,$conf,1,101000,-1,$rxHex,$txHex,CHECADO_ANTES_DA_COPIA"
            $stats[$st]++
            for ($b=0;$b -lt 4;$b++) {
                if ($mode -ne 0 -and -not $masks[$b]) { continue }
                $a=$tx[3+$b].ToString('X2'); $z=$rx[3+$b].ToString('X2'); $m=$masks[$b].ToString('X2'); $d=($tx[3+$b]-bxor $rx[3+$b]).ToString('X2')
                $changed=if ($a -eq $z) {'NAO'} else {'SIM'}
                Add-RunLine $s 'RUIDO' "$seq,$mode,$b,$m,123,$a,$z,$d,$changed,SIM,0,00,00,88"
            }
        }
        $s.Summary="# RESUMO,$tech,total=$count,OK=$($stats.OK),CORROMPIDO=$($stats.CORROMPIDO),PERDIDO=0,NAO_DETECTADO=$($stats.NAO_DETECTADO),taxa=NA"
        $s.Validation="# VALIDACAO,esperados=$count,sequencias_processadas=$count,nao_classificados=0,rodada_valida=SIM"
        return $s
    }
    foreach ($tech in @('SUM8','CRC8')) { for ($m=0;$m -le 4;$m++) {
        $s=Make-TestRun $tech $m 10
        $review=Get-RunReview $s
        Assert $review.estrutura_ok ("tecnica/modo: $tech/$m - "+($review.problemas-join '; '))
    }}
    $s=Make-TestRun 'SUM8' 1 1
    # O injetor declara efeito correto em seus proprios dados, mas o RX nao mudou.
    $f=$s.RxRows[0].Split(','); $f[3]='0';$f[4]='ACEITO';$f[5]='OK';$f[6]='SIM';$f[10]=$f[11]
    $s.RxRows[0]=$f-join ','; $s.Summary='# RESUMO,SUM8,total=1,OK=1,CORROMPIDO=0,PERDIDO=0,NAO_DETECTADO=0,taxa=NA'
    $review=Get-RunReview $s
    Assert (-not $review.estrutura_ok -and ($review.problemas-join ';') -match 'CRUZAMENTO') 'GPIO SIM nao pode prevalecer sobre RX/copia divergentes.'
    $s=Make-TestRun; $s.Validation=$null
    Assert (-not (Get-RunReview $s).estrutura_ok) 'Validacao ausente.'
    $s=Make-TestRun; $f=$s.RxRows[0].Split(','); $f[11]='NA'; $s.RxRows[0]=$f-join ','
    Assert (-not (Get-RunReview $s).estrutura_ok) 'Referencia ausente apesar do resumo declarar rodada valida.'
    $s=Make-TestRun; Add-RunLine $s 'RX' $s.RxRows[0]
    Assert (-not (Get-RunReview $s).estrutura_ok) 'Duplicata.'
    $s=Make-TestRun; Add-RunLine $s 'RUIDO' '# SINCRONISMO_INVALIDO,seq=0'
    Assert (-not (Get-RunReview $s).estrutura_ok) 'Diagnostico de sincronismo.'
    $s=Make-TestRun; $s.RxRows[0]=$s.RxRows[0].Replace(',0,ACEITO,',',1,ACEITO,')
    Assert (-not (Get-RunReview $s).estrutura_ok) 'Residuo divergente do recalculo.'
    $s=Make-TestRun; Assert (-not (Get-RunReview $s $true).estrutura_ok) 'Interrupcao.'
    $s=Make-TestRun 'SUM8' 0 1000
    $r=Get-RunReview $s
    Assert ($r.estrutura_ok -and $r.rx_linhas -eq 1000 -and $r.ruido_linhas -eq 4000) '1000 quadros e 4000 amostras.'
    Write-Output 'PASS: SUM8/CRC8 modos0..4; GPIO SIM divergente do RX rejeitado; residuos/corpos/status conferidos; interrupcao; referencia ausente; duplicata; diagnostico; 1000 quadros/4000 amostras. Apenas software.'
    exit 0
}
if ($ReviewFolder) {
    $meta=Get-Content -LiteralPath (Join-Path $ReviewFolder 'metadados.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $s=New-RunState $meta.tecnica ([int]$meta.modo_ruido) ([int]$meta.quadros)
    foreach ($line in (Get-Content -LiteralPath (Join-Path $ReviewFolder 'rx_bruto.log') -Encoding UTF8)) { Add-RunLine $s 'RX' $line }
    foreach ($line in (Get-Content -LiteralPath (Join-Path $ReviewFolder 'ruido_bruto.log') -Encoding UTF8)) { Add-RunLine $s 'RUIDO' $line }
    Get-RunReview $s | ConvertTo-Json -Depth 7
    exit 0
}

Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$script:active=$false; $script:ports=@{}; $script:writers=@{}; $script:buffers=@{}
$script:lastFolder=$null; $script:finishAt=$null; $script:lastData=Get-Date; $script:startedAt=$null
$script:state=$null; $script:manifest=$null
$form=New-Object Windows.Forms.Form
$form.Text='Coletor CSV v2 - ESP32'; $form.Size=New-Object Drawing.Size(850,650)
$form.MinimumSize=$form.Size; $form.StartPosition='CenterScreen'
$form.Font=New-Object Drawing.Font('Segoe UI',10)
function Label($text,$x,$y,$width=170,$height=28) {
    $c=New-Object Windows.Forms.Label; $c.Text=$text; $c.SetBounds($x,$y,$width,$height); $form.Controls.Add($c); return $c
}
function Combo($items,$x,$y,$width=140) {
    $c=New-Object Windows.Forms.ComboBox; $c.DropDownStyle='DropDownList'; $c.SetBounds($x,$y,$width,28)
    foreach ($item in $items) { [void]$c.Items.Add([string]$item) }
    if ($c.Items.Count) { $c.SelectedIndex=0 }; $form.Controls.Add($c); return $c
}
$null=Label '1. Deixe o TX parado (x). Feche somente os Monitores Seriais do RX completo e do RUIDO.' 20 15 790 30
$null=Label '2. Confira as portas abaixo. O Monitor Serial do TX pode continuar aberto no Arduino.' 20 45 790 30
$portNames=@([IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
$null=Label 'RX completo' 20 90
$rxCombo=Combo $portNames 20 120
$null=Label 'RUIDO' 190 90
$noiseCombo=Combo $portNames 190 120
if ($rxCombo.Items.Contains('COM6')) { $rxCombo.SelectedItem='COM6' }
if ($noiseCombo.Items.Contains('COM3')) { $noiseCombo.SelectedItem='COM3' }
$null=Label 'Tecnica' 360 90
$techCombo=Combo @('SUM8','CRC8') 360 120
$null=Label 'Modo RUIDO' 530 90 105
$modeCombo=Combo @('0','1','2','3','4') 530 120 105
$null=Label 'Quadros' 665 90 130
$countCombo=Combo @('10','1000') 665 120 120
$start=New-Object Windows.Forms.Button; $start.Text='Preparar e gravar'; $start.SetBounds(20,165,200,42); $form.Controls.Add($start)
$stop=New-Object Windows.Forms.Button; $stop.Text='Interromper e salvar'; $stop.SetBounds(240,165,200,42); $stop.Enabled=$false; $form.Controls.Add($stop)
$open=New-Object Windows.Forms.Button; $open.Text='Abrir pasta salva'; $open.SetBounds(460,165,200,42); $open.Enabled=$false; $form.Controls.Add($open)
$status=Label 'Triagem inicial: SUM8, modo 0, 10 quadros. Nao transmita antes do aviso PRONTO.' 20 220 785 72
$status.Font=New-Object Drawing.Font('Segoe UI',11,[Drawing.FontStyle]::Bold)
$progress=Label 'RX: 0 linhas | RUIDO: 0 linhas' 20 290 785
$log=New-Object Windows.Forms.TextBox; $log.Multiline=$true; $log.ReadOnly=$true; $log.ScrollBars='Vertical'
$log.SetBounds(20,325,790,210); $form.Controls.Add($log)
$null=Label 'Arquivos sao gravados durante a coleta. Este programa nao grava firmware nem inicia o TX.' 20 548 790 40
$timer=New-Object Windows.Forms.Timer; $timer.Interval=75
function Show-Log([string]$message) {
    if ($log.TextLength -gt 15000) { $log.Text=$log.Text.Substring($log.TextLength-8000) }
    $log.AppendText($message+[Environment]::NewLine)
}
function Close-Resources {
    foreach ($p in $script:ports.Values) { try { $p.Close(); $p.Dispose() } catch {} }
    foreach ($w in $script:writers.Values) { try { $w.Dispose() } catch {} }
    $script:ports=@{}; $script:writers=@{}
}
function End-Capture([bool]$aborted) {
    $timer.Stop()
    try {
        foreach ($source in @('RX','RUIDO')) {
            if ($script:buffers[$source]) {
                $script:writers[$source+'_raw'].Write($script:buffers[$source])
                $script:state.Diagnostics.Add("$source terminou com fragmento sem quebra de linha; preservado no log bruto.")
            }
        }
        if ($aborted -and $script:ports.ContainsKey('RX')) { try { $script:ports.RX.Write("x`n") } catch {} }
        $review=Get-RunReview $script:state $aborted
        $script:manifest['encerrado_em']=(Get-Date).ToString('o')
        $script:manifest['revisao']=$review
        $script:manifest['resumo_rx']=$script:state.Summary
        $script:manifest['validacao_rx']=$script:state.Validation
        $script:manifest | ConvertTo-Json -Depth 7 | Set-Content -LiteralPath (Join-Path $script:lastFolder 'metadados.json') -Encoding UTF8
        $report=@('REVISAO AUTOMATICA DA COLETA',('Estrutura dos registros OK: '+$review.estrutura_ok),('RX linhas: '+$review.rx_linhas),('RUIDO linhas: '+$review.ruido_linhas),$script:state.Summary,$script:state.Validation,'','Revisao v2: cruza amostras RUIDO com RX/copia e recalcula a checagem. Nao certifica montagem ou calibracao fisica.','Problemas encontrados:')+@($review.problemas)
        $report | Set-Content -LiteralPath (Join-Path $script:lastFolder 'revisao.txt') -Encoding UTF8
        $status.Text=if ($review.estrutura_ok) { 'COLETA SALVA. Estrutura dos registros conferida. Envie revisao.txt para analise.' } else { 'ARQUIVOS SALVOS, MAS A COLETA PRECISA DE REVISAO. Envie revisao.txt; nao apague os logs.' }
        $status.ForeColor=if ($review.estrutura_ok) { [Drawing.Color]::DarkGreen } else { [Drawing.Color]::DarkRed }
        Show-Log $status.Text; Show-Log ('Pasta: '+$script:lastFolder)
        foreach ($msg in ($review.problemas | Select-Object -First 5)) { Show-Log $msg }
    } catch { $status.Text='Falha ao finalizar. Os arquivos ja gravados foram preservados: '+$_.Exception.Message }
    finally {
        Close-Resources; $script:active=$false; $start.Enabled=$true; $stop.Enabled=$false; $open.Enabled=($null -ne $script:lastFolder)
        foreach ($c in @($rxCombo,$noiseCombo,$techCombo,$modeCombo,$countCombo)) { $c.Enabled=$true }
    }
}

$start.Add_Click({
    try {
        $rp=[string]$rxCombo.SelectedItem; $np=[string]$noiseCombo.SelectedItem
        if (-not $rp -or -not $np -or $rp -eq $np) { throw 'Selecione duas portas diferentes: RX completo e RUIDO.' }
        $tech=[string]$techCombo.SelectedItem; $mode=[int][string]$modeCombo.SelectedItem; $count=[int][string]$countCombo.SelectedItem
        $script:state=New-RunState $tech $mode $count
        $name=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+$tech+'_modo'+$mode+'_'+$count
        $script:lastFolder=Join-Path (Join-Path $PSScriptRoot 'Coletas') $name
        [void][IO.Directory]::CreateDirectory($script:lastFolder)
        $script:manifest=@{iniciado_em=(Get-Date).ToString('o'); tecnica=$tech; modo_ruido=$mode; quadros=$count; porta_rx=$rp; porta_ruido=$np; baud=115200; rx_esperado='rx_completo v0.2'; coletor_versao='2'; ruido_versao_instalada='conferir na captura de inicializacao'; firmware_instalado_verificado_pelo_programa=$false; ajuste_isr_us_verificado_pelo_programa=$false; rx_preparado=$false; modo_ruido_confirmado=$false; arquivos=@('rx.csv','ruido.csv','rx_bruto.log','ruido_bruto.log','revisao.txt')}
        $script:manifest | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $script:lastFolder 'metadados.json') -Encoding UTF8
        $utf8=New-Object Text.UTF8Encoding($true)
        foreach ($source in @('RX','RUIDO')) {
            $lower=if ($source -eq 'RX') { 'rx' } else { 'ruido' }
            $script:writers[$source+'_raw']=[IO.StreamWriter]::new((Join-Path $script:lastFolder ($lower+'_bruto.log')),$false,$utf8)
            $script:writers[$source+'_csv']=[IO.StreamWriter]::new((Join-Path $script:lastFolder ($lower+'.csv')),$false,$utf8)
            $script:writers[$source+'_raw'].AutoFlush=$true; $script:writers[$source+'_csv'].AutoFlush=$true
            $header=if ($source -eq 'RX') { $RxHeader } else { $NoiseHeader }
            $script:writers[$source+'_csv'].WriteLine($header)
            $portName=if ($source -eq 'RX') { $rp } else { $np }
            $p=[IO.Ports.SerialPort]::new($portName,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
            $p.DtrEnable=$false; $p.RtsEnable=$false; $p.Handshake='None'; $p.ReadBufferSize=262144; $p.WriteTimeout=500
            $script:ports[$source]=$p; $script:buffers[$source]=''; $p.Open()
        }
        $script:active=$true; $script:startedAt=Get-Date; $script:lastData=Get-Date; $script:finishAt=$null
        $script:commandAt=(Get-Date).AddSeconds(2); $script:sentCommands=$false; $script:readyShown=$false
        $script:runFirstData=$null
        $log.Clear(); Show-Log ('Gravando em '+$script:lastFolder)
        $status.Text='Portas abertas. Aguardando estabilizar; mantenha o TX parado.'; $status.ForeColor=[Drawing.Color]::DarkBlue
        $start.Enabled=$false; $stop.Enabled=$true; $open.Enabled=$false
        foreach ($c in @($rxCombo,$noiseCombo,$techCombo,$modeCombo,$countCombo)) { $c.Enabled=$false }
        $timer.Start()
    } catch {
        Close-Resources; $script:active=$false
        $status.Text='Nao foi possivel preparar: '+$_.Exception.Message
        [void][Windows.Forms.MessageBox]::Show($status.Text+"`nFeche os Monitores Seriais do RX completo e do RUIDO e confira as portas.",'Coletor CSV')
    }
})

$timer.Add_Tick({
    try {
        if (-not $script:active) { return }
        foreach ($source in @('RX','RUIDO')) {
            $chunk=$script:ports[$source].ReadExisting()
            if ($chunk.Length) {
                $script:lastData=Get-Date; $script:buffers[$source]+=$chunk
                while (($idx=$script:buffers[$source].IndexOf("`n")) -ge 0) {
                    $line=$script:buffers[$source].Substring(0,$idx).TrimEnd([char]13)
                    $script:buffers[$source]=$script:buffers[$source].Substring($idx+1)
                    $script:writers[$source+'_raw'].WriteLine($line)
                    $old=if ($source -eq 'RX') { $script:state.RxRows.Count } else { $script:state.NoiseRows.Count }
                    Add-RunLine $script:state $source $line
                    $new=if ($source -eq 'RX') { $script:state.RxRows.Count } else { $script:state.NoiseRows.Count }
                    if ($new -gt $old) {
                        $script:writers[$source+'_csv'].WriteLine($line)
                        if ($null -eq $script:runFirstData) { $script:runFirstData=Get-Date }
                    }
                    if ($line -match '^# (PREPARADO|MODO_PROXIMO|RESUMO|VALIDACAO|SINCRONISMO_INVALIDO|TIMEOUT|AJUSTE_ISR_US)') { Show-Log ($source+': '+$line) }
                }
                if ($script:buffers[$source].Length -gt 100000) { throw 'Dados sem quebra de linha; confira baud 115200 e firmware.' }
            }
        }
        if (-not $script:sentCommands -and (Get-Date) -ge $script:commandAt) {
            $letter=if ($script:state.Technique -eq 'CRC8') { 'c' } else { 's' }
            $runLetter=if ($script:state.Count -eq 1000) { 'r' } else { 'd' }
            $script:ports.RUIDO.Write(([string]$script:state.Mode)+"`n")
            $script:ports.RX.Write("x`n$letter`n$runLetter`n")
            $script:sentCommands=$true
        }
        if ($script:state.Prepared -and $script:state.NoiseAck -and -not $script:readyShown) {
            $letter=if ($script:state.Technique -eq 'CRC8') { 'c' } else { 's' }
            $txRun=if ($script:state.Count -eq 1000) { 'g' } else { 't' }
            $status.Text="PRONTO E GRAVANDO. Agora, no Monitor Serial do TX, envie $letter e depois $txRun (separadamente)."
            $status.ForeColor=[Drawing.Color]::DarkGreen; Show-Log $status.Text
            $script:readyShown=$true; $script:manifest['rx_preparado']=$true; $script:manifest['modo_ruido_confirmado']=$true
        }
        $progress.Text="RX: $($script:state.RxRows.Count)/$($script:state.Count) linhas | RUIDO: $($script:state.NoiseRows.Count) linhas"
        if ($script:state.Validation -and $null -eq $script:finishAt) { $script:finishAt=(Get-Date).AddSeconds(2) }
        if ($null -ne $script:finishAt -and (Get-Date) -ge $script:finishAt) { End-Capture $false; return }
        if (-not $script:readyShown -and ((Get-Date)-$script:startedAt).TotalSeconds -gt 12) { throw 'Sem confirmacao das duas placas. Confira RX completo/COM e RUIDO/COM; arquivos parciais foram preservados.' }
        if ($null -ne $script:runFirstData -and ((Get-Date)-$script:lastData).TotalSeconds -gt 15) { throw 'Sem dados por 15 segundos e sem resumo completo. Confira TX e cabos; nao use coleta parcial.' }
        if ($null -ne $script:runFirstData -and ((Get-Date)-$script:runFirstData).TotalSeconds -gt ($script:state.Count*0.2+30)) { throw 'Duracao maxima excedida sem conclusao.' }
    } catch {
        Show-Log ('ERRO: '+$_.Exception.Message)
        $script:state.Diagnostics.Add($_.Exception.Message)
        End-Capture $true
    }
})
$stop.Add_Click({ if ($script:active) { End-Capture $true } })
$open.Add_Click({ if ($script:lastFolder -and (Test-Path -LiteralPath $script:lastFolder)) { Start-Process explorer.exe -ArgumentList ('"'+$script:lastFolder+'"') } })
$form.Add_FormClosing({
    if ($script:active) {
        $answer=[Windows.Forms.MessageBox]::Show('Interromper a coleta e preservar os arquivos parciais?','Coleta em andamento','YesNo')
        if ($answer -ne 'Yes') { $_.Cancel=$true; return }
        End-Capture $true
    }
    $timer.Dispose()
})
if ($CheckGui) {
    $timer.Dispose(); $form.Dispose()
    Write-Output 'PASS: interface construida e descartada sem abrir janela ou portas seriais.'
    exit 0
}
[void]$form.ShowDialog()
Close-Resources
