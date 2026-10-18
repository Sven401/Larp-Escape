"""Compile the real game sender, with no copies of its packet implementation."""
from pathlib import Path
Import('env')
env.BuildSources('$BUILD_DIR/production-communicator', str(Path(env['PROJECT_DIR']).parents[1] / 'lib/EspNowCommunicator'))
