import argparse
import ctypes
import hashlib
import json
import os
from pathlib import Path
import re
import struct
import subprocess
import sys
import xml.etree.ElementTree as ET


MAIN = "6e9b9134f6ed14e659d870a33118d164096551df"
MAIN_TREE = "8e9e4b8943411265388b4d109411e3b6f63eab34"
HEADERS = {"public_headers"} | {"public_headers_compile_" + str(index) for index in range(64)}
FUNCTIONAL = {
    "main": {"guest_icu_version", "guest_icu_unescape"},
    10962: {"guest_icu_version", "guest_icu_unescape", "guest_icu_tokenize"},
    10964: {"guest_icu_version", "guest_icu_unescape", "guest_icu_copy", "guest_icu_copy_guest_memory"},
    10965: {"guest_icu_version", "guest_icu_unescape", "guest_icu_utf_conversions", "guest_icu_utf_memory"},
    10967: {"guest_icu_version", "guest_icu_unescape", "guest_icu_search"},
}
PINS = {
    "main": (MAIN, MAIN_TREE, "core/libs/prx/libSceIcu/tests/Unescape.cpp", "1dc71529407172d6fa73033c7cc4e8697a59f602", "e679f1505e59c6937b28f870b32d258e5a1bea8b871944d80f869470584402ff", "guest_icu_unescape"),
    10962: ("bbccec1140eeccb5adf61f7c497791b183c1bf93", "34e9d6cb730faad7d1f669ec0e92712d23e62d90", "core/libs/tests/GuestIcuTokenize.cpp", "b0aab05bd2a8572c90b8819862acd5bf1e674274", "65f5bfa1e8dc8d919f83117794a6561464b2eb2bf92fb2357825086228f96f4d", "guest_icu_tokenize"),
    10964: ("4360e8f85b0bbd184199d9a3822dc30a8bd5f81e", "97741abfbf83816faa7109dc0132c8ad4cd0ac5f", "core/libs/prx/libSceIcu/tests/CopyGuestMemory.cpp", "740643bdeb2c190be7bcac5622186a749959ba17", "601d170defb4a363db422dec6d3818e9c7de816b28e8b63c9eede7686dedb44b", "guest_icu_copy_guest_memory"),
    10965: ("c2076fe30ee3442546af395930defd74b7bfac2a", "2f8bd9c7b0a98d3b44259b2661c73dd2fceb72e9", "core/libs/tests/GuestIcuUtfMemory.cpp", "77eb17433ab658f4939636a8d634521e20e731b7", "ce25c01398ba89446ac07bcb35677ffc11ca72a14a716f0c0e7cefea3437aa7e", "guest_icu_utf_memory"),
    10967: ("cc73317fb2bd06bf75e011587cc2fbfb6f973dea", "5ba31e1bfa02c0a31f476360a2d7a9585c9692bd", "core/libs/tests/GuestIcuSearch.cpp", "1aeb4f35a289f36ea85c334dbb41c22d2cfa6716", "abf562ba3ebd137482e44144a014cc3eb18871702e7cf59640c092905e6afed5", "guest_icu_search"),
}
VERIFIED = "guest-mapped copy and readonly no-op checks passed; write-watch verified"
UNAVAILABLE = "write-watch unavailable on this host"


def require(value, message):
    if not value:
        raise RuntimeError(message)


def digest(data):
    return hashlib.sha256(data).hexdigest()


def record(path):
    path = Path(path).resolve()
    data = path.read_bytes()
    return {"path": str(path), "sha256": digest(data), "bytes": len(data)}


def checked_file(entry):
    require(set(entry) >= {"path", "sha256"}, "missing pinned file identity")
    path = Path(entry["path"]).resolve()
    require(digest(path.read_bytes()) == entry["sha256"], "file digest mismatch: " + str(path))
    return path


def read_json(entry):
    return json.loads(checked_file(entry).read_text(encoding="utf-8-sig"))


def native_arguments(command):
    count = ctypes.c_int()
    parse = ctypes.windll.shell32.CommandLineToArgvW
    parse.argtypes = [ctypes.c_wchar_p, ctypes.POINTER(ctypes.c_int)]
    parse.restype = ctypes.POINTER(ctypes.c_wchar_p)
    result = parse(command, ctypes.byref(count))
    require(bool(result), "cannot decode native compile command")
    try:
        return [result[index] for index in range(count.value)]
    finally:
        free = ctypes.windll.kernel32.LocalFree
        free.argtypes = [ctypes.c_void_p]
        free.restype = ctypes.c_void_p
        free(ctypes.cast(result, ctypes.c_void_p))


