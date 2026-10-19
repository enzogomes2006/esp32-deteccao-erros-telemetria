"""Recalcula a série física CORPO64 sem modificar suas observações originais."""
from pathlib import Path
import argparse, collections, csv, hashlib, io, json, re, shutil, statistics

ROOT = Path(__file__).resolve().parents[1]
REV = ROOT / 'revisoes/corpo64'
SERIES = [
    '20261019_091242_692_SUM8_modo0_1000',
    '20261019_094547_976_SUM8_modo1_1000',
    '20261019_095219_782_SUM8_modo2_1000',
    '20261019_095929_835_SUM8_modo3_1000',
    '20261019_100416_161_CRC8_modo1_1000',
    '20261019_100921_806_CRC8_modo2_1000',
    '20261019_101329_766_CRC8_modo3_1000',
]
TRIAGEM = [
    '20261019_085335_636_SUM8_modo0_10',
    '20261019_085556_983_SUM8_modo1_10',
    '20261019_085705_555_SUM8_modo2_10',
    '20261019_085757_703_SUM8_modo3_10',
    '20261019_085918_397_CRC8_modo1_10',
    '20261019_090046_797_CRC8_modo2_10',
    '20261019_090207_975_CRC8_modo3_10',
    '20261019_102158_177_SUM8_modo0_10',
    '20261019_102351_826_CRC8_modo0_10',
]
FILES = ['metadados.json','revisao.txt','rx.csv','ruido.csv','rx_bruto.log','ruido_bruto.log']

def save_json(path, value):
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')

