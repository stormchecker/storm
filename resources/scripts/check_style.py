#!/usr/bin/env python3

"""
Check several coding-style conventions from doc/developers.md over every .cpp/.h file under src/, in one script:

  - doxygen-style:    doc comments must use '/*!'
  - include-order:    Order of includes is correct:
                      - own header first (bare filename; test files use the "storm-config.h" + "test/storm_gtest.h" pair instead),
                      - then external (<...>) includes
                      - then additional ("...") includes with full path.
  - pragma-once:      headers must start with '#pragma once'.
  - raw-throw:        exceptions must be thrown via STORM_LOG_THROW, not a raw 'throw'.
  - stdout-usage:     std::cout must not be used outside CLI code.

Use --checks to run a subset (comma-separated names above, or 'all', the default).
Exits with 1 if any selected check fails.

Per-file exceptions are listed in .check-style-ignore.
"""

import argparse
import os
import re
import sys
from abc import ABC, abstractmethod
from collections import defaultdict

# Regex for a .h file's leading pragma
PRAGMA_ONCE_RE = re.compile(r"^\s*#\s*pragma\s+once\b")


# --------------------------------------------------------------------------
# Shared helpers
# --------------------------------------------------------------------------


def strip_comments(lines):
    """Return a parallel list of lines with comment content blanked out
    (preserving line count/positions), so regexes never match inside a
    comment. Block comments spanning multiple lines are handled."""
    out = []
    in_block = False
    for line in lines:
        if in_block:
            end = line.find("*/")
            if end == -1:
                out.append("")
                continue
            in_block = False
            line = " " * (end + 2) + line[end + 2 :]
        result = []
        i = 0
        n = len(line)
        while i < n:
            if line[i : i + 2] == "//":
                break
            if line[i : i + 2] == "/*":
                end = line.find("*/", i + 2)
                if end == -1:
                    in_block = True
                    i = n
                    break
                result.append(" " * (end + 2 - i))
                i = end + 2
                continue
            result.append(line[i])
            i += 1
        out.append("".join(result))
    return out


# --------------------------------------------------------------------------
# Abstract check + shared driver
# --------------------------------------------------------------------------


class Check(ABC):
    """One coding-style check. Subclasses implement check_file() (inspect one
    already-read file, recording findings on self) and report() (print the
    findings, return True if any violation was found)."""

    name = None  # must be set by subclasses; matches a --checks name

    def __init__(self, src_dir, repo_root, ignore_sections):
        self.src_dir = src_dir
        self.repo_root = repo_root
        self.ignore_set = ignore_sections.get(self.name, set())
        self.skipped = 0

    def should_skip(self, path):
        """Default: skip files listed in .check-style-ignore under this
        check's own section. Subclasses with a structural exemption (e.g.
        "CLI code is always exempt") extend this rather than replacing it,
        so an ignore-file entry still works even for check-only checks."""
        return (
            os.path.relpath(os.path.abspath(path), self.repo_root).replace(os.sep, "/")
            in self.ignore_set
        )

    @abstractmethod
    def check_file(self, path, lines):
        """Inspect one already-read file (lines: list[str])."""
        raise NotImplementedError

    @abstractmethod
    def report(self):
        """Print this check's findings; return (error_count, skipped_count)."""
        raise NotImplementedError


# --------------------------------------------------------------------------
# doxygen-style check
# --------------------------------------------------------------------------


class DoxygenStyleCheck(Check):
    name = "doxygen-style"

    JAVADOC_OPEN_RE = re.compile(r"^\s*/\*\*(?!\*)")
    TRIPLE_SLASH_RE = re.compile(r"^\s*///(?!/)\s*\S")

    def __init__(self, src_dir, repo_root, ignore_sections):
        super().__init__(src_dir, repo_root, ignore_sections)
        self.by_file = {}
        self.total_javadoc = 0
        self.total_triple_slash = 0

    def check_file(self, path, lines):
        javadoc = sum(1 for line in lines if self.JAVADOC_OPEN_RE.match(line))
        triple_slash = sum(1 for line in lines if self.TRIPLE_SLASH_RE.match(line))

        if javadoc or triple_slash:
            self.by_file[path] = (javadoc, triple_slash)
            self.total_javadoc += javadoc
            self.total_triple_slash += triple_slash

    def report(self):
        print("\n=== doxygen-style ===")
        if self.by_file:
            total = self.total_javadoc + self.total_triple_slash
            print(
                "\n[wrong_doc_style] {} file(s), {} occurrence(s): use '/*!' instead of '/**' or '///'".format(
                    len(self.by_file), total
                )
            )
            for path in sorted(self.by_file):
                javadoc, triple_slash = self.by_file[path]
                parts = []
                if javadoc:
                    parts.append("{} '/**' block(s)".format(javadoc))
                if triple_slash:
                    parts.append("{} '///' line(s)".format(triple_slash))
                print("  {}: {}".format(path, ", ".join(parts)))

        return self.total_javadoc + self.total_triple_slash, self.skipped


