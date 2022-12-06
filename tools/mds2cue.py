#
# Copyright 2017-2022 Adrià Giménez Pastor.
#
# This file is part of adriagipas/CD.
#
# adriagipas/CD is free software: you can redistribute it and/or
# modify it under the terms of the GNU General Public License as
# published by the Free Software Foundation, either version 3 of the
# License, or (at your option) any later version.
#
# adriagipas/CD is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with adriagipas/CD.  If not, see <https://www.gnu.org/licenses/>.
#

import sys,struct,re

SUBQ_CRCTAB= [
    0x0000, 0x1021, 0x2042, 0x3063, 0x4084, 0x50A5, 0x60C6, 0x70E7, 0x8108,
    0x9129, 0xA14A, 0xB16B, 0xC18C, 0xD1AD, 0xE1CE, 0xF1EF, 0x1231, 0x0210,
    0x3273, 0x2252, 0x52B5, 0x4294, 0x72F7, 0x62D6, 0x9339, 0x8318, 0xB37B,
    0xA35A, 0xD3BD, 0xC39C, 0xF3FF, 0xE3DE, 0x2462, 0x3443, 0x0420, 0x1401,
    0x64E6, 0x74C7, 0x44A4, 0x5485, 0xA56A, 0xB54B, 0x8528, 0x9509, 0xE5EE,
    0xF5CF, 0xC5AC, 0xD58D, 0x3653, 0x2672, 0x1611, 0x0630, 0x76D7, 0x66F6,
    0x5695, 0x46B4, 0xB75B, 0xA77A, 0x9719, 0x8738, 0xF7DF, 0xE7FE, 0xD79D,
    0xC7BC, 0x48C4, 0x58E5, 0x6886, 0x78A7, 0x0840, 0x1861, 0x2802, 0x3823,
    0xC9CC, 0xD9ED, 0xE98E, 0xF9AF, 0x8948, 0x9969, 0xA90A, 0xB92B, 0x5AF5,
    0x4AD4, 0x7AB7, 0x6A96, 0x1A71, 0x0A50, 0x3A33, 0x2A12, 0xDBFD, 0xCBDC,
    0xFBBF, 0xEB9E, 0x9B79, 0x8B58, 0xBB3B, 0xAB1A, 0x6CA6, 0x7C87, 0x4CE4,
    0x5CC5, 0x2C22, 0x3C03, 0x0C60, 0x1C41, 0xEDAE, 0xFD8F, 0xCDEC, 0xDDCD,
    0xAD2A, 0xBD0B, 0x8D68, 0x9D49, 0x7E97, 0x6EB6, 0x5ED5, 0x4EF4, 0x3E13,
    0x2E32, 0x1E51, 0x0E70, 0xFF9F, 0xEFBE, 0xDFDD, 0xCFFC, 0xBF1B, 0xAF3A,
    0x9F59, 0x8F78, 0x9188, 0x81A9, 0xB1CA, 0xA1EB, 0xD10C, 0xC12D, 0xF14E,
    0xE16F, 0x1080, 0x00A1, 0x30C2, 0x20E3, 0x5004, 0x4025, 0x7046, 0x6067,
    0x83B9, 0x9398, 0xA3FB, 0xB3DA, 0xC33D, 0xD31C, 0xE37F, 0xF35E, 0x02B1,
    0x1290, 0x22F3, 0x32D2, 0x4235, 0x5214, 0x6277, 0x7256, 0xB5EA, 0xA5CB,
    0x95A8, 0x8589, 0xF56E, 0xE54F, 0xD52C, 0xC50D, 0x34E2, 0x24C3, 0x14A0,
    0x0481, 0x7466, 0x6447, 0x5424, 0x4405, 0xA7DB, 0xB7FA, 0x8799, 0x97B8,
    0xE75F, 0xF77E, 0xC71D, 0xD73C, 0x26D3, 0x36F2, 0x0691, 0x16B0, 0x6657,
    0x7676, 0x4615, 0x5634, 0xD94C, 0xC96D, 0xF90E, 0xE92F, 0x99C8, 0x89E9,
    0xB98A, 0xA9AB, 0x5844, 0x4865, 0x7806, 0x6827, 0x18C0, 0x08E1, 0x3882,
    0x28A3, 0xCB7D, 0xDB5C, 0xEB3F, 0xFB1E, 0x8BF9, 0x9BD8, 0xABBB, 0xBB9A,
    0x4A75, 0x5A54, 0x6A37, 0x7A16, 0x0AF1, 0x1AD0, 0x2AB3, 0x3A92, 0xFD2E,
    0xED0F, 0xDD6C, 0xCD4D, 0xBDAA, 0xAD8B, 0x9DE8, 0x8DC9, 0x7C26, 0x6C07,
    0x5C64, 0x4C45, 0x3CA2, 0x2C83, 0x1CE0, 0x0CC1, 0xEF1F, 0xFF3E, 0xCF5D,
    0xDF7C, 0xAF9B, 0xBFBA, 0x8FD9, 0x9FF8, 0x6E17, 0x7E36, 0x4E55, 0x5E74,
    0x2E93, 0x3EB2, 0x0ED1, 0x1EF0
    ]