def sha(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()

def rows(path):
    return list(csv.DictReader(io.StringIO(path.read_text(encoding='utf-8-sig'))))

def residue(data, technique):
    if technique == 'SUM8': return sum(data) % 256
    c=0
    for b in data:
        c ^= b
        for _ in range(8): c = ((c<<1)^7 if c&128 else c<<1)&255
    return c

def stats(values):
    values=list(values)
    if not values: return {'n':0,'media':None,'mediana':None,'min':None,'max':None}
    return dict(n=len(values),media=statistics.mean(values),mediana=statistics.median(values),min=min(values),max=max(values))

def check(folder):
    meta=json.loads((folder/'metadados.json').read_text(encoding='utf-8-sig'))
    tech,mode,n=meta['tecnica'],int(meta['modo_ruido']),int(meta['quadros'])
    rx,noise=rows(folder/'rx.csv'),rows(folder/'ruido.csv')
    assert len(rx)==n and len(noise)==8*n, folder
    assert sorted(int(r['seq_experimento']) for r in rx)==list(range(n)), folder
    raw=(folder/'rx_bruto.log').read_text(encoding='utf-8-sig').splitlines()
    raw_noise=(folder/'ruido_bruto.log').read_text(encoding='utf-8-sig').splitlines()
    assert [r for r in raw if re.match(r'^\d+,',r)]==list(csv_string(rx)), 'RX bruto/CSV'
    assert [r for r in raw_noise if re.match(r'^\d+,',r)]==list(csv_string(noise)), 'Injetor bruto/CSV'
    assert any('rodada_valida=SIM' in r for r in raw), folder
    decisions={}
    for pos,line in enumerate(raw):
        if line.startswith('# DECISAO_ANTES_DA_COPIA,'):
            d=dict(x.split('=',1) for x in line.split(',')[1:]); eid=int(d['evento'])
            assert eid not in decisions
            decisions[eid]=(pos,d)
    samples={(int(r['seq']),int(r['byte_corpo'])):r for r in noise}
    assert len(samples)==8*n
    counts=collections.Counter(); reasons=collections.Counter(); cases=[]
    field_effect=collections.Counter(); field_target=collections.Counter(); changed_frames=0
    for r in rx:
        seq=int(r['seq_experimento']); tx=bytes.fromhex(r['corpo_tx_hex']); body=bytes.fromhex(r['corpo_rx_hex'])
        assert len(tx)==len(body)==8
        assert tx[0]==7 and int.from_bytes(tx[1:3],'little')==seq and residue(tx,tech)==0
        assert r['tecnica']==tech and int(r['seq_referencia'])==seq
        assert r['cabecalho_diverge']==('SIM' if tx[:3]!=body[:3] else 'NAO')
        eid=int(r['evento_id']); pos,d=decisions[eid]
        assert pos<raw.index(','.join(r.values()))
        for k,dk in [('seq_rx','seq_rx'),('residuo','residuo'),('decisao','decisao'),('seq_enlace_proxima','proxima_seq'),('decisao_us','decisao_us')]: assert r[k]==d[dk]
        assert int(r['decisao_us'])<int(r['referencia_us'])
        if body[0]!=7:
            assert r['status']=='PERDIDO' and r['residuo']=='NA' and r['decisao']=='SEM_CHECAGEM'
        else:
            res=residue(body,tech); assert int(r['residuo'])==res
            expected='CORROMPIDO' if res else ('OK' if body[3:7]==tx[3:7] else 'NAO_DETECTADO')
            assert r['status']==expected
            assert r['decisao']==('DESCARTADO' if res else 'ACEITO')
        if r['decisao']!='ACEITO': assert r['entregue_aplicacao']=='NAO'
        masks=[]; effects=[]; before_time=-1
        for i in range(8):
            s=samples[seq,i]; mask=int(s['mascara_alvo_hex'],16); changed=int(s['mascara_mudou_hex'],16)
            assert int(s['modo'])==mode
            assert s['campo']==['LEN','SEQ_L','SEQ_H','P0','P1','P2','P3','CHK'][i]
            assert s['sincronismo_ok']=='SIM' and s['amostras_tardias_hex']=='00' and s['pulsos_tardios_hex']=='00'
            assert int(s['antes_hex'],16)==tx[i] and int(s['depois_amostrado_hex'],16)==body[i]
            assert (tx[i]&(~mask&255))==body[i] and (tx[i]^body[i])==changed
            assert s['mudou']==('SIM' if changed else 'NAO')
            assert int(s['timestamp_us'])>before_time; before_time=int(s['timestamp_us'])
            masks.append(mask); field_target[s['campo']]+=mask.bit_count()
            field_effect[s['campo']]+=changed.bit_count()
            if changed: effects.append({'campo':s['campo'],'indice':i,'antes':f'{tx[i]:02X}','depois':f'{body[i]:02X}','mascara':f'{changed:02X}'})
        bits=sum(m.bit_count() for m in masks)
        assert (mode==0 and bits==0) or (mode in (1,2) and bits==mode) or (mode==3 and sum(bool(m) for m in masks)==1 and 3<=bits<=8)
        if mode==3:
            m=next(m for m in masks if m)
            while not m&1: m>>=1
            assert m&(m+1)==0
        counts[r['status']]+=1; reasons[r['motivo']]+=1; changed_frames+=bool(effects)
        if r['status']=='NAO_DETECTADO':
            cases.append({'sequencia':seq,'evento_id':eid,'corpo_tx_hex':tx.hex().upper(),'corpo_rx_hex':body.hex().upper(),'soma_tx':sum(tx),'soma_rx':sum(body),'residuo':residue(body,tech),'entregue_aplicacao':r['entregue_aplicacao'],'efeitos':effects,'decisao_us':int(r['decisao_us']),'referencia_us':int(r['referencia_us'])})
    recorded=meta['revisao']['contagens']
    assert all(counts[k]==recorded.get(k,0) for k in ['OK','CORROMPIDO','PERDIDO','NAO_DETECTADO'])
    den=counts['CORROMPIDO']+counts['NAO_DETECTADO']
    return dict(pasta=folder.relative_to(ROOT).as_posix(),pasta_original=folder.name,tecnica=tech,modo=mode,quadros=n,contagens={k:counts[k] for k in ['OK','CORROMPIDO','PERDIDO','NAO_DETECTADO']},taxa_nao_deteccao=counts['NAO_DETECTADO']/den if den else None,denominador_nao_deteccao=den,verificacao_rx_us=stats(int(r['verificacao_us']) for r in rx if r['verificacao_us']!='NA'),intervalo_inicio_rx_us=stats(int(r['intervalo_inicio_us']) for r in rx if r['intervalo_inicio_us'] not in ('NA','0')),perdas_por_motivo={k:v for k,v in reasons.items() if k!='CHECADO_ANTES_DA_COPIA'},alvos_por_campo=dict(field_target),bits_alterados_por_campo=dict(field_effect),quadros_com_alteracao_fisica=changed_frames,cabecalhos_divergentes=sum(r['cabecalho_diverge']=='SIM' for r in rx),cabecalhos_aceitos_divergentes=sum(r['cabecalho_diverge']=='SIM' and r['decisao']=='ACEITO' for r in rx),amostras_ruido=len(noise),casos_nao_detectados=cases,validacao_independente='PASS')

def csv_string(records):
    for r in records: yield ','.join(r.values())

def controls_from_chat():
    ev=ROOT/'evidencias/corpo64'; out={}
    for tech,name in [('SUM8',TRIAGEM[-2]),('CRC8',TRIAGEM[-1])]:
        lines=(ev/f'tx_{tech}_10.log').read_text(encoding='utf-8').splitlines()
        data=list(csv.DictReader(io.StringIO('\n'.join(s for s in lines if not s.startswith('#')))))
        assert len(data)==10 and [int(r['seq']) for r in data]==list(range(10))
        references={int(r['seq_experimento']):r['corpo_tx_hex'] for r in rows(REV/'triagem'/name/'rx.csv')}
        for r in data:
            assert r['tecnica']==tech and r['corpo_tx_hex']==references[int(r['seq'])]
            assert residue(bytes.fromhex(r['corpo_tx_hex']),tech)==0
        out[tech]={'calculo_us':stats(int(r['calculo_us']) for r in data),'intervalo_us':stats(int(r['intervalo_us']) for r in data if int(r['seq'])>0),'fonte':f'evidencias/corpo64/tx_{tech}_10.log','cruzamento_referencias':'PASS'}
    log=(ev/'rx_recuperacao_SUM8_10.log').read_text(encoding='utf-8')
    numeric=[s for s in log.splitlines() if re.match(r'^\d+,',s)]
    header=next(s for s in log.splitlines() if s.startswith('evento_id,'))
    rec=list(csv.DictReader(io.StringIO('\n'.join([header]+numeric))))
    assert len(rec)==10 and sorted(int(r['seq_experimento']) for r in rec)==list(range(10))
    counts=collections.Counter(r['status'] for r in rec)
    assert counts=={'OK':7,'PERDIDO':3}
    accepted=[r for r in rec if r['status']=='OK']
    for r in accepted:
        assert r['corpo_tx_hex']==r['corpo_rx_hex'] and residue(bytes.fromhex(r['corpo_rx_hex']),'SUM8')==0
        assert int(r['decisao_us'])<int(r['referencia_us'])
    off=int(re.search(r'RX_DESLIGADO,.*instante_us=(\d+),',log)[1])
    on=int(re.search(r'RX_REATIVADO,instante_us=(\d+),',log)[1])
    first=next(r for r in accepted if int(r['decisao_us'])>=on)
    decision=int(first['decisao_us'])
    assert on-off==350443 and decision-on==38691
    assert 'janela_desligada_us=350443' in log and 'apos_reativar_ms=38.691' in log and 'rodada_valida=SIM' in log
    out['recuperacao']={'fonte':'evidencias/corpo64/rx_recuperacao_SUM8_10.log','tecnica':'SUM8','quadros':10,'ok':7,'perdidos':3,'sequencias_perdidas':[int(r['seq_experimento']) for r in rec if r['status']=='PERDIDO'],'uart_desligada_us':on-off,'apos_reativar_us':decision-on,'desde_deteccao_us':decision-94797303,'primeira_sequencia_aceita':int(first['seq_rx']),'origem':'Transcricao fornecida no chat; ver ORIGEM.md','validacao':'PASS'}
    return out

def main():
    p=argparse.ArgumentParser(); p.add_argument('--origem',type=Path); args=p.parse_args()
    provenance=[]
    if args.origem:
        for category,names in [('dados',SERIES),('triagem',TRIAGEM)]:
            for name in names:
                dest=REV/category/name; dest.mkdir(parents=True,exist_ok=True)
                for filename in FILES:
                    src=args.origem/name/filename; dst=dest/filename; shutil.copyfile(src,dst)
                    assert src.read_bytes()==dst.read_bytes()
                    provenance.append({'arquivo':dst.relative_to(ROOT).as_posix(),'origem_pasta':name,'bytes':dst.stat().st_size,'sha256':sha(dst)})
        excluded=[]
        for folder in sorted(args.origem.iterdir()):
            if not folder.is_dir() or folder.name in SERIES+TRIAGEM: continue
            dest=REV/'triagem/tentativas_iniciais'/folder.name
            shutil.copytree(folder,dest,dirs_exist_ok=True)
            excluded.append({'pasta':dest.relative_to(ROOT).as_posix(),'motivo':'Tentativa inicial fora das sete rodadas selecionadas; preservada sem compor resultados.'})
        save_json(REV/'PROVENIENCIA_COLETAS.json',{'data':'2026-10-19','origem':'Pastas do coletor local do usuario, copiadas byte a byte. Caminhos absolutos locais omitidos.','arquivos':provenance,'tentativas_fora_da_serie':excluded})
    results=[check(REV/'dados'/name) for name in SERIES]
    controls=[check(REV/'triagem'/name) for name in TRIAGEM]
    combined={t:stats(int(r['verificacao_us']) for run in results if run['tecnica']==t for r in rows(ROOT/run['pasta']/'rx.csv') if r['verificacao_us']!='NA') for t in ['SUM8','CRC8']}
    result={'data':'2026-10-19','escopo':'CORPO64','origem_medicoes':'Ensaios fisicos; testes sinteticos excluidos.','series':results,'triagem':controls,'verificacao_rx_agregada_us':combined,'controles_complementares':controls_from_chat(),'casos_nao_detectados':[dict(c,pasta=s['pasta']) for s in results for c in s['casos_nao_detectados']], 'definicao_taxa':'NAO_DETECTADO / (CORROMPIDO + NAO_DETECTADO); PERDIDO e OK excluidos.', 'limites':'Marcador, referencia e bits start/stop preservados; somente pulsos 1 para 0; nao houve leitura de flash para atestar hashes do firmware instalado.'}
    save_json(REV/'resultados_consolidados.json',result); save_json(ROOT/'resultados_consolidados.json',result)
    save_json(REV/'validacao/VALIDACAO_FISICA.json',{'data':'2026-10-19','estado':'REGISTROS_FISICOS_CONFERIDOS','quadros_series':sum(s['quadros'] for s in results),'amostras_series':sum(s['amostras_ruido'] for s in results),'series_validas':len(results),'controles_validos':len(controls),'cruzamento':'CSV e logs brutos identicos em registros; residuos, alvos, efeito fisico, sincronismo e ordem decisao/auditoria recalculados.', 'assinatura_firmware_gravado_verificada':False,'leitura_flash_realizada':False,'certificacao_eletrica':False})
    print(json.dumps({'series':len(results),'controles':len(controls),'tempos_rx':combined,'nao_detectados':[c['sequencia'] for c in result['casos_nao_detectados']]},ensure_ascii=False,indent=2))

if __name__=='__main__': main()