# --------------------------------------------------------------------------
# include-order check
# --------------------------------------------------------------------------


class IncludeViolation:
    def __init__(self, path, basename, issues=None, details=None):
        self.path = path
        self.basename = basename
        self.issues = issues or []
        self.details = details or {}


class IncludeOrderCheck(Check):
    name = "include-order"

    INCLUDE_RE = re.compile(r'^\s*#\s*include\s*([<"])([^">]+)[>"]')
    COND_START_RE = re.compile(r"^\s*#\s*(if|ifdef|ifndef)\b")
    COND_END_RE = re.compile(r"^\s*#\s*endif\b")
    PRAGMA_RE = re.compile(r"^\s*#\s*pragma\b")

    ISSUE_LABELS = {
        "comment_only_file": "file is empty, or contains only blank lines/comments -- no real code to anchor the include order",
        "missing": "own header not included",
        "not_first": "own header include is not the first include",
        "wrong_form": 'own header not included as bare "Name.h" (quoted, no directory)',
        "no_blank_after": "no blank line after the leading own-header/test-helper include(s)",
        "duplicate": "own header included more than once",
        "leading_blank": "file starts with blank line(s) before its content",
        "group_order": 'a system (<...>) include appears after a storm ("...") include has started',
        "missing_group_separator": "no blank line between the system includes and the storm includes",
        "blank_within_group": "a stray blank line splits a system or storm include block into extra sub-groups",
        "bare_include": "quoted include uses a bare filename but is not the file's own header (needs full path)",
        "include_in_conditional": "own header include appears inside a conditional compilation block",
        "comment_in_includes": "a comment is interspersed among the includes",
    }

    def __init__(self, src_dir, repo_root, ignore_sections):
        super().__init__(src_dir, repo_root, ignore_sections)
        self.compliant = 0
        self.by_issue = defaultdict(list)
        self.bare_details = {}

    # -- per-file helpers --------------------------------------------------

    def compute_preamble_end(self, lines):
        """Index of the first line that is not a leading blank line, comment, or '#pragma once'"""
        i, n, in_block = 0, len(lines), False
        while i < n:
            s = lines[i].strip()
            if in_block:
                if "*/" in s:
                    in_block = False
                i += 1
                continue
            if s == "" or s.startswith("//") or PRAGMA_ONCE_RE.match(s):
                i += 1
                continue
            if s.startswith("/*"):
                if "*/" not in s[2:]:
                    in_block = True
                i += 1
                continue
            break
        return i

    def scan_region_entries(self, lines, start):
        """Scan `lines` from index `start`, classifying each line as a blank,
        an include ("SYSTEM"/"STORM"), or "other" (comment / block-comment
        continuation / #pragma, e.g. diagnostic push/pop wrapping one
        include). Stops at the first line that is real code. Returns a list
        of (kind, absolute_index) entries, with any trailing blank/"other"
        entries after the last SYSTEM/STORM entry already dropped."""
        i, n, in_block = start, len(lines), False
        entries = []
        while i < n:
            raw = lines[i]
            s = raw.strip()
            if in_block:
                entries.append(("other", i))
                if "*/" in s:
                    in_block = False
                i += 1
                continue
            if s == "":
                entries.append(("blank", i))
                i += 1
                continue
            if s.startswith("//") or self.PRAGMA_RE.match(s):
                entries.append(("other", i))
                i += 1
                continue
            if s.startswith("/*"):
                entries.append(("other", i))
                if "*/" not in s[2:]:
                    in_block = True
                i += 1
                continue
            m = self.INCLUDE_RE.match(raw)
            if m:
                entries.append(("SYSTEM" if m.group(1) == "<" else "STORM", i))
                i += 1
                continue
            break

        # Drop trailing blank/comment entries after the last SYSTEM/STORM entry
        real = [idx for kind, idx in entries if kind in ("SYSTEM", "STORM")]
        if not real:
            return []
        last = max(real)
        return [(kind, idx) for kind, idx in entries if idx <= last]

    def analyze_include_order(self, cpp_path, lines, header_path):
        """Return None if compliant, else an IncludeViolation. `header_path`
        may be None for files without a same-named sibling header (own-header
        rules are skipped; a test-helper leading pair is still recognized if
        present) -- including all .h files, which never have one."""
        preamble_end = self.compute_preamble_end(lines)

        # comment_only_file: no real code follows the leading blanks/comments
        if preamble_end >= len(lines):
            return IncludeViolation(cpp_path, None, issues=["comment_only_file"])

        issues = set()
        # leading_blank: the file starts with blank line(s) before its content.
        if lines[0].strip() == "":
            issues.add("leading_blank")

        basename = os.path.basename(header_path) if header_path else None

        own_idx = []
        if header_path:
            rel_path = os.path.relpath(header_path, self.src_dir).replace(os.sep, "/")
            for idx, line in enumerate(lines):
                m = self.INCLUDE_RE.match(line)
                if m:
                    path = m.group(2).strip()
                    if path == basename or path == rel_path:
                        own_idx.append(idx)

        # Conditional-nesting depth in effect *at* each line index.
        depths = []
        depth = 0
        for line in lines:
            s = line.strip()
            depths.append(depth)
            if self.COND_START_RE.match(s):
                depth += 1
            elif self.COND_END_RE.match(s):
                depth = max(0, depth - 1)

        # include_in_conditional: an own-header include exists but sits
        # inside a conditional compilation block.
        if any(depths[i] > 0 for i in own_idx):
            return IncludeViolation(
                cpp_path, basename, issues=["include_in_conditional"]
            )

        leading_end = None
        if header_path is not None:
            # missing: a .cpp file has a sibling header but never includes it.
            if not own_idx:
                issues.add("missing")
            else:
                # duplicate: the own header is included more than once.
                if len(own_idx) > 1:
                    issues.add("duplicate")
                # not_first: the own header isn't the very first include.
                first = own_idx[0]
                if first != preamble_end:
                    issues.add("not_first")
                # wrong_form: not a bare quoted "Name.h" include.
                m = self.INCLUDE_RE.match(lines[first])
                quote, path = m.group(1), m.group(2)
                if quote != '"' or path != basename:
                    issues.add("wrong_form")
                leading_end = first + 1

        else:
            # Files without their own header (e.g. .h files, or .cpp files with no sibling header) may still start with the test-helper pair.
            if preamble_end + 1 < len(lines):
                # Check for test_pair #include "storm-config.h" immediately followed by #include "test/storm_gtest.h".
                ma = self.INCLUDE_RE.match(lines[preamble_end])
                mb = self.INCLUDE_RE.match(lines[preamble_end + 1])
                if (
                    ma
                    and mb
                    and ma.group(1) == '"'
                    and mb.group(1) == '"'
                    and ma.group(2).strip() == "storm-config.h"
                    and mb.group(2).strip() == "test/storm_gtest.h"
                ):
                    leading_end = preamble_end + 2

        region_start = preamble_end
        if leading_end is not None:
            nxt = leading_end
            # no_blank_after: no blank line separates the leading own-header/ test-helper include(s) from the include groups that follow.
            if nxt >= len(lines) or lines[nxt].strip() != "":
                issues.add("no_blank_after")
                region_start = nxt
            else:
                # One or more blank lines
                region_start = nxt
                while region_start < len(lines) and lines[region_start].strip() == "":
                    region_start += 1

        truncated = self.scan_region_entries(lines, region_start)

        # comment_in_includes: a comment is interspersed among the includes.
        # Reported alongside whatever else is found below, but the "other"
        # entries themselves are dropped first so they can't corrupt the
        # group-order state machine, which only knows about "blank"/"SYSTEM"/
        # "STORM".
        if any(kind == "other" for kind, _idx in truncated):
            issues.add("comment_in_includes")
            truncated = [(kind, idx) for kind, idx in truncated if kind != "other"]

        # group_order / missing_group_separator / blank_within_group: checked
        # against the system/storm include groups.
        current_category = None
        pending_blanks = 0
        for kind, _idx in truncated:
            if kind == "blank":
                pending_blanks += 1
                continue
            if current_category is None:
                current_category = kind
            elif kind == current_category:
                if pending_blanks > 0:
                    issues.add("blank_within_group")
            elif current_category == "STORM" and kind == "SYSTEM":
                issues.add("group_order")
                current_category = kind
            else:  # SYSTEM -> STORM transition
                if pending_blanks == 0:
                    issues.add("missing_group_separator")
                current_category = kind
            pending_blanks = 0

        # bare_include: a quoted include uses a bare filename. Scanned from
        # preamble_end (not region_start), since region_start is pushed past
        # the own header even when it's found late (not_first) -- an earlier
        # bare include must not be skipped just because the own header is
        # misplaced.
        bares = []
        for line in lines[preamble_end:]:
            m = self.INCLUDE_RE.match(line)
            if not m or m.group(1) != '"':
                continue
            inc = m.group(2).strip()
            if "/" in inc or inc in bares:
                continue
            if "storm-config.h" == inc:
                continue
            if basename and inc == basename:
                continue
            bares.append(inc)
        if bares:
            issues.add("bare_include")

        if not issues:
            return None
        return IncludeViolation(
            cpp_path,
            basename,
            issues=sorted(issues),
            details={"bare_include": bares},
        )

    def check_file(self, path, lines):
        header_path = None
        if path.endswith(".cpp"):
            header = os.path.splitext(path)[0] + ".h"
            header_path = header if os.path.isfile(header) else None

        violation = self.analyze_include_order(path, lines, header_path)
        if violation is None:
            self.compliant += 1
        else:
            for issue in violation.issues:
                self.by_issue[issue].append(path)
            if "bare_include" in violation.issues:
                self.bare_details[path] = violation.details.get("bare_include", [])

    def report(self):
        print("\n=== include-order ===")
        errors = 0
        for issue in self.ISSUE_LABELS:
            paths = self.by_issue.get(issue)
            if not paths:
                continue
            errors += len(paths)
            print(
                "\n[{}] {} file(s): {}".format(
                    issue, len(paths), self.ISSUE_LABELS[issue]
                )
            )
            for p in paths:
                print("  {}".format(p))
                if issue == "bare_include":
                    for bare in self.bare_details.get(p, []):
                        print('    "{}"'.format(bare))

        return errors, self.skipped


