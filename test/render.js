import fs from "node:fs";
import { packRle, quantizePngTo565 } from '../encoder/pack.js';

await quantizePngTo565("artwork/grey-gradient.png");

for (let f of ["circles", "red-circle", "grey-gradient", "square", "transparent-square"])
{
    console.log(`----- ${f} -----`);

    var packed = await packRle(`./artwork/${f}.png`, null, null);
    fs.writeFileSync(`./artwork/${f}.bin`, packed.data);

    delete packed.data;
    console.log(packed);
}