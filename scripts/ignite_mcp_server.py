#!/usr/bin/env python3
"""
Ignite Engine MCP Server (Stdio JSON-RPC 2.0)

Exposes Model Context Protocol (MCP) tools so any AI coding agent or assistant
can build, test, launch, inspect, and control Ignite Editor, Ignite Runtime,
packaged standalone games, and Ignite Dedicated Server via AgentExecutionContext.

Zero third-party Python dependencies (pure Python 3 stdlib).
Supports both newline-delimited JSON-RPC (MCP 2024-11-05+) and Content-Length framing.
"""

from __future__ import annotations

import json
import os
import shutil
import subprocess
import sys
import tempfile
import threading
import time
import uuid
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any


REPO_ROOT = Path(__file__).resolve().parent.parent
SHADER_SOURCE_DIR = REPO_ROOT / "ignite" / "editor" / "resources" / "shaders"


@dataclass
class ManagedSession:
    session_id: str
    target: str
    executable: Path
    process: subprocess.Popen[str]
    work_dir: Path
    cmd_file: Path | None = None
    res_file: Path | None = None
    log_file: Path | None = None
    started_at: float = field(default_factory=time.time)
    last_command_ts: float = 0.0

    def is_alive(self) -> bool:
        return self.process.poll() is None

    def to_dict(self) -> dict[str, Any]:
        return {
            "session_id": self.session_id,
            "target": self.target,
            "pid": self.process.pid,
            "alive": self.is_alive(),
            "returncode": self.process.poll(),
            "executable": str(self.executable),
            "cmd_file": str(self.cmd_file) if self.cmd_file else None,
            "res_file": str(self.res_file) if self.res_file else None,
            "log_file": str(self.log_file) if self.log_file else None,
            "uptime_sec": round(time.time() - self.started_at, 2),
        }


