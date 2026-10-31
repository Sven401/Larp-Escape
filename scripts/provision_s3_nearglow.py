"""Private USB setup for the autonomous S3 proximity laboratory.

No flash or erase operations. Pairing stays disarmed across a deliberate reboot;
arming is a separate operator action. Windows inherits the private directory ACL.
PySerial is imported only when an actual serial connection is requested.
"""
import argparse
from dataclasses import dataclass
from functools import wraps
import json
import os
from pathlib import Path
import re
import secrets
import sys
import tempfile
import time

ROOT = Path(__file__).resolve().parents[1]
MAX_COMMAND = 111
MAX_RESPONSE = 1024
INFO_FIELDS = {
    "type", "protocol", "mac", "peer", "paired", "armed", "persistedArmed",
    "durableKnown", "fresh", "ageMs", "filteredRssi", "level", "fault", "radio",
    "txIdle", "outputKnown", "outputPending", "black", "rgb", "restartRequired",
}
BOOL_FIELDS = INFO_FIELDS - {
    "type", "protocol", "mac", "peer", "ageMs", "filteredRssi", "level", "fault", "rgb",
}


class SetupError(Exception):
    """Only fixed, non-sensitive messages may cross the public API boundary."""


class ConnectionProblem(SetupError):
    pass


def safe_operation(function):
    @wraps(function)
    def wrapped(*args, **kwargs):
        try:
            return function(*args, **kwargs)
        except SetupError:
            raise
        except Exception:
            raise SetupError("Setup failed; no automatic pairing retry was performed.") from None
    return wrapped


def _integer(value, lower, upper):
    return type(value) is int and lower <= value <= upper


def validate_mac(value, allow_zero=False):
    if not isinstance(value, str) or not re.fullmatch(r"(?:[0-9a-fA-F]{2}:){5}[0-9a-fA-F]{2}", value):
        raise SetupError("Invalid station MAC.")
    result = value.upper()
    raw = bytes.fromhex(result.replace(":", ""))
    if (not any(raw) and not allow_zero) or raw[0] & 1:
        raise SetupError("Invalid station MAC.")
    return result


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise SetupError("Duplicate JSON field.")
        result[key] = value
    return result


def _reject_constant(unused):
    raise ValueError


def _decode_json(data):
    try:
        value = json.loads(data, object_pairs_hook=_unique_object, parse_constant=_reject_constant)
    except Exception:
        raise SetupError("Invalid JSON response or setup file.") from None
    if type(value) is not dict:
        raise SetupError("Invalid JSON response or setup file.")
    return value


def validate_info(value):
    if set(value) != INFO_FIELDS or value["type"] != "info" or type(value["protocol"]) is not int or value["protocol"] != 1:
        raise SetupError("Unsupported station INFO schema.")
    if any(type(value[field]) is not bool for field in BOOL_FIELDS):
        raise SetupError("Invalid station status types.")
    value = dict(value)
    value["mac"] = validate_mac(value["mac"])
    value["peer"] = validate_mac(value["peer"], allow_zero=not value["paired"])
    if not _integer(value["level"], 0, 1000) or not _integer(value["fault"], 0, 0xffffffff):
        raise SetupError("Invalid station status values.")
    age, rssi = value["ageMs"], value["filteredRssi"]
    if age is not None and not _integer(age, 0, 0xffffffff):
        raise SetupError("Invalid station measurement.")
    if rssi is not None and not _integer(rssi, -127, -1):
        raise SetupError("Invalid station measurement.")
    if value["fresh"] and (age is None or rssi is None):
        raise SetupError("Missing fresh station measurement.")
    rgb = value["rgb"]
    if type(rgb) is not list or len(rgb) != 3 or any(not _integer(v, 0, 32) for v in rgb):
        raise SetupError("Invalid station output.")
    if value["black"] and rgb != [0, 0, 0]:
        raise SetupError("Inconsistent station output.")
    return value


def _validate_ack(value, operation):
    fields = {"type", "op", "armed"}
    if operation != "REBOOT":
        fields.add("durable")
    if operation == "PAIR":
        fields.add("restartRequired")
    if set(value) != fields or value["type"] != "ack" or value["op"] != operation:
        raise SetupError("Unexpected station acknowledgement.")
    if type(value["armed"]) is not bool or value["armed"] != (operation == "ARM"):
        raise SetupError("Station did not confirm the requested arm state.")
    if operation != "REBOOT" and value["durable"] is not True:
        raise SetupError("Station did not confirm durable storage.")
    if operation == "PAIR" and value["restartRequired"] is not True:
        raise SetupError("Station did not confirm the pairing restart.")
    return value


