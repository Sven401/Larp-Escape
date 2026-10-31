"""Read exported Gerber/Excellon independently with gerbonara 1.6.3."""
import json
from pathlib import Path
from gerbonara import LayerStack

base = Path(__file__).resolve().parents[1] / 'pcb'
stack = LayerStack.open(base / 'exports/gerber')
out = base / 'pruefung'
for side in ('top', 'bottom'):
    (out / f'cam-{side}.svg').write_text(str(stack.to_pretty_svg(side=side, margin=2)), encoding='utf-8')
report = {'reader': 'gerbonara 1.6.3', 'board_bounds_mm': stack.board_bounds(),
          'graphic_layers': sorted('/'.join(k) for k in stack.graphic_layers),
          'pth_objects': len(stack.drill_pth.objects), 'npth_objects': len(stack.drill_npth.objects)}
assert report['pth_objects'] == 99 and report['npth_objects'] == 4, report
(out / 'cam-readback.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
print(json.dumps(report))
