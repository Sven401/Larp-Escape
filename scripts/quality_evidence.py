"""Record CI commands without changing their exit status or the tested sources."""
import argparse
import datetime
import json
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import time


def utc_now():
    return datetime.datetime.now(datetime.timezone.utc).isoformat()


def report_directory():
    return Path(os.environ.get("RUNNER_TEMP", tempfile.gettempdir())) / (
        "larp-quality-" + os.environ.get("GITHUB_JOB", "local")
    )


def initialize(directory):
    directory.mkdir(parents=True, exist_ok=True)
    if (directory / "provenance.json").exists():
        raise RuntimeError("Report already exists; use a fresh output directory")
    commit = subprocess.check_output(["git", "rev-parse", "HEAD"], text=True).strip()
    provenance = {
        "recorded_at_utc": utc_now(),
        "checked_out_commit": commit,
        "event_sha": os.environ.get("GITHUB_SHA"),
        "pull_request_head_sha": os.environ.get("QUALITY_HEAD_SHA"),
        "pull_request_base_sha": os.environ.get("QUALITY_BASE_SHA"),
        "event": os.environ.get("GITHUB_EVENT_NAME", "local"),
        "job": os.environ.get("GITHUB_JOB", "local"),
        "runner_image": os.environ.get("ImageOS"),
        "runner_image_version": os.environ.get("ImageVersion"),
        "python": sys.version,
        "run_url": (
            f"{os.environ.get('GITHUB_SERVER_URL', 'https://github.com')}/"
            f"{os.environ.get('GITHUB_REPOSITORY', '')}/actions/runs/"
            f"{os.environ.get('GITHUB_RUN_ID', '')}"
        ) if os.environ.get("GITHUB_RUN_ID") else None,
        "run_attempt": os.environ.get("GITHUB_RUN_ATTEMPT"),
    }
    (directory / "provenance.json").write_text(
        json.dumps(provenance, indent=2) + "\n", encoding="utf-8"
    )
    print(json.dumps(provenance, indent=2), flush=True)


def run_command(directory, name, command, cwd):
    if not (directory / "provenance.json").is_file():
        raise RuntimeError("Initialize evidence before running checks")
    if not re.fullmatch(r"[a-z0-9][a-z0-9-]*", name):
        raise ValueError("Command names must be lowercase letters, digits and hyphens")
    log_path = directory / (name + ".log")
    result_path = directory / (name + ".json")
    if log_path.exists() or result_path.exists():
        raise RuntimeError("Duplicate command name: " + name)
    started = utc_now()
    before = time.monotonic()
    code = 1
    # A missing executable still gets an explicit failed result and a log.
    with log_path.open("x", encoding="utf-8") as log:
        log.write("Command: " + json.dumps(command) + "\n")
        log.write("Working directory: " + str(Path(cwd).resolve()) + "\n")
        try:
            process = subprocess.Popen(
                command, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                text=True, encoding="utf-8", errors="replace",
            )
            for line in process.stdout:
                sys.stdout.write(line)
                sys.stdout.flush()
                log.write(line)
                log.flush()
            code = process.wait()
        except OSError as error:
            message = str(error) + "\n"
            sys.stderr.write(message)
            log.write(message)
        finally:
            result = {
                "name": name, "command": command,
                "working_directory": str(Path(cwd).resolve()),
                "started_at_utc": started,
                "duration_seconds": round(time.monotonic() - before, 3),
                "exit_code": code,
                "log": log_path.name,
            }
            result_path.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return code


def finish(directory):
    provenance = json.loads((directory / "provenance.json").read_text(encoding="utf-8"))
    results = [json.loads(p.read_text(encoding="utf-8"))
               for p in directory.glob("*.json") if p.name != "provenance.json"]
    results.sort(key=lambda result: result["started_at_utc"])
    lines = [
        "## Quality evidence: " + provenance["job"], "",
        "Checked-out commit: `" + provenance["checked_out_commit"] + "`",
        "PR head: `" + str(provenance["pull_request_head_sha"] or "not a PR") + "`",
        "PR base: `" + str(provenance["pull_request_base_sha"] or "not a PR") + "`",
        "Job status before reporting: **" + os.environ.get("QUALITY_JOB_STATUS", "local") + "**",
        "", "| Check | Command | Exit code | Seconds |",
        "| --- | --- | ---: | ---: |",
    ]
    for result in results:
        command = subprocess.list2cmdline(result["command"]).replace("|", "\\|")
        lines.append(f"| {result['name']} | `{command}` | {result['exit_code']} | {result['duration_seconds']} |")
    lines += ["", "Only executed commands are listed. A missing row is not a passed check.",
              "Logs and provenance.json contain tool versions, source audit and build sizes.",
              "These are host/build checks, not device or radio acceptance.", ""]
    summary = "\n".join(lines)
    (directory / "summary.md").write_text(summary, encoding="utf-8")
    if os.environ.get("GITHUB_STEP_SUMMARY"):
        with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as output:
            output.write(summary)
    print(summary)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=report_directory())
    sub = parser.add_subparsers(dest="action", required=True)
    sub.add_parser("init")
    run = sub.add_parser("run")
    run.add_argument("--name", required=True)
    run.add_argument("--cwd", default=".")
    run.add_argument("command", nargs=argparse.REMAINDER)
    sub.add_parser("finish")
    args = parser.parse_args()
    if args.action == "init":
        initialize(args.output)
    elif args.action == "finish":
        finish(args.output)
    else:
        command = args.command[1:] if args.command[:1] == ["--"] else args.command
        if not command:
            parser.error("run needs a command after --")
        return run_command(args.output, args.name, command, args.cwd)
    return 0


if __name__ == "__main__":
    sys.exit(main())