def _serial_factory(**kwargs):
    try:
        import serial
        return serial.Serial(**kwargs)
    except Exception:
        raise ConnectionProblem("USB serial connection unavailable; check port and PySerial.") from None


class Station:
    def __init__(self, port, expected_mac, factory=None, clock=None):
        self.port, self.mac = port, validate_mac(expected_mac)
        self.factory = factory or _serial_factory
        self.clock = clock or time.monotonic
        self.connection = None

    def open(self):
        connection = None
        try:
            connection = self.factory(port=None, baudrate=115200, timeout=0.25, write_timeout=2)
            # Set native USB control lines before opening the actual device.
            connection.dtr = True
            connection.rts = False
            connection.port = self.port
            connection.open()
            self.connection = connection
        except Exception:
            try:
                if connection is not None:
                    connection.close()
            except Exception:
                pass
            raise ConnectionProblem("USB serial connection unavailable.") from None
        return self

    def close(self):
        if self.connection is not None:
            try:
                self.connection.close()
            except Exception:
                pass
            self.connection = None

    def exchange(self, command, operation=None, timeout=3, boot_readiness=False, response_parser=None):
        allowed = command in ("INFO", "ARM", "STOP", "REBOOT", "BLE-ACCESS", "BLE-INFO") or re.fullmatch(
            r"PAIR (?:[0-9A-F]{2}:){5}[0-9A-F]{2} [0-9a-f]{64}", command)
        if not allowed or len(command.encode("ascii")) > MAX_COMMAND:
            raise SetupError("Unsupported USB command.")
        data = command.encode("ascii") + b"\n"
        try:
            self.connection.reset_input_buffer()
            if self.connection.write(data) != len(data):
                raise ConnectionProblem("USB command was not completely sent.")
            self.connection.flush()
            until = self.clock() + timeout
            for unused in range(64):
                if self.clock() >= until:
                    break
                line = self.connection.readline(MAX_RESPONSE + 2)
                if not line:
                    continue
                if len(line) > MAX_RESPONSE + 1 or not line.endswith(b"\n"):
                    raise SetupError("Oversized or incomplete station response.")
                # Only the explicit post-REBOOT INFO phase may encounter ROM/SDK
                # text. Keep the same byte/line/deadline limits and never log it.
                # JSON-looking responses still undergo the full strict checks.
                if boot_readiness and command == "INFO" and not line.lstrip().startswith((b"{", b"[")):
                    continue
                value = _decode_json(line)
                if value == {"type": "ready", "protocol": 1} and type(value["protocol"]) is int:
                    continue
                if value.get("type") == "error":
                    raise SetupError("Station rejected the USB command.")
                if operation:
                    return _validate_ack(value, operation)
                return (response_parser or validate_info)(value)
            raise ConnectionProblem("Station response timed out.")
        except SetupError:
            raise
        except Exception:
            raise ConnectionProblem("USB exchange failed; response contents were not logged.") from None

    def info(self, timeout=3, boot_readiness=False):
        value = self.exchange("INFO", timeout=timeout, boot_readiness=boot_readiness)
        if value["mac"] != self.mac:
            raise SetupError("Connected station does not match the expected MAC.")
        return value


def _endpoints(port_a, mac_a, port_b, mac_b, factory, clock):
    mac_a, mac_b = validate_mac(mac_a), validate_mac(mac_b)
    if not isinstance(port_a, str) or not isinstance(port_b, str) or not port_a.strip() or not port_b.strip():
        raise SetupError("Two explicit ports are required.")
    if port_a.strip().casefold() == port_b.strip().casefold() or mac_a == mac_b:
        raise SetupError("Ports and station MACs must be different.")
    return (Station(port_a, mac_a, factory, clock), Station(port_b, mac_b, factory, clock))


def _black(info):
    return info["outputKnown"] and not info["outputPending"] and info["black"] and info["rgb"] == [0, 0, 0]


def _disarmed(info):
    return info["durableKnown"] and not info["armed"] and not info["persistedArmed"]


def _preflight(infos, peers=None):
    for index, info in enumerate(infos):
        if info["fault"] != 0 or not _disarmed(info) or not _black(info) or not info["txIdle"] or info["restartRequired"]:
            raise SetupError("Both stations must be durably disarmed, idle and confirmed black.")
        if peers is not None and (not info["paired"] or info["peer"] != peers[index] or not info["radio"]):
            raise SetupError("Stored pairing or radio state does not match the intended pair.")


