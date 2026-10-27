import argparse
import hashlib
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


BRANCH = "refs/heads/ci/current-library-runtime-qa-20261027-e0cd1259"
MAIN = "e0cd1259a95e6dfaf97f8f73e74410bbc5d8d199"
MAIN_TREE = "41d6e32a5a74db9f851e5d1329f88314ac270cdb"
INPUT_SHA = "f6e4e529b20b13c5eafc6b73efb157892a79546cf642b5283e2c044fe1dbeb57"
HEADER_TESTS = {"public_headers"} | {"public_headers_compile_" + str(index) for index in range(64)}
REQUIRED_UNION_TESTS = HEADER_TESTS | {
    "guest_sigvec", "guest_sigvec_modules", "guest_avplayer", "guest_avplayer_callback_close",
    "guest_avplayer_paused_first_video", "guest_avplayer_http", "avplayer_http_transport", "guest_http",
    "guest_http_response_header", "guest_http_options", "guest_http_client", "avplayer_http_patched_modules"
} | {"guest_icu_" + name for name in ["version", "unescape", "tokenize", "comparisons", "copy", "copy_guest_memory", "utf_conversions", "utf_memory", "utf16", "search"]} | {
    "guest_crypto_memcmp", "guest_libressl3_digest", "guest_libressl3_asn1", "guest_libressl3_key_reference", "guest_libressl3_evp"
} | {"guest_libressl_" + name for name in ["x509_errors", "digest", "asn1", "asn1_allocator", "des", "runtime", "aes", "error", "ocsp", "stack", "ui", "rsa", "evp", "cipher", "alert", "version", "crypto_locks", "pkey_type", "object_text", "object_allocation"]}


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


def validate_environment(env):
    require(env.get("GITHUB_REPOSITORY") == "AZLisme/AnyPS5", "wrong repository")
    require(env.get("GITHUB_REPOSITORY_ID") == "1430544501", "wrong repository id")
    require(env.get("GITHUB_EVENT_NAME") == "push", "only push is allowed")
    require(env.get("GITHUB_REF") == BRANCH, "wrong QA branch")
    require(oid(env.get("GITHUB_SHA")), "invalid harness commit")
    require(all(oid(env.get(key)) for key in ["BASELINE_COMMIT", "BASELINE_TREE", "UNION_COMMIT", "UNION_TREE"]), "invalid control or union pins")
    require(oid(env.get("PRODUCT_COMMIT")) and oid(env.get("PRODUCT_TREE")), "invalid product pin")
    require(env.get("SOURCE_CASE") in {"baseline", "union"}, "invalid source case")
    expected = (env["BASELINE_COMMIT"], env["BASELINE_TREE"]) if env["SOURCE_CASE"] == "baseline" else (env["UNION_COMMIT"], env["UNION_TREE"])
    require((env["PRODUCT_COMMIT"], env["PRODUCT_TREE"]) == expected, "matrix product pin differs from its case")
    require(env["UNION_COMMIT"] != env["BASELINE_COMMIT"], "union cannot be the control")
    require(env["BASELINE_COMMIT"] != MAIN, "control must include the reviewed fixture projection")


