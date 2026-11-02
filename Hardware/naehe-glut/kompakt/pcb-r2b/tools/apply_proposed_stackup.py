"""Apply reviewed, explicitly unqualified stackup metadata; never route copper."""
from pathlib import Path
import re
from cad_environment import configure
configure()
import pcbnew
BASE=Path(__file__).resolve().parents[1]
TEMPLATE=BASE/'procurement/usb-stackup/stackup-trial/proposed-3313A-roundtrip.kicad_pcb'
def block(text, token):
 start=text.index('('+token);depth=0;quoted=False;escaped=False
 for i in range(start,len(text)):
  c=text[i]
  if escaped:escaped=False;continue
  if c=='\\' and quoted:escaped=True;continue
  if c=='"':quoted=not quoted
  if quoted:continue
  if c=='(':depth+=1
  elif c==')':
   depth-=1
   if not depth:return text[start:i+1]
 raise ValueError('Unclosed '+token)
def apply(path):
 path=Path(path);s=path.read_text(encoding='utf-8-sig');template=TEMPLATE.read_text(encoding='utf-8-sig');new=block(template,'stackup')
 if '(stackup' in s:s=s.replace(block(s,'stackup'),new,1)
 else:s=s.replace('(setup','(setup\n'+new,1)
 path.write_text(s,encoding='utf8');b=pcbnew.LoadBoard(str(path));t=pcbnew.LoadBoard(str(TEMPLATE));b.SetTitleBlock(t.GetTitleBlock());pcbnew.SaveBoard(str(path),b)
if __name__=='__main__':
 import argparse
 p=argparse.ArgumentParser();p.add_argument('pcb',type=Path);apply(p.parse_args().pcb)
