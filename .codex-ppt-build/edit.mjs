import fs from 'node:fs/promises';
import {FileBlob,PresentationFile} from '@oai/artifact-tool';
const p=await PresentationFile.importPptx(await FileBlob.load('东软充电桩平台答辩PPT-第5组.pptx'));
const {changes}=JSON.parse(await fs.readFile('.codex-ppt-build/changes.json','utf8'));
const snap=(await p.inspect({kind:'textbox',maxChars:500000})).ndjson.split('\n').filter(Boolean).map(s=>JSON.parse(s));
let count=0;
for(const [num, edits] of Object.entries(changes))for(const [old,value] of Object.entries(edits)){
 const targets=snap.filter(s=>s.slide===Number(num) && (s.text??s.textPreview??'').includes(old));
 for(const target of targets){p.resolve(target.id).text.replace(old,value);count++;}
}
await(await PresentationFile.exportPptx(p)).save('.codex-ppt-build/artifact-edited.pptx');
console.log('Edited text objects',count);
