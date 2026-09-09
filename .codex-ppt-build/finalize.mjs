import fs from 'node:fs/promises';
import path from 'node:path';
import {FileBlob,PresentationFile} from '@oai/artifact-tool';
import {finalizePresentation} from 'file:///C:/Users/JakeL/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations/container_tools/artifact_tool_utils.mjs';
const root=process.cwd(), skill='C:/Users/JakeL/.codex/plugins/cache/openai-primary-runtime/presentations/26.905.11957/skills/presentations';
const p=await PresentationFile.importPptx(await FileBlob.load('.codex-ppt-build/candidate.pptx'));
for(let i=0;i<p.slides.items.length;i++){const b=await p.export({slide:p.slides.items[i],format:'png',scale:1.5});await fs.writeFile(`.codex-ppt-build/after-${i+1}.png`,new Uint8Array(await b.arrayBuffer()));}
const r=await finalizePresentation({workspaceDir:root,candidatePath:path.join(root,'.codex-ppt-build/candidate.pptx'),finalPath:path.join(root,'output/东软充电桩平台答辩PPT-第5组-完善版.pptx'),pythonExecutable:'C:/Users/JakeL/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',integrityValidatorPath:path.join(skill,'container_tools/inspect_presentation_package_integrity.py'),layoutValidatorPath:path.join(skill,'container_tools/inspect_presentation_layout_geometry.py'),explicitTotalSlideCount:22,verifyArtifactToolImport:true,receiptPath:path.join(root,'.codex-ppt-build/validation.json')});console.log(JSON.stringify(r));