def guard(args):
    validate_environment(os.environ)
    harness = Path(args.harness).resolve()
    require(command(["git", "rev-parse", "HEAD"], harness) == os.environ["GITHUB_SHA"], "harness checkout mismatch")
    parents = command(["git", "show", "-s", "--format=%P", "HEAD"], harness).split()
    require(parents == [os.environ["UNION_COMMIT"]], "harness must have the exact product union as its sole parent")
    inputs = harness / ".github/qa/current-library-frozen-inputs.json"
    require(digest(inputs) == INPUT_SHA, "frozen input manifest changed")
    manifest = json.loads(inputs.read_text(encoding="utf-8"))
    require(manifest["main"] == MAIN, "frozen main mismatch")
    require(manifest["main_tree"] == MAIN_TREE, "frozen main tree mismatch")
    for case, prefix in [("baseline_control", "BASELINE"), ("union_product", "UNION")]:
        require(manifest[case]["commit"] == os.environ[prefix + "_COMMIT"] and manifest[case]["tree"] == os.environ[prefix + "_TREE"], "manifest product differs from workflow: " + case)
    require(manifest["baseline_control"]["kind"] == "frozen-main-plus-test-only-fixture-projection", "control scope mismatch")
    require(oid(manifest["fixture_projection"]["commit"]) and oid(manifest["fixture_projection"]["source_head"]), "invalid shared fixture provenance")
    require(manifest["fixture_projection"]["source_pr"] == 11985, "wrong shared fixture source")
    require(len(manifest["fixture_projection"]["files"]) == 1, "fixture projection must contain one reviewed file")
    require(len(manifest["private_candidates"]) == 1 and manifest["private_candidates"][0]["pr"] == 10957, "wrong private candidate scope")
    require(oid(manifest["private_candidates"][0]["head"]), "invalid private HTTP candidate")
    require(manifest["private_candidates"][0]["published_lease"] == next(entry["head"] for entry in manifest["inputs"] if entry["pr"] == 10957), "private HTTP lease differs from published input")
    changed = command(["git", "diff", "--name-only", "HEAD^", "HEAD"], harness).splitlines()
    allowed = {
        ".github/workflows/current-library-windows-qa.yml",
        ".github/scripts/current_library_windows_qa.py",
        ".github/qa/current-library-frozen-inputs.json"
    }
    require(set(changed) == allowed, "harness commit must add exactly the three reviewed QA files")
    statuses = command(["git", "diff", "--name-status", "HEAD^", "HEAD"], harness).splitlines()
    require(all(line.startswith("A\t") for line in statuses), "harness may only add new QA files")
    require(command(["git", "status", "--porcelain", "--untracked-files=all"], harness) == "", "dirty harness checkout")
    write(Path(args.out) / "guard.json", {"case": os.environ["SOURCE_CASE"], "harness_commit": os.environ["GITHUB_SHA"],
        "harness_parent": parents[0], "product_commit": os.environ["PRODUCT_COMMIT"],
        "product_tree": os.environ["PRODUCT_TREE"], "frozen_inputs": record(inputs), "inputs": manifest,
        "changed_harness_files": changed, "repository": os.environ["GITHUB_REPOSITORY"], "ref": BRANCH})


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


def source(args):
    root = Path(args.source).resolve()
    inventory = tree_inventory(root, os.environ["PRODUCT_COMMIT"], os.environ["PRODUCT_TREE"], Path(args.out) / (args.name + "-failure.json"))
    inputs = json.loads((Path(args.harness) / ".github/qa/current-library-frozen-inputs.json").read_text(encoding="utf-8"))
    projection = inputs["fixture_projection"]
    expected = [{"pr": "main", "head": MAIN}, {"pr": 11165, "head": "066fead614f618231c7298305264e7a48cb31345"},
        {"role": "shared-test-only-fixture-projection", "head": projection["commit"]}]
    if os.environ["SOURCE_CASE"] == "union":
        expected += inputs["inputs"] + inputs["private_candidates"]
    proofs = []
    for entry in expected:
        result = subprocess.run(["git", "merge-base", "--is-ancestor", entry["head"], os.environ["PRODUCT_COMMIT"]], cwd=root, capture_output=True)
        proofs.append({**entry, "exit_code": result.returncode, "stderr": result.stderr.decode("utf-8", errors="replace")})
        require(result.returncode == 0, "expected source input is not an ancestor: " + str(entry.get("pr", entry.get("role"))))
    inventory["source_ancestry"] = proofs
    for entry in projection["files"]:
        require(oid(entry["blob"]), "invalid projected fixture blob")
        require(command(["git", "rev-parse", "--verify", "HEAD:" + entry["path"]], root) == entry["blob"], "shared projected fixture differs from reviewed blob")
    inventory["fixture_projection"] = projection
    inventory["source_scope"] = "frozen-main-plus-test-only-fixture-projection control" if os.environ["SOURCE_CASE"] == "baseline" else "ten-topic integration plus private HTTP candidate and shared test-only fixture projection"
    path = Path(args.out) / (args.name + ".json")
    write(path, inventory)
    if args.name == "source-after":
        before = json.loads((Path(args.out) / "source-before.json").read_text(encoding="utf-8"))
        require(inventory == before, "source/dependency inventory changed during build/tests")


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


def run(args):
    arguments = args.command[1:] if args.command[:1] == ["--"] else args.command
    require(bool(arguments), "missing command")
    result = run_process(arguments, Path(args.out) / (args.name + ".log"), args.cwd)
    write(Path(args.out) / (args.name + ".json"), result)
    return result["exit_code"]


