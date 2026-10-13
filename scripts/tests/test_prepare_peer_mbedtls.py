"""Offline checks for the pinned download and extraction boundary."""

import contextlib
import hashlib
import io
from pathlib import Path
import sys
import tarfile
import tempfile
import unittest
from unittest import mock

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

import native_peer_mbedtls
import prepare_peer_mbedtls as prepare


def archive_bytes(extra=None):
    result = io.BytesIO()
    with tarfile.open(fileobj=result, mode="w:gz") as archive:
        # Exercise the production auditor, including its minimum inventory.
        for index in range(100):
            part = "include" if index < 50 else "library"
            data = f"fixture {index}\n".encode()
            member = tarfile.TarInfo(f"upstream/{part}/file{index}.h")
            member.size = len(data)
            archive.addfile(member, io.BytesIO(data))
        if extra is not None:
            archive.addfile(extra, io.BytesIO(b"bad") if extra.isfile() else None)
    return result.getvalue()


@contextlib.contextmanager
def pinned_fixture(data):
    digest = hashlib.sha256(data).hexdigest()
    with mock.patch.object(prepare, "ARCHIVE_SHA256", digest), \
            mock.patch.object(native_peer_mbedtls, "ARCHIVE_SHA256", digest):
        yield


class PreparePeerMbedTlsTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory(prefix="larp-prepare-test-")
        self.addCleanup(self.temporary.cleanup)
        self.destination = Path(self.temporary.name) / "inputs"
        self.destination.mkdir()

    def store_archive(self, data):
        archive = self.destination / (prepare.SOURCE_NAME + ".tar.gz")
        archive.write_bytes(data)
        return archive

    def test_download_is_audited_and_reused_without_network(self):
        data = archive_bytes()
        with pinned_fixture(data), mock.patch.object(prepare, "urlopen", return_value=io.BytesIO(data)) as network:
            root, archive, count = prepare.prepare(self.destination)
            self.assertEqual(count, 100)
            self.assertEqual(archive.read_bytes(), data)
            self.assertEqual((root / "include/file0.h").read_bytes(), b"fixture 0\n")
            self.assertEqual(prepare.prepare(self.destination), (root, archive, count))
            network.assert_called_once_with(prepare.ARCHIVE_URL, timeout=60)

    def test_changed_download_never_becomes_an_accepted_archive(self):
        with mock.patch.object(prepare, "urlopen", return_value=io.BytesIO(b"wrong archive")):
            with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                prepare.prepare(self.destination)
        self.assertEqual(list(self.destination.iterdir()), [])

    def test_existing_wrong_hash_fails_before_extraction_and_preserves_file(self):
        archive = self.store_archive(b"wrong archive")
        with mock.patch.object(prepare.tarfile, "open") as extraction, mock.patch.object(prepare, "urlopen") as network:
            with self.assertRaisesRegex(RuntimeError, "SHA-256 mismatch"):
                prepare.prepare(self.destination)
            extraction.assert_not_called()
            network.assert_not_called()
        self.assertEqual(archive.read_bytes(), b"wrong archive")

    def test_malformed_archive_leaves_no_partial_source_tree(self):
        data = b"not a tar archive"
        archive = self.store_archive(data)
        with pinned_fixture(data), self.assertRaises(tarfile.TarError):
            prepare.prepare(self.destination)
        self.assertEqual(list(self.destination.iterdir()), [archive])

    def test_traversal_is_rejected_without_writing_outside_source_tree(self):
        member = tarfile.TarInfo("upstream/include/../../escape.h")
        member.size = 3
        data = archive_bytes(member)
        archive = self.store_archive(data)
        sentinel = self.destination / "keep.txt"
        sentinel.write_text("foreign file", encoding="utf-8")
        with pinned_fixture(data), self.assertRaisesRegex(RuntimeError, "Unsafe archive path"):
            prepare.prepare(self.destination)
        self.assertEqual(set(self.destination.iterdir()), {archive, sentinel})
        self.assertEqual(sentinel.read_text(encoding="utf-8"), "foreign file")

    def test_source_link_is_rejected(self):
        member = tarfile.TarInfo("upstream/include/link.h")
        member.type = tarfile.SYMTYPE
        member.linkname = "../../outside.h"
        data = archive_bytes(member)
        archive = self.store_archive(data)
        with pinned_fixture(data), self.assertRaisesRegex(RuntimeError, "link or special"):
            prepare.prepare(self.destination)
        self.assertEqual(list(self.destination.iterdir()), [archive])

    def test_changed_existing_source_is_not_replaced(self):
        data = archive_bytes()
        self.store_archive(data)
        with pinned_fixture(data):
            root, _, _ = prepare.prepare(self.destination)
            changed = root / "include/file0.h"
            changed.write_bytes(b"local change")
            with self.assertRaisesRegex(RuntimeError, "source differs"):
                prepare.prepare(self.destination)
            self.assertEqual(changed.read_bytes(), b"local change")

    def test_environment_export_preserves_existing_entries(self):
        environment = self.destination / "github-env"
        environment.write_text("EXISTING=value\n", encoding="utf-8")
        root = self.destination / prepare.SOURCE_NAME
        archive = self.destination / (prepare.SOURCE_NAME + ".tar.gz")
        prepare.export_environment(environment, root, archive)
        self.assertEqual(environment.read_text(encoding="utf-8"),
                         f"EXISTING=value\nLARP_PEER_MBEDTLS_DIR={root}\nLARP_PEER_MBEDTLS_ARCHIVE={archive}\n")

    def test_failure_does_not_export_environment(self):
        self.store_archive(b"wrong archive")
        environment = self.destination / "github-env"
        arguments = ["prepare_peer_mbedtls.py", "--destination", str(self.destination),
                     "--github-env", str(environment)]
        with mock.patch.object(sys, "argv", arguments), contextlib.redirect_stderr(io.StringIO()):
            self.assertEqual(prepare.main(), 1)
        self.assertFalse(environment.exists())


if __name__ == "__main__":
    unittest.main()
