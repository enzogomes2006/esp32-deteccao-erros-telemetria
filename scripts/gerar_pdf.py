"""Gera PDF, gráficos vetoriais e fonte LaTeX do relatório Markdown atual."""
from pathlib import Path
import html, json, re
from reportlab.lib import colors
from reportlab.lib.enums import TA_LEFT
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.pdfbase import pdfmetrics
from reportlab.pdfbase.ttfonts import TTFont
from reportlab.platypus import SimpleDocTemplate, Paragraph, Spacer, Table, TableStyle, KeepTogether, Preformatted
from reportlab.graphics.shapes import Drawing, Rect, String, Line
from reportlab.graphics import renderSVG

ROOT=Path(__file__).resolve().parents[1]
NAVY=colors.HexColor('#12334C'); TEAL=colors.HexColor('#247C83'); LIGHT=colors.HexColor('#EEF4F7')
W=A4[0]-112

def fonts():
    candidates=[Path('C:/Windows/Fonts'),Path('/usr/share/fonts/truetype/dejavu')]
    for folder in candidates:
        sets=[('arial.ttf','arialbd.ttf','ariali.ttf','consola.ttf'),('DejaVuSans.ttf','DejaVuSans-Bold.ttf','DejaVuSans-Oblique.ttf','DejaVuSansMono.ttf')]
        for regular,bold,italic,mono in sets:
            if all((folder/n).exists() for n in [regular,bold,italic,mono]):
                for name,file in [('Body',regular),('Body-Bold',bold),('Body-Italic',italic),('Mono',mono)]: pdfmetrics.registerFont(TTFont(name,str(folder/file)))
                pdfmetrics.registerFontFamily('Body',normal='Body',bold='Body-Bold',italic='Body-Italic',boldItalic='Body-Bold')
                return
    raise RuntimeError('Instale Arial e Consolas ou DejaVu Sans/Mono para gerar o PDF.')

def charts(data):
    d=Drawing(W,325); left=88; width=W-left-12
    colorset=[colors.HexColor(c) for c in ['#247C83','#E29B43','#8996A4','#B83E55']]
    for v in range(0,1001,250):
        x=left+width*v/1000
        d.add(Line(x,56,x,300,strokeColor=colors.HexColor('#DDE5EB'),strokeWidth=.5))
        d.add(String(x,39,str(v),fontName='Body',fontSize=8,textAnchor='middle',fillColor=NAVY))
    for i,s in enumerate(data['series']):
        y=275-i*32; d.add(String(left-8,y+6,f"{s['tecnica']}/{s['modo']}",fontName='Body',fontSize=9,textAnchor='end',fillColor=NAVY))
        x=left
        for key,c in zip(['OK','CORROMPIDO','PERDIDO','NAO_DETECTADO'],colorset):
            count=s['contagens'][key]; size=count*width/1000
            if count:
                d.add(Rect(x,y,size,20,fillColor=c,strokeColor=None))
                if count>=80: d.add(String(x+size/2,y+6,str(count),fontName='Body-Bold',fontSize=8,textAnchor='middle',fillColor=colors.white if key in ['OK','PERDIDO'] else NAVY))
            x+=size
    x=5
    for name,c in zip(['OK','Corrompido','Perdido','Não detectado'],colorset):
        d.add(Rect(x,12,9,9,fillColor=c,strokeColor=None)); d.add(String(x+13,13,name,fontName='Body',fontSize=8,fillColor=NAVY)); x+=115
    t=Drawing(W,255); base=60; x0=85; bw=45
    for v in [0,1,2,3,4]:
        y=base+v*38; t.add(Line(48,y,W-5,y,strokeColor=colors.HexColor('#DDE5EB'),strokeWidth=.5));t.add(String(38,y-3,str(v),fontName='Body',fontSize=8,textAnchor='end',fillColor=NAVY))
    for i,s in enumerate(data['series']):
        x=x0+i*55; mean=s['verificacao_rx_us']['media']; color=TEAL if s['tecnica']=='SUM8' else colors.HexColor('#E29B43')
        t.add(Rect(x,base,bw,mean*38,fillColor=color,strokeColor=None));t.add(String(x+bw/2,base+mean*38+7,f'{mean:.3f}'.replace('.',','),fontName='Body-Bold',fontSize=8,textAnchor='middle',fillColor=NAVY));t.add(String(x+bw/2,43,f"{s['tecnica']}/{s['modo']}",fontName='Body',fontSize=8,textAnchor='middle',fillColor=NAVY))
    t.add(String(48,233,'Verificação média no RX (µs)',fontName='Body-Bold',fontSize=10,fillColor=NAVY))
    return {'contagens.svg':d,'tempos_rx.svg':t}