def toolchain(args):
    out = Path(args.out)
    tools = {}
    for name in ["gcc", "g++", "cmake", "ninja", "python", "git", "objdump", "nm", "ccache"]:
        path = shutil.which(name)
        require(path is not None, "missing tool: " + name)
        tools[name] = {**record(path), "version": command([path, "--version"])}
    require(command([shutil.which("g++"), "-dumpfullversion"]) == "15.2.0", "wrong GNU compiler version")
    require(command([shutil.which("g++"), "-dumpmachine"]) == "x86_64-w64-mingw32", "wrong compiler target")
    require(Path(tools["g++"]["path"]).resolve().parent == Path("C:/winlibs/mingw64/bin").resolve(), "wrong compiler location")
    runtimes = [record(Path("C:/winlibs/mingw64/bin") / name) for name in ["libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"]]
    environment = {key: os.environ.get(key) for key in ["ImageOS", "ImageVersion", "RUNNER_OS", "RUNNER_ARCH", "PATH", "SSL_CERT_FILE", "CONFIGURE", "CMAKE_BUILD_PARALLEL_LEVEL", "WINLIBS_RELEASE", "WINLIBS_ZIP"]}
    write(out / "toolchain.json", {"tools": tools, "compiler_runtime_DLLs": runtimes,
        "downloaded_WinLibs_archive": record(args.archive), "download_url": "https://github.com/brechtsanders/winlibs_mingw/releases/download/15.2.0posix-14.0.0-ucrt-r7/" + os.environ["WINLIBS_ZIP"],
        "environment": environment, "ccache_configuration": command(["ccache", "--show-config"]),
        "Windows": command(["cmd.exe", "/d", "/c", "ver"]),
        "FFmpeg_scope": "Normal repository-pinned prebuilt package, no local vendor build or sanitizer claim."})


def test_list(args):
    require(not (Path(args.build) / "select.tests").exists(), "unexpected selective CTest filter")
    require(not (Path(args.build) / "select.ninja").exists(), "unexpected selective build filter")
    text = command(["ctest", "--test-dir", args.build, "--show-only=json-v1"])
    data = json.loads(text)
    names = {item["name"] for item in data["tests"]}
    require(bool(names), "no tests registered")
    expected = REQUIRED_UNION_TESTS if os.environ["SOURCE_CASE"] == "union" else HEADER_TESTS
    require(expected <= names, "required current tests missing: " + str(sorted(expected - names)))
    write(Path(args.out) / "ctest-registration.json", data)
    write(Path(args.out) / "test-list.json", {"registered": len(names), "case": os.environ["SOURCE_CASE"], "required_union_tests": sorted(REQUIRED_UNION_TESTS)})


def junit_outcomes(path, expected):
    cases = ET.parse(path).getroot().findall(".//testcase")
    found = {}
    totals = {"passed": 0, "failed": 0, "skipped": 0}
    for case in cases:
        name = case.get("name")
        require(name not in found, "duplicate JUnit test: " + str(name))
        failed = case.find("failure") is not None or case.find("error") is not None
        skipped = case.find("skipped") is not None or case.get("status") in {"notrun", "disabled", "skipped"}
        status = "failed" if failed else "skipped" if skipped else "passed"
        found[name] = status
        totals[status] += 1
    require(bool(cases), "empty JUnit")
    errors = [name + ": " + found.get(name, "missing") for name in sorted(expected) if found.get(name) != "passed"]
    return {"actual_testcases": len(cases), "totals": totals, "required_focus": {name: found.get(name, "missing") for name in sorted(expected)}, "errors": errors}


def collect(args):
    out = Path(args.out)
    build = Path(args.build)
    captured = out / "captured"
    captured.mkdir(parents=True, exist_ok=True)
    names = ["CMakeCache.txt", "compile_commands.json", "build.ninja", ".ninja_log", "ctest.log", "ctest.xml"]
    for name in names:
        path = build / name
        if path.is_file():
            shutil.copy2(path, captured / name)
    temporary = build / "Testing/Temporary"
    if temporary.exists():
        shutil.copytree(temporary, captured / "Testing/Temporary", dirs_exist_ok=True)
    if (build / "build.ninja").exists():
        for name, arguments in [("ninja-deps", ["ninja", "-C", str(build), "-t", "deps"]), ("ninja-commands", ["ninja", "-C", str(build), "-t", "commands"])]:
            result = run_process(arguments, out / (name + ".log"), console=False)
            write(out / (name + ".json"), result)
            require(result["exit_code"] == 0, "failed build metadata capture: " + name)
    artifacts = []
    staged_payloads = []
    selected = {"libc.prx", "libkernel.prx", "libSceLibcInternal.prx", "libSceHttp.prx", "libSceAvPlayer.prx", "libSceAvPlayer.native.prx", "libSceIcu.prx", "libSceLibreSsl.prx", "libSceLibreSsl3.prx"}
    for path in sorted(build.rglob("*")):
        if not path.is_file() or path.suffix.lower() not in {".prx", ".exe", ".dll", ".a"}:
            continue
        entry = record(path)
        entry["relative_path"] = str(path.relative_to(build))
        artifacts.append(entry)
        relative = path.relative_to(build)
        http_stage = relative.parent.parent.name == "avplayer_http_patched_modules" and re.fullmatch(r"run-[0-9a-f]{16}", relative.parent.name) is not None
        staged_payload = http_stage and path.suffix.lower() in {".exe", ".dll"}
        if path.name in selected or staged_payload:
            target = captured / "selected-products" / path.relative_to(build)
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(path, target)
            if staged_payload:
                staged_payloads.append(entry)
    write(out / "staged-fixture-payloads.json", staged_payloads)
    write(out / "all-build-artifact-hashes.json", artifacts)
    bodies = []
    for path in [build / "Testing/Temporary/LastTest.log", build / "ctest.xml", build / "ctest.log"]:
        if path.exists():
            for number, line in enumerate(path.read_text(encoding="utf-8", errors="replace").splitlines(), 1):
                if any(word in line for word in ["HTTP provider ", "patched ", "guest HTTP client", "AvPlayer HTTP", "Internal-first", "libc-first", "sigvec"]):
                    bodies.append({"file": str(path), "line": number, "text": line})
    write(out / "provider-and-focused-output.json", {"rows": bodies, "limit": "Selected raw excerpts; full JUnit, CTest log and Testing/Temporary remain authoritative. No rerun performed."})
    package = []
    for directory in (build / "externals").glob("ffmpeg*"):
        for path in sorted(directory.rglob("*")) if directory.is_dir() else [directory]:
            if path.is_file():
                package.append(record(path))
    write(out / "FFmpeg-pinned-prebuilt-package-hashes.json", package)
    write(out / "collect.json", {"artifacts_hashed": len(artifacts), "FFmpeg_files_hashed": len(package), "staged_fixture_payloads_copied": len(staged_payloads)})


