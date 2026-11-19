#!/usr/bin/env python3
"""
nds_extract.py - Extractor de ROMs Nintendo DS para el proyecto de recomp.

Uso:  python nds_extract.py rom.nds salida/

Genera:
  salida/arm9.bin            ARM9 descomprimido (cargado en arm9_load)
  salida/arm7.bin            ARM7
  salida/overlays/ovNN.bin   overlays ARM9 descomprimidos
  salida/files/...           sistema de archivos (NitroFS)
  salida/manifest.json       direcciones de carga, overlays y hash
"""
import sys, os, struct, json, hashlib

def blz_decompress(data: bytes) -> bytes:
    """Descompresion BLZ (LZ77 hacia atras) usada por ARM9 y overlays."""
    n = len(data)
    if n < 8:
        return data
    inc = struct.unpack_from('<I', data, n - 4)[0]
    if inc == 0:
        return data
    hdr = data[n - 5]
    enc = struct.unpack_from('<I', data, n - 8)[0] & 0xFFFFFF
    dec_len = n + inc
    out = bytearray(dec_len)
    out[:n] = data
    src = n - hdr
    dst = dec_len
    stop = n - enc
    while src > stop:
        src -= 1
        flags = data[src]
        for _ in range(8):
            if src <= stop:
                break
            if flags & 0x80:
                src -= 1; b1 = data[src]
                src -= 1; b2 = data[src]
                length = (b1 >> 4) + 3
                disp = (((b1 & 0xF) << 8) | b2) + 3
                for _ in range(length):
                    dst -= 1
                    out[dst] = out[dst + disp]
            else:
                src -= 1
                dst -= 1
                out[dst] = data[src]
            flags = (flags << 1) & 0xFF
    return bytes(out)

class NDS:
    def __init__(self, path):
        self.d = open(path, 'rb').read()
        d = self.d
        self.title = d[0:12].rstrip(b'\0').decode('ascii', 'replace')
        self.gamecode = d[12:16].decode('ascii', 'replace')
        (self.arm9_off, self.arm9_entry, self.arm9_load, self.arm9_size,
         self.arm7_off, self.arm7_entry, self.arm7_load, self.arm7_size,
         self.fnt_off, self.fnt_size, self.fat_off, self.fat_size,
         self.ovt9_off, self.ovt9_size, self.ovt7_off, self.ovt7_size) = \
            struct.unpack_from('<16I', d, 0x20)
        self.sha1 = hashlib.sha1(d).hexdigest()

    def fat(self, fid):
        return struct.unpack_from('<II', self.d, self.fat_off + fid * 8)

    def file(self, fid):
        s, e = self.fat(fid)
        return self.d[s:e]

    def overlays9(self):
        res = []
        for i in range(self.ovt9_size // 32):
            oid, ram, size, bss, si, ei, fid, comp = struct.unpack_from(
                '<8I', self.d, self.ovt9_off + i * 32)
            res.append(dict(id=oid, ram=ram, size=size, bss=bss, file_id=fid,
                            compressed=bool(comp >> 24 & 1),
                            comp_size=comp & 0xFFFFFF))
        return res

    def tree(self):
        fnt = self.d[self.fnt_off:self.fnt_off + self.fnt_size]
        nd = struct.unpack_from('<H', fnt, 6)[0]
        dirs = {0xF000 + i: struct.unpack_from('<IHH', fnt, i * 8) for i in range(nd)}
        files = {}
        def walk(did, path):
            off, fid, _ = dirs[did]
            p = off
            while True:
                t = fnt[p]; p += 1
                if t == 0:
                    break
                ln = t & 0x7F
                name = fnt[p:p + ln].decode('latin1'); p += ln
                if t & 0x80:
                    sid = struct.unpack_from('<H', fnt, p)[0]; p += 2
                    walk(sid, path + name + '/')
                else:
                    files[fid] = path + name; fid += 1
        walk(0xF000, '')
        return files

def main():
    if len(sys.argv) != 3:
        print(__doc__); sys.exit(1)
    rom, out = sys.argv[1], sys.argv[2]
    nds = NDS(rom)
    os.makedirs(os.path.join(out, 'overlays'), exist_ok=True)

    a9_raw = nds.d[nds.arm9_off:nds.arm9_off + nds.arm9_size]
    a9 = blz_decompress(a9_raw)
    open(os.path.join(out, 'arm9.bin'), 'wb').write(a9)
    a7 = nds.d[nds.arm7_off:nds.arm7_off + nds.arm7_size]
    open(os.path.join(out, 'arm7.bin'), 'wb').write(a7)

    ovs = nds.overlays9()
    for ov in ovs:
        raw = nds.file(ov['file_id'])
        dec = blz_decompress(raw) if ov['compressed'] else raw
        ov['decompressed_size'] = len(dec)
        open(os.path.join(out, 'overlays', 'ov%02d.bin' % ov['id']), 'wb').write(dec)

    files = nds.tree()
    ov_ids = {o['file_id'] for o in ovs}
    n = 0
    for fid, path in files.items():
        dst = os.path.join(out, 'files', path)
        os.makedirs(os.path.dirname(dst), exist_ok=True)
        open(dst, 'wb').write(nds.file(fid))
        n += 1

    manifest = dict(title=nds.title, gamecode=nds.gamecode, sha1=nds.sha1,
                    arm9=dict(load=nds.arm9_load, entry=nds.arm9_entry,
                              size_compressed=nds.arm9_size, size=len(a9)),
                    arm7=dict(load=nds.arm7_load, entry=nds.arm7_entry, size=nds.arm7_size),
                    overlays9=ovs, file_count=n)
    json.dump(manifest, open(os.path.join(out, 'manifest.json'), 'w'), indent=2)
    print(f"[ok] {nds.title} ({nds.gamecode}) sha1={nds.sha1}")
    print(f"     ARM9 {nds.arm9_size:#x} -> {len(a9):#x} bytes, {len(ovs)} overlays, {n} archivos")

if __name__ == '__main__':
    main()