def inline(text):
    s=html.escape(text)
    s=re.sub(r'`([^`]+)`',r'<font name="Mono">\1</font>',s)
    s=re.sub(r'\*\*([^*]+)\*\*',r'<b>\1</b>',s)
    s=re.sub(r'\[([^\]]+)\]\(([^)]+)\)',lambda m:f'<link href="{m[2]}" color="#247C83">{m[1]}</link>',s)
    return s

def blocks(text):
    lines=text.splitlines(); i=0
    while i<len(lines):
        s=lines[i].strip()
        if not s: i+=1;continue
        if s.startswith('```'):
            i+=1; b=[]
            while i<len(lines) and not lines[i].startswith('```'): b.append(lines[i]);i+=1
            yield 'code','\n'.join(b);i+=1;continue
        if s.startswith('|'):
            b=[]
            while i<len(lines) and lines[i].strip().startswith('|'):
                r=[c.strip() for c in lines[i].strip().strip('|').split('|')]
                if not all(re.fullmatch(r':?-+:?',c) for c in r):b.append(r)
                i+=1
            yield 'table',b;continue
        if s.startswith('#'):
            n=len(s)-len(s.lstrip('#'));yield f'h{n}',s[n:].strip();i+=1;continue
        if s.startswith('!['):yield 'image',re.search(r'\]\(([^)]+)\)',s)[1];i+=1;continue
        if s.startswith('- '):yield 'bullet',s[2:];i+=1;continue
        if re.match(r'^\d+\. ',s):yield 'p',s;i+=1;continue
        b=[s];i+=1
        while i<len(lines) and lines[i].strip() and not re.match(r'^\d+\. ',lines[i].strip()) and not lines[i].lstrip().startswith(('#','|','```','![','- ')):
            b.append(lines[i].strip());i+=1
        yield 'p',' '.join(b)

def widths(rows):
    n=len(rows[0])
    if n==2:return [95,W-95]
    if n==3:return [95,75,W-170]
    if n==4:return [W/n]*n
    if n==5:return [W/n]*n
    return [W/n]*n

def footer(canvas,doc):
    canvas.saveState();canvas.setStrokeColor(colors.HexColor('#CCD8E0'));canvas.line(56,43,A4[0]-56,43)
    canvas.setFont('Body',8);canvas.setFillColor(NAVY)
    canvas.drawString(56,30,'ESP32 | CORPO64 | 19/10/2026');canvas.drawRightString(A4[0]-56,30,str(doc.page));canvas.restoreState()

def latex_escape(s):
    chars={'\\':r'\textbackslash{}','&':r'\&','%':r'\%','$':r'\$','#':r'\#','_':r'\_','{':r'\{','}':r'\}','~':r'\textasciitilde{}','^':r'\textasciicircum{}','µ':r'\ensuremath{\mu}','Ω':r'\ensuremath{\Omega}','→':r'\ensuremath{\rightarrow}','·':r'\textperiodcentered{}'}
    return ''.join(chars.get(c,c) for c in s)

def latex(text):
    out=[r'\documentclass[11pt,a4paper]{article}',r'\usepackage[T1]{fontenc}',r'\usepackage[utf8]{inputenc}',r'\usepackage[brazil]{babel}',r'\usepackage{lmodern,longtable,array,geometry,hyperref,graphicx}',r'\geometry{margin=2.3cm}',r'\setlength{\parindent}{0pt}',r'\setlength{\parskip}{0.6em}',r'\emergencystretch=3em',r'\begin{document}']
    for kind,value in blocks(text):
        if kind=='h1':out.append(r'\begin{center}{\LARGE\bfseries '+latex_escape(value)+r'}\end{center}')
        elif kind in ('h2','h3'):out.append(('\\section*{' if kind=='h2' else '\\subsection*{')+latex_escape(value)+'}')
        elif kind=='table':
            ws=widths(value);available=15.8
            spec=''.join(r'>{\raggedright\arraybackslash}p{'+str(round(w/W*available-.3,2))+'cm}' for w in ws)
            out.append(r'\begin{longtable}{'+spec+'}')
            for j,row in enumerate(value):out.append(' & '.join(latex_escape(c) for c in row)+r' \\ '+(r'\hline\endhead' if j==0 else ''))
            out.append(r'\end{longtable}')
        elif kind=='code':out += [r'\begin{verbatim}',value,r'\end{verbatim}']
        elif kind=='image':out.append(r'\begin{center}\includegraphics[width=\linewidth]{../'+value.replace('.svg','.pdf')+r'}\end{center}')
        else:out.append(latex_escape(value).replace('**','')+('\n' if kind!='bullet' else r'\par'))
    out.append(r'\end{document}')
    (ROOT/'latex/main.tex').write_text('\n\n'.join(out)+'\n',encoding='utf-8')

