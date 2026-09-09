import fs from 'node:fs/promises';
import {FileBlob,PresentationFile} from '@oai/artifact-tool';
const p=await PresentationFile.importPptx(await FileBlob.load('东软充电桩平台答辩PPT-第5组.pptx'));
await fs.writeFile('.codex-ppt-build/inspect.ndjson',(await p.inspect({kind:'slide,textbox,image,layout',maxChars:300000})).ndjson);
for(let i=0;i<p.slides.items.length;i++) {try{let b=await p.export({slide:p.slides.items[i],format:'png',scale:1});await fs.writeFile(`.codex-ppt-build/before-${i+1}.png`,new Uint8Array(await b.arrayBuffer()));}catch(e){console.log('render',i+1,e.message)}}
console.log('slides',p.slides.items.length);
