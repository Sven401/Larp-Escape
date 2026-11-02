"""Generate the native KiCad schematic from reviewed circuit.json.

All component pins, including unused DevKit pins, are explicit. Each local
wire carries its electrical net label; unused pins receive explicit NC markers.
The generated native source is editable in KiCad and must pass KiCad ERC.
"""

import json
from pathlib import Path
import uuid

BASE = Path(__file__).resolve().parents[1]
NAMESPACE = uuid.UUID("d582f673-9ab4-4e2e-a4d1-897ac1830bc4")


def uid(name):
    return str(uuid.uuid5(NAMESPACE, name))


def q(text):
    return json.dumps(str(text), ensure_ascii=False)


def effects(size=1.27, extra=""):
    return f"(effects (font (size {size} {size})) {extra})"


def main():
    circuit = json.loads((BASE / "circuit.json").read_text(encoding="utf-8"))
    parts = circuit["parts"]
    root_id = uid("root")
    libs = []
    instances = []
    wires = []
    texts = []
    # Schematic sheet is deliberately spread out. Connectors are on the left;
    # functional blocks follow from input, enable and buffer to output.
    positions = {p['ref']:(60+(i%10)*110,100+(i//10)*110) for i,p in enumerate(parts)}
    for part in parts:
        ref = part["ref"]
        name = "NearGlowR2a:" + ref
        count = part["pin_count"]
        half = (count + 1) // 2
        h = max(5.08, (half + 1) * 2.54)
        pin_defs = []
        x, y = (round(v/1.27)*1.27 for v in positions[ref])
        for n in range(1, count + 1):
            left = n <= half
            row = n - 1 if left else n - half - 1
            px = -10.16 if left else 10.16
            py = (half - 1) * 1.27 - row * 2.54
            angle = 0 if left else 180
            number = part.get("pin_numbers",list(map(str,range(1,count+1))))[n-1]
            net = part["pins"].get(number)
            pin_type = part.get("types", {}).get(number, "passive")
            pin_name = part.get('pin_names',{}).get(number,number)
            pin_defs.append(f'(pin {pin_type} line (at {px} {py} {angle}) (length 2.54) (name {q(pin_name)} {effects()}) (number {q(number)} {effects()}))')
            # Native symbol Y is upward; schematic sheet Y is downward.
            wx, wy = x + px, y - py
            if net:
                ex = wx + (-5.08 if left else 5.08)
                wires.append(f'(wire (pts (xy {wx} {wy}) (xy {ex} {wy})) (stroke (width 0) (type default)) (uuid {uid(ref+"wire"+str(n))}))')
                angle_label = 0 if left else 180
                justify = "(justify left bottom)" if left else "(justify right bottom)"
                wires.append(f'(label {q(net)} (at {ex} {wy} {angle_label}) {effects(1.0, justify)} (uuid {uid(ref+"label"+str(n))}))')
            else:
                wires.append(f'(no_connect (at {wx} {wy}) (uuid {uid(ref+"nc"+str(n))}))')
        libs.append(f'''(symbol {q(name)}
          (pin_names (offset 0.508)) (in_bom {"yes" if part.get("in_bom",True) else "no"}) (on_board yes)
          (property "Reference" "{ref[0]}" (at 0 {h/2+2.54} 0) {effects()})
          (property "Value" {q(part['value'])} (at 0 {-h/2-2.54} 0) {effects()})
          (symbol {q(ref+'_0_1')} (rectangle (start -7.62 {h/2}) (end 7.62 {-h/2}) (stroke (width 0.254) (type default)) (fill (type background))))
          (symbol {q(ref+'_1_1')} {' '.join(pin_defs)})
        )''')
        symbol_id = uid(ref)
        instances.append(f'''(symbol (lib_id {q(name)}) (at {x} {y} 0) (unit 1)
          (in_bom {"yes" if part.get("in_bom",True) else "no"}) (on_board yes) (dnp {"yes" if part.get("dnp") else "no"}) (uuid {symbol_id})
          (property "Reference" {q(ref)} (at {x} {y-h/2-2.54} 0) {effects()})
          (property "Value" {q(part['value'])} (at {x} {y+h/2+2.54} 0) {effects(1.0)})
          (property "Footprint" {q(part['footprint'])} (at {x} {y} 0) {effects(1.0,'hide')})
          (property "MPN" {q(part['mpn'])} (at {x} {y} 0) {effects(1.0,'hide')})
          (instances (project "naehe-glut-r2a" (path "/{root_id}" (reference {q(ref)}) (unit 1))))
        )''')
    # Explicit supply-source flags keep the supply contract in ERC.
    libs.append('''(symbol "NearGlowR2a:PWR_FLAG" (power) (pin_names (offset 0)) (in_bom no) (on_board no)
       (property "Reference" "#FLG" (at 0 2.54 0) (effects (font (size 1.27 1.27)) hide))
       (property "Value" "PWR_FLAG" (at 0 5.08 0) (effects (font (size 1.27 1.27))))
       (symbol "PWR_FLAG_0_1" (polyline (pts (xy 0 0) (xy 0 1.27) (xy -1.27 1.27) (xy 0 2.54) (xy 1.27 1.27) (xy 0 1.27)) (stroke (width 0) (type default)) (fill (type none))))
       (symbol "PWR_FLAG_1_1" (pin power_out line (at 0 0 90) (length 0) (name "pwr" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))))''')
    for i, net in enumerate(circuit["power_flags"]):
        x,y=round((40+i*40)/1.27)*1.27,round(245/1.27)*1.27
        ref=f"#FLG0{i+1}"
        instances.append(f'''(symbol (lib_id "NearGlowR2a:PWR_FLAG") (at {x} {y} 0) (unit 1) (in_bom no) (on_board no) (dnp no) (uuid {uid(ref)})
          (property "Reference" {q(ref)} (at {x} {y} 0) {effects(1.0,'hide')})
          (property "Value" "PWR_FLAG" (at {x} {y-5.08} 0) {effects()})
          (instances (project "naehe-glut-r2a" (path "/{root_id}" (reference {q(ref)}) (unit 1)))))''')
        wires.append(f'(label {q(net)} (at {x} {y} 0) {effects(1.0,"(justify left bottom)")} (uuid {uid(ref+"label")}))')
    text_notes = [(25,15,"NAEHE GLUT R2a | ELECTRICAL DRAFT - NOT RELEASED FOR MANUFACTURING"),
                  (25,25,"Independent MCU buck-boost; current-limited LED boost; UVLO; no USB/charger onboard."),
                  (25,35,"Battery pack/charger qualification and PWM transient survival remain mandatory order gates.")]
    for i,(x,y,t) in enumerate(text_notes):
        texts.append(f'(text {q(t)} (at {x} {y} 0) {effects(1.27,"(justify left)")} (uuid {uid("note"+str(i))}))')
    schematic = f'''(kicad_sch (version 20250114) (generator "eeschema") (uuid {root_id})
      (paper "A0") (title_block (title "Naehe Glut ESP32-S3 R2a") (date "2026-11-01") (rev "R2a"))
      (lib_symbols {' '.join(libs)})
      {' '.join(wires)} {' '.join(texts)} {' '.join(instances)}
      (sheet_instances (path "/" (page "1")))
    )'''
    (BASE / "naehe-glut-r2a.kicad_sch").write_text(schematic, encoding="utf-8")
    external=' '.join(libs).replace('(symbol "NearGlowR2a:', '(symbol "')
    (BASE/'NearGlowR2a.kicad_sym').write_text('(kicad_symbol_lib (version 20241209) (generator "kicad_symbol_editor") '+external+')',encoding='utf-8')
    (BASE/'sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "NearGlowR2a") (type "KiCad") (uri "${KIPRJMOD}/NearGlowR2a.kicad_sym") (options "") (descr "Project-local carrier symbols")))\n',encoding='utf-8')
    libraries=sorted({p['footprint'].split(':')[0] for p in parts})
    entries=[]
    for name in libraries:
        uri='${KIPRJMOD}/NearGlowR2a.pretty' if name=='NearGlowR2a' else '${KICAD10_FOOTPRINT_DIR}/'+name+'.pretty'
        entries.append(f'(lib (name "{name}") (type "KiCad") (uri "{uri}") (options "") (descr "KiCad community library or reviewed local geometry"))')
    if not (BASE/'fp-lib-table').exists():
        (BASE/'fp-lib-table').write_text('(fp_lib_table (version 7) '+''.join(entries)+')\n',encoding='utf-8')
    if not (BASE/'naehe-glut-r2a.kicad_pro').exists():
        (BASE/'naehe-glut-r2a.kicad_pro').write_text(json.dumps({'meta':{'filename':'naehe-glut-r2a.kicad_pro','version':1}}),encoding='utf-8')



if __name__ == "__main__":
    main()