def _private_destination(path):
    raw = Path(path).expanduser()
    destination = raw.resolve()
    if destination.is_relative_to(ROOT) or raw.is_symlink() or destination.exists() or not destination.parent.is_dir():
        raise SetupError("Use a new private file in an existing directory outside this repository.")
    return destination


def _write_prepared(path, record):
    encoded = (json.dumps(record, separators=(",", ":")) + "\n").encode("utf-8")
    fd = os.open(path, os.O_WRONLY | os.O_CREAT | os.O_EXCL, 0o600)
    with os.fdopen(fd, "wb") as stream:
        stream.write(encoded)
        stream.flush()
        os.fsync(stream.fileno())
    return encoded


def _mark_paired(path, record, original):
    if path.is_symlink() or path.read_bytes() != original:
        raise SetupError("Private pair file changed; pairing phase was not overwritten.")
    record = dict(record, phase="paired")
    fd, temporary = tempfile.mkstemp(prefix=".s3-pair-", dir=path.parent)
    try:
        with os.fdopen(fd, "w", encoding="utf-8") as stream:
            json.dump(record, stream, separators=(",", ":"))
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if os.path.exists(temporary):
            os.unlink(temporary)


def load_pair(path, require_paired=False):
    try:
        with Path(path).expanduser().open("rb") as stream:
            data = stream.read(4097)
        if len(data) > 4096:
            raise SetupError("Private pair file is too large.")
        value = _decode_json(data)
        if set(value) != {"schemaVersion", "protocol", "macA", "macB", "secret", "phase"}:
            raise SetupError("Unsupported private pair file.")
        if type(value["schemaVersion"]) is not int or value["schemaVersion"] != 1 or type(value["protocol"]) is not int or value["protocol"] != 1:
            raise SetupError("Unsupported private pair file.")
        value["macA"], value["macB"] = validate_mac(value["macA"]), validate_mac(value["macB"])
        if value["macA"] == value["macB"] or value["phase"] not in ("prepared", "paired"):
            raise SetupError("Invalid private pair state.")
        if not isinstance(value["secret"], str) or not re.fullmatch(r"[0-9a-f]{64}", value["secret"]) or not any(bytes.fromhex(value["secret"])):
            raise SetupError("Invalid private pair key.")
        if require_paired and value["phase"] != "paired":
            raise SetupError("Arming requires a completed paired setup file.")
        return value
    except SetupError:
        raise
    except Exception:
        raise SetupError("Private pair file could not be read.") from None


@safe_operation
def station_info(port, mac, factory=None, clock=None):
    station = Station(port, mac, factory, clock)
    try:
        station.open()
        return station.info()
    finally:
        station.close()


@safe_operation
def pair_stations(port_a, mac_a, port_b, mac_b, output, factory=None, clock=None, sleep=None):
    clock, sleep = clock or time.monotonic, sleep or time.sleep
    destination = _private_destination(output)
    stations = _endpoints(port_a, mac_a, port_b, mac_b, factory, clock)
    try:
        for station in stations:
            station.open()
        _preflight([s.info() for s in stations])
        key = secrets.token_bytes(32)
        if len(key) != 32 or not any(key):
            raise SetupError("Pair key generation failed.")
        record = {"schemaVersion": 1, "protocol": 1, "macA": stations[0].mac,
                  "macB": stations[1].mac, "secret": key.hex(), "phase": "prepared"}
        prepared = _write_prepared(destination, record)
        for station, peer in zip(stations, reversed(stations)):
            station.exchange("PAIR " + peer.mac + " " + key.hex(), "PAIR")
        for station in stations:
            station.exchange("REBOOT", "REBOOT")
            station.close()
        deadline = clock() + 30
        for station in stations:
            while clock() < deadline:
                try:
                    station.open()
                    info = station.info(timeout=min(3, max(0.01, deadline - clock())), boot_readiness=True)
                    peer = stations[1] if station is stations[0] else stations[0]
                    if not info["paired"] or info["peer"] != peer.mac or info["fault"] or not _disarmed(info):
                        raise SetupError("Pairing after restart did not match the intended disarmed pair.")
                    if info["radio"] and info["txIdle"] and not info["restartRequired"] and _black(info):
                        break
                    station.close()
                    sleep(min(0.25, max(0, deadline - clock())))
                except ConnectionProblem:
                    station.close()
                    sleep(min(0.25, max(0, deadline - clock())))
            else:
                raise SetupError("Pair restart was not confirmed; private setup remains prepared.")
        _mark_paired(destination, record, prepared)
        return {"paired": True, "armed": False}
    finally:
        for station in stations:
            station.close()