def fixture_command(lane, fixture):
    commands = read_json(lane["compile_commands"])
    selected = []
    for item in commands:
        path = Path(item["file"])
        if not path.is_absolute():
            path = Path(item["directory"]) / path
        if path.resolve() == fixture:
            selected.append(item)
    require(len(selected) == 1, "missing or ambiguous fixture compilation")
    entry = selected[0]
    arguments = entry.get("arguments") or native_arguments(entry["command"])
    require("-c" in arguments, "not a fixture compilation")
    require(not any("coverage" in arg or "profile-arcs" in arg for arg in arguments), "instrumented fixture is outside the normal-build gate")
    return entry, arguments


def macro_arguments(arguments):
    arguments = list(arguments)
    if Path(arguments[0]).name.lower() in {"ccache", "ccache.exe"}:
        arguments.pop(0)
    require(bool(arguments) and Path(arguments[0]).name.lower() in {"g++", "g++.exe"}, "macro command must use the actual GNU C++ compiler")
    require(Path(arguments[0]).resolve().parent == Path("C:/winlibs/mingw64/bin").resolve(), "wrong macro compiler directory")
    result = []
    index = 0
    while index < len(arguments):
        arg = arguments[index]
        require("@" not in arg, "response files are not allowed in the macro recipe")
        require(not arg.startswith(("-save-temps", "--save-temps", "-fdump", "-fprofile", "-ftest-coverage", "-fplugin", "-Wp,", "-MJ", "-aux", "--serialize-diagnostics")), "side-effecting compiler flag in macro recipe")
        require(arg not in {"-E", "-S", "-M", "-MM", "--version", "-dumpmachine", "-dumpfullversion"}, "not a normal compiler recipe")
        if arg == "-c":
            index += 1
            continue
        if arg in {"-o", "-MF", "-MT", "-MQ"}:
            require(index + 1 < len(arguments), "incomplete compile option")
            index += 2
            continue
        if arg in {"-MD", "-MMD", "-MP"}:
            index += 1
            continue
        require(not arg.startswith(("-o", "-MF", "-MT", "-MQ")), "unsupported joined output option")
        result.append(arg)
        index += 1
    return result + ["-dM", "-E", "-o", "-"]


def pe_imports(path):
    data = path.read_bytes()
    require(data[:2] == b"MZ" and len(data) >= 64, "not a native PE: " + str(path))
    pe = struct.unpack_from("<I", data, 60)[0]
    require(data[pe:pe + 4] == b"PE\0\0", "invalid PE signature")
    machine, sections = struct.unpack_from("<HH", data, pe + 4)
    require(machine == 0x8664, "not an AMD64 PE")
    optional_bytes = struct.unpack_from("<H", data, pe + 20)[0]
    optional = pe + 24
    require(struct.unpack_from("<H", data, optional)[0] == 0x20b, "not PE32+")
    ranges = []
    for index in range(sections):
        address = optional + optional_bytes + 40 * index
        virtual_size, virtual, raw_size, raw = struct.unpack_from("<IIII", data, address + 8)
        ranges.append((virtual, max(virtual_size, raw_size), raw))

    def offset(rva):
        matches = [raw + rva - virtual for virtual, size, raw in ranges if virtual <= rva < virtual + size]
        require(len(matches) == 1 and matches[0] < len(data), "ambiguous PE RVA")
        return matches[0]

    imports_rva = struct.unpack_from("<I", data, optional + 120)[0]
    imports = []
    if imports_rva:
        position = offset(imports_rva)
        while True:
            descriptor = struct.unpack_from("<IIIII", data, position)
            if not any(descriptor):
                break
            start = offset(descriptor[3])
            end = data.find(b"\0", start)
            require(end >= start, "unterminated PE DLL name")
            imports.append(data[start:end].decode("ascii").lower())
            position += 20
    return imports


def junit_case(path, name):
    cases = [case for case in ET.parse(path).getroot().iter("testcase") if case.get("name") == name]
    require(len(cases) == 1, "missing or ambiguous actual JUnit testcase")
    case = cases[0]
    require(case.get("status") not in {"notrun", "disabled", "skipped"}, "testcase did not execute")
    require(not any(case.find(tag) is not None for tag in ("failure", "error", "skipped")), "testcase failed or skipped")
    return "\n".join("".join(node.itertext()) for node in case.findall("system-out"))


