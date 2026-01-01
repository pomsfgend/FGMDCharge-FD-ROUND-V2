#!/usr/bin/env python3
"""Inspect or force the ESP-IDF two-copy otadata slot selector used by this cube.
The CRC is the same reflected CRC-32 used by ESP-IDF for ota_seq with initial 0xFFFFFFFF.
"""
import argparse, struct, subprocess, sys, tempfile, pathlib, zlib
OTADATA=0xD000
SIZE=0x2000
ENTRY=32

def crc(seq:int)->int:
    return zlib.crc32(struct.pack('<I',seq), 0xFFFFFFFF) & 0xFFFFFFFF

def call_read(port,path):
    return subprocess.call([sys.executable,'-m','esptool','--chip','esp32c3','--port',port,'read-flash',hex(OTADATA),hex(SIZE),str(path)])

def call_write(port,path):
    return subprocess.call([sys.executable,'-m','esptool','--chip','esp32c3','--port',port,'write-flash',hex(OTADATA),str(path)])

def show(path):
    b=path.read_bytes()
    for i in range(2):
        e=b[i*ENTRY:(i+1)*ENTRY]
        seq,=struct.unpack_from('<I',e,0); stored,=struct.unpack_from('<I',e,28); state,=struct.unpack_from('<I',e,24)
        ok=(stored==crc(seq))
        print(f'entry{i}: seq={seq} slot={((seq-1)%2) if seq not in (0,0xFFFFFFFF) else "?"} state=0x{state:08X} crc=0x{stored:08X} valid_crc={ok}')

def force(port,slot):
    if slot not in (0,1): raise SystemExit('slot must be 0 or 1')
    with tempfile.TemporaryDirectory() as td:
        path=pathlib.Path(td)/'otadata.bin'
        if call_read(port,path)!=0: raise SystemExit(1)
        b=bytearray(path.read_bytes())
        # seq 1 boots OTA0, seq 2 boots OTA1. Mirror to both copies so an older valid copy cannot win.
        seq=slot+1
        e=bytearray(b[:ENTRY]); e[:ENTRY]=b'\xFF'*ENTRY
        struct.pack_into('<I',e,0,seq)
        struct.pack_into('<I',e,28,crc(seq))
        for off in (0,ENTRY): b[off:off+ENTRY]=e
        path.write_bytes(b)
        if call_write(port,path)!=0: raise SystemExit(1)
        print(f'Forced OTA{slot}; seq={seq}; CRC=0x{crc(seq):08X}')

def main():
    p=argparse.ArgumentParser(); p.add_argument('--port',required=True); p.add_argument('action',choices=['show','ota0','ota1'])
    a=p.parse_args()
    if a.action=='show':
        with tempfile.TemporaryDirectory() as td:
            path=pathlib.Path(td)/'otadata.bin'
            if call_read(a.port,path)==0: show(path)
    else: force(a.port,0 if a.action=='ota0' else 1)
if __name__=='__main__': main()