@dataclass(frozen=True)
class StopReport:
    durable_a: bool
    durable_b: bool
    black_a: bool
    black_b: bool

    @property
    def complete(self):
        return self.durable_a and self.durable_b and self.black_a and self.black_b


def _stop_clients(stations, clock, sleep):
    durable = [False, False]
    black = [False, False]
    for index, station in enumerate(stations):
        try:
            station.exchange("STOP", "STOP")
            durable[index] = True
        except SetupError:
            pass
    deadline = clock() + 10
    for unused in range(64):
        for index, station in enumerate(stations):
            if black[index]:
                continue
            try:
                info = station.info()
                durable[index] = durable[index] and _disarmed(info)
                if _black(info):
                    black[index] = True
            except SetupError:
                pass
        if all(black) or clock() >= deadline:
            break
        sleep(0.1)
    return StopReport(durable[0], durable[1], black[0], black[1])


@safe_operation
def arm_stations(port_a, port_b, pair_file, factory=None, clock=None, sleep=None):
    clock, sleep = clock or time.monotonic, sleep or time.sleep
    pair = load_pair(pair_file, require_paired=True)
    stations = _endpoints(port_a, pair["macA"], port_b, pair["macB"], factory, clock)
    try:
        for station in stations:
            station.open()
        _preflight([s.info() for s in stations], [pair["macB"], pair["macA"]])
        try:
            for station in stations:
                station.exchange("ARM", "ARM")
            for station, peer in zip(stations, reversed(stations)):
                info = station.info()
                if not info["armed"] or not info["persistedArmed"] or not info["durableKnown"] or info["fault"] or info["restartRequired"] or not info["radio"] or not info["paired"] or info["peer"] != peer.mac:
                    raise SetupError("Armed state was not confirmed.")
        except SetupError:
            stopped = _stop_clients(stations, clock, sleep)
            if stopped.complete:
                raise SetupError("Arming failed; both stations were durably stopped and confirmed black.") from None
            raise SetupError("Arming failed; stop or black output could not be confirmed on both stations.") from None
        return {"armed": True, "durable": True}
    finally:
        for station in stations:
            station.close()


@safe_operation
def stop_stations(port_a, port_b, pair_file, factory=None, clock=None, sleep=None):
    clock, sleep = clock or time.monotonic, sleep or time.sleep
    pair = load_pair(pair_file)
    stations = _endpoints(port_a, pair["macA"], port_b, pair["macB"], factory, clock)
    try:
        for station in stations:
            station.open()
        for station in stations:
            station.info()  # identify both before the first mutation
        return _stop_clients(stations, clock, sleep)
    finally:
        for station in stations:
            station.close()



BLE_ACCESS_FIELDS = {"schemaVersion", "protocol", "deviceId", "secret", "name", "stationMac"}
BLE_INFO_FIELDS = {"type", "protocol", "ready", "fault", "curveRevision", "appliedRevision", "curveDurableKnown", "points"}


def validate_ble_access(value):
    if set(value) != BLE_ACCESS_FIELDS or type(value["schemaVersion"]) is not int or value["schemaVersion"] != 1 or value["protocol"] != "LARP-PROX-BLE/1":
        raise SetupError("Unsupported BLE access schema.")
    for field, length in (("deviceId", 32), ("secret", 64)):
        if type(value[field]) is not str or not re.fullmatch("[0-9a-f]{" + str(length) + "}", value[field]) or not any(bytes.fromhex(value[field])):
            raise SetupError("Invalid BLE access identity or key.")
    if len(set(bytes.fromhex(value["secret"]))) == 1:
        raise SetupError("Invalid BLE access key.")
    if type(value["name"]) is not str or not 1 <= len(value["name"]) <= 64 or any(ord(c) < 32 for c in value["name"]):
        raise SetupError("Invalid BLE access name.")
    result = dict(value)
    result["stationMac"] = validate_mac(value["stationMac"])
    return result


def validate_ble_info(value):
    if set(value) != BLE_INFO_FIELDS or value["type"] != "ble-info" or type(value["protocol"]) is not int or value["protocol"] != 1:
        raise SetupError("Unsupported BLE status schema.")
    if any(type(value[f]) is not bool for f in ("ready", "curveDurableKnown")) or not _integer(value["fault"], 0, 65535):
        raise SetupError("Invalid BLE status flags.")
    if not _integer(value["curveRevision"], 1, 0xffffffff) or not _integer(value["appliedRevision"], 0, 0xffffffff):
        raise SetupError("Invalid BLE curve revision.")
    points = value["points"]
    if type(points) is not list or len(points) != 4:
        raise SetupError("Invalid BLE curve points.")
    for i, point in enumerate(points):
        if type(point) is not list or len(point) != 2 or not _integer(point[0], -127, -1) or not _integer(point[1], 0, 100):
            raise SetupError("Invalid BLE curve points.")
        if i and (point[0] <= points[i-1][0] or point[1] < points[i-1][1]):
            raise SetupError("Invalid BLE curve order.")
    return dict(value)


