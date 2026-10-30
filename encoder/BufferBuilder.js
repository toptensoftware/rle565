export class BufferBuilder
{
    constructor()
    {
        this.pos = 0;
        this.buffer = Buffer.alloc(0x10000);
        this.bufferUsed = 0;
    }

    use(len)
    {
        // Track used length
        if (len > this.bufferUsed)
            this.bufferUsed = len;

        // Ensure buffer big enough
        if (len > this.buffer.length)
        {
            let newLen = this.buffer.length + 0x10000;
            while (newLen < len)
                newLen += 0x10000;

            let newBuf = Buffer.alloc(newLen);
            this.buffer.copy(newBuf);
            this.buffer = newBuf;
        }
    }

    reserve(len)
    {
        this.pos += len;
        this.use(this.pos);
    }

    writeUInt8(value)
    {
        this.use(this.pos + 1);
        this.buffer.writeUInt8(value, this.pos);
        this.pos++;
    }

    writeUInt16(value)
    {
        this.use(this.pos + 2);
        this.buffer.writeUInt16LE(value, this.pos);
        this.pos += 2;
    }

    writeUInt32(value)
    {
        this.use(this.pos + 4);
        this.buffer.writeUInt32LE(value, this.pos);
        this.pos += 4;
    }

    writeInt16(value)
    {
        this.use(this.pos + 2);
        this.buffer.writeInt16LE(value, this.pos);
        this.pos += 2;
    }

    writeInt32(value)
    {
        this.use(this.pos + 4);
        this.buffer.writeInt32LE(value, this.pos);
        this.pos += 4;
    }

    writeDouble(value)
    {
        this.use(this.pos + 8);
        this.buffer.writeDoubleLE(value, this.pos);
        this.pos += 8;
    }

    writeVarLen(value, cont)
    {
        // Write MSB first
        if ((value & 0xFFFFFF80)!=0)
            this.writeVarLen(value >> 7, true);

        // Write
        this.writeUInt8((value & 0x7F) | (cont ? 0x80 : 0));
    }
 
    seek(pos)
    {
        this.pos = pos;
    }

    get length()
    {
        return this.bufferUsed;
    }

    finalize()
    {
        return this.buffer.subarray(0, this.bufferUsed);
    }
}