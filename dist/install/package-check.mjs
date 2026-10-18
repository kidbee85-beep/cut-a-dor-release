// SPDX-License-Identifier: GPL-3.0-only
import {productOf, logicalImage} from './fm1pkg.js';
export const KNOWN_HASHES = Object.freeze({
 cad:'5d4be92061cc5bf1396d3ff599ac3a1ef9ec1261afe003b74e32f4298307f614',
 stock:'db1642b2b6fa5c2cccb11ffd13878068bb28601678d3644049f99dc40e7edb8a'
});
export async function checkedPackage(raw,hash,product){
 if(raw.length<960||raw.length>2*1024*1024)throw Error('Invalid firmware package size. Nothing was written.');
 const actual=[...new Uint8Array(await crypto.subtle.digest('SHA-256',raw))].map(x=>x.toString(16).padStart(2,'0')).join('');
 if(actual!==hash)throw Error('The firmware fingerprint does not match. Nothing was written.');
 const id=productOf(raw);
 if(!/^FM-1_\d+$/.test(id)||(product&&id!==product))throw Error('This package has the wrong device identity.');
 return {raw,product:id,image:logicalImage(raw),hash};
}