def last_test_case(text, name):
    starts = list(re.finditer(r"^\d+/\d+ Testing: (.+)\r?$", text, re.M))
    selected = []
    for index, match in enumerate(starts):
        if match.group(1).strip() == name:
            selected.append(text[match.end():starts[index + 1].start() if index + 1 < len(starts) else len(text)])
    require(len(selected) == 1, "missing or ambiguous LastTest case")
    require(re.search(r"^Test Passed\.\r?$", selected[0], re.M), "LastTest does not record a pass")
    require("<end of output>" in selected[0], "LastTest output is truncated")
    return selected[0]


def evaluate(lane):
    require(sys.platform == "win32" and os.name == "nt", "gate must execute on native Windows")
    require(lane.get("schema") == 1 and lane["frozen_main"] == MAIN, "wrong lane schema or frozen main")
    baseline = lane["pr"] == "main"
    head, tree, relative, blob, source_sha, name = PINS[lane["pr"]]
    require(lane["product_commit"] == head and lane["product_tree"] == tree, "not the exact standalone topic")
    for key in ("APS5_NO_WRITE_WATCH", "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH"):
        require(key not in os.environ, "unexpected runtime override, including an empty environment value: " + key)
    source = Path(lane["source_root"]).resolve()
    build = Path(lane["build_root"]).resolve()
    before = read_json(lane["source_before"])
    after = read_json(lane["source_after"])
    require(before == after and before["head"] == head and before["tree"] == tree, "source verification changed or has wrong product")
    toolchain = read_json(lane["toolchain"])
    compiler = toolchain["tools"]["g++"]
    require(Path(compiler["path"]).resolve().parent == Path("C:/winlibs/mingw64/bin").resolve(), "wrong native compiler directory")
    require("15.2.0" in compiler["version"] and "ucrt-posix-seh" in compiler["version"] and "r7" in compiler["version"], "wrong native compiler identity")
    require("Windows" in toolchain["Windows"], "missing native Windows version")
    checked_file(compiler)
    rows = [entry for entry in before["tracked_files"] if entry["path"] == relative]
    require(len(rows) == 1 and rows[0]["git_blob"] == blob, "fixture source proof is absent or wrong")
    fixture = (source / relative).resolve()
    actual = fixture.read_bytes()
    require(digest(actual) == rows[0]["sha256"], "fixture differs from source inventory")
    require(digest(actual) == source_sha or rows[0].get("checkout_CRLF_to_LF") and digest(actual.replace(b"\r\n", b"\n")) == source_sha, "fixture source differs from exact reviewed bytes")
    entry, arguments = fixture_command(lane, fixture)
    macros = read_json(lane["macro_receipt"])
    require(macros["compile_entry_sha256"] == digest(json.dumps(entry, sort_keys=True).encode()), "macro receipt is not tied to actual fixture compile command")
    require(macros["command"] == macro_arguments(arguments) and macros["cwd"] == entry["directory"] and macros["exit_code"] == 0, "wrong or failed exact-flags preprocessing")
    require(Path(macros["command"][0]).resolve() == Path(compiler["path"]).resolve(), "macro command and pinned compiler differ")
    macro_output = checked_file(macros["stdout"]).read_text(encoding="utf-8-sig")
    require(re.search(r"^#define _WIN32(?:\s+1)?\s*$", macro_output, re.M), "actual fixture compile flags do not define _WIN32")
    require(re.search(r"^#define __x86_64__ 1\s*$", macro_output, re.M), "actual compile flags are not x86-64")
    artifacts = read_json(lane["artifact_manifest"])
    executable = checked_file(lane["executable"])
    require(executable.parent == build / "tests", "executable outside normal build/tests")
    require(executable.name.lower() == name + "_tests.exe", "wrong exact fixture executable")
    imports = pe_imports(executable)
    products = [executable]
    provider_names = set()
    graph = {executable.name.lower(): imports}
    for provider in lane["providers"]:
        path = checked_file(provider)
        require(path.parent == build / "core/libs/libs/unpatched" and path.suffix.lower() == ".prx", "provider is not a normal unpatched build module")
        require(path.name.lower() not in graph, "duplicate provider identity")
        graph[path.name.lower()] = pe_imports(path)
        products.append(path)
        provider_names.add(path.name.lower())
    require({"libsceicu.prx", "libc.prx", "libkernel.prx"} <= provider_names, "missing native provider artifacts")
    pending = [executable.name.lower()]
    reachable = set()
    while pending:
        node = pending.pop()
        if node in reachable:
            continue
        reachable.add(node)
        pending.extend(name for name in graph.get(node, []) if name in graph)
    required_reachable = {"libsceicu.prx", "libc.prx"} if baseline else {"libsceicu.prx", "libc.prx", "libkernel.prx"}
    require(required_reachable <= reachable, "required normal providers are not reachable through actual PE imports")
    runtime_names = {"libstdc++-6.dll", "libgcc_s_seh-1.dll", "libwinpthread-1.dll"}
    compiler_runtimes = {Path(item["path"]).name.lower(): item["sha256"] for item in toolchain["compiler_runtime_DLLs"]}
    runtime_copies = {}
    for item in lane["runtime_dlls"]:
        path = checked_file(item)
        require(path.parent == build / "tests" and path.name.lower() in runtime_names, "wrong runtime DLL location or name")
        require(path.name.lower() not in runtime_copies and item["sha256"] == compiler_runtimes[path.name.lower()], "runtime DLL is duplicated or differs from WinLibs")
        runtime_copies[path.name.lower()] = item["sha256"]
        products.append(path)
    require(set(runtime_copies) == runtime_names, "normal fixture runtime DLL copies missing")
    for product in products:
        require(sum(Path(item["path"]).resolve() == product and item["sha256"] == record(product)["sha256"] for item in artifacts) == 1, "product is not uniquely pinned by normal artifact manifest")
    require(not any(path.suffix.lower() == ".prx" for path in (build / "tests").iterdir()), "PRX shadow staging exists in build/tests")
    registration = read_json(lane["postbuild_registration"])
    native_commands = {}
    functional_imports = {}
    for test in sorted(FUNCTIONAL[lane["pr"]]):
        matches = [item for item in registration["tests"] if item["name"] == test]
        require(len(matches) == 1, "missing or ambiguous postbuild functional registration: " + test)
        registered = matches[0]
        command = registered.get("command", [])
        expected_executable = build / "tests" / (test + "_tests.exe")
        require(len(command) == 1 and Path(command[0]).is_absolute() and Path(command[0]).resolve() == expected_executable, "postbuild registration has wrong absolute native command: " + test)
        properties = {item["name"]: item["value"] for item in registered.get("properties", [])}
        path_rules = [item for item in properties.get("ENVIRONMENT_MODIFICATION", []) if item.partition("=")[0].upper() == "PATH"]
        require(bool(path_rules) and all(rule.startswith("PATH=path_list_prepend:") and Path(rule.split(":", 1)[1]).resolve() == build / "core/libs/libs/unpatched" for rule in path_rules), "postbuild registration lacks exact normal provider PATH prepend: " + test)
        for item in properties.get("ENVIRONMENT", []):
            require(item.partition("=")[0].upper() not in {"PATH", "APS5_NO_WRITE_WATCH", "LD_PRELOAD", "LD_LIBRARY_PATH", "DYLD_INSERT_LIBRARIES", "DYLD_LIBRARY_PATH"}, "postbuild registration overrides runtime binding: " + test)
        functional_imports[test] = pe_imports(expected_executable)
        executable_record = record(expected_executable)
        require(sum(Path(item["path"]).resolve() == expected_executable and item["sha256"] == executable_record["sha256"] for item in artifacts) == 1, "functional executable is absent from normal artifact manifest: " + test)
        native_commands[test] = str(expected_executable)
    require(len(lane["runs"]) == 1, "expected one actual normal functional run")
    header_lane = {**lane, "runs": [lane["header_run"]]}
    header_outcomes = validate_runs(header_lane, HEADERS)
    outcomes = validate_runs(lane, FUNCTIONAL[lane["pr"]], name, executable, native_commands)
    require(all(Path(run["junit"]["path"]).resolve() != Path(lane["header_run"]["junit"]["path"]).resolve() for run in lane["runs"]), "header and functional JUnit must be separate actual records")
    return {"status": "passed", "pr": lane["pr"], "product_commit": head, "product_tree": tree, "frozen_main": MAIN,
        "fixture": record(fixture), "executable": record(executable), "native_PE_imports": imports, "provider_import_graph": graph,
        "postbuild_functional_commands": native_commands, "functional_PE_imports": functional_imports,
        "runs": outcomes, "header_run": header_outcomes, "HostWrite_capability_credit": not baseline,
        "basis": "Separate actual65header run and one actual functional record; exact unmodified fixture, normal native build/provider artifacts and _WIN32 preprocessing;64also requires its real write-watch completion marker. Bare main has only its two existing ICU cases and no HostWrite capability credit.",
        "provider_binding_scope": "Resolved-path inference from source/normal PE transitive imports, absolute CTest executable path, pinned unpatched providers, build/tests WinLibs runtime copies and clean runtime override environment; not a direct loaded-DLL trace.",
        "limits": "This gate is an evidence consistency check, not independent artifact attestation. It must be a required future harness stage using actual receipts. It does not validate synthetic evidence, full project CI, platform portability, backend subcases outside these exact fixtures or arbitrary loaded-module provenance."}


