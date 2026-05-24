#!/usr/bin/env python3
"""Start a local BoostGateway stack and run the Qt client headless gate."""

from __future__ import annotations

import argparse
import json
import os
import signal
import socket
import subprocess
import time
from pathlib import Path
from typing import Any


CLIENT_ROOT = Path(__file__).resolve().parents[1]
DEFAULT_SERVER_ROOT = CLIENT_ROOT.parent / "BoostAsioDemo"


def resolve_executable(build_dir: Path, relative: str) -> Path:
    candidate = build_dir / relative
    if candidate.exists():
        return candidate
    if candidate.with_suffix(".exe").exists():
        return candidate.with_suffix(".exe")
    return candidate


def reserve_port(host: str) -> int:
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as sock:
        sock.bind((host, 0))
        return int(sock.getsockname()[1])


def wait_for_port(host: str, port: int, timeout_s: float) -> bool:
    deadline = time.monotonic() + timeout_s
    while time.monotonic() < deadline:
        try:
            with socket.create_connection((host, port), timeout=0.2):
                return True
        except OSError:
            time.sleep(0.1)
    return False


def run_command(name: str, command: list[str], cwd: Path, checks: list[dict[str, Any]]) -> bool:
    started = time.monotonic()
    result = subprocess.run(
        command,
        cwd=cwd,
        text=True,
        encoding="utf-8",
        errors="replace",
        capture_output=True,
    )
    passed = result.returncode == 0
    checks.append(
        {
            "name": name,
            "passed": passed,
            "command": command,
            "duration_seconds": round(time.monotonic() - started, 3),
            "stdout": (result.stdout or "")[-8000:],
            "stderr": (result.stderr or "")[-8000:],
        }
    )
    return passed


def start_process(
    name: str,
    command: list[str],
    cwd: Path,
    env: dict[str, str],
    log_dir: Path,
    checks: list[dict[str, Any]],
) -> subprocess.Popen[str] | None:
    log_dir.mkdir(parents=True, exist_ok=True)
    stdout_path = log_dir / f"{name}.stdout.log"
    stderr_path = log_dir / f"{name}.stderr.log"
    stdout_file = stdout_path.open("w", encoding="utf-8", errors="replace")
    stderr_file = stderr_path.open("w", encoding="utf-8", errors="replace")
    try:
        proc = subprocess.Popen(
            command,
            cwd=cwd,
            env=env,
            text=True,
            encoding="utf-8",
            errors="replace",
            stdout=stdout_file,
            stderr=stderr_file,
            start_new_session=True,
        )
        proc._bgtc_stdout_file = stdout_file  # type: ignore[attr-defined]
        proc._bgtc_stderr_file = stderr_file  # type: ignore[attr-defined]
        checks.append({"name": f"start-{name}", "passed": True, "command": command})
        return proc
    except OSError as exc:
        stdout_file.close()
        stderr_file.close()
        checks.append({"name": f"start-{name}", "passed": False, "command": command, "stderr": str(exc)})
        return None


def terminate_process(name: str, proc: subprocess.Popen[str], checks: list[dict[str, Any]]) -> None:
    try:
        os.killpg(proc.pid, signal.SIGTERM)
    except (ProcessLookupError, PermissionError):
        proc.terminate()
    try:
        proc.communicate(timeout=5)
    except subprocess.TimeoutExpired:
        try:
            os.killpg(proc.pid, signal.SIGKILL)
        except (ProcessLookupError, PermissionError):
            proc.kill()
        proc.communicate(timeout=5)
    for attr in ("_bgtc_stdout_file", "_bgtc_stderr_file"):
        handle = getattr(proc, attr, None)
        if handle is not None:
            try:
                handle.close()
            except OSError:
                pass
    checks.append({"name": f"stop-{name}", "passed": True})


