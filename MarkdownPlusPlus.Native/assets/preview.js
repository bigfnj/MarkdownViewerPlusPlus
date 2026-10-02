(function () {
  var suppressScrollNotifyUntil = 0;
  var scrollNotifyQueued = false;
  var scrollRequestId = 0;
  var contentRevision = 0;
  var pdfExportDisabledLinks = [];

  function clampRatio(ratio) {
    ratio = Number(ratio);
    if (!Number.isFinite(ratio)) {
      return 0;
    }

    return Math.max(0, Math.min(1, ratio));
  }

  function maxScrollTop() {
    var root = document.documentElement;
    var body = document.body;
    var scrollHeight = Math.max(
      root ? root.scrollHeight : 0,
      body ? body.scrollHeight : 0
    );

    return Math.max(0, scrollHeight - window.innerHeight);
  }

  function currentScrollRatio() {
    var maxScroll = maxScrollTop();
    if (maxScroll <= 0) {
      return 0;
    }

    return clampRatio(window.scrollY / maxScroll);
  }

  function scrollNotifySuppressed() {
    return Date.now() < suppressScrollNotifyUntil;
  }

  function suppressScrollNotifications(milliseconds) {
    suppressScrollNotifyUntil = Math.max(suppressScrollNotifyUntil, Date.now() + milliseconds);
  }

  function parseSourcePosition(value) {
    var match = /^(\d+):\d+-(\d+):\d+$/.exec(value || "");
    if (!match) {
      return null;
    }

    return {
      start: Number(match[1]),
      end: Number(match[2])
    };
  }

  function sourceBlocks() {
    return Array.prototype.slice.call(document.querySelectorAll("[data-sourcepos]"))
      .map(function (node) {
        var source = parseSourcePosition(node.getAttribute("data-sourcepos"));
        return source ? { node: node, source: source } : null;
      })
      .filter(Boolean);
  }

  function blockMetrics(block) {
    var rect = block.node.getBoundingClientRect();
    var top = rect.top + window.scrollY;
    var height = Math.max(1, rect.height);
    return {
      top: top,
      bottom: top + height,
      height: height
    };
  }

  function lineRatioWithinBlock(sourceLine, source) {
    var lineSpan = Math.max(0, source.end - source.start);
    if (lineSpan === 0) {
      return 0;
    }

    return clampRatio((sourceLine - source.start) / lineSpan);
  }

  function sourceLineWithinBlock(anchorY, block, metrics) {
    var lineSpan = Math.max(0, block.source.end - block.source.start);
    if (lineSpan === 0) {
      return block.source.start;
    }

    var ratio = clampRatio((anchorY - metrics.top) / metrics.height);
    return Math.round(block.source.start + ratio * lineSpan);
  }

  function currentSourceLine() {
    var blocks = sourceBlocks();
    var anchorY = Math.max(0, window.scrollY + 24);
    var candidate = null;

    blocks.forEach(function (block) {
      var metrics = blockMetrics(block);
      if (metrics.top <= anchorY && anchorY <= metrics.bottom) {
        if (!candidate || metrics.top >= candidate.top) {
          candidate = {
            line: sourceLineWithinBlock(anchorY, block, metrics),
            top: metrics.top
          };
        }
      } else if (metrics.top <= anchorY) {
        if (!candidate || metrics.top >= candidate.top) {
          candidate = { line: block.source.end, top: metrics.top };
        }
      }
    });

    if (candidate) {
      return candidate.line;
    }

    if (blocks.length > 0) {
      return blocks[0].source.start;
    }

    return 0;
  }

  function postScrollMessage(sourceLine, ratio, force) {
    if (
      !window.chrome ||
      !window.chrome.webview ||
      typeof window.chrome.webview.postMessage !== "function"
    ) {
      return;
    }

    var message = {
      type: "previewScroll",
      line: sourceLine,
      ratio: ratio,
      force: !!force
    };

    window.chrome.webview.postMessage(message);
  }

  function postScrollRatio() {
    postScrollMessage(currentSourceLine(), currentScrollRatio(), false);
  }

  function postScrollRatioIfAllowed() {
    if (scrollNotifySuppressed()) {
      return;
    }

    postScrollRatio();
  }

  function queueScrollNotification() {
    if (scrollNotifyQueued) {
      return;
    }

    scrollNotifyQueued = true;
    window.requestAnimationFrame(function () {
      scrollNotifyQueued = false;
      postScrollRatioIfAllowed();
    });
  }

  function canPostMessage() {
    return window.chrome &&
      window.chrome.webview &&
      typeof window.chrome.webview.postMessage === "function";
  }

  function cssEscape(value) {
    if (window.CSS && typeof window.CSS.escape === "function") {
      return window.CSS.escape(value);
    }

    return String(value).replace(/["\\#.;?+*~':"!^$[\]()=>|/@]/g, "\\$&");
  }

  function decodeAnchor(value) {
    try {
      return decodeURIComponent(value.replace(/\+/g, "%20"));
    } catch (_) {
      return value;
    }
  }

  function slugifyHeading(text) {
    return String(text || "")
      .trim()
      .toLowerCase()
      .replace(/[^\w\s-]/g, "")
      .replace(/\s+/g, "-")
      .replace(/-+/g, "-")
      .replace(/^-|-$/g, "");
  }

  function assignHeadingIds() {
    var used = Object.create(null);

    Array.prototype.slice.call(document.querySelectorAll(".markdown-body h1, .markdown-body h2, .markdown-body h3, .markdown-body h4, .markdown-body h5, .markdown-body h6"))
      .forEach(function (heading) {
        var base = heading.id || slugifyHeading(heading.textContent);
        if (!base) {
          return;
        }

        var candidate = base;
        var index = 1;
        while (used[candidate]) {
          candidate = base + "-" + index++;
        }

        used[candidate] = true;
        if (!heading.id) {
          heading.id = candidate;
        }
      });
  }

  function findAnchorTarget(hash) {
    if (!hash || hash.charAt(0) !== "#") {
      return null;
    }

    var id = decodeAnchor(hash.slice(1));
    if (!id) {
      return null;
    }

    return document.getElementById(id) ||
      document.querySelector('[name="' + cssEscape(id) + '"]') ||
      Array.prototype.slice.call(document.querySelectorAll(".markdown-body h1, .markdown-body h2, .markdown-body h3, .markdown-body h4, .markdown-body h5, .markdown-body h6"))
        .filter(function (heading) {
          return slugifyHeading(heading.textContent) === id;
        })[0] ||
      null;
  }

  function sourceLineForNode(node) {
    while (node && node !== document) {
      if (node.getAttribute) {
        var source = parseSourcePosition(node.getAttribute("data-sourcepos"));
        if (source) {
          return source.start;
        }
      }

      node = node.parentNode;
    }

    return currentSourceLine();
  }

  function scrollToAnchor(hash) {
    var target = findAnchorTarget(hash);
    if (!target) {
      return false;
    }

    var sourceLine = sourceLineForNode(target);
    suppressScrollNotifications(240);
    target.scrollIntoView({ block: "start", inline: "nearest" });

    try {
      if (history && typeof history.replaceState === "function") {
        history.replaceState(null, document.title, hash);
      } else {
        window.location.hash = hash;
      }
    } catch (_) {
    }

    postScrollMessage(sourceLine, currentScrollRatio(), true);

    window.requestAnimationFrame(function () {
      postScrollMessage(sourceLine, currentScrollRatio(), true);
    });

    window.setTimeout(function () {
      postScrollMessage(sourceLine, currentScrollRatio(), true);
    }, 80);

    return true;
  }

  function isLocalPreviewLink(anchor, rawHref) {
    if (!rawHref || rawHref.charAt(0) === "#") {
      return false;
    }

    if (/^https:\/\/markdownplusplus\.document(?:\/|$)/i.test(anchor.href || "")) {
      return true;
    }

    return !/^[a-z][a-z0-9+.-]*:/i.test(rawHref);
  }

  function isExternalPreviewLink(anchor, rawHref) {
    var href = anchor.href || rawHref || "";
    return /^https?:\/\//i.test(href) &&
      !/^https:\/\/markdownplusplus\.document(?:\/|$)/i.test(href) &&
      !/^https:\/\/markdownplusplus\.local(?:\/|$)/i.test(href);
  }

  function handleLinkClick(event) {
    if (event.defaultPrevented || event.altKey || event.button > 1) {
      return;
    }

    var anchor = event.target && event.target.closest ? event.target.closest("a[href]") : null;
    if (!anchor) {
      return;
    }

    var rawHref = anchor.getAttribute("href") || "";
    if (rawHref.charAt(0) === "#") {
      event.preventDefault();
      scrollToAnchor(rawHref);
      return;
    }

    if (!canPostMessage()) {
      return;
    }

    var isLocal = isLocalPreviewLink(anchor, rawHref);
    var isExternal = isExternalPreviewLink(anchor, rawHref);
    if (!isLocal && !isExternal) {
      return;
    }

    event.preventDefault();
    window.chrome.webview.postMessage({
      type: "linkClick",
      href: isExternal ? anchor.href : rawHref
    });
  }

  function shouldDisableLinkForPdf(anchor) {
    var rawHref = anchor.getAttribute("href") || "";
    if (!rawHref || rawHref.charAt(0) === "#") {
      return false;
    }

    if (/^https:\/\/markdownplusplus\.document(?:\/|$)/i.test(anchor.href || "")) {
      return true;
    }

    if (/^file:/i.test(anchor.href || rawHref)) {
      return true;
    }

    return !/^[a-z][a-z0-9+.-]*:/i.test(rawHref);
  }

  function restorePdfExportLinks() {
    pdfExportDisabledLinks.forEach(function (entry) {
      if (entry.anchor && entry.anchor.setAttribute) {
        entry.anchor.setAttribute("href", entry.href);
      }
    });
    pdfExportDisabledLinks = [];
  }

  function preparePdfExport() {
    restorePdfExportLinks();

    pdfExportDisabledLinks = Array.prototype.slice.call(document.querySelectorAll("a[href]"))
      .filter(shouldDisableLinkForPdf)
      .map(function (anchor) {
        var entry = {
          anchor: anchor,
          href: anchor.getAttribute("href") || ""
        };
        anchor.removeAttribute("href");
        return entry;
      });

    return true;
  }

  function scrollToRatio(ratio) {
    ratio = clampRatio(ratio);
    var requestId = ++scrollRequestId;

    function apply() {
      if (requestId !== scrollRequestId) {
        return;
      }

      suppressScrollNotifications(220);
      window.scrollTo(0, Math.round(maxScrollTop() * ratio));
    }

    apply();
    window.setTimeout(apply, 60);
    window.setTimeout(apply, 250);

    if (document.readyState !== "complete") {
      window.addEventListener("load", apply, { once: true });
    }
  }

  function scrollToSourceLine(sourceLine, fallbackRatio, anchorRatio) {
    sourceLine = Number(sourceLine);
    fallbackRatio = clampRatio(fallbackRatio);
    anchorRatio = Number(anchorRatio);
    if (!Number.isFinite(anchorRatio)) {
      anchorRatio = 0.03;
    }
    anchorRatio = clampRatio(anchorRatio);
    var requestId = ++scrollRequestId;

    if (!Number.isFinite(sourceLine) || sourceLine <= 0) {
      scrollToRatio(fallbackRatio);
      return;
    }

    function findTarget() {
      var blocks = sourceBlocks();
      var best = null;

      blocks.forEach(function (block) {
        if (block.source.start <= sourceLine && sourceLine <= block.source.end) {
          if (!best || block.source.start >= best.source.start) {
            best = block;
          }
        } else if (block.source.start >= sourceLine) {
          if (!best || block.source.start < best.source.start) {
            best = block;
          }
        }
      });

      if (!best && blocks.length > 0) {
        best = blocks[blocks.length - 1];
      }

      return best;
    }

    function apply() {
      if (requestId !== scrollRequestId) {
        return;
      }

      var target = findTarget();
      if (!target) {
        scrollToRatio(fallbackRatio);
        return;
      }

      suppressScrollNotifications(240);
      var metrics = blockMetrics(target);
      var innerOffset = metrics.height * lineRatioWithinBlock(sourceLine, target.source);
      var top = Math.max(0, Math.round(metrics.top + innerOffset - Math.round(window.innerHeight * anchorRatio)));
      if (Math.abs(window.scrollY - top) > 2) {
        window.scrollTo(0, top);
      }
    }

    document.querySelectorAll(".markdown-body img").forEach(function (image) {
      if (image.complete) {
        return;
      }

      image.addEventListener("load", apply, { once: true });
      image.addEventListener("error", apply, { once: true });
    });

    apply();
    window.setTimeout(apply, 80);
    window.setTimeout(apply, 300);

    if (document.readyState !== "complete") {
      window.addEventListener("load", apply, { once: true });
    }
  }

  function normalizeMermaidBlocks() {
    var blocks = document.querySelectorAll('pre[lang="mermaid"], pre > code.language-mermaid, pre > code.lang-mermaid');

    blocks.forEach(function (node) {
      var source = node;
      var container = node;

      if (node.tagName.toLowerCase() === "code" && node.parentElement) {
        container = node.parentElement;
      }

      if (container.classList.contains("mermaid")) {
        return;
      }

      var mermaidBlock = document.createElement("pre");
      mermaidBlock.className = "mermaid";
      mermaidBlock.textContent = source.textContent.trim();
      if (container.hasAttribute("data-sourcepos")) {
        mermaidBlock.setAttribute("data-sourcepos", container.getAttribute("data-sourcepos"));
      }
      container.replaceWith(mermaidBlock);
    });
  }

  function mermaidErrorText(error) {
    if (!error) {
      return "Unknown Mermaid render error.";
    }

    if (typeof error === "string") {
      return error;
    }

    if (typeof error.message === "string" && error.message) {
      return error.message;
    }

    if (typeof error.str === "string" && error.str) {
      return error.str;
    }

    try {
      return JSON.stringify(error);
    } catch (_) {
      return String(error);
    }
  }

  function showMermaidError(block, source, error) {
    block.classList.add("mermaid-error");
    block.removeAttribute("data-processed");
    block.textContent = [
      "Mermaid render error",
      "",
      mermaidErrorText(error),
      "",
      "Source:",
      source || ""
    ].join("\n");
  }

  function renderMermaidBlock(block) {
    var source = block.getAttribute("data-mermaid-source") || block.textContent.trim();
    block.setAttribute("data-mermaid-source", source);
    block.classList.remove("mermaid-error");
    block.textContent = source;
    block.removeAttribute("data-processed");

    return Promise.resolve(typeof window.mermaid.parse === "function" ? window.mermaid.parse(source) : true)
      .then(function () {
        return window.mermaid.run({
          nodes: [block]
        });
      })
      .catch(function (error) {
        showMermaidError(block, source, error);
        console.error(error);
      });
  }

  // ---------------------------------------------------------------------------
  // Copy
  //
  // Chromium's default copy inlines the COMPUTED style of every selected node, which drags
  // the preview's dark theme onto the clipboard: measured on 2026-10-02, a pane copy pasted
  // into Word as rgb(230, 237, 243) text, i.e. near-white on a white page. Bold and italic
  // survived; nobody could see them. Worse, it is not even consistent -- when the selection
  // lets Chromium hoist the shared colour onto a wrapper span, Word discards the wrapper and
  // the same copy pastes fine. Two shapes of one defect, decided by what you happened to select.
  //
  // So we serialise the selection ourselves. The preview DOM carries no inline styles (all
  // styling lives in preview.css), so cloning it yields clean semantic markup and the paste
  // target applies its own colours. The only styles added back are structural ones that no
  // target supplies by itself, and none of them set a foreground colour.
  var COPY_STRUCTURAL_STYLES = [
    ["table", "border-collapse: collapse; margin: 8px 0;"],
    ["th", "border: 1px solid #d0d7de; padding: 6px 13px; text-align: left;"],
    ["td", "border: 1px solid #d0d7de; padding: 6px 13px;"],
    ["pre", "background: #f6f8fa; padding: 12px; border-radius: 6px; white-space: pre;"],
    ["blockquote", "border-left: 4px solid #d0d7de; margin: 8px 0; padding: 0 1em;"]
  ];

  // Inline tags worth rebuilding around a partial selection. Selecting three words inside a
  // <strong> gives a range whose cloneContents() is a bare text node, so without this the
  // emphasis would be lost -- a regression against the very behaviour we are fixing.
  var COPY_INLINE_ANCESTORS = {
    STRONG: true, B: true, EM: true, I: true, CODE: true, A: true,
    DEL: true, S: true, SUP: true, SUB: true, MARK: true
  };

  function selectionToContainer(selection) {
    var container = document.createElement("div");

    for (var index = 0; index < selection.rangeCount; index++) {
      container.appendChild(selection.getRangeAt(index).cloneContents());
    }

    if (selection.rangeCount !== 1) {
      return container;
    }

    var node = selection.getRangeAt(0).commonAncestorContainer;
    if (node && node.nodeType === 3) {
      node = node.parentNode;
    }

    while (node && node.tagName && COPY_INLINE_ANCESTORS[node.tagName.toUpperCase()]) {
      var wrapper = node.cloneNode(false);
      while (container.firstChild) {
        wrapper.appendChild(container.firstChild);
      }
      container.appendChild(wrapper);
      node = node.parentNode;
    }

    return container;
  }

  // Ctrl+A selects the whole body, so the clone carries the page's own <script> elements.
  // Their text leaked into the plain-text flavour as window.MarkdownPlusPlusOptions={...}.
  function stripNonContentNodes(container) {
    Array.prototype.slice.call(container.querySelectorAll("script, style, noscript, template"))
      .forEach(function (node) {
        if (node.parentNode) {
          node.parentNode.removeChild(node);
        }
      });
  }

  // A rendered diagram is an <svg>, and pasting raw inline SVG makes a mess: Word turned one
  // diagram into three 15x11pt broken shapes while still rendering the node labels as stray
  // text. Word 2016+, Outlook and OneNote do render an <img> whose source is an SVG data URI,
  // so hand them that instead. The text flavour carries the Mermaid source separately.
  function mermaidBlockToImage(block) {
    var svg = block.querySelector("svg");
    if (!svg) {
      return null;
    }

    var clone = svg.cloneNode(true);
    if (!clone.getAttribute("xmlns")) {
      clone.setAttribute("xmlns", "http://www.w3.org/2000/svg");
    }

    var image = document.createElement("img");
    var viewBox = (clone.getAttribute("viewBox") || "").split(/[\s,]+/);
    if (viewBox.length === 4) {
      var width = Math.round(Number(viewBox[2]));
      var height = Math.round(Number(viewBox[3]));
      if (width > 0 && height > 0) {
        image.setAttribute("width", String(width));
        image.setAttribute("height", String(height));
      }
    }

    try {
      var markup = new XMLSerializer().serializeToString(clone);
      image.setAttribute("src", "data:image/svg+xml;charset=utf-8," + encodeURIComponent(markup));
    } catch (_) {
      return null;
    }

    return image;
  }

  function replaceMermaidBlocks(container) {
    Array.prototype.slice.call(container.querySelectorAll("pre.mermaid"))
      .forEach(function (block) {
        var replacement = mermaidBlockToImage(block);
        if (!replacement) {
          // Not rendered, or serialisation failed. The textContent is the Mermaid source in
          // that case, which is better in a paste than an empty hole.
          return;
        }
        if (block.parentNode) {
          block.parentNode.replaceChild(replacement, block);
        }
      });
  }

  function buildCopyHtml(container) {
    stripNonContentNodes(container);
    replaceMermaidBlocks(container);

    Array.prototype.slice.call(container.querySelectorAll("[data-sourcepos]"))
      .forEach(function (node) {
        node.removeAttribute("data-sourcepos");
      });

    COPY_STRUCTURAL_STYLES.forEach(function (rule) {
      Array.prototype.slice.call(container.querySelectorAll(rule[0]))
        .forEach(function (node) {
          var existing = node.getAttribute("style");
          node.setAttribute("style", existing ? existing + ";" + rule[1] : rule[1]);
        });
    });

    return container.innerHTML;
  }

  function collapseInlineWhitespace(value) {
    return String(value || "").replace(/\s+/g, " ").trim();
  }

  // Block elements that own their text. Anything else that contains element children is a
  // wrapper to descend through: a whole-document selection clones a single <article>, and
  // treating that as one block collapsed the entire document onto one line.
  var COPY_TEXT_LEAF_BLOCKS = {
    P: true, H1: true, H2: true, H3: true, H4: true, H5: true, H6: true,
    PRE: true, TABLE: true, UL: true, OL: true, DL: true, HR: true
  };

  function blockToText(node, lines) {
    if (node.nodeType === 3) {
      var raw = collapseInlineWhitespace(node.nodeValue);
      if (raw) {
        lines.push(raw);
      }
      return;
    }

    if (node.nodeType !== 1) {
      return;
    }

    var tag = node.tagName ? node.tagName.toUpperCase() : "";

    if (tag === "SCRIPT" || tag === "STYLE" || tag === "NOSCRIPT" || tag === "TEMPLATE") {
      return;
    }

    if (tag === "HR") {
      lines.push("---");
      return;
    }

    if (!COPY_TEXT_LEAF_BLOCKS[tag] &&
        !(tag === "PRE" && node.classList && node.classList.contains("mermaid")) &&
        node.children.length > 0) {
      Array.prototype.slice.call(node.childNodes).forEach(function (child) {
        blockToText(child, lines);
      });
      return;
    }

    if (tag === "PRE" && node.classList && node.classList.contains("mermaid")) {
      // The rendered diagram is an <svg>, whose textContent is the node labels run together.
      // The source is already parked on the element by renderMermaidBlock, and it is what a
      // reader of a plain-text paste can actually use.
      var source = node.getAttribute("data-mermaid-source");
      if (source) {
        lines.push(source);
      }
      return;
    }

    if (tag === "UL" || tag === "OL") {
      var items = Array.prototype.slice.call(node.children).map(function (item, index) {
        var marker = tag === "OL" ? (index + 1) + ". " : "- ";
        return marker + collapseInlineWhitespace(item.textContent);
      });
      if (items.length > 0) {
        lines.push(items.join("\n"));
      }
      return;
    }

    if (tag === "TABLE") {
      var rows = Array.prototype.slice.call(node.querySelectorAll("tr")).map(function (row) {
        return Array.prototype.slice.call(row.children).map(function (cell) {
          return collapseInlineWhitespace(cell.textContent);
        }).join("\t");
      });
      if (rows.length > 0) {
        lines.push(rows.join("\n"));
      }
      return;
    }

    if (tag === "PRE") {
      lines.push(String(node.textContent || "").replace(/\s+$/, ""));
      return;
    }

    var text = collapseInlineWhitespace(node.textContent);
    if (text) {
      lines.push(text);
    }
  }

  // Chromium's plain-text flavour joins a heading straight onto the paragraph below it with a
  // single newline, which is where "everything is clumped together" comes from in targets that
  // strip HTML. Block elements get a blank line between them here.
  function buildCopyText(container) {
    var lines = [];

    Array.prototype.slice.call(container.childNodes).forEach(function (child) {
      blockToText(child, lines);
    });

    if (lines.length === 0) {
      return collapseInlineWhitespace(container.textContent);
    }

    return lines.join("\n\n");
  }

  function handleCopy(event) {
    if (!event.clipboardData) {
      return;
    }

    var selection = window.getSelection();
    if (!selection || selection.rangeCount === 0 || selection.isCollapsed) {
      return;
    }

    var container = selectionToContainer(selection);
    if (!container.textContent && !container.querySelector("img, svg")) {
      return;
    }

    var text = buildCopyText(container);
    var html = buildCopyHtml(container);
    if (!html) {
      return;
    }

    event.clipboardData.setData("text/html", '<meta charset="utf-8">' + html);
    event.clipboardData.setData("text/plain", text);
    event.preventDefault();
  }

  window.MarkdownPlusPlusPreview = {
    scrollToRatio: scrollToRatio,
    scrollToSourceLine: scrollToSourceLine,
    currentScrollRatio: currentScrollRatio,
    currentSourceLine: currentSourceLine,
    preparePdfExport: preparePdfExport,
    restorePdfExport: restorePdfExportLinks,
    renderMermaid: function () {
      if (window.MarkdownPlusPlusOptions && window.MarkdownPlusPlusOptions.mermaidEnabled === false) {
        return Promise.resolve();
      }

      normalizeMermaidBlocks();

      if (!window.mermaid) {
        return Promise.resolve();
      }

      window.mermaid.initialize({
        startOnLoad: false,
        securityLevel: "strict",
        theme: window.matchMedia && window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "default"
      });

      return Array.prototype.slice.call(document.querySelectorAll(".mermaid"))
        .reduce(function (chain, block) {
          return chain.then(function () {
            return renderMermaidBlock(block);
          });
        }, Promise.resolve());
    },
    replaceContent: function (html, title, sourceLine, fallbackRatio, anchorRatio, revision) {
      revision = Number(revision) || contentRevision + 1;
      if (revision < contentRevision) {
        return true;
      }

      var article = document.querySelector(".markdown-body");
      if (!article) {
        return false;
      }

      // Refuse an empty update instead of blanking the pane and reporting success.
      // `article.innerHTML = html || ""` wiped the preview and still returned true, so the
      // host believed the content had been applied and never fell back to a full
      // re-navigation -- a silent, permanent blank that survived a restart because the same
      // empty content was re-applied every time. Returning false makes the host re-navigate.
      if (typeof html !== "string" || html.length === 0) {
        return false;
      }

      contentRevision = revision;
      suppressScrollNotifications(650);
      article.innerHTML = html;
      assignHeadingIds();
      if (typeof title === "string") {
        document.title = title;
      }

      function restoreScroll() {
        scrollToSourceLine(sourceLine, fallbackRatio, anchorRatio);
      }

      restoreScroll();
      Promise.resolve(window.MarkdownPlusPlusPreview.renderMermaid()).then(function () {
        restoreScroll();
        window.setTimeout(restoreScroll, 150);
      });

      return true;
    }
  };

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", function () {
      assignHeadingIds();
      window.MarkdownPlusPlusPreview.renderMermaid();
    });
  } else {
    assignHeadingIds();
    window.MarkdownPlusPlusPreview.renderMermaid();
  }

  window.addEventListener("scroll", queueScrollNotification, { passive: true });
  document.addEventListener("click", handleLinkClick);
  document.addEventListener("auxclick", handleLinkClick);
  document.addEventListener("copy", handleCopy);
})();
