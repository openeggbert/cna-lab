#!/usr/bin/env python3
"""Dependency-free structural validation for the Explore2D static website."""

from __future__ import annotations

from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import urlsplit


ROOT = Path(__file__).resolve().parent


class Document(HTMLParser):
    def __init__(self, path: Path) -> None:
        super().__init__()
        self.path = path
        self.ids: list[str] = []
        self.refs: list[tuple[str, str, str]] = []
        self.copy_targets: list[str] = []
        self.images_without_alt: list[str] = []
        self.lesson_checks: list[str] = []
        self.lesson_articles: list[str] = []
        self.has_description = False
        self.has_title = False

    def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        values = dict(attrs)
        if value := values.get("id"):
            self.ids.append(value)
        for attribute in ("href", "src"):
            if value := values.get(attribute):
                self.refs.append((tag, attribute, value))
        if value := values.get("data-copy"):
            self.copy_targets.append(value)
        if tag == "img" and not values.get("alt"):
            self.images_without_alt.append(values.get("src", "<unknown>"))
        if value := values.get("data-lesson-check"):
            self.lesson_checks.append(value)
        if value := values.get("data-lesson"):
            self.lesson_articles.append(value)
        if tag == "meta" and values.get("name") == "description" and values.get("content"):
            self.has_description = True

    def handle_startendtag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
        self.handle_starttag(tag, attrs)

def parse_documents() -> dict[Path, Document]:
    documents: dict[Path, Document] = {}
    for path in sorted(ROOT.glob("*.html")):
        document = Document(path)
        source = path.read_text(encoding="utf-8")
        document.feed(source)
        document.has_title = "<title>" in source and "</title>" in source
        documents[path.resolve()] = document
    return documents


def main() -> None:
    documents = parse_documents()
    assert len(documents) == 3, f"expected 3 HTML pages, found {len(documents)}"

    for path, document in documents.items():
        assert document.has_title, f"{path.name}: missing title"
        assert document.has_description, f"{path.name}: missing meta description"
        assert len(document.ids) == len(set(document.ids)), f"{path.name}: duplicate ids"
        assert not document.images_without_alt, f"{path.name}: images without alt: {document.images_without_alt}"
        for target in document.copy_targets:
            assert target in document.ids, f"{path.name}: copy target #{target} does not exist"

        for _, _, reference in document.refs:
            parsed = urlsplit(reference)
            if parsed.scheme or reference.startswith("//"):
                continue
            target_path = (path.parent / parsed.path).resolve() if parsed.path else path
            assert target_path.exists(), f"{path.name}: missing local target {reference}"
            if parsed.fragment and target_path.suffix == ".html":
                target_document = documents.get(target_path)
                assert target_document is not None, f"{path.name}: unparsed page {target_path.name}"
                assert parsed.fragment in target_document.ids, (
                    f"{path.name}: missing anchor {reference}"
                )

    tutorial = documents[(ROOT / "tutorial.html").resolve()]
    expected_lessons = [f"{number:02d}" for number in range(1, 25)]
    assert tutorial.lesson_checks == expected_lessons, "tutorial checkbox sequence is not 01..24"
    assert tutorial.lesson_articles == expected_lessons, "tutorial article sequence is not 01..24"

    css = (ROOT / "styles.css").read_text(encoding="utf-8")
    script = (ROOT / "site.js").read_text(encoding="utf-8")
    assert css.count("{") == css.count("}"), "unbalanced CSS braces"
    assert script.count("{") == script.count("}"), "unbalanced JavaScript braces"

    print(
        f"Explore2D website OK: {len(documents)} pages, "
        f"{len(expected_lessons)} lessons, all local links and anchors valid"
    )


if __name__ == "__main__":
    main()
