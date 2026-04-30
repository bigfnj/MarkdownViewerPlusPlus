# Markdown++ Local Link Click Smoke Test

Use this file to verify that local Markdown links clicked in the Markdown++
preview open in Notepad++ instead of silently doing nothing.

## Test Steps

1. Open this file in Notepad++.
2. Open the Markdown++ preview.
3. Click each link below in the preview pane.
4. Confirm Notepad++ opens or switches to the target Markdown file.
5. Confirm the fragment link scrolls within this preview.
6. Return to this file and repeat with the next link.

## Links

Same-folder Markdown file:

[Open the main smoke test](./markdownplusplus-smoke.md)

Parent-folder Markdown file:

[Open BUILDING_NATIVE.md](../BUILDING_NATIVE.md)

Fragment-only link should scroll inside this preview and should not open a file:

[Jump to expected behavior](#expected-behavior)

External HTTPS link should open in the default browser:

[External example](https://example.com/)

## Expected Behavior

- Same-folder and parent-folder Markdown links open in Notepad++.
- The current Markdown++ preview follows the newly opened Markdown file if
  automatic Markdown opening is enabled.
- Fragment-only links scroll inside this preview and do not ask Notepad++ to open a file.
- External HTTPS links open in the default browser.