def write_gateway_config(path: Path, ports: dict[str, int]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(
        json.dumps(
            {
                "gateway": {"http_management_port": ports["http"]},
                "backends": {
                    "login": {"host": "127.0.0.1", "port": ports["login"]},
                    "room": {"host": "127.0.0.1", "port": ports["room"]},
                    "battle": {"host": "127.0.0.1", "port": ports["battle"]},
                    "match": {"host": "127.0.0.1", "port": ports["match"]},
                    "leaderboard": {"host": "127.0.0.1", "port": ports["leaderboard"]},
                },
            },
            indent=2,
        ),
        encoding="utf-8",
    )


def write_summary(path: Path, summary: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(summary, indent=2, sort_keys=True), encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--server-root", type=Path, default=DEFAULT_SERVER_ROOT)
    parser.add_argument("--server-build-dir", type=Path)
    parser.add_argument("--client-build-dir", type=Path, default=CLIENT_ROOT / "build/local")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--skip-build", action="store_true")
    parser.add_argument("--summary-path", type=Path, default=CLIENT_ROOT / "runtime/validation/tank-client-live-gate-summary.json")
    parser.add_argument("--ready-timeout-seconds", type=float, default=30.0)
    args = parser.parse_args()

    server_root = args.server_root.resolve()
    server_build_dir = (args.server_build_dir or (server_root / "build")).resolve()
    client_build_dir = args.client_build_dir.resolve()
    checks: list[dict[str, Any]] = []

    if not args.skip_build:
        run_command(
            "configure-client-headless-gate",
            [
                "cmake",
                "-S",
                str(CLIENT_ROOT),
                "-B",
                str(client_build_dir),
                f"-DBOOST_GATEWAY_SERVER_ROOT={server_root}",
                f"-DBOOST_GATEWAY_SERVER_BUILD_DIR={server_build_dir}",
            ],
            CLIENT_ROOT,
            checks,
        )
        run_command("build-client-headless-gate", ["cmake", "--build", str(client_build_dir), "--target", "tank_headless_gate"], CLIENT_ROOT, checks)
        run_command(
            "build-server-stack",
            [
                "cmake", "--build", str(server_build_dir), "--target",
                "boost_gateway_sdk", "v2_gateway_demo", "v2_login_backend",
                "v2_room_backend",
                "v2_battle_backend", "v2_match_backend", "v2_leaderboard_backend",
            ],
            server_root,
            checks,
        )

    if any(not check.get("passed", False) for check in checks):
        write_summary(args.summary_path, {"summary_version": 1, "overall_pass": False, "failed_step": "build", "checks": checks})
        return 1

    executables = {
        "gateway": resolve_executable(server_build_dir, "examples/v2_gateway_demo/v2_gateway_demo"),
        "login": resolve_executable(server_build_dir, "examples/v2_login_backend/v2_login_backend"),
        "room": resolve_executable(server_build_dir, "examples/v2_room_backend/v2_room_backend"),
        "battle": resolve_executable(server_build_dir, "examples/v2_battle_backend/v2_battle_backend"),
        "match": resolve_executable(server_build_dir, "examples/v2_match_backend/v2_match_backend"),
        "leaderboard": resolve_executable(server_build_dir, "examples/v2_leaderboard_backend/v2_leaderboard_backend"),
        "headless": client_build_dir / "tank_headless_gate",
    }
    missing = [name for name, path in executables.items() if not path.exists()]
    if missing:
        checks.append({"name": "resolve-binaries", "passed": False, "stderr": "missing: " + ", ".join(missing)})
        write_summary(args.summary_path, {"summary_version": 1, "overall_pass": False, "failed_step": "resolve-binaries", "checks": checks})
        return 1

    ports = {name: reserve_port(args.host) for name in ["gateway", "http", "login", "room", "battle", "match", "leaderboard"]}
    temp_config = CLIENT_ROOT / "runtime/validation/tank-client-live-gateway.json"
    write_gateway_config(temp_config, ports)

    env = os.environ.copy()
    env["V2_BACKEND_CONNECTION_POOL_SIZE"] = "1"
    env["CONFIG_PATH"] = str(temp_config)
    log_dir = CLIENT_ROOT / "runtime/validation/live-gate-logs"
    processes: list[tuple[str, subprocess.Popen[str]]] = []

    try:
        for name in ["login", "room", "battle", "match", "leaderboard"]:
            command = [str(executables[name]), str(ports[name])]
            proc = start_process(name, command, server_root, env, log_dir, checks)
            if proc is not None:
                processes.append((name, proc))
            ready = proc is not None and wait_for_port(args.host, ports[name], args.ready_timeout_seconds)
            checks.append({"name": f"{name}-ready", "passed": ready, "port": ports[name]})
            if not ready:
                raise RuntimeError(f"{name} backend did not become ready")

        gateway_command = [
            str(executables["gateway"]),
            "--port", str(ports["gateway"]),
            "--http-port", str(ports["http"]),
            "--login-port", str(ports["login"]),
            "--room-port", str(ports["room"]),
            "--battle-port", str(ports["battle"]),
            "--matchmaking-port", str(ports["match"]),
            "--leaderboard-port", str(ports["leaderboard"]),
        ]
        gateway_proc = start_process("gateway", gateway_command, server_root, env, log_dir, checks)
        if gateway_proc is not None:
            processes.append(("gateway", gateway_proc))
        gateway_ready = gateway_proc is not None and wait_for_port(args.host, ports["gateway"], args.ready_timeout_seconds)
        checks.append({"name": "gateway-ready", "passed": gateway_ready, "port": ports["gateway"]})
        if not gateway_ready:
            raise RuntimeError("gateway did not become ready")

        headless_summary = CLIENT_ROOT / "runtime/validation/tank-client-headless-summary.json"
        run_command("run-tank-headless-gate", [str(executables["headless"]), args.host, str(ports["gateway"])], CLIENT_ROOT, checks)
        local_summary = CLIENT_ROOT / "tank-client-headless-summary.json"
        if local_summary.exists():
            local_summary.replace(headless_summary)
    except Exception as exc:  # noqa: BLE001
        checks.append({"name": "live-gate-exception", "passed": False, "stderr": str(exc)})
    finally:
        for name, proc in reversed(processes):
            terminate_process(name, proc, checks)
        try:
            temp_config.unlink()
        except OSError:
            pass

    failed = [check for check in checks if not check.get("passed", False)]
    summary = {
        "summary_version": 1,
        "overall_pass": not failed,
        "failed_step": failed[0]["name"] if failed else "",
        "ports": ports,
        "checks": checks,
    }
    write_summary(args.summary_path, summary)
    print(f"tank client live gate: {'PASS' if summary['overall_pass'] else 'FAIL'}")
    print(f"summary: {args.summary_path}")
    return 0 if summary["overall_pass"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