# --------------------------------------------------------------------------
# pragma-once check
# --------------------------------------------------------------------------


class PragmaOnceCheck(Check):
    name = "pragma-once"

    def __init__(self, src_dir, repo_root, ignore_sections):
        super().__init__(src_dir, repo_root, ignore_sections)
        self.missing_pragma_once = []

    def check_file(self, path, lines):
        if not path.endswith(".h"):
            return
        code_lines = strip_comments(lines)
        for i, line in enumerate(code_lines):
            if line.strip() != "":
                # First line with code should be #pragma once
                if not PRAGMA_ONCE_RE.match(lines[i]):
                    self.missing_pragma_once.append((path, i + 1))
                break

    def report(self):
        print("\n=== pragma-once ===")
        if self.missing_pragma_once:
            print(
                "\n[missing_pragma_once] {} file(s): header does not start with '#pragma once'".format(
                    len(self.missing_pragma_once)
                )
            )
            for p, line_no in self.missing_pragma_once:
                print("  {}:{}".format(p, line_no))

        return len(self.missing_pragma_once), self.skipped


# --------------------------------------------------------------------------
# raw-throw check
# --------------------------------------------------------------------------


class RawThrowCheck(Check):
    name = "raw-throw"

    THROW_RE = re.compile(r"\bthrow\s+([A-Za-z_]\w*(?:::[A-Za-z_]\w*)*)\s*[({]")

    def __init__(self, src_dir, repo_root, ignore_sections):
        super().__init__(src_dir, repo_root, ignore_sections)
        self.raw_throws = []

    def check_file(self, path, lines):
        code_lines = strip_comments(lines)
        for idx, line in enumerate(code_lines):
            for m in self.THROW_RE.finditer(line):
                self.raw_throws.append((path, idx + 1, m.group(0).strip()))

    def report(self):
        print("\n=== raw-throw ===")
        if self.raw_throws:
            print(
                "\n[raw_throw] {} occurrence(s): use STORM_LOG_THROW instead of throwing directly".format(
                    len(self.raw_throws)
                )
            )
            for p, line_no, snippet in self.raw_throws:
                print("  {}:{}: {}".format(p, line_no, snippet))

        return len(self.raw_throws), self.skipped


