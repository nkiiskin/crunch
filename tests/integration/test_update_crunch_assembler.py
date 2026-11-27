#!/usr/bin/env python3
"""Exercise safe, reproducible assembler updates using local Git repositories."""

import hashlib
import pathlib
import shutil
import subprocess
import sys
import tempfile
import unittest

SCRIPT = pathlib.Path(__file__).resolve().parents[2] / "tools/update-crunch-assembler"


def git(repo, *args):
    """Run Git in a test repository.

    Args:
        repo: Repository path.
        args: Git command arguments.
    Returns:
        Stripped standard output.
    """
    return subprocess.run(["git", "-C", str(repo), *args], check=True,
                          capture_output=True, text=True).stdout.strip()


class UpdateTests(unittest.TestCase):
    def setUp(self):
        """Create upstream and compiler repositories.

        Args:
            self: Test instance.
        Returns:
            None.
        """
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        root = pathlib.Path(self.temp.name)
        self.upstream = root / "upstream"
        self.suite = root / "suite"
        for repo in (self.upstream, self.suite):
            repo.mkdir()
            git(repo, "init", "-q")
            git(repo, "config", "user.name", "Test")
            git(repo, "config", "user.email", "test@example.invalid")
        (self.upstream / "source.c").write_text("old\n")
        git(self.upstream, "add", "source.c")
        git(self.upstream, "commit", "-qm", "old")
        old = git(self.upstream, "rev-parse", "HEAD")
        (self.suite / "vendor").mkdir()
        (self.suite / "tools").mkdir()
        shutil.copyfile(SCRIPT, self.suite / "tools/update-crunch-assembler")
        (self.suite / "source.c").write_text("old\n")
        (self.suite / "vendor/crunch-assembler.rev").write_text(old + "\n")
        digest = hashlib.sha256(b"old\n").hexdigest()
        (self.suite / "vendor/crunch-assembler.sha256").write_text(f"{digest}  source.c\n")
        git(self.suite, "add", ".")
        git(self.suite, "commit", "-qm", "baseline")
        (self.upstream / "source.c").write_text("new\n")
        git(self.upstream, "commit", "-qam", "new")
        self.new = git(self.upstream, "rev-parse", "HEAD")

    def update(self, revision):
        """Run the updater against the local upstream.

        Args:
            self: Test instance.
            revision: Requested commit.
        Returns:
            Completed process.
        """
        return subprocess.run([sys.executable, self.suite / "tools/update-crunch-assembler",
                               "--source", self.upstream, revision],
                              text=True, capture_output=True)

    def test_update_uses_committed_contents(self):
        """Verify only committed upstream data is copied.

        Args:
            self: Test instance.
        Returns:
            None.
        """
        (self.upstream / "source.c").write_text("uncommitted\n")
        result = self.update(self.new)
        self.assertEqual(result.returncode, 0, result.stderr)
        self.assertEqual((self.suite / "source.c").read_text(), "new\n")
        self.assertEqual((self.suite / "vendor/crunch-assembler.rev").read_text(),
                         self.new + "\n")
        digest = hashlib.sha256(b"new\n").hexdigest()
        self.assertEqual((self.suite / "vendor/crunch-assembler.sha256").read_text(),
                         f"{digest}  source.c\n")

    def test_refuses_modified_files_and_pin(self):
        """Verify local source and pin changes are preserved.

        Args:
            self: Test instance.
        Returns:
            None.
        """
        (self.suite / "source.c").write_text("local\n")
        self.assertNotEqual(self.update(self.new).returncode, 0)
        self.assertEqual((self.suite / "source.c").read_text(), "local\n")
        (self.suite / "source.c").write_text("old\n")
        (self.suite / "vendor/crunch-assembler.rev").write_text("local\n")
        self.assertNotEqual(self.update(self.new).returncode, 0)
        self.assertEqual((self.suite / "source.c").read_text(), "old\n")

    def test_rejects_bad_commit_or_missing_upstream_file(self):
        """Reject invalid revisions and incomplete source trees.

        Args:
            self: Test instance.
        Returns:
            None.
        """
        self.assertNotEqual(self.update("main").returncode, 0)
        git(self.upstream, "rm", "-q", "source.c")
        git(self.upstream, "commit", "-qm", "remove file")
        self.assertNotEqual(self.update(git(self.upstream, "rev-parse", "HEAD")).returncode, 0)
        self.assertEqual((self.suite / "source.c").read_text(), "old\n")


if __name__ == "__main__":
    unittest.main()
