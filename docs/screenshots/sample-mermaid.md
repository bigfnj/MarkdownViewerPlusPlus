# Mermaid Diagrams In Markdown++

Markdown++ bundles Mermaid and renders it **offline**, so diagrams appear in the
preview with *no network access*. This file mixes short explanations with three
diagrams of increasing complexity so a single screenshot shows both the prose and
the rendered graphs.

Formatting still works around diagrams: **bold**, *italic*, <u>underline</u>, and
`inline code` all render normally between the fenced `mermaid` blocks.

## 1. A simple flowchart

A basic left-to-right flow. Each `A --> B` line is one edge, and the text in
brackets is the node label.

```mermaid
graph LR
    A[Idea] --> B[Draft]
    B --> C[Review]
    C --> D[Publish]
```

## 2. A sequence diagram

Sequence diagrams show *who talks to whom, and in what order*. Solid arrows are
calls, dashed arrows are responses.

```mermaid
sequenceDiagram
    participant U as User
    participant P as Markdown++
    participant W as WebView2
    U->>P: Save keystroke
    P->>P: Run cmark-gfm
    P->>W: Hand off HTML + Mermaid
    W-->>U: Live preview updates
```

## 3. A more complex flowchart

This one adds a **decision node**, a **subgraph**, and per-node styling. Note the
strict-parser rule: the subgraph uses an explicit alias (`PIPE`) and is styled by
that alias rather than by its quoted title.

```mermaid
graph TD
    START([Open file]) --> DETECT{Markdown extension?}
    DETECT -->|Yes| AUTO[Auto-open preview]
    DETECT -->|No| MANUAL[Toggle from menu]
    AUTO --> RENDER
    MANUAL --> RENDER

    subgraph PIPE ["Render pipeline"]
        direction LR
        RENDER[cmark-gfm] --> MERM[Mermaid pass]
        MERM --> HTML[WebView2 HTML]
    end

    HTML --> DONE([Live preview])

    style START fill:#dcffe4,stroke:#22863a,stroke-width:2px
    style DONE fill:#dcffe4,stroke:#22863a,stroke-width:2px
    style DETECT fill:#fff5b1,stroke:#b08800,stroke-width:2px
    style PIPE fill:#f1f8ff,stroke:#0366d6
```

If a diagram is malformed, Markdown++ shows the error inline and keeps the other
diagrams on the page working.
