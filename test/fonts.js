import fs from "node:fs";
import { packFontFiles } from "../encoder/font.js";

// Packs the test fonts.  The atlases were generated from Open Sans with:
//
//   msdf-atlas-gen -font OpenSans-Regular.ttf -type softmask -size 20
//                  -pxalign on -yorigin top -chars "[0x20,0x7e],0xb0,0xe9,0x20ac"
//                  -imageout artwork/opensans-20.png -json artwork/opensans-20.json

for (let f of ["opensans-20"])
{
    const packed = await packFontFiles(`./artwork/${f}.json`, `./artwork/${f}.png`);
    fs.writeFileSync(`./artwork/${f}.rfnt`, packed.data);
    console.log(`----- ${f} -----`);
    console.log(packed.stats);
}
