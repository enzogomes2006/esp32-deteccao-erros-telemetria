param([Parameter(Mandatory=$true)][string]$FixtureRoot)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot '../coletor/Revisar.ps1')
function Assert($Condition,[string]$Message){if(-not $Condition){throw "FALHOU: $Message"}}
function Read-Fixture([string]$Technique,[int]$Mode){
    $s=New-RunState $Technique $Mode 1000
    foreach($line in (Get-Content -LiteralPath (Join-Path $FixtureRoot ($Technique+'_'+$Mode+'_rx.log')))){Add-RunLine $s RX $line}
    foreach($line in (Get-Content -LiteralPath (Join-Path $FixtureRoot ($Technique+'_'+$Mode+'_ruido.log')))){Add-RunLine $s RUIDO $line}
    return $s
}
foreach($tech in @('SUM8','CRC8')){foreach($mode in 0..4){
    $s=Read-Fixture $tech $mode
    $r=Get-RunReview $s
    Assert $r.estrutura_ok ("Integracao $tech/$mode : "+($r.problemas|Select-Object -First 8))
    Assert ($r.rx_linhas-eq 1000 -and $r.ruido_linhas-eq 8000) 'Cobertura de todos os bytes.'
    Write-Output "PASS: fixture SINTETICA $tech/$mode, 1000 eventos/8000 amostras."
}}
$baseline=Read-Fixture SUM8 0
$gap=New-RunState SUM8 0 10
foreach($line in (Get-Content -LiteralPath (Join-Path $FixtureRoot 'SUM8_gap_rx.log'))){Add-RunLine $gap RX $line}
foreach($line in (Get-Content -LiteralPath (Join-Path $FixtureRoot 'SUM8_gap_ruido.log'))){Add-RunLine $gap RUIDO $line}
$review=Get-RunReview $gap
Assert ($review.estrutura_ok -and $review.contagens.PERDIDO-eq 3) ('Perdas com evidencia independente: '+($review.problemas-join ';'))
$gap.LinkAmbiguities=1
Assert (-not (Get-RunReview $gap).estrutura_ok) 'Ambiguidade impede atribuir perdas sem evento.'
Write-Output 'PASS: perda de tres eventos sem dupla contagem; inferencia ambigua rejeitada.'
# Uma fixture pequena isola adulteracoes sem repetir toda a revisao de 1000.
function Small-Fixture {
    $s=New-RunState SUM8 0 1
    Add-RunLine $s RX '# PREPARADO_RX_CORPO64,SUM8,1,primeira_seq=0,pausa_automatica=NAO,versao=1.0'
    Add-RunLine $s RUIDO '# MODO_PROXIMO_QUADRO,0,escopo=CORPO64,versao=2.0'
    $d=$baseline.RawRx|Where-Object {$_ -like '# DECISAO_ANTES_DA_COPIA,evento=1,*'}|Select-Object -First 1
    Add-RunLine $s RX $d;Add-RunLine $s RX $baseline.RxRows[0]
    for($i=0;$i-lt 8;$i++){Add-RunLine $s RUIDO $baseline.NoiseRows[$i]}
    Add-RunLine $s RX '# RESUMO,SUM8,total=1,OK=1,CORROMPIDO=0,PERDIDO=0,NAO_DETECTADO=0,taxa=NA'
    Add-RunLine $s RX '# VALIDACAO,esperados=1,auditados=1,nao_classificados=0,rodada_valida=SIM'
    return $s
}
$s=Small-Fixture;Assert (Get-RunReview $s).estrutura_ok 'Fixture pequena correta.'
$s=Small-Fixture;$s.NoiseAck=$false;Assert (-not (Get-RunReview $s).estrutura_ok) 'Firmware antigo nao pode ser aceito.'
$s=Small-Fixture;$s.Validation=$null;Assert (-not (Get-RunReview $s).estrutura_ok) 'Validacao ausente.'
$s=Small-Fixture;$s.NoiseRows.RemoveAt(0);Assert (-not (Get-RunReview $s).estrutura_ok) 'Amostra LEN ausente.'
$s=Small-Fixture;$s.RxRows.Add($s.RxRows[0]);Assert (-not (Get-RunReview $s).estrutura_ok) 'Duplicata.'
$s=Small-Fixture;$f=$s.RxRows[0].Split(',');$f[8]='1';$s.RxRows[0]=$f-join ',';Assert (-not (Get-RunReview $s).estrutura_ok) 'Residuo falso.'
$s=Small-Fixture;$f=$s.RxRows[0].Split(',');$f[21]='0';$s.RxRows[0]=$f-join ',';Assert (-not (Get-RunReview $s).estrutura_ok) 'Referencia antes da decisao.'
$s=Small-Fixture;$f=$s.NoiseRows[0].Split(',');$f[6]='06';$f[7]='06';$s.NoiseRows[0]=$f-join ',';Assert (-not (Get-RunReview $s).estrutura_ok) 'GPIO SIM nao esconde divergencia no header.'
$s=Small-Fixture;$f=$s.NoiseRows[7].Split(',');$f[13]='01';$s.NoiseRows[7]=$f-join ',';Assert (-not (Get-RunReview $s).estrutura_ok) 'Pulso CHK tardio.'
$s=Small-Fixture;$s.Summary=$s.Summary.Replace('taxa=NA','taxa=0');Assert (-not (Get-RunReview $s).estrutura_ok) 'Denominador zero.'
$s=Small-Fixture;Assert (-not (Get-RunReview $s $true).estrutura_ok) 'Coleta interrompida.'
$s=New-RunState SUM8 0 1
Add-RunLine $s RX '# PREPARADO_RX_CORPO64,SUM8,1,primeira_seq=0,versao=1.0'
Add-RunLine $s RX $baseline.RxRows[0]
Assert (($s.Diagnostics-join ';') -match 'Sem decisao anterior') 'Linha de auditoria sem decisao previa.'
Write-Output 'PASS: mutacoes rejeitadas: firmware antigo, copia/validacao ausente, amostra LEN ausente, duplicata, residuo, timestamps, header, atraso CHK, taxa, interrupcao, decisao ausente.'
