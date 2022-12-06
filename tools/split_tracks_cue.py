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

import sys,os.path,re,pathlib

class Disc:
    def __init__(self):
        self.files= []
    def add(self,f):
        self.files.append(f)
    def __repr__(self):
        return '<Disc files:%s>'%str(self.files)

class File:
    def __init__(self,name):
        self.tracks= []
        self.name= name
    def add(self,t):
        self.tracks.append(t)
    def __repr__(self):
        return '<File name:%s tracks:%s>'%(self.name,
                                           str(self.tracks))
class Track:
    def __init__(self,typ):
        self.type= typ
        self.mm= -1
        self.ss= -1
        self.ff= -1
        self.pregap= 0
    def set_pos(self,txt):
        tmp= txt.split(':')
        self.mm= int(tmp[0])
        self.ss= int(tmp[1])
        self.ff= int(tmp[2])
    def set_pregap(self,txt):
        tmp= txt.split(':')
        mm= int(tmp[0])
        ss= int(tmp[1])
        ff= int(tmp[2])
        self.pregap= mm*60*75 + ss*75 + ff
    def __repr__(self):
        return '<Track %s %d:%d:%d>'%(self.type,self.mm,self.ss,self.ff)
    @property
    def offset(self):
        return (self.mm*60*75 + self.ss*75 + self.ff)*0x930
    
def load_cue(fn):
    dirname= os.path.dirname(fn)
    ret= Disc()
    cfile,ctrack= None,None
    with open(fn) as f:
        for line in f:
            line= line.strip()
            l= line.split()
            if l==[]: continue
            if l[0]=='FILE':
                tmp= re.sub(r'^FILE[ ]+"','',line)
                tmp= re.sub(r'"[ ]+BINARY[ ]*$','',tmp)
                tmp= os.path.join(dirname,tmp)
                cfile= File(tmp)
                ret.add(cfile)
            elif l[0]=='TRACK':
                ctrack= Track(l[2])
                cfile.add(ctrack)
            elif l[0]=='INDEX' and ctrack.mm==-1:
                assert l[1]=='01'
                ctrack.set_pos(l[2])
            elif l[0]=='PREGAP' and ctrack.mm==-1:
                ctrack.set_pregap(l[1])
                
    return ret

def split_tracks(disc,pref):
    N= 0
    for fil in disc.files:
        if len(fil.tracks)==1:
            print("[WW] Ignorant fitxer '%s'"%fil.name,file=sys.stderr)
            continue
        # obté offsets i grandàries
        sizes= []
        size= pathlib.Path(fil.name).stat().st_size
        for i,t in enumerate(fil.tracks[:-1]):
            tmp= fil.tracks[i+1].offset-t.offset
            sizes.append((t.offset,tmp,t.pregap))
        tmp= size-fil.tracks[-1].offset
        sizes.append((fil.tracks[-1].offset,tmp,fil.tracks[-1].pregap))
        assert(sizes[0][0]==0)
        # Crea fitxers.
        with open(fil.name,'rb') as f:
            for off,size,gap in sizes:
                fn= '%s-%03d.bin'%(pref,N)
                print("[II] Creant '%s' ..."%fn,file=sys.stderr)
                with open(fn,'wb') as f2:
                    if gap>0: f2.write(b'\0'*(gap*0x930))
                    data= f.read(size)
                    f2.write(data)
                    N+= 1
        
if len(sys.argv)!=3:
    sys.exit('%s <cue> <out_prefix>'%sys.argv[0])
cue_fn= sys.argv[1]
pref= sys.argv[2]

disc= load_cue(cue_fn)
split_tracks(disc,pref)
