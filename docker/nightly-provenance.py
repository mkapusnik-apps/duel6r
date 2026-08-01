#!/usr/bin/env python3
"""Create or verify the provenance sidecar for a nightly runtime archive."""

import argparse
import hashlib
import json
import pathlib
import re
import sys


FORMAT = "duel6r-nightly-provenance-v1"
GIT_SHA_RE = re.compile(r"^[0-9a-f]{40}$")
IMAGE_DIGEST_RE = re.compile(r"^sha256:[0-9a-f]{64}$")


def archive_sha256(path: pathlib.Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as archive:
        for chunk in iter(lambda: archive.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def require_git_sha(value: str, field: str) -> None:
    if not GIT_SHA_RE.fullmatch(value):
        raise ValueError(f"{field} must be a lowercase 40-character Git SHA")


def require_image(value: str, repository: str, image: str) -> None:
    prefix = f"ghcr.io/{repository}/{image}@"
    if not value.startswith(prefix) or not IMAGE_DIGEST_RE.fullmatch(value[len(prefix) :]):
        raise ValueError(f"{image} must be an exact ghcr.io/{repository}/{image}@sha256 digest reference")


def expected_document(args: argparse.Namespace) -> dict:
    archive = pathlib.Path(args.archive)
    require_git_sha(args.commit, "source.commit")
    require_git_sha(args.tree, "source.tree")
    require_image(args.linux_image, args.repository, "build")
    require_image(args.windows_image, args.repository, "build-w64")
    return {
        "format": FORMAT,
        "repository": args.repository,
        "source": {"commit": args.commit, "tree": args.tree},
        "build_images": {
            "linux": args.linux_image,
            "windows": args.windows_image,
        },
        "archive": {
            "name": archive.name,
            "sha256": archive_sha256(archive),
        },
    }


def create(args: argparse.Namespace) -> None:
    document = expected_document(args)
    output = pathlib.Path(args.output)
    output.write_text(json.dumps(document, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(f"Wrote nightly provenance to {output}")


def verify(args: argparse.Namespace) -> None:
    provenance = pathlib.Path(args.provenance)
    document = json.loads(provenance.read_text(encoding="utf-8"))
    expected = expected_document(args)
    if document != expected:
        raise ValueError("provenance content does not match the expected archive, source, or images")
    print(f"Validated nightly provenance in {provenance}")


def add_facts(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--repository", required=True)
    parser.add_argument("--commit", required=True)
    parser.add_argument("--tree", required=True)
    parser.add_argument("--linux-image", required=True)
    parser.add_argument("--windows-image", required=True)
    parser.add_argument("--archive", required=True)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    commands = parser.add_subparsers(dest="command", required=True)

    create_parser = commands.add_parser("create")
    add_facts(create_parser)
    create_parser.add_argument("--output", required=True)

    verify_parser = commands.add_parser("verify")
    add_facts(verify_parser)
    verify_parser.add_argument("--provenance", required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        if args.command == "create":
            create(args)
        else:
            verify(args)
    except (OSError, ValueError, json.JSONDecodeError) as error:
        print(f"Nightly provenance validation failed: {error}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
