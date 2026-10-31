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
    circuit = json.loads((BASE / "pcb/circuit.json").read_text(encoding="utf-8"))
    parts = circuit["parts"]
    root_id = uid("root")
    libs = []
    instances = []
    wires = []
    texts = []
    # Schematic sheet is deliberately spread out. Connectors are on the left;
    # functional blocks follow from input, enable and buffer to output.
    positions = {
        "J1": (35,60), "J3": (85,60), "J2": (145,35), "F1": (210,35),
        "C1": (275,35), "C2": (335,35), "U1": (210,100),
        "R1": (145,150), "R2": (145,175), "R3": (275,100),
        "R4": (335,100), "J4": (335,150), "R5": (145,220),
        "R6": (210,220), "Q1": (275,220), "R7": (335,220),
        "R8": (275,175), "R9": (335,175),
    }
    for part in parts:
        ref = part["ref"]
        name = "NearGlow:" + ref
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
            pin_name = number
            if ref == "U1":
                pin_name = {1:"1OE_N",2:"1A",3:"1Y",4:"2OE_N",5:"2A",6:"2Y",7:"GND",8:"3Y",9:"3A",10:"3OE_N",11:"4Y",12:"4A",13:"4OE_N",14:"VCC"}[n]
            if ref == "Q1":
                pin_name = {1:"E",2:"B",3:"C"}[n]
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
          (pin_names (offset 0.508)) (in_bom yes) (on_board yes)
          (property "Reference" "{ref[0]}" (at 0 {h/2+2.54} 0) {effects()})
          (property "Value" {q(part['value'])} (at 0 {-h/2-2.54} 0) {effects()})
          (symbol {q(ref+'_0_1')} (rectangle (start -7.62 {h/2}) (end 7.62 {-h/2}) (stroke (width 0.254) (type default)) (fill (type background))))
          (symbol {q(ref+'_1_1')} {' '.join(pin_defs)})
        )''')
        symbol_id = uid(ref)
        instances.append(f'''(symbol (lib_id {q(name)}) (at {x} {y} 0) (unit 1)
          (in_bom yes) (on_board yes) (dnp no) (uuid {symbol_id})
          (property "Reference" {q(ref)} (at {x} {y-h/2-2.54} 0) {effects()})
          (property "Value" {q(part['value'])} (at {x} {y+h/2+2.54} 0) {effects(1.0)})
          (property "Footprint" {q(part['footprint'])} (at {x} {y} 0) {effects(1.0,'hide')})
          (instances (project "naehe-glut-r1" (path "/{root_id}" (reference {q(ref)}) (unit 1))))
        )''')
    # Explicit supply-source flags keep the supply contract in ERC.
    libs.append('''(symbol "NearGlow:PWR_FLAG" (power) (pin_names (offset 0)) (in_bom no) (on_board no)
       (property "Reference" "#FLG" (at 0 2.54 0) (effects (font (size 1.27 1.27)) hide))
       (property "Value" "PWR_FLAG" (at 0 5.08 0) (effects (font (size 1.27 1.27))))
       (symbol "PWR_FLAG_0_1" (polyline (pts (xy 0 0) (xy 0 1.27) (xy -1.27 1.27) (xy 0 2.54) (xy 1.27 1.27) (xy 0 1.27)) (stroke (width 0) (type default)) (fill (type none))))
       (symbol "PWR_FLAG_1_1" (pin power_out line (at 0 0 90) (length 0) (name "pwr" (effects (font (size 1.27 1.27)))) (number "1" (effects (font (size 1.27 1.27)))))))''')
    for i, net in enumerate(["LED5V", "GND"]):
        x,y=round((40+i*40)/1.27)*1.27,round(245/1.27)*1.27
        ref=f"#FLG0{i+1}"
        instances.append(f'''(symbol (lib_id "NearGlow:PWR_FLAG") (at {x} {y} 0) (unit 1) (in_bom no) (on_board no) (dnp no) (uuid {uid(ref)})
          (property "Reference" {q(ref)} (at {x} {y} 0) {effects(1.0,'hide')})
          (property "Value" "PWR_FLAG" (at {x} {y-5.08} 0) {effects()})
          (instances (project "naehe-glut-r1" (path "/{root_id}" (reference {q(ref)}) (unit 1)))))''')
        wires.append(f'(label {q(net)} (at {x} {y} 0) {effects(1.0,"(justify left bottom)")} (uuid {uid(ref+"label")}))')
    text_notes = [(25,15,"NAEHE GLUT R1 | ESP32-S3-DevKitC-1-N8R8 | carrier has NO connection to ESP 5V/3V3"),
                  (25,265,"ESP powered by its own USB cable. Carrier USB-C supplies LED5V only. Common GND."),
                  (25,272,"GPIO16 HIGH enables buffer. Boot/input HiZ = output disabled; output pulldown. Stop is a firmware function."),
                  (25,279,"Disconnect BOTH USB power cables for service. Removing ESP power alone does not clear latched LEDs.")]
    for i,(x,y,t) in enumerate(text_notes):
        texts.append(f'(text {q(t)} (at {x} {y} 0) {effects(1.27,"(justify left)")} (uuid {uid("note"+str(i))}))')
    schematic = f'''(kicad_sch (version 20250114) (generator "eeschema") (uuid {root_id})
      (paper "A3") (title_block (title "Naehe Glut ESP32-S3 Carrier") (date "2026-10-31") (rev "R1"))
      (lib_symbols {' '.join(libs)})
      {' '.join(wires)} {' '.join(texts)} {' '.join(instances)}
      (sheet_instances (path "/" (page "1")))
    )'''
    (BASE / "pcb/naehe-glut-r1.kicad_sch").write_text(schematic, encoding="utf-8")
    external=' '.join(libs).replace('(symbol "NearGlow:', '(symbol "')
    (BASE/'pcb/NearGlow.kicad_sym').write_text('(kicad_symbol_lib (version 20241209) (generator "kicad_symbol_editor") '+external+')',encoding='utf-8')
    (BASE/'pcb/sym-lib-table').write_text('(sym_lib_table (version 7) (lib (name "NearGlow") (type "KiCad") (uri "${KIPRJMOD}/NearGlow.kicad_sym") (options "") (descr "Project-local carrier symbols")))\n',encoding='utf-8')
    libraries=sorted({p['footprint'].split(':')[0] for p in parts})
    (BASE/'pcb/fp-lib-table').write_text('(fp_lib_table (version 7) '+''.join(f'(lib (name "{name}") (type "KiCad") (uri "${{KIPRJMOD}}/NearGlow.pretty") (options "") (descr "Vendored KiCad footprints"))' for name in libraries)+')\n',encoding='utf-8')


if __name__ == "__main__":
    main()