def validate_runs(lane, expected, name=None, executable=None, native_commands=None):
    require(bool(lane["runs"]), "no actual CTest runs")
    outcomes = []
    seen = set()
    for run in lane["runs"]:
        receipt = read_json(run["ctest_receipt"])
        require(receipt["exit_code"] == 0 and "-V" in receipt["command"], "CTest did not complete verbosely")
        require(checked_file(receipt["log"]) == checked_file(run["verbose_ctest_log"]), "CTest log does not belong to its command receipt")
        junit = checked_file(run["junit"])
        require(junit not in seen, "same JUnit cannot count as another repeat")
        seen.add(junit)
        actual_names = [case.get("name") for case in ET.parse(junit).getroot().iter("testcase")]
        require(len(actual_names) == len(expected) and set(actual_names) == expected, "actual JUnit does not contain exactly this run's test set")
        outputs = {test: junit_case(junit, test) for test in sorted(expected)}
        verbose = checked_file(run["verbose_ctest_log"]).read_text(encoding="utf-8-sig")
        last_text = checked_file(run["last_test_log"]).read_text(encoding="utf-8-sig")
        for test in sorted(expected):
            actual_block = last_test_case(last_text, test)
            if native_commands is not None:
                actual_command = re.findall(r'^Command: "([^"\r\n]+)"\s*$', actual_block, re.M)
                require(len(actual_command) == 1 and Path(actual_command[0]).is_absolute() and Path(actual_command[0]).resolve() == Path(native_commands[test]), "functional LastTest command differs from native postbuild registration: " + test)
            require(re.search(r"\b" + re.escape(test) + r"\s+\.{2,}\s+Passed", verbose), "verbose CTest pass is missing: " + test)
        if name is None:
            outcomes.append({"required_passed": sorted(expected), "junit": record(junit), "HostWrite_capability_credit": False})
            continue
        output = outputs[name]
        block = last_test_case(last_text, name)
        command = re.findall(r'^Command: "([^"\r\n]+)"\s*$', block, re.M)
        require(len(command) == 1 and Path(command[0]).resolve() == executable, "LastTest ran a different executable")
        require(re.search(r"\b" + re.escape(name) + r"\s+\.{2,}\s+Passed", verbose), "verbose CTest pass is missing")
        if lane["pr"] == 10964:
            for label, text in [("JUnit", output), ("LastTest", block), ("verbose CTest", verbose)]:
                require(text.count(VERIFIED) == 1 and UNAVAILABLE not in text, label + " lacks one unambiguous real write-watch completion marker")
        outcomes.append({"test": name, "required_passed": sorted(expected), "junit": record(junit), "capability_marker_required": lane["pr"] == 10964})
    return outcomes


