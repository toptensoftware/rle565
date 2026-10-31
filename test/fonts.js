import fs from "node:fs";
import { packFont } from "../encoder/font.js";

// Packs the test fonts from Open Sans (Apache License 2.0, see
// artwork/OpenSans-LICENSE.txt)

const fonts = [
    { file: "opensans-20", size: 20, subpixel: 1 },
    { file: "opensans-20-sp4", size: 20, subpixel: 4 },
];

for (const f of fonts)
{
    const packed = packFont("./artwork/OpenSans-Regular.ttf", {
        size: f.size,
        subpixel: f.subpixel,
        charset: "32-126,0xB0,0xE9,0x20AC",
    });
    fs.writeFileSync(`./artwork/${f.file}.rfnt`, packed.data);
    console.log(`----- ${f.file} -----`);
    console.log(packed.stats);
}
