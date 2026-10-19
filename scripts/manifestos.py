"""Atualiza hashes SHA256; cada manifesto exclui somente a si próprio."""
from pathlib import Path
import hashlib,json
ROOT=Path(__file__).resolve().parents[1]

def build(folder):
    dest=folder/'MANIFESTO_SHA256.json'; entries={}
    for p in sorted(folder.rglob('*')):
        if not p.is_file() or '.git' in p.parts or '__pycache__' in p.parts or p==dest:continue
        if p.suffix in ['.pyc','.aux','.toc','.out','.synctex.gz'] or p.name in ['main.log']:continue
        if any(part in ['Coletas','build','.arduinoIDE'] for part in p.relative_to(folder).parts):continue
        data=p.read_bytes(); entries[p.relative_to(folder).as_posix()]={'bytes':len(data),'sha256':hashlib.sha256(data).hexdigest()}
    dest.write_text(json.dumps({'algoritmo':'SHA256','data':'2026-10-19','observacao':'Exclui este manifesto, metadados Git e arquivos locais de compilacao/cache. Manifestos historicos sao copias datadas.','arquivos':entries},ensure_ascii=False,indent=2)+'\n',encoding='utf-8')
    for name,record in entries.items():assert hashlib.sha256((folder/name).read_bytes()).hexdigest()==record['sha256']
    return len(entries)

if __name__=='__main__':
    print('Arquivos revisão:',build(ROOT/'revisoes/corpo64'))
    print('Arquivos repositório:',build(ROOT))