def check_crc(data):
    crc= 0;
    for v in data[:10]:
        v= int(v)
        crc= SUBQ_CRCTAB[(crc>>8)^v] ^ ((crc<<8)&0xFFFF)
    crc= (~crc)&0xFFFF
    data_crc= struct.unpack('>H',data[10:])[0]
    #if crc!=data_crc: print(crc,data_crc)
    return crc==data_crc

def INT(data,pos): return struct.unpack('i',data[pos:pos+4])[0]
def ULONG(data,pos): return struct.unpack('L',data[pos:pos+8])[0]
def SHORT(data,pos): return struct.unpack('h',data[pos:pos+2])[0]
def CHAR(data,pos): return struct.unpack('b',data[pos:pos+1])[0]
def UCHAR(data,pos): return struct.unpack('B',data[pos:pos+1])[0]
    
class MDS_Session:
    def __init__(self,f):
        data= f.read(0x18)
        self.start_sector= INT(data,0)
        self.end_sector= INT(data,0x04)
        self.number= SHORT(data,0x08)
        self.num_data_blocks= CHAR(data,0xa)
        self.num_data_blocks_point_gt_a0= CHAR(data,0x0b)
        self.first_track_number= SHORT(data,0x0c)
        self.last_track_number= SHORT(data,0x0e)
        self.offset_first_data_block= INT(data,0x14)
    def __repr__(self):
        return """
<Session
  Start sector: %d
  End sector: %d
  Number: %d
  Number of data blocks: %d
  Number of data blocks with point >= A0h: %d
  First track number: %d
  Last track number: %d
  Offest first data-block: %d
>
"""%(self.start_sector,
     self.end_sector,
     self.number,
     self.num_data_blocks,
     self.num_data_blocks_point_gt_a0,
     self.first_track_number,
     self.last_track_number,
     self.offset_first_data_block)
# end MDS_Session

class MDS_Data:

    def __init__(self,f):
        data= f.read(0x50)
        self.track_mode= UCHAR(data,0x00)
        self.num_subchannels= UCHAR(data,0x01)
        self.adr_control= UCHAR(data,0x02)
        self.trackno= UCHAR(data,0x03)
        self.point= UCHAR(data,0x04)
        self.mm= UCHAR(data,0x09)
        self.ss= UCHAR(data,0x0a)
        self.ff= UCHAR(data,0x0b)
        self.mdf_offset= ULONG(data,0x28)

    def __repr__(self):
        return """
<Data
  Track mode: %s
  Number of subchannels: %d
  ADR/control: %s
  TrackNo: %d
  Point: %s
  Minute: %d
  Second: %d
  Frame: %d
  MDF offset: %d
"""%(hex(self.track_mode),
     self.num_subchannels,
     hex(self.adr_control),
     self.trackno,
     hex(self.point),
     self.mm,self.ss,self.ff,
     self.mdf_offset)
    
# end MDS_Data

class MDS_NumSectors:

    def __init__(self,f):
        data= f.read(8)
        self.num_sectors= [INT(data,0),INT(data,4)]

    def __repr__(self):
        return '<NumSectors %s>'%str(self.num_sectors)
    
# end MDS_NumSectors