class IgniteMcpServer:
    def __init__(self, repo_root: Path = REPO_ROOT) -> None:
        self.repo_root = repo_root
        self.sessions: dict[str, ManagedSession] = {}
        self.lock = threading.Lock()
        self.session_base_dir = Path(tempfile.gettempdir()) / "ignite_mcp_sessions"
        self.session_base_dir.mkdir(parents=True, exist_ok=True)

    # ------------------------------------------------------------------
    # Build & Path Helpers
    # ------------------------------------------------------------------

    @staticmethod
    def _find_msbuild() -> list[str]:
        which = shutil.which("msbuild")
        if which:
            return [which]

        vswhere = Path(r"C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe")
        if vswhere.exists():
            try:
                out = subprocess.check_output(
                    [
                        str(vswhere),
                        "-latest",
                        "-requires",
                        "Microsoft.Component.MSBuild",
                        "-find",
                        r"MSBuild\**\Bin\MSBuild.exe",
                    ],
                    text=True,
                    stderr=subprocess.DEVNULL,
                )
                for line in out.splitlines():
                    candidate = Path(line.strip())
                    if candidate.exists():
                        return [str(candidate)]
            except Exception:
                pass

        dotnet = shutil.which("dotnet")
        if dotnet:
            return [dotnet, "msbuild"]
        raise FileNotFoundError("Could not locate msbuild or dotnet CLI on PATH or via vswhere.exe")

    def _bin_dir(self, config: str = "Debug") -> Path:
        return self.repo_root / "bin" / config

    def _resolve_executable(self, target: str, config: str = "Debug", executable_path: str | None = None) -> Path:
        if executable_path:
            p = Path(executable_path)
            if not p.is_absolute():
                p = (self.repo_root / p).resolve()
            if not p.exists():
                raise FileNotFoundError(f"Executable not found: {p}")
            return p

        mapping = {
            "editor": "Ignite.Editor.exe",
            "runtime": "Ignite.Runtime.exe",
            "server": "Ignite.Network.exe",
            "network": "Ignite.Network.exe",
            "test": "Ignite.Test.exe",
        }
        key = target.lower()
        if key not in mapping:
            raise ValueError(f"Unknown target '{target}'. Expected one of editor, runtime, server, test, executable")
        p = self._bin_dir(config) / mapping[key]
        if not p.exists():
            raise FileNotFoundError(f"Binary not built yet: {p}. Run ignite_build_solution first.")
        return p

    def sync_shaders(self, config: str = "Debug", target_dir: str | None = None) -> dict[str, Any]:
        src = SHADER_SOURCE_DIR
        if not src.exists():
            raise FileNotFoundError(f"Shader source directory missing: {src}")

        dst = Path(target_dir) if target_dir else (self._bin_dir(config) / "resources" / "shaders")
        if not dst.is_absolute():
            dst = (self.repo_root / dst).resolve()
        dst.mkdir(parents=True, exist_ok=True)

        copied = 0
        for root, _, files in os.walk(src):
            rel = Path(root).relative_to(src)
            out_dir = dst / rel
            out_dir.mkdir(parents=True, exist_ok=True)
            for fname in files:
                src_file = Path(root) / fname
                dst_file = out_dir / fname
                if (
                    not dst_file.exists()
                    or src_file.stat().st_mtime > dst_file.stat().st_mtime
                    or src_file.stat().st_size != dst_file.stat().st_size
                ):
                    shutil.copy2(src_file, dst_file)
                    copied += 1

        return {"source": str(src), "destination": str(dst), "files_updated": copied}

    # ------------------------------------------------------------------
    # Atomic Command Watch Protocol
    # ------------------------------------------------------------------

    @staticmethod
    def _atomic_write_json(path: Path, data: Any) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        tmp = path.with_name(f"{path.name}.{uuid.uuid4().hex[:8]}.tmp")
        tmp.write_text(json.dumps(data, indent=2), encoding="utf-8")
        for attempt in range(8):
            try:
                os.replace(tmp, path)
                return
            except PermissionError:
                if attempt == 7:
                    raise
                time.sleep(0.015)

    def _send_watch_command(
        self,
        session: ManagedSession,
        command_name: str,
        params: dict[str, Any] | None = None,
        timeout_sec: float = 15.0,
    ) -> dict[str, Any]:
        if not session.cmd_file or not session.res_file:
            raise ValueError(f"Session '{session.session_id}' does not have an AgentController watch channel.")
        if not session.is_alive():
            raise RuntimeError(f"Session '{session.session_id}' already exited with code {session.process.poll()}.")

        req_id = uuid.uuid4().hex
        payload: dict[str, Any] = dict(params or {})
        payload["command"] = command_name
        payload["request_id"] = req_id

        # Ensure NTFS file timestamp strictly changes relative to previous write
        now = time.time()
        if now - session.last_command_ts < 0.02:
            time.sleep(0.02)

        self._atomic_write_json(session.cmd_file, payload)
        session.last_command_ts = time.time()

        deadline = time.time() + max(0.5, float(timeout_sec))
        while time.time() < deadline:
            if session.res_file.exists():
                try:
                    text = session.res_file.read_text(encoding="utf-8")
                    if text.strip():
                        parsed = json.loads(text)
                        if parsed.get("request_id") == req_id:
                            return parsed
                except Exception:
                    pass
            if not session.is_alive():
                if command_name == "quit":
                    return {"ok": True, "command": "quit", "request_id": req_id, "note": "Process exited"}
                raise RuntimeError(
                    f"Target process for session '{session.session_id}' exited unexpectedly "
                    f"(code={session.process.poll()}). Log: {session.log_file}"
                )
            time.sleep(0.05)

        raise TimeoutError(
            f"Timed out after {timeout_sec}s waiting for AgentController response to '{command_name}' "
            f"(session={session.session_id}, log={session.log_file})"
        )

    # ------------------------------------------------------------------
    # Tool Handlers
    # ------------------------------------------------------------------

    def tool_build_solution(self, args: dict[str, Any]) -> dict[str, Any]:
        config = str(args.get("config", "Debug"))
        platform = str(args.get("platform", "x64"))
        solution = str(args.get("solution", "IGN.slnx"))
        targets = args.get("targets")
        do_sync = bool(args.get("sync_shaders", True))

        sln_path = Path(solution)
        if not sln_path.is_absolute():
            sln_path = (self.repo_root / sln_path).resolve()
        if not sln_path.exists():
            raise FileNotFoundError(f"Solution not found: {sln_path}")

        cmd = self._find_msbuild() + [
            str(sln_path),
            f"/p:Configuration={config}",
            f"/p:Platform={platform}",
            "/m",
            "/v:minimal",
        ]
        if targets:
            if isinstance(targets, list):
                cmd.append(f"/t:{';'.join(str(t) for t in targets)}")
            else:
                cmd.append(f"/t:{targets}")

        t0 = time.time()
        proc = subprocess.run(
            cmd,
            cwd=str(self.repo_root),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=float(args.get("timeout_sec", 600.0)),
        )
        duration = round(time.time() - t0, 2)

        shader_info = None
        if proc.returncode == 0 and do_sync:
            shader_info = self.sync_shaders(config=config)

        tail_lines = (proc.stdout + "\n" + proc.stderr).strip().splitlines()[-60:]
        return {
            "success": proc.returncode == 0,
            "returncode": proc.returncode,
            "duration_sec": duration,
            "command": cmd,
            "shader_sync": shader_info,
            "output_tail": "\n".join(tail_lines),
        }

    def tool_run_tests(self, args: dict[str, Any]) -> dict[str, Any]:
        config = str(args.get("config", "Debug"))
        test_filter = str(args.get("filter", "")).strip()
        if bool(args.get("sync_shaders", True)):
            self.sync_shaders(config=config)

        exe = self._resolve_executable("test", config=config)
        cmd = [str(exe)]
        if test_filter:
            cmd.append(f"--gtest_filter={test_filter}")
        cmd.append("--gtest_color=no")

        t0 = time.time()
        proc = subprocess.run(
            cmd,
            cwd=str(exe.parent),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=float(args.get("timeout_sec", 180.0)),
        )
        duration = round(time.time() - t0, 2)
        return {
            "success": proc.returncode == 0,
            "returncode": proc.returncode,
            "duration_sec": duration,
            "executable": str(exe),
            "stdout": proc.stdout[-12000:],
            "stderr": proc.stderr[-4000:],
        }

    def tool_sync_shaders(self, args: dict[str, Any]) -> dict[str, Any]:
        return self.sync_shaders(config=str(args.get("config", "Debug")), target_dir=args.get("target_dir"))

    def tool_launch_app(self, args: dict[str, Any]) -> dict[str, Any]:
        target = str(args.get("target", "editor"))
        config = str(args.get("config", "Debug"))
        exe = self._resolve_executable(target, config=config, executable_path=args.get("executable_path"))

        if bool(args.get("sync_shaders", True)) and target.lower() in {"editor", "runtime"}:
            self.sync_shaders(config=config)

        session_id = str(args.get("session_id") or f"{target.lower()}-{uuid.uuid4().hex[:8]}")
        work_dir = Path(args["watch_dir"]) if args.get("watch_dir") else (self.session_base_dir / session_id)
        work_dir.mkdir(parents=True, exist_ok=True)

        cmd_file = (work_dir / "command.json").resolve()
        res_file = (work_dir / "response.json").resolve()
        log_file = (work_dir / "process.log").resolve()
        for f in (cmd_file, res_file):
            if f.exists():
                f.unlink()

        cli: list[str] = [
            str(exe),
            "--watch-command-file",
            str(cmd_file),
            "--response-file",
            str(res_file),
        ]
        if project := args.get("project_path"):
            p = Path(str(project))
            if not p.is_absolute():
                p = (self.repo_root / p).resolve()
            if target.lower() == "editor":
                cli.append(f"--project={p}")
            else:
                cli.extend(["--project", str(p)])
        if bool(args.get("windowed", True)):
            cli.append("--windowed")
        if bool(args.get("offscreen", False)):
            cli.append("--offscreen")
        if bool(args.get("headless", False)):
            cli.append("--headless")
        if bool(args.get("nogui", False)):
            cli.append("--nogui")
        if bool(args.get("hide_overlay", False)):
            cli.append("--hide-overlay")
        if connect := args.get("connect"):
            cli.extend(["--connect", str(connect)])
        for extra in args.get("extra_args", []) or []:
            cli.append(str(extra))

        log_fh = open(log_file, "w", encoding="utf-8", errors="replace")
        proc = subprocess.Popen(
            cli,
            cwd=str(exe.parent),
            stdout=log_fh,
            stderr=subprocess.STDOUT,
            text=True,
        )

        session = ManagedSession(
            session_id=session_id,
            target=target,
            executable=exe,
            process=proc,
            work_dir=work_dir,
            cmd_file=cmd_file,
            res_file=res_file,
            log_file=log_file,
        )
        with self.lock:
            self.sessions[session_id] = session

        wait_ready = bool(args.get("wait_for_ready", True))
        ready_status = None
        if wait_ready:
            ready_status = self._send_watch_command(
                session, "ping", timeout_sec=float(args.get("ready_timeout_sec", 20.0))
            )

        out = session.to_dict()
        if ready_status is not None:
            out["initial_status"] = ready_status
        return out

    def tool_send_command(self, args: dict[str, Any]) -> dict[str, Any]:
        session_id = str(args["session_id"])
        with self.lock:
            session = self.sessions.get(session_id)
        if not session:
            raise KeyError(f"Unknown session_id '{session_id}'. Active: {list(self.sessions.keys())}")

        command = str(args["command"])
        params = dict(args.get("params") or {})
        timeout_sec = float(args.get("timeout_sec", 15.0))
        return self._send_watch_command(session, command, params=params, timeout_sec=timeout_sec)

    def tool_bake_gi(self, args: dict[str, Any]) -> dict[str, Any]:
        params: dict[str, Any] = {}
        if args.get("volume") is not None:
            params["volume"] = args["volume"]
        if args.get("output_path"):
            params["output_path"] = str(args["output_path"])
        return self.tool_send_command({
            "session_id": args["session_id"],
            "command": "bake_gi",
            "params": params,
            "timeout_sec": float(args.get("timeout_sec", 30.0)),
        })

    def tool_bake_reflection_probes(self, args: dict[str, Any]) -> dict[str, Any]:
        params: dict[str, Any] = {}
        if args.get("output_dir"):
            params["output_dir"] = str(args["output_dir"])
        return self.tool_send_command({
            "session_id": args["session_id"],
            "command": "bake_reflection_probes",
            "params": params,
            "timeout_sec": float(args.get("timeout_sec", 60.0)),
        })

    def tool_clear_baked_gi(self, args: dict[str, Any]) -> dict[str, Any]:
        return self.tool_send_command({
            "session_id": args["session_id"],
            "command": "clear_baked_gi",
            "params": {},
            "timeout_sec": float(args.get("timeout_sec", 15.0)),
        })

    def tool_get_gi_status(self, args: dict[str, Any]) -> dict[str, Any]:
        return self.tool_send_command({
            "session_id": args["session_id"],
            "command": "get_gi_status",
            "params": {},
            "timeout_sec": float(args.get("timeout_sec", 15.0)),
        })

    def tool_set_gi_debug_mode(self, args: dict[str, Any]) -> dict[str, Any]:
        return self.tool_send_command({
            "session_id": args["session_id"],
            "command": "set_gi_debug_mode",
            "params": {"mode": int(args["mode"])},
            "timeout_sec": float(args.get("timeout_sec", 15.0)),
        })

    def tool_get_gpu_timings(self, args: dict[str, Any]) -> dict[str, Any]:
        return self.tool_send_command({
            "session_id": args["session_id"],
            "command": "get_gpu_timings",
            "params": {},
            "timeout_sec": float(args.get("timeout_sec", 15.0)),
        })

    def tool_stop_app(self, args: dict[str, Any]) -> dict[str, Any]:
        session_id = str(args["session_id"])
        with self.lock:
            session = self.sessions.get(session_id)
        if not session:
            raise KeyError(f"Unknown session_id '{session_id}'")

        graceful = False
        if session.is_alive() and session.cmd_file and session.res_file:
            try:
                self._send_watch_command(session, "quit", timeout_sec=5.0)
                session.process.wait(timeout=5.0)
                graceful = True
            except Exception:
                pass

        if session.is_alive():
            session.process.terminate()
            try:
                session.process.wait(timeout=3.0)
            except subprocess.TimeoutExpired:
                session.process.kill()
                session.process.wait(timeout=3.0)

        return {
            "session_id": session_id,
            "graceful": graceful,
            "returncode": session.process.poll(),
        }

    def tool_run_command_script(self, args: dict[str, Any]) -> dict[str, Any]:
        target = str(args.get("target", "editor"))
        config = str(args.get("config", "Debug"))
        exe = self._resolve_executable(target, config=config, executable_path=args.get("executable_path"))

        if bool(args.get("sync_shaders", True)) and target.lower() in {"editor", "runtime"}:
            self.sync_shaders(config=config)

        run_dir = self.session_base_dir / f"script-{uuid.uuid4().hex[:8]}"
        run_dir.mkdir(parents=True, exist_ok=True)

        if script_path_arg := args.get("script_path"):
            script_path = Path(str(script_path_arg))
            if not script_path.is_absolute():
                script_path = (self.repo_root / script_path).resolve()
        else:
            commands = args.get("commands")
            if not isinstance(commands, list):
                raise ValueError("Provide either 'commands' (JSON array) or 'script_path'")
            script_path = run_dir / "commands.json"
            self._atomic_write_json(script_path, commands)

        if report_arg := args.get("report_path"):
            report_path = Path(str(report_arg))
            if not report_path.is_absolute():
                report_path = (self.repo_root / report_path).resolve()
        else:
            report_path = run_dir / "report.json"

        cli: list[str] = [
            str(exe),
            "--command-script",
            str(script_path),
            "--command-report",
            str(report_path),
        ]
        if bool(args.get("quit_after_script", True)):
            cli.append("--quit-after-script")
        if project := args.get("project_path"):
            p = Path(str(project))
            if not p.is_absolute():
                p = (self.repo_root / p).resolve()
            if target.lower() == "editor":
                cli.append(f"--project={p}")
            else:
                cli.extend(["--project", str(p)])
        if bool(args.get("windowed", True)):
            cli.append("--windowed")
        if bool(args.get("offscreen", False)):
            cli.append("--offscreen")
        if bool(args.get("headless", False)):
            cli.append("--headless")
        if bool(args.get("nogui", False)):
            cli.append("--nogui")
        if bool(args.get("hide_overlay", False)):
            cli.append("--hide-overlay")

        t0 = time.time()
        proc = subprocess.run(
            cli,
            cwd=str(exe.parent),
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=float(args.get("timeout_sec", 60.0)),
        )
        duration = round(time.time() - t0, 2)

        report_data = None
        if report_path.exists():
            try:
                report_data = json.loads(report_path.read_text(encoding="utf-8"))
            except Exception as exc:
                report_data = {"parse_error": str(exc)}

        return {
            "success": proc.returncode == 0,
            "returncode": proc.returncode,
            "duration_sec": duration,
            "script_path": str(script_path),
            "report_path": str(report_path),
            "report": report_data,
            "stdout_tail": proc.stdout[-4000:],
            "stderr_tail": proc.stderr[-2000:],
        }

    def tool_package_game(self, args: dict[str, Any]) -> dict[str, Any]:
        output_dir = str(args["output_dir"])
        out_path = Path(output_dir)
        if not out_path.is_absolute():
            out_path = (self.repo_root / out_path).resolve()

        pkg_params = {
            "output_dir": str(out_path),
            "include_server": bool(args.get("include_server", False)),
            "include_pdbs": bool(args.get("include_pdbs", False)),
            "overwrite": bool(args.get("overwrite", True)),
        }
        if args.get("executable_name"):
            pkg_params["executable_name"] = str(args["executable_name"])

        if session_id := args.get("session_id"):
            return self.tool_send_command(
                {
                    "session_id": session_id,
                    "command": "package_game",
                    "params": pkg_params,
                    "timeout_sec": float(args.get("timeout_sec", 60.0)),
                }
            )

        project_path = str(args.get("project_path", "")).strip()
        if not project_path:
            raise ValueError("Provide either 'session_id' (running Editor/Runtime) or 'project_path'")

        cmd_entry = {"command": "package_game", **pkg_params}
        return self.tool_run_command_script(
            {
                "target": str(args.get("target", "runtime")),
                "config": str(args.get("config", "Debug")),
                "project_path": project_path,
                "commands": [cmd_entry],
                "quit_after_script": True,
                "offscreen": True,
                "nogui": True,
                "headless": True,
                "windowed": False,
                "timeout_sec": float(args.get("timeout_sec", 60.0)),
            }
        )

    def tool_start_server(self, args: dict[str, Any]) -> dict[str, Any]:
        config = str(args.get("config", "Debug"))
        exe = self._resolve_executable("server", config=config)
        port = int(args.get("port", 8192))
        tickrate = int(args.get("tick_rate", 60))
        snapshotrate = int(args.get("snapshot_rate", 20))

        session_id = str(args.get("session_id") or f"server-{uuid.uuid4().hex[:8]}")
        work_dir = self.session_base_dir / session_id
        work_dir.mkdir(parents=True, exist_ok=True)
        log_file = work_dir / "server.log"

        cli = [
            str(exe),
            "--port",
            str(port),
            "--tickrate",
            str(tickrate),
            "--snapshotrate",
            str(snapshotrate),
        ]
        if project := args.get("project_path"):
            p = Path(str(project))
            if not p.is_absolute():
                p = (self.repo_root / p).resolve()
            cli.extend(["--project", str(p)])
        if scene := args.get("scene_path"):
            s = Path(str(scene))
            if not s.is_absolute():
                s = (self.repo_root / s).resolve()
            cli.extend(["--scene", str(s)])

        log_fh = open(log_file, "w", encoding="utf-8", errors="replace")
        proc = subprocess.Popen(cli, cwd=str(exe.parent), stdout=log_fh, stderr=subprocess.STDOUT, text=True)

        session = ManagedSession(
            session_id=session_id,
            target="server",
            executable=exe,
            process=proc,
            work_dir=work_dir,
            log_file=log_file,
        )
        with self.lock:
            self.sessions[session_id] = session
        return session.to_dict()

    def tool_list_sessions(self, _: dict[str, Any]) -> dict[str, Any]:
        with self.lock:
            items = [s.to_dict() for s in self.sessions.values()]
        return {"sessions": items}

    # ------------------------------------------------------------------
    # Tool Registry & MCP JSON-RPC Dispatch
    # ------------------------------------------------------------------

    def get_tool_definitions(self) -> list[dict[str, Any]]:
        return [
            {
                "name": "ignite_build_solution",
                "description": "Build IGN.slnx using MSBuild and sync edited shaders from ignite/editor/resources/shaders to bin/<config>/resources/shaders.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "config": {"type": "string", "default": "Debug", "enum": ["Debug", "Release", "Shipping"]},
                        "platform": {"type": "string", "default": "x64"},
                        "targets": {"type": "array", "items": {"type": "string"}},
                        "sync_shaders": {"type": "boolean", "default": True},
                    },
                },
            },
            {
                "name": "ignite_sync_shaders",
                "description": "Copy HLSL/HLSLI shaders from ignite/editor/resources/shaders/ to bin/<config>/resources/shaders/.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "config": {"type": "string", "default": "Debug"},
                        "target_dir": {"type": "string"},
                    },
                },
            },
            {
                "name": "ignite_run_tests",
                "description": "Sync shaders and run Ignite.Test.exe GoogleTest suite with optional --gtest_filter.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "config": {"type": "string", "default": "Debug"},
                        "filter": {"type": "string", "default": ""},
                        "sync_shaders": {"type": "boolean", "default": True},
                    },
                },
            },
            {
                "name": "ignite_launch_app",
                "description": "Launch Ignite.Editor.exe, Ignite.Runtime.exe, or a packaged game with AgentController watch-file live MCP control.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "target": {"type": "string", "enum": ["editor", "runtime", "executable"], "default": "editor"},
                        "config": {"type": "string", "default": "Debug"},
                        "executable_path": {"type": "string"},
                        "project_path": {"type": "string"},
                        "windowed": {"type": "boolean", "default": True},
                        "offscreen": {"type": "boolean", "default": False},
                        "headless": {"type": "boolean", "default": False},
                        "nogui": {"type": "boolean", "default": False},
                        "hide_overlay": {"type": "boolean", "default": False},
                        "connect": {"type": "string"},
                        "wait_for_ready": {"type": "boolean", "default": True},
                        "ready_timeout_sec": {"type": "number", "default": 20.0},
                    },
                },
            },
            {
                "name": "ignite_send_command",
                "description": "Send a live JSON command over the watch channel to a running Ignite session (create_entity, add_component, set_camera, set_render_settings, run_lua, physics_raycast, play, stop, capture_screenshot, package_game, etc.).",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id", "command"],
                    "properties": {
                        "session_id": {"type": "string"},
                        "command": {"type": "string"},
                        "params": {"type": "object", "default": {}},
                        "timeout_sec": {"type": "number", "default": 15.0},
                    },
                },
            },
            {
                "name": "ignite_stop_app",
                "description": "Gracefully quit and stop a managed Ignite session (Editor, Runtime, packaged game, or Dedicated Server).",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id"],
                    "properties": {"session_id": {"type": "string"}},
                },
            },
            {
                "name": "ignite_bake_gi",
                "description": "Start a time-sliced irradiance-volume GI bake in a running Ignite Editor session.",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id"],
                    "properties": {
                        "session_id": {"type": "string"},
                        "volume": {"description": "Volume entity name or UUID", "oneOf": [{"type": "string"}, {"type": "integer"}]},
                        "output_path": {"type": "string"},
                        "timeout_sec": {"type": "number", "default": 30.0},
                    },
                },
            },
            {
                "name": "ignite_bake_reflection_probes",
                "description": "Bake all reflection probes in a running Ignite Editor session.",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id"],
                    "properties": {
                        "session_id": {"type": "string"},
                        "output_dir": {"type": "string"},
                        "timeout_sec": {"type": "number", "default": 60.0},
                    },
                },
            },
            {
                "name": "ignite_clear_baked_gi",
                "description": "Clear baked GI volume and reflection probe references from the active scene.",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id"],
                    "properties": {"session_id": {"type": "string"}, "timeout_sec": {"type": "number", "default": 15.0}},
                },
            },
            {
                "name": "ignite_get_gi_status",
                "description": "Read the progress and completion state of the active GI bake.",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id"],
                    "properties": {"session_id": {"type": "string"}, "timeout_sec": {"type": "number", "default": 15.0}},
                },
            },
            {
                "name": "ignite_set_gi_debug_mode",
                "description": "Set GI debug rendering mode: 0 for color or 9..12 for diffuse, specular, AO, and probe weights.",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id", "mode"],
                    "properties": {"session_id": {"type": "string"}, "mode": {"type": "integer", "minimum": 0, "maximum": 12}},
                },
            },
            {
                "name": "ignite_get_gpu_timings",
                "description": "Read the latest SceneRenderer GPU timestamp zones, including depth prepass and GTAO.",
                "inputSchema": {
                    "type": "object",
                    "required": ["session_id"],
                    "properties": {"session_id": {"type": "string"}, "timeout_sec": {"type": "number", "default": 15.0}},
                },
            },
            {
                "name": "ignite_run_command_script",
                "description": "Execute a sequence of AgentController JSON commands headlessly or in a window via --command-script and return the parsed JSON execution report.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "target": {"type": "string", "enum": ["editor", "runtime", "executable"], "default": "editor"},
                        "config": {"type": "string", "default": "Debug"},
                        "executable_path": {"type": "string"},
                        "project_path": {"type": "string"},
                        "commands": {"type": "array", "items": {"type": "object"}},
                        "script_path": {"type": "string"},
                        "report_path": {"type": "string"},
                        "quit_after_script": {"type": "boolean", "default": True},
                        "windowed": {"type": "boolean", "default": True},
                        "offscreen": {"type": "boolean", "default": False},
                        "headless": {"type": "boolean", "default": False},
                        "nogui": {"type": "boolean", "default": False},
                        "hide_overlay": {"type": "boolean", "default": False},
                        "timeout_sec": {"type": "number", "default": 60.0},
                    },
                },
            },
            {
                "name": "ignite_package_game",
                "description": "Package an Ignite project into a standalone self-contained distributable folder containing <Name>.exe, assets, shaders, scripts, and runtime DLLs.",
                "inputSchema": {
                    "type": "object",
                    "required": ["output_dir"],
                    "properties": {
                        "output_dir": {"type": "string"},
                        "session_id": {"type": "string"},
                        "project_path": {"type": "string"},
                        "executable_name": {"type": "string"},
                        "include_server": {"type": "boolean", "default": False},
                        "include_pdbs": {"type": "boolean", "default": False},
                        "overwrite": {"type": "boolean", "default": True},
                    },
                },
            },
            {
                "name": "ignite_start_server",
                "description": "Start a headless Ignite.Network.exe dedicated server process for a scene/project.",
                "inputSchema": {
                    "type": "object",
                    "properties": {
                        "scene_path": {"type": "string"},
                        "project_path": {"type": "string"},
                        "port": {"type": "integer", "default": 8192},
                        "tick_rate": {"type": "integer", "default": 60},
                        "snapshot_rate": {"type": "integer", "default": 20},
                        "config": {"type": "string", "default": "Debug"},
                    },
                },
            },
            {
                "name": "ignite_list_sessions",
                "description": "List active and exited Ignite sessions launched by this MCP server.",
                "inputSchema": {"type": "object", "properties": {}},
            },
        ]

    def handle_request(self, req: dict[str, Any]) -> dict[str, Any] | None:
        method = req.get("method", "")
        req_id = req.get("id")
        params = req.get("params") or {}

        # JSON-RPC notifications have no id and expect no reply
        if req_id is None:
            return None

        try:
            if method == "initialize":
                proto = params.get("protocolVersion", "2024-11-05")
                result = {
                    "protocolVersion": proto,
                    "capabilities": {"tools": {"listChanged": False}},
                    "serverInfo": {"name": "ignite-agent-mcp", "version": "1.0.0"},
                }
            elif method == "ping":
                result = {}
            elif method == "tools/list":
                result = {"tools": self.get_tool_definitions()}
            elif method == "tools/call":
                tool_name = params.get("name", "")
                tool_args = params.get("arguments") or {}
                dispatch = {
                    "ignite_build_solution": self.tool_build_solution,
                    "ignite_sync_shaders": self.tool_sync_shaders,
                    "ignite_run_tests": self.tool_run_tests,
                    "ignite_launch_app": self.tool_launch_app,
                    "ignite_send_command": self.tool_send_command,
                    "ignite_bake_gi": self.tool_bake_gi,
                    "ignite_bake_reflection_probes": self.tool_bake_reflection_probes,
                    "ignite_clear_baked_gi": self.tool_clear_baked_gi,
                    "ignite_get_gi_status": self.tool_get_gi_status,
                    "ignite_set_gi_debug_mode": self.tool_set_gi_debug_mode,
                    "ignite_get_gpu_timings": self.tool_get_gpu_timings,
                    "ignite_stop_app": self.tool_stop_app,
                    "ignite_run_command_script": self.tool_run_command_script,
                    "ignite_package_game": self.tool_package_game,
                    "ignite_start_server": self.tool_start_server,
                    "ignite_list_sessions": self.tool_list_sessions,
                }
                handler = dispatch.get(tool_name)
                if not handler:
                    return {
                        "jsonrpc": "2.0",
                        "id": req_id,
                        "error": {"code": -32601, "message": f"Unknown tool: {tool_name}"},
                    }
                payload = handler(tool_args)
                is_err = isinstance(payload, dict) and ((payload.get("ok") is False) or (payload.get("success") is False))
                result = {
                    "content": [{"type": "text", "text": json.dumps(payload, indent=2)}],
                    "isError": bool(is_err),
                }
            else:
                return {
                    "jsonrpc": "2.0",
                    "id": req_id,
                    "error": {"code": -32601, "message": f"Method not found: {method}"},
                }

            return {"jsonrpc": "2.0", "id": req_id, "result": result}
        except Exception as exc:
            return {
                "jsonrpc": "2.0",
                "id": req_id,
                "result": {
                    "content": [{"type": "text", "text": json.dumps({"error": str(exc)}, indent=2)}],
                    "isError": True,
                },
            }

    def cleanup_all(self) -> None:
        with self.lock:
            sessions = list(self.sessions.values())
        for session in sessions:
            if session.is_alive():
                try:
                    self.tool_stop_app({"session_id": session.session_id})
                except Exception:
                    pass

    def serve_stdio(self) -> None:
        stdin_buf = sys.stdin.buffer
        stdout_buf = sys.stdout.buffer
        use_content_length = False

        try:
            while True:
                line = stdin_buf.readline()
                if not line:
                    break
                stripped = line.strip()
                if not stripped:
                    continue

                if stripped.lower().startswith(b"content-length:"):
                    use_content_length = True
                    content_length = int(stripped.split(b":", 1)[1].strip())
                    while True:
                        hdr = stdin_buf.readline()
                        if not hdr or hdr in (b"\r\n", b"\n"):
                            break
                    body = stdin_buf.read(content_length)
                    if not body:
                        break
                    req = json.loads(body.decode("utf-8"))
                else:
                    req = json.loads(stripped.decode("utf-8"))

                resp = self.handle_request(req)
                if resp is not None:
                    encoded = json.dumps(resp, separators=(",", ":")).encode("utf-8")
                    if use_content_length:
                        header = f"Content-Length: {len(encoded)}\r\n\r\n".encode("ascii")
                        stdout_buf.write(header + encoded)
                    else:
                        stdout_buf.write(encoded + b"\n")
                    stdout_buf.flush()
        finally:
            self.cleanup_all()


def main() -> int:
    if "--self-test" in sys.argv:
        srv = IgniteMcpServer()
        init_res = srv.handle_request({"jsonrpc": "2.0", "id": 1, "method": "initialize", "params": {}})
        tools_res = srv.handle_request({"jsonrpc": "2.0", "id": 2, "method": "tools/list", "params": {}})
        assert init_res and "result" in init_res
        assert tools_res and len(tools_res["result"]["tools"]) >= 10
        print(json.dumps({"ok": True, "tools": [t["name"] for t in tools_res["result"]["tools"]]}, indent=2))
        return 0

    IgniteMcpServer().serve_stdio()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
