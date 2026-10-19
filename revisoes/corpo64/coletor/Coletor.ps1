param([switch]$CheckGui,[string]$ReviewFolder)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'Revisar.ps1')
if($ReviewFolder) {
    $meta=Get-Content -LiteralPath (Join-Path $ReviewFolder 'metadados.json') -Raw -Encoding UTF8 | ConvertFrom-Json
    $state=New-RunState $meta.tecnica ([int]$meta.modo_ruido) ([int]$meta.quadros)
    foreach($line in (Get-Content -LiteralPath (Join-Path $ReviewFolder 'rx_bruto.log') -Encoding UTF8)){Add-RunLine $state 'RX' $line}
    foreach($line in (Get-Content -LiteralPath (Join-Path $ReviewFolder 'ruido_bruto.log') -Encoding UTF8)){Add-RunLine $state 'RUIDO' $line}
    $review=Get-RunReview $state
    $review | ConvertTo-Json -Depth 7
    if(-not $review.estrutura_ok){exit 1}
    exit 0
}
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing
[Windows.Forms.Application]::EnableVisualStyles()
$script:active=$false; $script:ports=@{}; $script:writers=@{}; $script:buffers=@{}
$script:lastFolder=$null; $script:finishAt=$null; $script:lastData=Get-Date; $script:startedAt=$null
$script:state=$null; $script:manifest=$null
$form=New-Object Windows.Forms.Form
$form.Text='Coletor v3 CORPO64 - ESP32'; $form.Size=New-Object Drawing.Size(850,650)
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
$null=Label '1. Deixe o TX parado (x). Feche somente os Monitores Seriais do RX CORPO64 e do RUIDO.' 20 15 790 30
$null=Label '2. Confira as portas abaixo. O Monitor Serial do TX pode continuar aberto no Arduino.' 20 45 790 30
$portNames=@([IO.Ports.SerialPort]::GetPortNames() | Sort-Object)
$null=Label 'RX CORPO64' 20 90
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
        $report=@('REVISAO AUTOMATICA DA COLETA',('Estrutura dos registros OK: '+$review.estrutura_ok),('RX linhas: '+$review.rx_linhas),('RUIDO linhas: '+$review.ruido_linhas),$script:state.Summary,$script:state.Validation,'','Revisao v3 CORPO64: cruza os oito bytes, residuos e decisao anterior a copia. Nao certifica montagem ou calibracao fisica.','Problemas encontrados:')+@($review.problemas)
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
        if (-not $rp -or -not $np -or $rp -eq $np) { throw 'Selecione duas portas diferentes: RX CORPO64 e RUIDO.' }
        $tech=[string]$techCombo.SelectedItem; $mode=[int][string]$modeCombo.SelectedItem; $count=[int][string]$countCombo.SelectedItem
        $script:state=New-RunState $tech $mode $count
        $name=(Get-Date -Format 'yyyyMMdd_HHmmss_fff')+'_'+$tech+'_modo'+$mode+'_'+$count
        $script:lastFolder=Join-Path (Join-Path $PSScriptRoot 'Coletas') $name
        [void][IO.Directory]::CreateDirectory($script:lastFolder)
        $script:manifest=@{iniciado_em=(Get-Date).ToString('o'); tecnica=$tech; modo_ruido=$mode; quadros=$count; porta_rx=$rp; porta_ruido=$np; baud=115200; rx_esperado='RX CORPO64 v1.0'; coletor_versao='3'; escopo='CORPO64'; ruido_versao_instalada='conferir na captura de inicializacao'; firmware_instalado_verificado_pelo_programa=$false; ajuste_isr_us_verificado_pelo_programa=$false; rx_preparado=$false; modo_ruido_confirmado=$false; arquivos=@('rx.csv','ruido.csv','rx_bruto.log','ruido_bruto.log','revisao.txt')}
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
        [void][Windows.Forms.MessageBox]::Show($status.Text+"`nFeche os Monitores Seriais do RX CORPO64 e do RUIDO e confira as portas.",'Coletor CSV')
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
                    if ($line -match '^# (PREPARADO|MODO_PROXIMO|RESUMO|VALIDACAO|SINCRONISMO_INVALIDO|TIMEOUT|AUDITORIA_INVALIDA)') { Show-Log ($source+': '+$line) }
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
        if (-not $script:readyShown -and ((Get-Date)-$script:startedAt).TotalSeconds -gt 12) { throw 'Sem confirmacao das duas placas. Confira RX CORPO64/COM e RUIDO/COM; arquivos parciais foram preservados.' }
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