# --------------------------------------------------------------------------
# stdout-usage check
# --------------------------------------------------------------------------


class StdoutUsageCheck(Check):
    name = "stdout-usage"

    STD_COUT_RE = re.compile(r"\bstd::cout\b")

    def __init__(self, src_dir, repo_root, ignore_sections):
        super().__init__(src_dir, repo_root, ignore_sections)
        self.violations = []

    def check_file(self, path, lines):
        code_lines = strip_comments(lines)
        for idx, line in enumerate(code_lines):
            if self.STD_COUT_RE.search(line):
                self.violations.append((path, idx + 1, line))

    def report(self):
        print("\n=== stdout-usage ===")
        if self.violations:
            print("\n[stdout] {} occurrence(s): ".format(len(self.violations)))
            for p, line_no, line in self.violations:
                print("  {}:{}\t{}".format(p, line_no, line.strip()))

        return len(self.violations), self.skipped


# --------------------------------------------------------------------------
# main
# --------------------------------------------------------------------------

CHECK_CLASSES = {
    "doxygen-style": DoxygenStyleCheck,
    "include-order": IncludeOrderCheck,
    "pragma-once": PragmaOnceCheck,
    "raw-throw": RawThrowCheck,
    "stdout-usage": StdoutUsageCheck,
}


