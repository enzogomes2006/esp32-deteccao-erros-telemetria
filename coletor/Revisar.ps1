# Validacao independente da interface. PowerShell 5.1, sem Python.
$RxHeader='evento_id,seq_experimento,seq_rx,seq_referencia,seq_enlace_esperada,seq_enlace_proxima,tecnica,len_rx,residuo,decisao,status,payload_confere,cabecalho_diverge,entregue_aplicacao,verificacao_us,intervalo_inicio_us,recuperacao_ms,corpo_rx_hex,corpo_tx_hex,motivo,decisao_us,referencia_us'
$NoiseHeader='seq,modo,byte_corpo,campo,mascara_alvo_hex,timestamp_us,antes_hex,depois_amostrado_hex,mascara_mudou_hex,mudou,sincronismo_ok,atraso_amostra_max_us,amostras_tardias_hex,pulsos_tardios_hex,duracao_byte_us'
function New-RunState([string]$Technique,[int]$Mode,[int]$Count) {
    [pscustomobject]@{Technique=$Technique;Mode=$Mode;Count=$Count;Prepared=$false;NoiseAck=$false
        RxRows=[Collections.Generic.List[string]]::new();NoiseRows=[Collections.Generic.List[string]]::new()
        Diagnostics=[Collections.Generic.List[string]]::new();Summary=$null;Validation=$null
        RawRx=[Collections.Generic.List[string]]::new();RawNoise=[Collections.Generic.List[string]]::new()
        Decisions=@{};Evidence=0;LinkAmbiguities=0}
}
function Add-RunLine($State,[string]$Source,[string]$Line) {
    $Line=$Line.TrimEnd([char]13)
    if($Source -eq 'RX') {
        $State.RawRx.Add($Line)
        if($Line -match '^# PREPARADO_RX_CORPO64,([^,]+),(\d+),primeira_seq=0,') {
            if($State.Prepared){$State.Diagnostics.Add('RX preparado novamente durante coleta.')}
            if($Matches[1] -eq $State.Technique -and [int]$Matches[2] -eq $State.Count){$State.Prepared=$true}
            else{$State.Diagnostics.Add('Configuracao RX divergente.')}
        }
        if(-not $State.Prepared){return}
        if($Line -match '^# DECISAO_ANTES_DA_COPIA,evento=(\d+),seq_rx=(\d+|NA),residuo=(\d+|NA),decisao=(ACEITO|DESCARTADO|SEM_CHECAGEM),proxima_seq=(\d+),decisao_us=(\d+)$') {
            $id=[int]$Matches[1]
            if($State.Decisions.ContainsKey($id)){$State.Diagnostics.Add("Decisao duplicada: $id")}
            $State.Decisions[$id]=@{seq=$Matches[2];res=$Matches[3];dec=$Matches[4];next=$Matches[5];time=$Matches[6];position=$State.RawRx.Count}
        }
        if($Line -match '^# AUSENCIA_ENLACE,total=(\d+),'){$State.Evidence=[int]$Matches[1]}
        if($Line -match '^# ENLACE,.*ambiguidades=(\d+),'){$State.LinkAmbiguities=[int]$Matches[1]}
        if($Line -match '^\d+,') {
            $f=$Line.Split(',');$State.RxRows.Add($Line)
            if($f.Count -eq 22 -and $f[0] -ne '0') {
                $id=[int]$f[0]
                if(-not $State.Decisions.ContainsKey($id)){$State.Diagnostics.Add("Sem decisao anterior a auditoria: evento $id")}
                else {
                    $d=$State.Decisions[$id]
                    if($d.seq -ne $f[2] -or $d.res -ne $f[8] -or $d.dec -ne $f[9] -or $d.next -ne $f[5] -or $d.time -ne $f[20]) {
                        $State.Diagnostics.Add("Decisao alterada entre checagem e auditoria: evento $id")
                    }
                }
            }
        }
        if($Line -match '^# RESUMO,'){$State.Summary=$Line}
        if($Line -match '^# VALIDACAO,'){$State.Validation=$Line}
        if($Line -match '^# (AUDITORIA_INVALIDA|RODADA_INCOMPLETA_OU_INVALIDA)'){$State.Diagnostics.Add($Line)}
    } else {
        $State.RawNoise.Add($Line)
        if($Line -match '^# MODO_PROXIMO_QUADRO,(\d+),escopo=CORPO64,versao=2.0$') {
            if([int]$Matches[1] -eq $State.Mode){$State.NoiseAck=$true}
            else{$State.Diagnostics.Add('Modo do injetor divergente.')}
        }
        if($Line -match '^\d+,'){$State.NoiseRows.Add($Line)}
        if($Line -match '^# (SINCRONISMO_INVALIDO|DIAG|TIMEOUT|ERRO_)'){$State.Diagnostics.Add($Line)}
    }
}
function Read-HexBody([string]$Hex) {
    if($Hex -notmatch '^[0-9A-Fa-f]{16}$'){return $null}
    $v=[byte[]]::new(8)
    for($i=0;$i -lt 8;$i++){$v[$i]=[Convert]::ToByte($Hex.Substring(2*$i,2),16)}
    return ,$v
}
function Get-BodyResidue([byte[]]$Bytes,[string]$Technique) {
    $r=0
    foreach($b in $Bytes) {
        if($Technique -eq 'SUM8'){$r=($r+$b)-band 255}
        else{$r=$r-bxor $b;for($i=0;$i -lt 8;$i++){if($r-band 128){$r=(($r-shl 1)-bxor 7)-band 255}else{$r=($r-shl 1)-band 255}}}
    }
    return $r
}
function Get-PopCount([int]$v){$n=0;for($i=0;$i -lt 8;$i++){if($v-band (1-shl $i)){$n++}};return $n}
function Get-RunReview($State,[bool]$Aborted=$false) {
    $issues=[Collections.Generic.List[string]]::new()
    foreach($msg in $State.Diagnostics){$issues.Add($msg)}
    if($Aborted){$issues.Add('Coleta interrompida; arquivos parciais preservados.')}
    if(-not $State.Prepared){$issues.Add('Sem confirmacao do RX CORPO64 preparado.')}
    if(-not $State.NoiseAck){$issues.Add('Sem confirmacao do injetor CORPO64 v2.0/modo.')}
    if(-not $State.Validation -or $State.Validation -notmatch 'rodada_valida=SIM(?:,|$)'){$issues.Add('RX nao confirmou rodada valida.')}
    if($State.RxRows.Count -ne $State.Count){$issues.Add('Quantidade de linhas RX divergente.')}
    $counts=@{OK=0;CORROMPIDO=0;PERDIDO=0;NAO_DETECTADO=0}
    $records=@{};$noise=@{};$seen=@{};$events=@{};$inferred=0;$headerAccepted=0
    foreach($line in $State.RxRows) {
        $f=$line.Split(',')
        if($f.Count -ne 22 -or $f[1] -notmatch '^\d{1,5}$' -or $f[0] -notmatch '^\d{1,8}$' -or $f[4] -notmatch '^\d{1,5}$' -or $f[5] -notmatch '^\d{1,5}$'){$issues.Add('Formato RX invalido.');continue}
        $seq=[int]$f[1];$eventId=[int]$f[0]
        if($seq -ge $State.Count -or $seen.ContainsKey($seq)){$issues.Add("Sequencia experimental repetida/fora: $seq")}
        $seen[$seq]=$true
        if($f[6] -ne $State.Technique){$issues.Add("Tecnica RX divergente: $seq")}
        if($counts.ContainsKey($f[10])){$counts[$f[10]]++}else{$issues.Add("Classe RX invalida: $seq")}
        if($eventId -eq 0) {
            $inferred++
            if($f[2] -ne 'NA' -or $f[3] -ne 'NA' -or $f[10] -ne 'PERDIDO' -or $f[17] -ne 'NA' -or $f[18] -ne 'NA' -or $f[19] -ne 'AUSENCIA_COM_EVIDENCIA_ENLACE'){$issues.Add("Perda inferida inconsistente: $seq")}
            continue
        }
        if($events.ContainsKey($eventId)){$issues.Add("Evento RX duplicado: $eventId")};$events[$eventId]=$true
        $rx=Read-HexBody $f[17];$tx=Read-HexBody $f[18]
        if($null -eq $tx -or $null -eq $rx){$issues.Add("Corpos completos ausentes: $seq");continue}
        if($tx[0] -ne 7 -or ($tx[1]+256*$tx[2]) -ne $seq -or $f[3] -ne [string]$seq -or (Get-BodyResidue $tx $State.Technique) -ne 0){$issues.Add("Referencia invalida: $seq")}
        if($f[2] -ne [string]($rx[1]+256*$rx[2]) -or $f[7] -ne [string]$rx[0]){$issues.Add("Header recebido diferente do CSV: $seq")}
        $same=($rx[3]-eq $tx[3] -and $rx[4]-eq $tx[4] -and $rx[5]-eq $tx[5] -and $rx[6]-eq $tx[6])
        $headerDiff=($rx[0]-ne $tx[0] -or $rx[1]-ne $tx[1] -or $rx[2]-ne $tx[2])
        $conf=if($same){'SIM'}else{'NAO'};$hd=if($headerDiff){'SIM'}else{'NAO'}
        if($f[11] -ne $conf -or $f[12] -ne $hd){$issues.Add("Comparacao de payload/header divergente: $seq")}
        if($f[20] -notmatch '^\d+$' -or $f[21] -notmatch '^\d+$' -or [long]$f[21] -lt [long]$f[20]){$issues.Add("Ordem de timestamps invalida: $seq")}
        if($rx[0] -ne 7) {
            if($f[8] -ne 'NA' -or $f[9] -ne 'SEM_CHECAGEM' -or $f[10] -ne 'PERDIDO' -or $f[13] -ne 'NAO' -or $f[19] -ne 'LEN_INVALIDO'){$issues.Add("LEN invalido com classificacao errada: $seq")}
            if($f[4] -ne $f[5]){$issues.Add("LEN invalido alterou sequencia do enlace: $seq")}
        } else {
            $r=Get-BodyResidue $rx $State.Technique
            $dec=if($r -eq 0){'ACEITO'}else{'DESCARTADO'}
            $st=if($r -ne 0){'CORROMPIDO'}elseif($same){'OK'}else{'NAO_DETECTADO'}
            if($f[8] -ne [string]$r -or $f[9] -ne $dec -or $f[10] -ne $st -or $f[19] -ne 'CHECADO_ANTES_DA_COPIA'){$issues.Add("Residuo/decisao/classe divergente: $seq")}
            if($r -ne 0 -and ($f[4] -ne $f[5] -or $f[13] -ne 'NAO')){$issues.Add("Descarte alterou o enlace ou entregou payload: $seq")}
            if($r -eq 0 -and $headerDiff){$headerAccepted++}
            if($r -eq 0) {
                $delta=(([int]$f[2]-[int]$f[4])+65536)%65536
                $next=if($delta -lt 32768){([int]$f[2]+1)%65536}else{[int]$f[4]}
                $delivery=if($delta -lt 32768){'SIM'}else{'NAO'}
                if($f[5] -ne [string]$next -or $f[13] -ne $delivery){$issues.Add("Avanco de sequencia inconsistente: $seq")}
            }
        }
        $records[$seq]=@{RX=$rx;TX=$tx;Decision=[long]$f[20]}
    }
    if($seen.Count -ne $State.Count){$issues.Add('Faltam sequencias experimentais unicas.')}
    if($inferred -gt 0 -and ($State.Evidence -ne $inferred -or $headerAccepted -gt 0 -or $State.LinkAmbiguities -gt 0)){$issues.Add('Perdas inferidas sem evidencia exata ou com enlace/cabecalho ambiguo.')}
    $names=@('LEN','SEQ_L','SEQ_H','P0','P1','P2','P3','CHK')
    foreach($line in $State.NoiseRows) {
        $f=$line.Split(',')
        if($f.Count -ne 15 -or $f[0] -notmatch '^\d+$' -or $f[2] -notmatch '^[0-7]$'){$issues.Add('Formato RUIDO invalido.');continue}
        $seq=[int]$f[0];$index=[int]$f[2];$key="$seq/$index"
        if($seq -ge $State.Count -or $noise.ContainsKey($key)){$issues.Add("RUIDO repetido/fora: $key")}
        if($f[1] -ne [string]$State.Mode -or $f[3] -ne $names[$index] -or $f[10] -ne 'SIM' -or $f[12] -ne '00' -or $f[13] -ne '00'){$issues.Add("Modo/campo/sincronismo/atraso divergente: $key")}
        try {
            foreach($i in @(4,6,7,8,12,13)){if($f[$i] -notmatch '^[0-9A-Fa-f]{2}$'){throw 'hex invalido'}}
            $mask=[Convert]::ToInt32($f[4],16);$before=[Convert]::ToInt32($f[6],16);$after=[Convert]::ToInt32($f[7],16);$changed=[Convert]::ToInt32($f[8],16)
            $changedFlag=if($changed){'SIM'}else{'NAO'}
            if(($before-bxor $after) -ne $changed -or ($before-band (255-bxor $mask)) -ne $after -or $f[9] -ne $changedFlag){$issues.Add("Efeito fisico inconsistente: $key")}
            if($records.ContainsKey($seq)) {
                if($before -ne $records[$seq].TX[$index] -or $after -ne $records[$seq].RX[$index]){$issues.Add("CRUZAMENTO: RUIDO diverge de RX/copia: $key")}
            }
            # ESP32 diferentes tem origens de relogio diferentes. Nao comparar
            # timestamp absoluto do injetor com o timestamp absoluto do RX.
            if($f[5] -notmatch '^\d{1,16}$'){throw 'timestamp invalido'}
            $noise[$key]=@{Mask=$mask;Before=$before;After=$after;Time=[long]$f[5]}
        } catch {$issues.Add("Campos invalidos no RUIDO: $key")}
    }
    for($seq=0;$seq -lt $State.Count;$seq++) {
        $bits=0;$nonzero=0;$masks=[int[]]::new(8);$clean=[byte[]]::new(8)
        for($i=0;$i -lt 8;$i++) {
            $key="$seq/$i"
            if(-not $noise.ContainsKey($key)){$issues.Add("Falta amostra do corpo: $key");continue}
            $masks[$i]=$noise[$key].Mask;$bits+=Get-PopCount $masks[$i];$clean[$i]=$noise[$key].Before
            if($masks[$i]){$nonzero++}
            if($i -gt 0 -and $noise.ContainsKey("$seq/"+($i-1)) -and $noise[$key].Time -le $noise["$seq/"+($i-1)].Time){$issues.Add("Ordem dos timestamps do proprio injetor divergente: $key")}
        }
        if($clean[0] -ne 7 -or ($clean[1]+256*$clean[2]) -ne $seq -or (Get-BodyResidue $clean $State.Technique) -ne 0){$issues.Add("Corpo limpo do injetor invalido: $seq")}
        $ok=switch($State.Mode){0{$bits-eq 0}1{$bits-eq 1}2{$bits-eq 2}3{$nonzero-eq 1 -and $bits-ge 3 -and $bits-le 8}4{$nonzero-eq 2 -and $masks[3]-eq 128 -and $masks[4]-eq 128}}
        if(-not $ok){$issues.Add("Alvos incorretos para o modo: $seq")}
        if($State.Mode -eq 3){foreach($mask in $masks){if($mask){$v=$mask;while(($v-band 1)-eq 0){$v=$v-shr 1};if(($v-band ($v+1))-ne 0){$issues.Add("Rajada nao contigua: $seq")}}}}
    }
    if(-not $State.Summary){$issues.Add('Resumo ausente.')}
    else {
        if($State.Summary -notmatch ('^# RESUMO,'+[regex]::Escape($State.Technique)+',total='+$State.Count+',')){$issues.Add('Total/tecnica do resumo divergente.')}
        foreach($name in $counts.Keys){$m=[regex]::Match($State.Summary,(','+$name+'=(\d+)(?:,|$)'));if(-not $m.Success -or [int]$m.Groups[1].Value -ne $counts[$name]){$issues.Add("Resumo diverge das linhas: $name")}}
        $den=$counts.CORROMPIDO+$counts.NAO_DETECTADO
        $m=[regex]::Match($State.Summary,',taxa=([^,]+)$')
        if(-not $m.Success){$issues.Add('Taxa ausente.')}
        elseif($den -eq 0){if($m.Groups[1].Value -ne 'NA'){$issues.Add('Taxa sem denominador deve ser NA.')}}
        else {try{$rate=[double]::Parse($m.Groups[1].Value,[Globalization.CultureInfo]::InvariantCulture);if([double]::IsNaN($rate) -or [double]::IsInfinity($rate) -or [math]::Abs($rate-$counts.NAO_DETECTADO/$den)-gt 0.00000051){$issues.Add('Taxa de nao deteccao divergente.')}}catch{$issues.Add('Taxa invalida.')}}
    }
    [pscustomobject]@{estrutura_ok=($issues.Count-eq 0);contagens=$counts;cabecalhos_aceitos_divergentes=$headerAccepted;rx_linhas=$State.RxRows.Count;ruido_linhas=$State.NoiseRows.Count;problemas=@($issues.ToArray())}
}
