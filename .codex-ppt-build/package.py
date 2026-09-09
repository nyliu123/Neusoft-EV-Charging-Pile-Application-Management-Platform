import json,zipfile,re,html,xml.etree.ElementTree as E
from pathlib import Path
src=zipfile.ZipFile('东软充电桩平台答辩PPT-第5组.pptx'); files={n:src.read(n) for n in src.namelist()}; data=json.loads(Path('.codex-ppt-build/changes.json').read_text(encoding='utf8'))
def patch(raw,m):
    found=set()
    def f(x):
        t=html.unescape(x.group(2))
        if t in m:found.add(t);return x.group(1)+html.escape(m[t],quote=False)+x.group(3)
        return x.group(0)
    out=re.sub(r'(<a:t(?:\s[^>]*)?>)(.*?)(</a:t>)',f,raw.decode(),flags=re.S)
    assert set(m)==found,set(m)-found
    return out.encode()
for n,m in data['changes'].items():files[f'ppt/slides/slide{n}.xml']=patch(files[f'ppt/slides/slide{n}.xml'],m)
for i,m in enumerate(data['newslides'],20):
    files[f'ppt/slides/slide{i}.xml']=patch(src.read('ppt/slides/slide17.xml'),m)
    r=src.read('ppt/slides/_rels/slide17.xml.rels').decode()
    r=re.sub(r'<Relationship\b[^>]*Type="[^"]*/notesSlide"[^>]*/>','',r)
    files[f'ppt/slides/_rels/slide{i}.xml.rels']=r.encode()
# Preserve package assets and all original XML formatting. Only text and slide order change.
r=files['ppt/_rels/presentation.xml.rels'].decode(); existing=[int(x) for x in re.findall(r'Id="rId(\d+)"',r)]; start=max(existing)+1
for i in range(20,23):r=r.replace('</Relationships>',f'<Relationship Id="rId{start+i-20}" Type="http://schemas.openxmlformats.org/officeDocument/2006/relationships/slide" Target="slides/slide{i}.xml"/></Relationships>')
files['ppt/_rels/presentation.xml.rels']=r.encode()
p=files['ppt/presentation.xml'].decode(); ids=re.findall(r'<p:sldId\b[^>]*/>',p); assert len(ids)==19
maxid=max(int(re.search(r'\bid="(\d+)"',s).group(1)) for s in ids)
new=[f'<p:sldId id="{maxid+j+1}" r:id="rId{start+j}"/>' for j in range(3)]
order=ids[:8]+new[:2]+ids[8:17]+new[2:]+ids[17:]
p=re.sub(r'(<p:sldIdLst>).*?(</p:sldIdLst>)',lambda m:m.group(1)+''.join(order)+m.group(2),p,flags=re.S);files['ppt/presentation.xml']=p.encode()
c=files['[Content_Types].xml'].decode()
for i in range(20,23):c=c.replace('</Types>',f'<Override PartName="/ppt/slides/slide{i}.xml" ContentType="application/vnd.openxmlformats-officedocument.presentationml.slide+xml"/></Types>')
files['[Content_Types].xml']=c.encode()
for orig,actual in [(20,9),(21,10),(22,20)]+[(n,n+2) for n in range(9,18)]+[(18,21)]:
    files[f'ppt/slides/slide{orig}.xml']=patch(files[f'ppt/slides/slide{orig}.xml'],{str(orig).zfill(2):str(actual).zfill(2)})
if 'docProps/app.xml' in files:files['docProps/app.xml']=re.sub(rb'<Slides>\d+</Slides>',b'<Slides>22</Slides>',files['docProps/app.xml'])
with zipfile.ZipFile('.codex-ppt-build/candidate.pptx','w',zipfile.ZIP_DEFLATED) as out:
    for n,b in files.items():out.writestr(n,b)
for n,b in files.items():
    if n.startswith(('ppt/media/','ppt/theme/','ppt/slideMasters/','ppt/slideLayouts/')):assert b==src.read(n)
print('22 slides; original media, themes, masters and layouts preserved byte-for-byte')