def load_ignore_sections(repo_root, filename):
    """Parse .check-style-ignore:
    - '[section]' headers introduce a section;
    - '#' comments and blank lines are skipped;
    - everything else is a repo-relative path belonging to the current section.
    Returns {section_name: set(paths)}."""
    sections = defaultdict(set)
    ignore_file = os.path.join(repo_root, filename)
    if not os.path.isfile(ignore_file):
        return sections
    current = None
    with open(ignore_file, "r", encoding="utf-8") as f:
        for line_no, raw_line in enumerate(f, start=1):
            line = raw_line.strip()
            if not line or line.startswith("#"):
                continue
            if line.startswith("[") and line.endswith("]"):
                current = line[1:-1].strip()
                continue
            if current is None:
                raise ValueError(
                    "{}:{}: entry outside any [section]: {!r}".format(
                        ignore_file, line_no, line
                    )
                )
            sections[current].add(line)
    return sections


def main():
    parser = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter
    )
    parser.add_argument(
        "--src-dir", default="src", help="Directory to scan (default: src)"
    )
    parser.add_argument(
        "--checks",
        default="all",
        help="Comma-separated subset of {{{}}}, or 'all' (default)".format(
            ", ".join(CHECK_CLASSES)
        ),
    )
    args = parser.parse_args()

    if not os.path.isdir(args.src_dir):
        print("error: {} is not a directory".format(args.src_dir), file=sys.stderr)
        return 2

    selected = (
        list(CHECK_CLASSES)
        if args.checks.strip() in ("all", "")
        else [c.strip() for c in args.checks.split(",")]
    )
    unknown = [c for c in selected if c not in CHECK_CLASSES]
    if unknown:
        print(
            "error: unknown check(s) {} -- must be one of {} or 'all'".format(
                unknown, list(CHECK_CLASSES)
            ),
            file=sys.stderr,
        )
        return 2

    repo_root = os.path.dirname(os.path.abspath(args.src_dir.rstrip(os.sep)))
    ignore_sections = load_ignore_sections(repo_root, ".check-style-ignore")

    checks = [
        CHECK_CLASSES[name](args.src_dir, repo_root, ignore_sections)
        for name in selected
    ]

    # Check all .cpp and .h files
    no_files = 0
    for root, _dirs, files in os.walk(args.src_dir):
        for file in sorted(files):
            if file.endswith(".cpp") or file.endswith(".h"):
                no_files += 1
                path = os.path.join(root, file)
                with open(path, "r", encoding="utf-8", errors="replace") as f:
                    lines = f.readlines()
                for check in checks:
                    if check.should_skip(path):
                        check.skipped += 1
                        continue
                    check.check_file(path, lines)

    print("Checked {} files".format(no_files))

    total_errors = 0
    total_skipped = 0
    for check in checks:
        errors, skipped = check.report()
        total_errors += errors
        total_skipped += skipped
        print("\n{} error(s) found. Skipped {} file(s).".format(errors, skipped))

    if len(checks) > 1:
        print(
            "\n=== total ===\n{} error(s) found. Skipped {} file(s).".format(
                total_errors, total_skipped
            )
        )

    return 1 if total_errors else 0


if __name__ == "__main__":
    sys.exit(main())