def main():
    parser = argparse.ArgumentParser(allow_abbrev=False)
    parser.add_argument("--lane", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--capture-macros", action="store_true")
    args = parser.parse_args()
    lane = json.loads(Path(args.lane).read_text(encoding="utf-8-sig"))
    require(sys.platform == "win32", "native Windows required")
    out = Path(args.out).resolve()
    out.parent.mkdir(parents=True, exist_ok=True)
    if args.capture_macros:
        fixture = (Path(lane["source_root"]) / PINS[lane["pr"]][2]).resolve()
        entry, arguments = fixture_command(lane, fixture)
        command = macro_arguments(arguments)
        result = subprocess.run(command, cwd=entry["directory"], capture_output=True)
        stdout = out.with_suffix(".macros.txt")
        stderr = out.with_suffix(".stderr.txt")
        stdout.write_bytes(result.stdout)
        stderr.write_bytes(result.stderr)
        value = {"compile_entry_sha256": digest(json.dumps(entry, sort_keys=True).encode()), "command": command,
            "cwd": entry["directory"], "exit_code": result.returncode, "stdout": record(stdout), "stderr": record(stderr)}
        out.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")
        require(result.returncode == 0, "exact-flags preprocessing failed")
        return
    value = evaluate(lane)
    value["lane_manifest"] = record(args.lane)
    value["gate_script"] = record(__file__)
    out.write_text(json.dumps(value, indent=2) + "\n", encoding="utf-8")


if __name__ == "__main__":
    main()
