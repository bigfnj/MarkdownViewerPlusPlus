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

      contentRevision = revision;
      suppressScrollNotifications(650);
      article.innerHTML = html || "";
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
})();
