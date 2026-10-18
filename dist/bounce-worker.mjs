// SPDX-License-Identifier: GPL-3.0-only
import {renderWav} from './bounce.mjs';
self.onmessage=async({data:m})=>{try{const bytes=await renderWav(m.wasm,m.project,m.banks,m.bars);self.postMessage({bytes},[bytes.buffer]);}catch(e){self.postMessage({error:e.message});}};