def main():
    fonts(); data=json.loads((ROOT/'resultados_consolidados.json').read_text(encoding='utf-8')); graphs=charts(data)
    graphdir=ROOT/'revisoes/corpo64/graficos'; graphdir.mkdir(parents=True,exist_ok=True)
    from reportlab.graphics import renderPDF
    for name,drawing in graphs.items():
        renderSVG.drawToFile(drawing,str(graphdir/name))
        renderPDF.drawToFile(drawing,str(graphdir/name.replace('.svg','.pdf')))
    text=(ROOT/'Relatorio_Final_ESP32.md').read_text(encoding='utf-8')
    styles=getSampleStyleSheet()
    styles.add(ParagraphStyle(name='Text',fontName='Body',fontSize=9.6,leading=14,spaceAfter=8,splitLongWords=True,textColor=colors.HexColor('#233746')))
    styles.add(ParagraphStyle(name='TitleNow',parent=styles['Text'],fontName='Body-Bold',fontSize=23,leading=29,spaceAfter=18,textColor=NAVY))
    styles.add(ParagraphStyle(name='H2Now',parent=styles['Text'],fontName='Body-Bold',fontSize=14,leading=18,spaceBefore=16,spaceAfter=9,keepWithNext=True,textColor=NAVY))
    styles.add(ParagraphStyle(name='H3Now',parent=styles['Text'],fontName='Body-Bold',fontSize=11,leading=15,spaceBefore=10,keepWithNext=True,textColor=TEAL))
    styles.add(ParagraphStyle(name='CellNow',parent=styles['Text'],fontSize=8,leading=11,spaceAfter=0))
    styles.add(ParagraphStyle(name='CodeNow',fontName='Mono',fontSize=8,leading=11,leftIndent=8,spaceBefore=7,spaceAfter=10))
    story=[]; pending=[]; skipped=set(); items=list(blocks(text))
    def emit(flowables, keep=False):
        nonlocal pending
        group=pending+flowables
        if pending or keep:story.append(KeepTogether(group))
        else:story.extend(group)
        pending=[]
    for idx,(kind,value) in enumerate(items):
        if idx in skipped:continue
        if kind in ['h1','h2','h3']:
            pending.append(Paragraph(inline(value),styles[{'h1':'TitleNow','h2':'H2Now','h3':'H3Now'}[kind]]))
        elif kind=='table':
            tab=Table([[Paragraph(inline(c),styles['CellNow']) for c in row] for row in value],colWidths=widths(value),repeatRows=1,hAlign='LEFT')
            tab.setStyle(TableStyle([('BACKGROUND',(0,0),(-1,0),LIGHT),('TEXTCOLOR',(0,0),(-1,0),NAVY),('LINEBELOW',(0,0),(-1,0),1,TEAL),('LINEBELOW',(0,1),(-1,-1),.3,colors.HexColor('#DCE5EB')),('VALIGN',(0,0),(-1,-1),'TOP'),('TOPPADDING',(0,0),(-1,-1),7),('BOTTOMPADDING',(0,0),(-1,-1),7),('LEFTPADDING',(0,0),(-1,-1),6),('RIGHTPADDING',(0,0),(-1,-1),6)]))
            emit([tab,Spacer(1,10)],keep=True)
        elif kind=='image':
            group=[graphs[Path(value).name],Spacer(1,7)]
            if idx+1<len(items) and items[idx+1][0]=='p' and items[idx+1][1].startswith('Figura '):
                group.append(Paragraph(inline(items[idx+1][1]),styles['Text']));skipped.add(idx+1)
            emit(group,keep=True)
        elif kind=='code':emit([Preformatted(value,styles['CodeNow'])])
        else:emit([Paragraph(('• ' if kind=='bullet' else '')+inline(value),styles['Text'])])
    pdf=ROOT/'Relatorio_Final_ESP32.pdf'
    SimpleDocTemplate(str(pdf),pagesize=A4,rightMargin=56,leftMargin=56,topMargin=48,bottomMargin=57,title='Detecção de erros em telemetria - CORPO64',author='Enzo Aurélio',pageCompression=1).build(story,onFirstPage=footer,onLaterPages=footer)
    latex(text)
    (ROOT/'latex/LEIA_LATEX.md').write_text('# Fonte LaTeX atual\n\nmain.tex contém o mesmo conteúdo do relatório Markdown CORPO64 e é regenerado por scripts/gerar_pdf.py. Compile com pdfLaTeX na pasta latex/. Os gráficos PDF ficam em revisoes/corpo64/graficos/.\n\nO PDF publicado na raiz é diagramado com ReportLab a partir do Markdown pelo mesmo script. A compilação LaTeX é uma alternativa de layout; não há promessa de paginação idêntica. A fonte antiga foi preservada em historico/payload_2026-10-17/latex/.\n',encoding='utf-8')
    print('PDF, dois gráficos vetoriais e fonte LaTeX gerados.')

if __name__=='__main__':main()