def finalize(args):
    out = Path(args.out)
    mandatory = ["guard", "source-before", "toolchain", "configure", "registration", "test-list", "build", "libs", "ctest", "skip-validator", "source-after", "collect"]
    failures = []
    outcomes = json.loads(os.environ.get("QA_OUTCOMES", "{}"))
    require(set(outcomes) == set(mandatory), "missing or extra workflow stage outcomes")
    failures.extend(name + ": workflow outcome " + outcome for name, outcome in outcomes.items() if outcome != "success")
    for name in mandatory:
        path = out / (name + ".json")
        if not path.is_file():
            failures.append(name + ": missing receipt")
            continue
        value = json.loads(path.read_text(encoding="utf-8"))
        if value.get("exit_code", 0) != 0:
            failures.append(name + ": exit " + str(value["exit_code"]))
    for name in ["captured/ctest.xml", "captured/Testing/Temporary/LastTest.log"]:
        if not (out / name).is_file():
            failures.append("missing required evidence: " + name)
    if (out / "captured/ctest.xml").is_file():
        try:
            expected = REQUIRED_UNION_TESTS if os.environ["SOURCE_CASE"] == "union" else HEADER_TESTS
            outcomes = junit_outcomes(out / "captured/ctest.xml", expected)
            write(out / "actual-test-outcomes.json", outcomes)
            failures.extend(outcomes["errors"])
            if outcomes["totals"]["failed"]:
                failures.append("full JUnit contains failed tests")
        except (ET.ParseError, RuntimeError) as error:
            failures.append("JUnit validation: " + str(error))
    write(out / "final-result.json", {"case": os.environ["SOURCE_CASE"], "product_commit": os.environ["PRODUCT_COMMIT"],
        "product_tree": os.environ["PRODUCT_TREE"], "harness_commit": os.environ["GITHUB_SHA"], "failures": failures,
        "scope": "Owned-fork Windows fixture-applied frozen-main control or ten-topic integration with the same fixture projection and private HTTP candidate. The control is not bare main. Actual totals and skips require JUnit/skip-validator qualification; not upstream required-check completion or prerequisite landing."})
    write(out / "evidence-manifest.json", [record(path) for path in sorted(out.rglob("*")) if path.is_file() and path.name != "evidence-manifest.json"])
    require(not failures, "QA command failure or missing stage: " + "; ".join(failures))


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("action", choices=["guard", "source", "run", "toolchain", "test-list", "collect", "finalize"])
    parser.add_argument("--out", required=True)
    parser.add_argument("--harness")
    parser.add_argument("--source")
    parser.add_argument("--build")
    parser.add_argument("--archive")
    parser.add_argument("--name")
    parser.add_argument("--cwd")
    args, extra = parser.parse_known_args()
    args.command = extra
    return {"guard": guard, "source": source, "run": run, "toolchain": toolchain, "test-list": test_list, "collect": collect, "finalize": finalize}[args.action](args) or 0


if __name__ == "__main__":
    sys.exit(main())
