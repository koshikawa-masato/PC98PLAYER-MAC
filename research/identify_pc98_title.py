#!/usr/bin/env python3
"""Identify a game folder by file hashes, or look up an archive filename."""
import argparse
import hashlib
import json
from pathlib import Path
import unicodedata


def normalized(value):
    return " ".join(unicodedata.normalize("NFKC", value).casefold().split())


def identify_folder(folder, editions):
    hashes = {}
    matches = []
    for edition in editions:
        for required in edition["required_files"]:
            path = folder / required["path"]
            if not path.is_file() or path.stat().st_size != required["size"]:
                break
            if path not in hashes:
                with path.open("rb") as source:
                    digest = hashlib.sha256()
                    for block in iter(lambda: source.read(65536), b""):
                        digest.update(block)
                hashes[path] = digest.hexdigest()
            if hashes[path] != required["sha256"]:
                break
        else:
            matches.append(edition)
    return matches


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    query = parser.add_mutually_exclusive_group(required=True)
    query.add_argument("--folder", type=Path)
    query.add_argument("--archive", help="ZIP file name")
    args = parser.parse_args()

    db = json.loads(Path(__file__).with_name("pc9800cg-ratings.json").read_text())
    titles = db["titles"]
    if args.folder is not None:
        if not args.folder.is_dir():
            parser.error(f"folder not found: {args.folder}")
        matches = [
            {"title_id": edition["title_id"], "edition_id": edition["id"],
             "platform": edition["platform"]}
            for edition in identify_folder(args.folder, db["editions"])
        ]
        status = "verified"
    else:
        name = Path(args.archive).name
        if name.lower().endswith(".zip"):
            name = name[:-4]
        matches = [
            {"title_id": title_id}
            for title_id, title in titles.items()
            if any(normalized(name) == normalized(alias)
                   for alias in [title["display_name"], *title["aliases"]])
        ]
        status = "candidate"

    for match in matches:
        match["display_name"] = titles[match["title_id"]]["display_name"]
    if len(matches) != 1:
        status = "ambiguous" if matches else "unknown"
    print(json.dumps({"status": status, "matches": matches}, ensure_ascii=False))


if __name__ == "__main__":
    main()