def _ble_destination(path):
    destination = _private_destination(path)
    for ancestor in (destination.parent, *destination.parent.parents):
        if (ancestor / ".git").exists() or (ancestor / ".git").is_symlink():
            raise SetupError("BLE access files must be outside Git repositories.")
    return destination


@safe_operation
def export_ble_access(port, mac, output, factory=None, clock=None, sleep=None):
    destination = _ble_destination(output)
    station = Station(port, validate_mac(mac), factory, clock or time.monotonic)
    try:
        station.open()
        before = station.info()
        if before["fault"] or not before["paired"] or not before["radio"] or not before["durableKnown"] or before["restartRequired"]:
            raise SetupError("Station is not ready for explicit BLE setup.")
        access = station.exchange("BLE-ACCESS", response_parser=validate_ble_access)
        after = station.info()
        context = ("mac", "peer", "paired", "armed", "persistedArmed", "durableKnown", "radio", "restartRequired", "fault")
        if access["stationMac"] != station.mac or any(before[f] != after[f] for f in context):
            raise SetupError("Station identity or autonomous authority changed during BLE setup.")
        _write_prepared(destination, access)
        return {"exported": True, "stationMac": station.mac}
    finally:
        station.close()


@safe_operation
def station_ble_info(port, mac, factory=None, clock=None):
    station = Station(port, validate_mac(mac), factory, clock or time.monotonic)
    try:
        station.open()
        station.info()
        result = station.exchange("BLE-INFO", response_parser=validate_ble_info)
        station.info()
        return result
    finally:
        station.close()

class ArgumentParser(argparse.ArgumentParser):
    def error(self, message):
        self.exit(2, "Invalid command line; use --help.\n")


def main(argv=None):
    parser = ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    info = commands.add_parser("info", help="Read a known station's public status")
    info.add_argument("--port", required=True)
    info.add_argument("--mac", required=True)
    ble_info = commands.add_parser("ble-info", help="Read a known station's public BLE/curve status")
    ble_info.add_argument("--port", required=True)
    ble_info.add_argument("--mac", required=True)
    access = commands.add_parser("export-ble-access", help="Explicit private access export; does not arm or stop")
    access.add_argument("--port", required=True)
    access.add_argument("--mac", required=True)
    access.add_argument("--output", required=True, type=Path)
    pair = commands.add_parser("pair", help="Pair two known stations, leaving both disarmed")
    pair.add_argument("--port-a", required=True)
    pair.add_argument("--mac-a", required=True)
    pair.add_argument("--port-b", required=True)
    pair.add_argument("--mac-b", required=True)
    pair.add_argument("--output", required=True, type=Path)
    for name in ("arm", "stop"):
        command = commands.add_parser(name)
        command.add_argument("--port-a", required=True)
        command.add_argument("--port-b", required=True)
        command.add_argument("--pair-file", required=True, type=Path)
    args = parser.parse_args(argv)
    try:
        if args.command == "info":
            result = station_info(args.port, args.mac)
        elif args.command == "ble-info":
            result = station_ble_info(args.port, args.mac)
        elif args.command == "export-ble-access":
            result = export_ble_access(args.port, args.mac, args.output)
        elif args.command == "pair":
            result = pair_stations(args.port_a, args.mac_a, args.port_b, args.mac_b, args.output)
        elif args.command == "arm":
            result = arm_stations(args.port_a, args.port_b, args.pair_file)
        else:
            report = stop_stations(args.port_a, args.port_b, args.pair_file)
            result = {"stopped": report.complete, "durableA": report.durable_a,
                      "durableB": report.durable_b, "blackA": report.black_a, "blackB": report.black_b}
            print(json.dumps(result))
            return 0 if report.complete else 1
        print(json.dumps(result))
        return 0
    except Exception:
        print("USB setup failed. Check identities, private setup phase and both station states; no automatic re-pairing was attempted.", file=sys.stderr)
        return 1
    except KeyboardInterrupt:
        print("USB setup interrupted; inspect both stations before continuing.", file=sys.stderr)
        return 130


if __name__ == "__main__":
    raise SystemExit(main())
