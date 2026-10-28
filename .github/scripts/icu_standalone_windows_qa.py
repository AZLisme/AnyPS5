import argparse
import hashlib
import importlib.util
import json
import os
from pathlib import Path
import re
import shutil
import stat
import subprocess
import sys
import time
import xml.etree.ElementTree as ET

MAIN = '6e9b9134f6ed14e659d870a33118d164096551df'
DRIVER_PARENT = 'babf2bc72012bd926f35408b3d602d162a5ecd1b'
BRANCH = 'refs/heads/ci/icu-utf16-search-windows-20261028'
INPUT_SHA = '99865de56824d8bd112c2159e6c7b3cc7b1ed66631371dde3d9ce0cd8e6558af'
HEADER_TESTS = {"public_headers"} | {"public_headers_compile_" + str(i) for i in range(64)}
QA_FILES = {".github/workflows/icu-standalone-windows-qa.yml", ".github/scripts/icu_standalone_windows_qa.py", ".github/scripts/icu_windows_capability_gate.py", ".github/qa/icu-standalone-windows-inputs.json"}

def require(condition, message):
    if not condition:
        raise RuntimeError(message)

def digest(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            result.update(block)
    return result.hexdigest()

def record(path):
    path = Path(path)
    return {"path": str(path), "bytes": path.stat().st_size, "sha256": digest(path)}

def write(path, value):
    path = Path(path)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")

def command(arguments, cwd=None):
    return subprocess.check_output(arguments, cwd=cwd, text=True, encoding="utf-8").strip()

def oid(value):
    return re.fullmatch(r"[0-9a-f]{40}", value or "") is not None

def git_blob(data):
    return hashlib.sha1(b"blob " + str(len(data)).encode("ascii") + b"\0" + data).hexdigest()

def windows_link_target_equal(expected, actual):
    if expected == actual:
        return True
    left, right = expected.replace(b"\\", b"/"), actual.replace(b"\\", b"/")
    namespaces = (b"//?/", b"//./", b"/??/")
    return not left.startswith(namespaces) and not right.startswith(namespaces) and left == right

def tree_inventory(source, expected_head, expected_tree, mismatch_file=None):
    require(command(["git", "rev-parse", "HEAD"], source) == expected_head, "product HEAD mismatch")
    require(command(["git", "rev-parse", "HEAD^{tree}"], source) == expected_tree, "product tree mismatch")
    require(not (source / ".github/workflows/current-library-windows-qa.yml").exists(), "QA workflow leaked into product source")
    require(not (source / ".github/scripts/current_library_windows_qa.py").exists(), "QA helper leaked into product source")
    require(not (source / ".github/qa/current-library-frozen-inputs.json").exists(), "QA manifest leaked into product source")
    contexts = []
    files = []

    def reject(details):
        print("SOURCE_VERIFIER_MISMATCH " + json.dumps(details), flush=True)
        if mismatch_file is not None:
            write(mismatch_file, details)
        raise RuntimeError("source bytes differ from Git blob: " + details["path"])

    def visit(root, relative, expected):
        require(command(["git", "rev-parse", "HEAD"], root) == expected, "gitlink checkout mismatch: " + relative)
        entries = subprocess.check_output(["git", "ls-tree", "-rz", "HEAD"], cwd=root).split(b"\0")
        contexts.append({"path": relative, "head": expected, "tree": command(["git", "rev-parse", "HEAD^{tree}"], root)})
        checkout_policy = None
        info_attributes = None
        declaration_blobs = {}
        for entry in entries:
            if not entry:
                continue
            metadata, raw_name = entry.split(b"\t", 1)
            mode, kind, blob = metadata.decode("ascii").split()
            name = raw_name.decode("utf-8")
            path = root / name
            label = str(Path(relative) / name).replace("\\", "/")
            if kind == "commit":
                require(path.is_dir(), "missing submodule: " + label)
                visit(path, label, blob)
                continue
            require(kind == "blob", "unknown Git tree item")
            details = {"path": label, "context": str(root), "git_mode": mode, "expected_git_blob": blob, "platform": sys.platform}
            try:
                info = path.lstat()
                native = path.is_symlink()
                details.update({"lstat_mode": info.st_mode, "file_attributes": getattr(info, "st_file_attributes", None),
                    "reparse_tag": getattr(info, "st_reparse_tag", None), "native_symlink": native})
                if not native and not stat.S_ISREG(info.st_mode):
                    reject(details)
                data = os.fsencode(os.readlink(path)) if native else path.read_bytes()
                actual_blob = git_blob(data)
                details.update({"actual_git_blob": actual_blob, "actual_sha256": hashlib.sha256(data).hexdigest(),
                    "actual_bytes": len(data), "actual_bytes_hex_prefix": data[:256].hex()})
                valid = actual_blob == blob and not native
                link = {}
                normalized = False
                declared_eol = None
                text_policy = {}
                if mode == "120000":
                    expected_target = subprocess.check_output(["git", "cat-file", "blob", blob], cwd=root)
                    config = subprocess.run(["git", "config", "--get", "core.symlinks"], cwd=root, capture_output=True)
                    link = {"representation": "native" if native else "literal_file", "core_symlinks": config.stdout.decode("utf-8", errors="replace").strip(),
                        "core_symlinks_exit": config.returncode, "core_symlinks_stderr": config.stderr.decode("utf-8", errors="replace")[:256],
                        "lstat_mode": info.st_mode, "reparse_tag": getattr(info, "st_reparse_tag", None), "expected_target_hex": expected_target[:4096].hex(),
                        "expected_target_bytes": len(expected_target), "expected_target_sha256": hashlib.sha256(expected_target).hexdigest(),
                        "actual_target_hex": data[:4096].hex(), "target_hex_truncated": len(expected_target) > 4096 or len(data) > 4096}
                    equivalent = native and sys.platform == "win32" and windows_link_target_equal(expected_target, data)
                    valid = git_blob(expected_target) == blob and (data == expected_target or equivalent)
                    link["windows_separator_equivalence"] = bool(equivalent and data != expected_target)
                elif not native and not valid and b"\r\n" in data:
                    if info_attributes is None:
                        info_attributes = Path(command(["git", "rev-parse", "--git-path", "info/attributes"], root))
                        if not info_attributes.is_absolute():
                            info_attributes = root / info_attributes
                    details["info_attributes"] = str(info_attributes)
                    if info_attributes.exists():
                        details["reason"] = "Git info attributes override not allowed"
                        reject(details)
                    declarations = []
                    for parent in Path(name).parents:
                        attribute_name = str(parent / ".gitattributes").replace("\\", "/")
                        if (root / attribute_name).exists():
                            if attribute_name not in declaration_blobs:
                                declaration_blobs[attribute_name] = command(["git", "rev-parse", "--verify", "HEAD:" + attribute_name], root)
                            declarations.append({"path": attribute_name, "git_blob": declaration_blobs[attribute_name]})
                    details["tracked_attribute_files"] = declarations
                    names = ["eol", "text", "filter", "working-tree-encoding", "ident"]
                    attributes = subprocess.check_output(["git", "-c", "core.attributesFile=", "check-attr", "-z", *names, "--", name],
                        cwd=root, env={**os.environ, "GIT_ATTR_NOSYSTEM": "1"}).split(b"\0")
                    if len(attributes) != 3 * len(names) + 1 or attributes[-1] != b"" or any(attributes[i] != raw_name for i in range(0, len(attributes) - 1, 3)):
                        reject(details)
                    values = {attributes[i + 1].decode(): attributes[i + 2].decode("utf-8", errors="backslashreplace") for i in range(0, len(attributes) - 1, 3)}
                    if set(values) != set(names):
                        reject(details)
                    if checkout_policy is None:
                        checkout_policy = {"platform": sys.platform}
                        for key in ["core.autocrlf", "core.eol"]:
                            config = subprocess.run(["git", "config", "--get", key], cwd=root, capture_output=True)
                            if config.returncode not in {0, 1}:
                                reject({**details, "config_error": key, "stderr": config.stderr.decode(errors="replace")[:256]})
                            checkout_policy[key] = config.stdout.decode().strip() if config.returncode == 0 else None
                    autocrlf = checkout_policy["core.autocrlf"].lower() if checkout_policy["core.autocrlf"] is not None else None
                    core_eol = checkout_policy["core.eol"].lower() if checkout_policy["core.eol"] is not None else None
                    native_crlf = autocrlf in {"true", "yes", "on", "1"} or (autocrlf in {None, "false", "no", "off", "0"} and (core_eol == "crlf" or (core_eol in {None, "native"} and sys.platform == "win32")))
                    declared_eol = values["eol"]
                    allowed_policy = values["text"] in {"unspecified", "set", "auto"} and (declared_eol == "crlf" or (declared_eol == "unspecified" and values["text"] in {"set", "auto"} and native_crlf))
                    safe_attributes = all(values[key] in {"unspecified", "unset"} for key in ["filter", "working-tree-encoding", "ident"])
                    text_policy = {"attributes": values, "checkout": checkout_policy, "checkout_allows_crlf": allowed_policy, "safe_attributes": safe_attributes}
                    details["text_policy"] = text_policy
                    if not declarations or not allowed_policy or not safe_attributes:
                        reject(details)
                    classification = subprocess.check_output(["git", "--literal-pathspecs", "-c", "core.attributesFile=", "ls-files", "--eol", "-z", "--", name],
                        cwd=root, env={**os.environ, "GIT_ATTR_NOSYSTEM": "1"}).split(b"\0")
                    header = classification[0].split(b"\t", 1) if len(classification) == 2 and classification[1] == b"" else []
                    text_policy["Git_index_worktree_eol"] = header[0].decode(errors="backslashreplace") if header else "invalid-eol-output"
                    classified_text = len(header) == 2 and header[1] == raw_name and header[0].split()[:2] == [b"i/lf", b"w/crlf"]
                    normalized = bool(declarations) and allowed_policy and safe_attributes and classified_text and git_blob(data.replace(b"\r\n", b"\n")) == blob
                    valid = normalized
                details.update({"actual_git_blob": actual_blob, "actual_sha256": hashlib.sha256(data).hexdigest(),
                    "actual_bytes": len(data), "actual_bytes_hex_prefix": data[:256].hex(), "symlink": link, "declared_eol": declared_eol})
                if not valid:
                    reject(details)
            except (OSError, subprocess.CalledProcessError) as error:
                details["read_error"] = str(error)[:2048]
                reject(details)
            files.append({"path": label, "mode": mode, "git_blob": blob, "actual_git_blob": actual_blob, "sha256": hashlib.sha256(data).hexdigest(), "bytes": len(data), "symlink": link,
                "checkout_CRLF_to_LF": normalized, "declared_eol": declared_eol, "text_policy": text_policy, "tracked_attribute_files": details.get("tracked_attribute_files", [])})
        dirty = command(["git", "diff", "--name-only", "HEAD", "--"], root)
        if dirty:
            reject({"path": relative, "context": str(root), "reason": "tracked source/index differs from HEAD", "dirty_paths": dirty[:4096]})

    visit(source, ".", expected_head)
    return {"head": expected_head, "tree": expected_tree,
        "parents": command(["git", "show", "-s", "--format=%P", "HEAD"], source).split(),
        "contexts": contexts, "tracked_files": files}

def run_process(arguments, log, cwd=None, console=True):
    started = time.time()
    log = Path(log)
    log.parent.mkdir(parents=True, exist_ok=True)
    with log.open("wb") as stream:
        child = subprocess.Popen(arguments, cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
        for block in iter(lambda: child.stdout.read(65536), b""):
            stream.write(block)
            stream.flush()
            if console:
                sys.stdout.buffer.write(block)
                sys.stdout.buffer.flush()
        code = child.wait()
    return {"command": arguments, "cwd": str(Path(cwd or ".").resolve()), "exit_code": code,
        "started_unix": started, "elapsed_seconds": time.time() - started, "log": record(log)}

def inputs():
    path = Path(__file__).resolve().parents[1] / "qa/icu-standalone-windows-inputs.json"
    require(digest(path) == INPUT_SHA, "frozen ICU input manifest changed")
    return json.loads(path.read_text(encoding="utf-8"))


def case():
    manifest = inputs()
    name = os.environ.get("SOURCE_CASE")
    require(name in manifest["cases"], "unknown standalone source case")
    return manifest["cases"][name]


def validate_environment(env):
    manifest = inputs()
    require(env.get("GITHUB_REPOSITORY") == "AZLisme/AnyPS5" and env.get("GITHUB_REPOSITORY_ID") == "1430544501", "wrong own fork")
    require(env.get("GITHUB_EVENT_NAME") == "push" and env.get("GITHUB_REF") == BRANCH, "wrong isolated activation event/ref")
    require(oid(env.get("GITHUB_SHA")), "invalid harness commit")
    require(env.get("SOURCE_CASE") in manifest["cases"], "wrong source case")
    selected = manifest["cases"][env["SOURCE_CASE"]]
    require((env.get("PRODUCT_COMMIT"), env.get("PRODUCT_TREE")) == (selected["commit"], selected["tree"]), "matrix source pins differ")
    require(env.get("BASELINE_COMMIT") == MAIN and env.get("BASELINE_TREE") == manifest["baseline_tree"], "wrong bare baseline")
    require(int(env.get("EXPECTED_FUNCTIONAL_COUNT", -1)) == len(selected["tests"]) and int(env.get("EXPECTED_TOTAL_COUNT", -1)) == selected["expected_total"], "wrong focused counts")
    require("APS5_NO_WRITE_WATCH" not in env, "write watch disabled, including by value zero/empty")
    for key in ["CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "LD_PRELOAD", "DYLD_INSERT_LIBRARIES"]:
        require(not env.get(key), "unreviewed build/runtime environment override: " + key)


def guard(args):
    validate_environment(os.environ)
    root = Path(args.harness).resolve()
    require(command(["git", "rev-parse", "HEAD"], root) == os.environ["GITHUB_SHA"], "driver checkout mismatch")
    require(command(["git", "show", "-s", "--format=%P", "HEAD"], root).split() == [DRIVER_PARENT], "driver sole parent must be the exact frozen candidate")
    changed = command(["git", "diff", "--name-status", "HEAD^", "HEAD"], root).splitlines()
    require(set(changed) == {"A\t" + name for name in QA_FILES}, "driver must add exactly four reviewed QA files")
    require(command(["git", "status", "--porcelain", "--untracked-files=all"], root) == "", "dirty driver")
    require(not Path("build").exists(), "existing build directory/cache not permitted")
    write(Path(args.out) / "guard.json", {"harness_commit": os.environ["GITHUB_SHA"], "harness_parent": DRIVER_PARENT,
        "case": os.environ["SOURCE_CASE"], "inputs": inputs(), "source_pin": case(),
        "QA_files": [record(root / name) for name in sorted(QA_FILES)], "environment_guard": "passed"})


def source(args):
    root = Path(args.source).resolve()
    selected = case()
    require(not any((root / name).exists() for name in QA_FILES), "new harness leaked into product checkout")
    inventory = tree_inventory(root, selected["commit"], selected["tree"], Path(args.out) / (args.name + "-failure.json"))
    actual = {}
    for entry in subprocess.check_output(["git", "ls-tree", "-rz", "HEAD"], cwd=root).split(b"\0"):
        if entry:
            metadata, name = entry.split(b"\t", 1)
            mode, kind, blob = metadata.decode().split()
            if kind == "commit":
                actual[name.decode()] = blob
    require(actual == {row["path"]: row["commit"] for row in selected["direct_gitlinks"]}, "wrong current direct gitlink inventory")
    for item in selected["critical_blobs"]:
        require(command(["git", "rev-parse", "HEAD:" + item["path"]], root) == item["git_blob"], "critical source pin differs: " + item["path"])
    result = subprocess.run(["git", "merge-base", "--is-ancestor", MAIN, selected["commit"]], cwd=root, capture_output=True)
    require(result.returncode == 0, "frozen main is not an ancestor")
    inventory["direct_gitlinks"] = actual
    inventory["scope"] = "exact frozen main baseline" if os.environ["SOURCE_CASE"] == "baseline" else "exact standalone packaging-fix candidate PR" + os.environ["SOURCE_CASE"]
    write(Path(args.out) / (args.name + ".json"), inventory)
    if args.name == "source-after":
        require(inventory == json.loads((Path(args.out) / "source-before.json").read_text()), "source/dependency bytes changed during QA")


def run(args):
    values = args.command[1:] if args.command[:1] == ["--"] else args.command
    require(bool(values), "missing run command")
    if args.name == "configure":
        require(not Path("build").exists(), "configuration cannot reuse an existing build")
        require(not any("LAUNCHER" in value or "INITIAL_CACHE" in value or value == "-C" for value in values), "configuration seed/cache launcher not permitted")
    result = run_process(values, Path(args.out) / (args.name + ".log"), args.cwd)
    write(Path(args.out) / (args.name + ".json"), result)
    return result["exit_code"]


def toolchain(args):
    require(sys.platform == "win32" and os.name == "nt", "native Windows required")
    require(digest(args.archive) == inputs()["WinLibs_archive_sha256"], "official compiler package differs")
    tools = {}
    for name in ["gcc", "g++", "cmake", "ninja", "python", "git", "objdump", "nm"]:
        path = shutil.which(name)
        require(path is not None, "missing native tool: " + name)
        tools[name] = {**record(path), "version": command([path, "--version"])}
    require(command([tools["g++"]["path"], "-dumpfullversion"]) == "15.2.0", "wrong compiler version")
    require(command([tools["g++"]["path"], "-dumpmachine"]) == "x86_64-w64-mingw32", "wrong compiler target")
    require(Path(tools["g++"]["path"]).resolve().parent == Path("C:/winlibs/mingw64/bin").resolve(), "wrong compiler executable")
    write(Path(args.out) / "toolchain.json", {"tools": tools, "Windows": command(["cmd.exe", "/d", "/c", "ver"]),
        "downloaded_WinLibs_archive": record(args.archive), "compiler_runtime_DLLs": [record(Path("C:/winlibs/mingw64/bin") / n) for n in ["libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"]],
        "environment": {key: os.environ.get(key) for key in ["ImageOS", "ImageVersion", "RUNNER_OS", "RUNNER_ARCH", "PATH", "PATHEXT", "SSL_CERT_FILE", "CONFIGURE", "WINLIBS_RELEASE", "WINLIBS_ZIP", "APS5_NO_WRITE_WATCH", "CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH"]}, "cache_policy": "Fresh product build; no binary/object/ccache restore or prior V4 artifact inputs."})


def registration(build):
    return json.loads(command(["ctest", "--test-dir", str(build), "--show-only=json-v1"]))


def test_list(args):
    build = Path(args.build).resolve()
    require(not any((build / name).exists() for name in ["select.tests", "select.ninja"]), "no generated selective test/build projection allowed")
    data = registration(build)
    names = {test["name"] for test in data["tests"]}
    required = HEADER_TESTS | set(case()["tests"])
    require(required <= names and {n for n in names if n.startswith("public_headers")} == HEADER_TESTS, "missing/changed focused registration")
    write(Path(args.out) / "ctest-registration.json", data)
    write(Path(args.out) / "test-list.json", {"registered": len(names), "selected": sorted(required), "functional": case()["tests"], "scope": "Existing normal registry retained; exact anchored selection only at execution."})


def build_targets(args):
    build = Path(args.build).resolve()
    cache = (build / "CMakeCache.txt").read_text()
    require("CMAKE_BUILD_TYPE:STRING=Release" in cache and "BUILD_TESTING:BOOL=ON" in cache, "not normal Release/testing configuration")
    require(not re.search(r"CMAKE_(?:C|CXX)_COMPILER_LAUNCHER:[^=]*=.+", cache), "compiler cache/launcher not permitted")
    result = run_process(["cmake", "--build", str(build), "--target", *case()["targets"], "--parallel", "2"], Path(args.out) / "build-targets.log")
    write(Path(args.out) / "build-targets.json", result)
    if result["exit_code"] == 0:
        data = registration(build)
        for test in data["tests"]:
            if test["name"] in case()["tests"]:
                require(test.get("command") and Path(test["command"][0]).resolve().is_relative_to(build / "tests"), "wrong normal fixture command")
        write(Path(args.out) / "ctest-registration-postbuild.json", data)
    return result["exit_code"]


def cap_module(path):
    spec = importlib.util.spec_from_file_location("reviewed_ICU_capability", path)
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module


def minimal_lane(args):
    return {"schema": 1, "pr": case()["pr"], "frozen_main": MAIN,
        "product_commit": case()["commit"], "product_tree": case()["tree"],
        "source_root": str(Path(args.source).resolve()), "build_root": str(Path(args.build).resolve()),
        "compile_commands": record(Path(args.build) / "compile_commands.json")}


def macros(args):
    out = Path(args.out).resolve()
    lane = out / "macro-lane.json"
    write(lane, minimal_lane(args))
    result = run_process([sys.executable, args.capability, "--lane", str(lane), "--out", str(out / "macro-receipt.json"), "--capture-macros"], out / "macros.log")
    write(out / "macros.json", result)
    return result["exit_code"]


def ctest_run(args, kind):
    out = Path(args.out).resolve() / kind
    out.mkdir(parents=True, exist_ok=False)
    selected = HEADER_TESTS if kind == "headers" else set(case()["tests"])
    pattern = "^(" + "|".join(re.escape(n) for n in sorted(selected)) + ")$"
    result = run_process(["ctest", "--test-dir", str(Path(args.build).resolve()), "-V", "-R", pattern,
        "--timeout", "30", "--parallel", "2" if kind == "headers" else "1", "--output-on-failure", "--output-junit", str(out / "ctest.xml"), "--output-log", str(out / "ctest.log")], out / "verbose.log")
    temporary = Path(args.build) / "Testing/Temporary"
    if temporary.exists():
        shutil.copytree(temporary, out / "Testing/Temporary", dirs_exist_ok=True)
    write(out / "ctest-receipt.json", result)
    write(Path(args.out) / ("ctest-" + kind + ".json"), result)
    return result["exit_code"]


def collect(args):
    out = Path(args.out).resolve()
    build = Path(args.build).resolve()
    captured = out / "captured"
    captured.mkdir(parents=True, exist_ok=True)
    for name in ["CMakeCache.txt", "compile_commands.json", "build.ninja", ".ninja_log"]:
        if (build / name).is_file():
            shutil.copy2(build / name, captured / name)
    artifacts = []
    products = {"libSceIcu.prx", "libc.prx", "libkernel.prx", "libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"} | {name + ".exe" for name in case()["targets"]}
    for path in sorted(build.rglob("*")) if build.exists() else []:
        if path.is_file() and path.suffix.lower() in {".exe", ".prx", ".dll", ".a"}:
            artifacts.append(record(path))
            if path.name in products:
                destination = captured / "products" / path.relative_to(build)
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copy2(path, destination)
                result = run_process(["objdump", "-p", str(path)], out / "objdump" / (str(path.relative_to(build)).replace("\\", "__").replace("/", "__") + ".txt"), console=False)
                require(result["exit_code"] == 0, "PE import/export capture failed")
    write(out / "all-build-artifact-hashes.json", artifacts)
    if (build / "build.ninja").exists():
        for name, commandline in [("ninja-commands", ["ninja", "-C", str(build), "-t", "commands"]), ("ninja-deps", ["ninja", "-C", str(build), "-t", "deps"])]:
            result = run_process(commandline, out / (name + ".log"), console=False)
            write(out / (name + ".json"), result)
            require(result["exit_code"] == 0, "build provenance capture failed")
    write(out / "collect.json", {"artifacts": len(artifacts), "provider_paths": "Normalunpatched productproviders; no patchedlibs substitution.", "provider_binding": "Resolved-path inference from exact source/CTest/PE imports/staging/environment; no direct module-load trace."})


def run_entry(out, kind):
    directory = out / kind
    return {"ctest_receipt": record(directory / "ctest-receipt.json"), "junit": record(directory / "ctest.xml"),
        "last_test_log": record(directory / "Testing/Temporary/LastTest.log"), "verbose_ctest_log": record(directory / "verbose.log")}


def capability(args):
    out = Path(args.out).resolve()
    build = Path(args.build).resolve()
    lane = minimal_lane(args)
    module = cap_module(args.capability)
    name = "guest_icu_unescape" if case()["pr"] == "main" else module.PINS[case()["pr"]][5]
    lane.update({"source_before": record(out / "source-before.json"), "source_after": record(out / "source-after.json"),
        "toolchain": record(out / "toolchain.json"), "macro_receipt": record(out / "macro-receipt.json"),
        "artifact_manifest": record(out / "all-build-artifact-hashes.json"),
        "executable": record(build / "tests" / (name + "_tests.exe")),
        "providers": [record(build / "core/libs/libs/unpatched" / name) for name in ["libSceIcu.prx", "libc.prx", "libkernel.prx"]],
        "runtime_dlls": [record(build / "tests" / name) for name in ["libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"]],
        "runs": [run_entry(out, "functional")], "header_run": run_entry(out, "headers"),
        "postbuild_registration": record(out / "ctest-registration-postbuild.json"),
        "environment": {key: os.environ.get(key) for key in ["ImageOS", "ImageVersion", "RUNNER_OS", "RUNNER_ARCH", "PATH", "PATHEXT", "SSL_CERT_FILE", "CONFIGURE", "WINLIBS_RELEASE", "WINLIBS_ZIP", "APS5_NO_WRITE_WATCH", "CC", "CXX", "CFLAGS", "CXXFLAGS", "CPPFLAGS", "LDFLAGS", "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH"]}})
    path = out / "lane.json"
    write(path, lane)
    result = run_process([sys.executable, args.capability, "--lane", str(path), "--out", str(out / "capability-proof.json")], out / "capability.log")
    write(out / "capability.json", result)
    return result["exit_code"]


def finalize(args):
    out = Path(args.out).resolve()
    mandatory = ["guard", "source-before", "toolchain", "configure", "registration", "test-list", "build-targets", "macros", "ctest-functional", "ctest-headers", "source-after", "collect", "capability"]
    outcomes = json.loads(os.environ.get("QA_OUTCOMES", "{}"))
    failures = [name + ": " + outcomes.get(name, "missing") for name in mandatory if outcomes.get(name) != "success"]
    if set(outcomes) != set(mandatory):
        failures.append("missing or extra workflow outcomes")
    for name in mandatory:
        path = out / (name + ".json")
        if not path.is_file():
            failures.append(name + ": missing receipt")
        elif json.loads(path.read_text()).get("exit_code", 0) != 0:
            failures.append(name + ": nonzero exit")
    if (out / "capability-proof.json").is_file():
        if json.loads((out / "capability-proof.json").read_text()).get("status") != "passed":
            failures.append("capability gate did not pass")
    else:
        failures.append("missing capability proof")
    write(out / "final-result.json", {"case": os.environ["SOURCE_CASE"], "product_commit": os.environ["PRODUCT_COMMIT"],
        "product_tree": os.environ["PRODUCT_TREE"], "harness_commit": os.environ["GITHUB_SHA"], "failures": failures,
        "selected_functional": case()["tests"], "selected_headers": 65, "expected_total": case()["expected_total"],
        "scope": "Exact standalone packaging-fix candidate or exact frozen main baseline. Single focused nativeWindows pass and65headers only; no fullsuite/upstreamCI/dependency/review/merge or oldV4runtime credit. Providerbinding is source/PE/normalpath inference, not directloadedmoduletrace."})
    write(out / "evidence-manifest.json", [record(path) for path in sorted(out.rglob("*")) if path.is_file() and path.name != "evidence-manifest.json"])
    require(not failures, "; ".join(failures))


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("action", choices=["guard", "source", "run", "toolchain", "test-list", "build-targets", "macros", "ctest-functional", "ctest-headers", "capability", "collect", "finalize"])
    for name in ["out", "harness", "source", "build", "archive", "name", "cwd", "capability"]:
        parser.add_argument("--" + name, required=name == "out")
    args, extra = parser.parse_known_args()
    args.command = extra
    selected = {"guard": guard, "source": source, "run": run, "toolchain": toolchain, "test-list": test_list,
        "build-targets": build_targets, "macros": macros, "ctest-functional": lambda a: ctest_run(a, "functional"),
        "ctest-headers": lambda a: ctest_run(a, "headers"), "capability": capability, "collect": collect, "finalize": finalize}
    return selected[args.action](args) or 0


if __name__ == "__main__":
    sys.exit(main())