class MDS:

    def __read_filename(self,f,fn):
        self.fn= fn
        # Obté offset
        data= f.read(0x10)
        offset= INT(data,0x00)
        fformat= UCHAR(data,0x04)
        # Llig text.
        with open(fn,'rb') as f2:
            data= f2.read()
        data= data[offset:]
        try:
            if fformat: # 16 bit
                ret= data.decode('utf-16')
            else:
                ret= data.decode('utf-8')
        except:
            ret= None
        return ret
        
    def __read_header(self,f):
        data= f.read(0x60) ## era 0x60 !!!!!
        # Magic number
        mnum= data[:16].decode('utf-8')
        assert mnum=='MEDIA DESCRIPTOR'
        # Obté metadades
        media_type= SHORT(data,0x12)
        num_sess= SHORT(data,0x14)
        if media_type!=0: sys.exit('sols es suporten CDs')
        return num_sess
        
    def __init__(self,fn):
        with open(fn,'rb') as f:
            num_sess= self.__read_header(f)
            self.sessions= []
            for i in range(num_sess):
                s= MDS_Session(f)
                self.sessions.append(s)
            num_data= sum([x.num_data_blocks for x in self.sessions])
            self.data= []
            for i in range(num_data):
                d= MDS_Data(f)
                self.data.append(d)
            self.num_sectors= []
            for i in range(num_data):
                nums= MDS_NumSectors(f)
                self.num_sectors.append(nums)
            self.fname= self.__read_filename(f,fn)
            self.N= num_data
    # end __init__

    def __repr__(self):
        return """
<MDS
  Sessions: %s
  Data blocks: %s
  Num sectors: %s
  File name: %s
  N: %d
>
"""%(self.sessions,
     self.data,
     self.num_sectors,
     self.fname,
     self.N)
    # end __repr__

    TRACK_SIZES= {
        0xec : (0x990,0x930), # Grandària sector, grandària que vull copiar,
        0xa9 : (0x930,0x930)
        }
    def write_bin(self,pref):
        # Obri el mdf.
        mdf= re.sub(r'[.]mds$','.mdf',self.fn)
        with open(mdf,'rb') as f:
            data= f.read()
        # Genera tracks
        for n in range(self.N):
            d= self.data[n]
            ns= self.num_sectors[n]
            if d.point>=0xa0: continue
            # Obté grandària
            rsize,wsize= self.TRACK_SIZES[d.track_mode]
            offset= d.mdf_offset
            # Bota el primer índex, sol ser el gap
            num_sectors= ns.num_sectors
            if num_sectors[0]==150 and d.point==1:
                num_sectors= ns.num_sectors[1:]
                #offset+= 150*rsize !! Aparentment no està desat açò en el mdf
                assert len(self.sessions)==1
                assert self.sessions[0].start_sector==-150
            num_sectors= sum(num_sectors)
            # Crea fitxer i escriu
            tmp= '%s_%d.bin'%(pref,d.point)
            print('[II] Creant %s ...'%tmp)
            with open(tmp,'wb') as f:
                for n in range(num_sectors):
                    sec= data[offset:offset+rsize]
                    offset+= rsize
                    f.write(sec[:wsize])
    # end write_bin

    def write_lsd(self,pref):
        def get_q(subsec):
            ret= []
            cb,c= 0,0
            for v in subsec:
                cb<<= 1
                if (int(v)&0x40)!=0: cb|= 0x1
                c+= 1
                if c==8:
                    ret.append(cb)
                    cb,c= 0,0
            return bytes(ret)
        def bcd(num):
            val= (num//10)*0x10
            val+= num%10
            return val
        def get_mm_ss_ff(num):
            mm= num//(60*75)
            tmp= num%(60*75)
            ss= tmp//75
            ff= tmp%75
            #return mm,ss,ff
            return bytes((bcd(mm),bcd(ss),bcd(ff)))
        def decode_q(q):
            return ' '.join([hex(int(x)) for x in q])
        # Obri el mdf.
        mdf= re.sub(r'[.]mds$','.mdf',self.fn)
        with open(mdf,'rb') as f:
            data= f.read()
        # Genera lsd
        for n in range(self.N):
            d= self.data[n]
            ns= self.num_sectors[n]
            if d.point>=0xa0: continue
            # Obté grandària
            if d.track_mode!=0xec: continue
            rsize,wsize= self.TRACK_SIZES[d.track_mode]
            offset= d.mdf_offset
            # Bota el primer índex, sol ser el gap
            num_sectors= ns.num_sectors
            if num_sectors[0]==150:
                num_sectors= ns.num_sectors[1:]
                #offset+= 150*rsize !! Aparentment no està desat açò en el mdf
                assert len(self.sessions)==1
            assert self.sessions[0].start_sector==-150
            num_sec_off= -self.sessions[0].start_sector
            num_sectors= sum(num_sectors)
            # Crea fitxer i escriu
            tmp= '%s_%d.lsd'%(pref,d.point)
            print('[II] Creant %s ...'%tmp)
            with open(tmp,'wb') as f:
                for n in range(num_sectors):
                    mmssff= get_mm_ss_ff(n+num_sec_off)
                    sec= data[offset:offset+rsize]
                    subsec= sec[wsize:]
                    q= get_q(subsec)
                    mm= int(mmssff[0])
                    if not check_crc(q):
                        f.write(mmssff)
                        f.write(q)
                        print(decode_q(mmssff),decode_q(q))
                    offset+= rsize
    # end write_lsd
    
# end MDS

if len(sys.argv)!=3:
    sys.exit('%s <mds> <out_pref>'%sys.argv[0])
mds_fn= sys.argv[1]
pref= sys.argv[2]

disc= MDS(mds_fn)
print(disc)
disc.write_bin(pref)
disc.write_lsd(pref)
