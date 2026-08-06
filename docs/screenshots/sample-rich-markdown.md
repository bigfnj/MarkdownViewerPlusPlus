# Markdown++ Feature Showcase

A single document that exercises most of what the preview renders. Open it in
Notepad++ with the **Markdown++** panel to capture a rich screenshot.

## Text formatting

This paragraph mixes **bold**, *italic*, ***bold italic***, ~~strikethrough~~,
<u>underline</u>, and `inline code`. You can also link to an
[external site](https://mermaid.js.org/) or drop a bare autolink like
https://github.com/bigfnj/MarkdownViewerPlusPlus and it becomes clickable.

> **Callout.** Blockquotes are handy for notes and warnings.
>
> > Nested quotes work too, one level deeper.

## Lists

Unordered, with nesting:

- Rendering engine
  - `cmark-gfm` for GitHub-flavored Markdown
  - Mermaid for diagrams
- Output
  - Copy HTML, export HTML, export PDF, print

Ordered:

1. Edit the Markdown source
2. Watch the preview update
3. Export when you are done

Task list:

- [x] Live preview panel
- [x] Offline Mermaid rendering
- [ ] Your next feature idea

## Table

| Feature            | Supported | Notes                                  |
| :----------------- | :-------: | :------------------------------------- |
| GFM tables         |    Yes    | Column alignment shown here            |
| Task lists         |    Yes    | Checkboxes render as above             |
| Strikethrough      |    Yes    | `~~like this~~`                        |
| Local images / SVG |    Yes    | Resolved relative to the document      |

## Code block

```python
def render(markdown: str) -> str:
    """Convert Markdown to HTML for the preview panel."""
    html = cmark_gfm(markdown)
    return with_mermaid(html)
```

```powershell
python .\tools\install-native-deps.py
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

## Local image

The preview resolves images relative to the file, including SVG:

![Markdown++ local SVG](../../smoke-tests/markdownplusplus-smoke-image.svg)

## Blockquote with inline everything

> You can combine **bold**, *italic*, `code`, and a [link](https://example.com)
> inside a quote, followed by a horizontal rule.

---

Small print and a final line of `inline code` to round out the sample.
