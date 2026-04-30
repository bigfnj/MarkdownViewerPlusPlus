# AI_UNDERSTANDING: MarkdownViewerPlusPlus
# Generated: 2026-04-24
# Purpose: Machine-readable full technical reference for AI context

---

## META

project=MarkdownViewerPlusPlus
version=0.8.2.0
type=Notepad++_Plugin_DLL
target_framework=.NET_4.0
platforms=[x86,x64,AnyCPU]
author="Savas Ziplies"
copyright="2017-2018"
guid=31492674-6fe0-485c-91f0-2e17244588ff
entry_namespace=Kbg.NppPluginNET
impl_namespace=com.insanitydesign.MarkdownViewerPlusPlus

---

## DIRECTORY_TREE

```
/MarkdownViewerPlusPlus/           [root]
  MarkdownViewerPlusPlus.sln       [VS2015 solution]
  /MarkdownViewerPlusPlus/         [project root]
    MarkdownViewerPlusPlus.csproj  [MSBuild; ILMerge; DllExport; ZIP tasks]
    packages.config                [NuGet manifest]
    app.config                     [assembly binding redirects]
    Main.cs                        [plugin stub; delegates to MarkdownViewer]
    MarkdownViewer.cs              [core plugin logic; state; event dispatch]
    MarkdownViewerConfiguration.cs [INI-backed config; Options struct]
    /PluginInfrastructure/
      UnmanagedExports.cs          [DllExport entry points -> Notepad++ ABI]
      NppPluginNETBase.cs          [PluginBase singleton; FuncItems; nppData]
      NotepadPPGateway.cs          [INotepadPPGateway interface + impl]
      IScintillaGateway.cs         [IScintillaGateway interface]
      ScintillaGateway.cs          [full Scintilla API via SendMessage]
      Win32.cs                     [P/Invoke: SendMessage, INI, scroll, menu]
      Msgs_h.cs                    [NppMsg + SciMsg enums]
      Docking_h.cs                 [NppTbData struct; NppTbMsg enum]
      Scintilla_iface.cs           [Scintilla enum constants]
      GatewayDomain.cs             [Colour, Position, TextRange, Cells, etc.]
      NppPluginNETHelper.cs        [NppData, FuncItem, ShortcutKey, FuncItems]
      ClikeStringArray.cs          [unmanaged string array marshaling]
      /DllExport/
        DllExportAttribute.cs      [attribute marking managed exports]
    /Forms/
      AbstractRenderer.cs          [base Form; toolbar; Markdig pipeline; export]
      MarkdownViewerRenderer.cs    [concrete renderer; HtmlPanel; image loader]
      MarkdownViewerHtmlPanel.cs   [HtmlPanel subclass; scroll sync; redraw]
      MarkdownViewerOptions.cs     [options dialog; tree nav; load/save events]
      AbstractOptionsPanel.cs      [UserControl base for option tabs]
      OptionsPanelGeneral.cs       [file extensions; new-file toggle]
      OptionsPanelHTML.cs          [custom CSS; open-after-export toggle]
      OptionsPanelPDF.cs           [orientation; page size; margins; open toggle]
      AboutDialog.cs               [about dialog]
    /Helper/
      ClipboardHelper.cs           [CF_HTML clipboard format builder]
    /Windows/
      WindowsMessage.cs            [WM_* constants; NMHDR; DockMgrMsg]
    /Properties/
      AssemblyInfo.cs
      Resources.Designer.cs        [embedded resource accessors]
    ILMerge.props                  [ILMerge MSBuild configuration]
    ILMergeOrder.txt               [merge order: Core -> HtmlRenderer -> Markdig -> Svg -> PdfSharp]
```

---

## DEPENDENCIES

| package | version | role |
|---|---|---|
| Markdig | 0.15.0 | markdown->HTML parser; CommonMark 0.28 |
| HtmlRenderer.Core | 1.5.1-beta3 | HTML rendering engine |
| HtmlRenderer.WinForms | 1.5.1-beta3 | HtmlPanel WinForms control |
| HtmlRenderer.PdfSharp | 1.5.1-beta3 | HTML->PDF via PdfSharp |
| PDFSharp | 1.50.4845-RC2a | PDF document generation |
| Svg | 2.3.0 | SVG.NET; bitmap conversion |
| ILMerge | 2.14.1208 | build: merge assemblies into single DLL |
| MSBuild.Extension.Pack | 1.9.1 | build: ZIP creation |
| MSBuild.ILMerge.Task | 1.0.5 | build: ILMerge MSBuild integration |
| Microsoft.Office.Interop.Outlook | 15.0.0.0 | COM interop; mail export |

assembly_redirect: PdfSharp 0.0.0.0-1.50.4845.0 -> 1.50.4845.0

---

## PLUGIN_INFRASTRUCTURE

### UNMANAGED_EXPORTS (UnmanagedExports.cs)
calling_convention=Cdecl for all exports

```
[DllExport] bool isUnicode()
  return true

[DllExport] void setInfo(NppData notepadPlusData)
  PluginBase.nppData = notepadPlusData
  Main.CommandMenuInit()

[DllExport] IntPtr getFuncsArray(ref int nbF)
  nbF = count of registered commands
  return PluginBase._funcItems.NativePointer

[DllExport] uint messageProc(uint Message, IntPtr wParam, IntPtr lParam)
  return 1  [unused]

[DllExport] IntPtr getName()
  return "MarkdownViewer++"

[DllExport] void beNotified(IntPtr notifyCode)
  marshal notifyCode -> ScNotification
  switch notification.Header.Code:
    NPPN_TBMODIFICATION -> RefreshItems(); Main.SetToolBarIcon()
    NPPN_SHUTDOWN -> Main.PluginCleanUp()
    _ -> Main.OnNotification(notification)
```

### WIN32 (Win32.cs)
```
MAX_PATH = 260

[DllImport("user32")] GetScrollInfo(IntPtr hWnd, int nBar, ref ScrollInfo lpsi) -> bool
[DllImport("user32")] GetMenu(IntPtr hWnd) -> IntPtr
[DllImport("user32")] CheckMenuItem(IntPtr hmenu, int uIDCheckItem, uint uCheck) -> int

[DllImport("kernel32")] GetPrivateProfileInt(string lpAppName, string lpKeyName, int nDefault, string lpFileName) -> int
[DllImport("kernel32")] GetPrivateProfileString(string lpAppName, string lpKeyName, string lpDefault, StringBuilder lpReturnedString, int nSize, string lpFileName) -> int
[DllImport("kernel32")] WritePrivateProfileString(string lpAppName, string lpKeyName, string lpString, string lpFileName) -> bool

SendMessage overloads (10+):
  (IntPtr hWnd, uint Msg, IntPtr wParam, string lParam)    LPWStr
  (IntPtr hWnd, uint Msg, IntPtr wParam, StringBuilder sb) LPWStr
  (IntPtr hWnd, uint Msg, IntPtr wParam, IntPtr lParam)
  (IntPtr hWnd, uint Msg, IntPtr wParam, out IntPtr lParam)
  (IntPtr hWnd, uint Msg, int wParam, NppMenuCmd lParam)
  (IntPtr hWnd, uint Msg, int wParam, IntPtr lParam)
  (IntPtr hWnd, uint Msg, int wParam, int lParam)
  (IntPtr hWnd, uint Msg, int wParam, out int lParam)
  (IntPtr hWnd, uint Msg, int wParam, StringBuilder sb)
  (IntPtr hWnd, uint Msg, int wParam, string lParam)

struct ScrollInfo {
  uint cbSize
  uint fMask        [SIF_RANGE|SIF_PAGE|SIF_POS|SIF_TRACKPOS|SIF_ALL]
  int nMin
  int nMax
  uint nPage
  int nPos
  int nTrackPos
}

enum ScrollInfoMask { SIF_RANGE=0x1, SIF_PAGE=0x2, SIF_POS=0x4, SIF_DISABLENOSCROLL=0x8, SIF_TRACKPOS=0x10, SIF_ALL=0x1F }
enum ScrollInfoBar { SB_HORZ=0, SB_VERT=1, SB_CTL=2, SB_BOTH=3 }
MF_BYCOMMAND=0x0, MF_CHECKED=0x8, MF_UNCHECKED=0x0
```

### MSGS_H (Msgs_h.cs)
```
NppMsg enum (notepad++ -> plugin):
  NPPM_GETCURRENTSCINTILLA          get active scintilla handle
  NPPM_GETFULLCURRENTPATH           full path of current file
  NPPM_GETFILENAME                  filename only
  NPPM_GETCURRENTDIRECTORY          directory of current file
  NPPM_DMMSHOW                      show dockable dialog
  NPPM_DMMHIDE                      hide dockable dialog
  NPPM_DMMREGASDCKDLG               register dockable dialog
  NPPM_SETMENUITEMCHECK             check/uncheck menu item
  NPPM_ADDTOOLBARICON               add icon to toolbar
  NPPM_GETPLUGINSCONFIGDIR          get plugins config dir path
  NPPN_TBMODIFICATION               toolbar created
  NPPN_BUFFERACTIVATED              active file/tab changed
  NPPN_SHUTDOWN                     notepad++ closing

SciMsg enum (scintilla messages):
  SCI_ADDTEXT / SCI_INSERTTEXT / SCI_GETTEXT
  SCI_GETLENGTH                     doc length (excl null)
  SCI_LINEFROMPOSITION
  SCI_POSITIONFROMLINE
  SCN_UPDATEUI                      selection/scroll changed
  SCN_MODIFIED                      content modified
  SC_MOD_INSERTTEXT                 modification type flag
  SC_MOD_DELETETEXT                 modification type flag
  SC_UPDATE_V_SCROLL                vertical scroll flag
```

### DOCKING (Docking_h.cs)
```
struct NppTbData {
  IntPtr hClient          window handle of dockable panel
  string pszName          "MarkdownViewer++"
  int dlgID               command index (0)
  NppTbMsg uMask          DWS_ICONTAB|DWS_ICONBAR|DWS_DF_CONT_RIGHT
  uint hIconTab           icon handle
  string pszAddInfo       unused
  RECT rcFloat            floating position
  int iPrevCont           previous container
  string pszModuleName    "MarkdownViewerPlusPlus"
}

enum NppTbMsg : uint {
  CONT_LEFT=0, CONT_RIGHT=1, CONT_TOP=2, CONT_BOTTOM=3
  DWS_ICONTAB=0x1
  DWS_ICONBAR=0x2
  DWS_DF_CONT_RIGHT=(1<<28)
  DWS_DF_FLOATING=0x80000000
}
```

### PLUGIN_BASE (NppPluginNETBase.cs)
```
static class PluginBase {
  internal static NppData nppData         [set in setInfo()]
  internal static FuncItems _funcItems    [registered commands]

  internal static IntPtr GetCurrentScintilla()
    sends NPPM_GETCURRENTSCINTILLA
    returns _scintillaMainHandle or _scintillaSecondHandle

  internal static void SetCommand(int index, string name,
      NppFuncItemDelegate func, ShortcutKey shortcut, bool checkOnInit)
    adds FuncItem to _funcItems
}

struct NppData {
  IntPtr _nppHandle
  IntPtr _scintillaMainHandle
  IntPtr _scintillaSecondHandle
}

delegate void NppFuncItemDelegate()
```

### GATEWAY_DOMAIN (GatewayDomain.cs)
```
class Colour {
  int Red, Green, Blue
  int Value { Red + (Green<<8) + (Blue<<16) }
}

class Position : IEquatable<Position> {
  int Value
  operators: +, -, ==, !=, <, >
  static Min(Position, Position), Max(Position, Position)
}

struct CharacterRange { int cpMin, cpMax }

class Cells { char[] Value }

class TextRange : IDisposable {
  IntPtr NativePointer          marshaled Sci_TextRange
  string lpstrText
  CharacterRange chrg { get; set; }
}

class KeyModifier {
  int Value    (SCK_KeyCode) | (SCMOD_modifier << 16)
}
```

### NPP_PLUGIN_NET_HELPER (NppPluginNETHelper.cs)
```
struct ShortcutKey {
  byte _isCtrl, _isAlt, _isShift, _key
}

struct FuncItem {
  [MarshalAs(ByValTStr, SizeConst=64)] string _itemName
  NppFuncItemDelegate _pFunc
  int _cmdID
  bool _init2Check
  ShortcutKey _pShKey
}

class FuncItems : IDisposable {
  List<FuncItem> _funcItems
  IntPtr _nativePointer         pointer to unmanaged array

  void Add(FuncItem)            reallocates native array
  IntPtr NativePointer { get }  returned to Notepad++ in getFuncsArray
}
```

### SCINTILLA_GATEWAY (ScintillaGateway.cs)
```
class ScintillaGateway : IScintillaGateway {
  private IntPtr scintilla

  IntPtr CurrentBufferID { get; set; }

  IntPtr SetScintillaHandle(IntPtr newHandle) -> old handle
  void SwitchScintillaHandle()               -> toggle main/secondary

  // 100+ methods; all use Win32.SendMessage(scintilla, SciMsg.*, ...)
  key methods:
    int GetLength()                           SCI_GETLENGTH
    string GetText(int length)                SCI_GETTEXT
    void InsertText(Position pos, string str) SCI_INSERTTEXT
    int GetCurrentLineNumber()                SCI_LINEFROMPOSITION(CurrentPos)
    int LineFromPosition(int pos)             SCI_LINEFROMPOSITION
    int PositionFromLine(int line)            SCI_POSITIONFROMLINE
    ScrollInfo GetScrollInfo(int bar, ScrollInfoMask mask)
    bool GetEndAtLastLine()
}
```

### NOTEPAD_GATEWAY (NotepadPPGateway.cs)
```
interface INotepadPPGateway {
  FileInfo GetCurrentFilePath()
  string GetCurrentFileName()
  string GetCurrentDirectory()
  void ShowDockableDialog(NppTbData tbData)
  void HideDockableDialog(int dlgId)
  // etc.
}
```

---

## CORE_PLUGIN_LOGIC

### MAIN (Main.cs)
```
class Main {
  const string PluginName = "MarkdownViewer++"

  private static MarkdownViewer MarkdownViewer = new MarkdownViewer()

  static void OnNotification(ScNotification n)    -> MarkdownViewer.OnNotification(n)
  static void CommandMenuInit()                   -> MarkdownViewer.CommandMenuInit()
  static void SetToolBarIcon()                    -> MarkdownViewer.SetToolBarIcon()
  static void PluginCleanUp()                     -> MarkdownViewer.PluginCleanUp()
}
```

### FILE_INFORMATION (MarkdownViewer.cs)
```
struct FileInformation {
  string FileDirectory { get; set; }
  string FileName {
    get { return fileName }
    set {
      fileName = value
      FileExtension = Path.GetExtension(fileName)
      // strip leading dot
    }
  }
  string FileExtension { get; private set; }
}
```

### MARKDOWN_VIEWER (MarkdownViewer.cs)
```
class MarkdownViewer {
  // gateways
  IScintillaGateway Editor { get; protected set; }
  INotepadPPGateway Notepad { get; protected set; }

  // state
  FileInformation FileInfo { get; protected set; }
  AbstractRenderer renderer                  dockable panel instance
  bool rendererVisible = false
  bool updateRenderer = true                 debounce flag
  MarkdownViewerConfiguration configuration

  // command indices
  int commandId = 0                          toggle renderer
  int commandIdSynchronize = 2               sync scrolling (checkable)
  int commandIdOptions = 4
  int commandIdAbout = 5

  Options Options { get { return configuration.options } }

  void CommandMenuInit()
    configuration = new MarkdownViewerConfiguration()
    configuration.Init()
    PluginBase.SetCommand(0, "MarkdownViewer++", MarkdownViewerCommand, Ctrl+Shift+M, false)
    PluginBase.SetCommand(1, "---", null, -, -)           [separator]
    PluginBase.SetCommand(2, "Synchronize scrolling", SynchronizeScrollingCommand, -, options.synchronizeScrolling)
    PluginBase.SetCommand(3, "---", null, -, -)           [separator]
    PluginBase.SetCommand(4, "Options", OptionsCommand, -, -)
    PluginBase.SetCommand(5, "About", AboutCommand, -, -)

  void MarkdownViewerCommand()
    toggle rendererVisible
    if showing:
      UpdateEditorInformation()
      Update(updateScrollBar:true, updateRenderer:true)
      renderer.SetFocus()
    ToggleToolbarIcon(rendererVisible)

  void ToggleToolbarIcon(bool show)
    if show: SendMessage(NPPM_DMMSHOW, renderer.Handle)
    else:    SendMessage(NPPM_DMMHIDE, renderer.Handle)
    SendMessage(NPPM_SETMENUITEMCHECK, commandId, show)

  void SetToolBarIcon()
    bitmap = Resources.markdown_16x16_solid
    hIcon = Icon.FromHandle(bitmap.GetHbitmap())
    toolbarIcons.hToolbarBmp = bitmap.GetHbitmap()
    toolbarIcons.hToolbarIcon = hIcon.Handle
    SendMessage(NPPM_ADDTOOLBARICON, commandId, ref toolbarIcons)

  void UpdateEditorInformation()
    Editor.SetScintillaHandle(PluginBase.GetCurrentScintilla())
    FileInfo.FileName = Notepad.GetCurrentFileName()
    FileInfo.FileDirectory = Notepad.GetCurrentDirectory()

  void Update(bool updateScrollBar=false, bool updateRenderer=false)
    if !rendererVisible: return
    if updateRenderer AND configuration.ValidateFileExtension(FileInfo.FileExtension, FileInfo.FileName):
      UpdateMarkdownViewer()
    if updateScrollBar AND options.synchronizeScrolling:
      UpdateScrollBar()

  void UpdateMarkdownViewer()
    int length = Editor.GetLength()
    string text = Editor.GetText(length + 1)   [+1 for null terminator]
    renderer.Render(text, FileInfo)
    updateRenderer = false

  void UpdateScrollBar()
    ScrollInfo si = Editor.GetScrollInfo(SB_VERT, SIF_RANGE|SIF_TRACKPOS|SIF_PAGE)
    double ratio
    if !Editor.GetEndAtLastLine():
      actualThumbHeight = si.nMax * si.nPage / (si.nMax - si.nPage + si.nPage)
      [complex calc avoiding edge case where last line not at bottom]
      ratio = min(1.0, adjusted_pos / (si.nMax - actualThumbHeight))
    else:
      ratio = si.nTrackPos / (double)(si.nMax - si.nPage)
    renderer.ScrollByRatioVertically(ratio)

  void OnNotification(ScNotification n)
    if !rendererVisible: return
    switch n.Header.Code:
      SCN_UPDATEUI:
        if n.Updated & SC_UPDATE_V_SCROLL:
          Update(updateScrollBar:true)
        if updateRenderer:
          Update(updateScrollBar:false, updateRenderer:true)
      NPPN_BUFFERACTIVATED:
        UpdateEditorInformation()
        Update(updateScrollBar:true, updateRenderer:true)
      SCN_MODIFIED:
        updateRenderer = (n.ModificationType & SC_MOD_INSERTTEXT)
                      || (n.ModificationType & SC_MOD_DELETETEXT)

  void SynchronizeScrollingCommand()
    options.synchronizeScrolling = !options.synchronizeScrolling
    SendMessage(NPPM_SETMENUITEMCHECK, commandIdSynchronize, options.synchronizeScrolling)

  void OptionsCommand()
    new MarkdownViewerOptions(ref configuration).ShowDialog()

  void AboutCommand()
    new AboutDialog().ShowDialog()

  void PluginCleanUp()
    configuration.Save()
}
```

---

## CONFIGURATION

### OPTIONS_STRUCT
```
struct Options {
  bool synchronizeScrolling        default: false
  string fileExtensions            default: ""  (empty = all)
  bool inclNewFiles                default: true

  string htmlCssStyle              default: ""
    get: replace literal \n -> Environment.NewLine
    set: replace Environment.NewLine -> literal \n

  PageOrientation pdfOrientation   default: Portrait
  PageSize pdfPageSize             default: A4
  string margins                   default: "5,5,5,5"  [top,right,bottom,left mm]

  bool pdfOpenExport               default: false
  bool htmlOpenExport              default: false

  int[] GetMargins()
    parse margins csv -> [top,right,bottom,left]
    fallback: [5,5,5,5]
}
```

### MARKDOWN_VIEWER_CONFIGURATION
```
class MarkdownViewerConfiguration {
  string iniFilePath    %APPDATA%\Notepad++\plugins\config\MarkdownViewerPlusPlus.ini
  string assemblyName   "MarkdownViewerPlusPlus"
  Options options       public field

  void Init()
    NPPM_GETPLUGINSCONFIGDIR -> dir
    mkdir if missing
    iniFilePath = Path.Combine(dir, assemblyName + ".ini")
    Load()

  void Load()
    options = GetDefaultOptions()
    foreach FieldInfo f in options.GetType().GetFields(Public|Instance):
      if f.FieldType == bool:   f.SetValue(options, GetPrivateProfileInt(assemblyName, f.Name, ...) != 0)
      if f.FieldType == string: f.SetValue(options, GetPrivateProfileString(assemblyName, f.Name, ..., 32767, iniFilePath))
      if f.FieldType.IsEnum:    f.SetValue(options, Enum.Parse(f.FieldType, GetPrivateProfileString(...)))

  void Save()
    foreach FieldInfo f in options.GetType().GetFields(Public|Instance):
      if f.FieldType == bool:   WritePrivateProfileString(assemblyName, f.Name, value ? "1" : "0", iniFilePath)
      else:                     WritePrivateProfileString(assemblyName, f.Name, f.GetValue(options).ToString(), iniFilePath)

  bool ValidateFileExtension(string ext, string fileName)
    if options.fileExtensions empty: return true
    if ext empty:
      return options.inclNewFiles && fileName.StartsWith("new ", OrdinalIgnoreCase)
    return options.fileExtensions.IndexOf(ext, OrdinalIgnoreCase) >= 0
}
```

---

## RENDERING_PIPELINE

### DATA_FLOW
```
Notepad++ text change / file switch / scroll
  -> beNotified(ScNotification)
  -> Main.OnNotification(n)
  -> MarkdownViewer.OnNotification(n)
  -> MarkdownViewer.Update(updateScrollBar, updateRenderer)
  -> MarkdownViewer.UpdateMarkdownViewer()
       Editor.GetText(length+1) -> rawText
  -> AbstractRenderer.Render(rawText, FileInfo)
       Markdig.Markdown.ToHtml(rawText, markdownPipeline) -> html
  -> MarkdownViewerRenderer.Render(html, FileInfo) [override]
       BuildHtml(html, fileName) -> fullHtmlDocument
  -> MarkdownViewerHtmlPanel.Text = fullHtmlDocument
       _htmlContainer.SetHtml(text, _baseCssData)
       Redraw() -> PerformLayout(); Invalidate(); InvokeMouseMove()
  -> HtmlRenderer renders to WinForms panel
```

### ABSTRACT_RENDERER (Forms/AbstractRenderer.cs)
```
abstract class AbstractRenderer : Form {
  protected MarkdownViewer markdownViewer
  protected string assemblyTitle
  protected Icon toolbarIcon

  protected MarkdownPipeline markdownPipeline
    = new MarkdownPipelineBuilder().UseAdvancedExtensions().Build()

  protected string patternCSSImportStatements = "(@import\\s.+;)"

  // toolbar buttons:
  //   Refresh, Export HTML, Export PDF, Print, Clipboard, Dropdown (mail/options/about)

  void Init()
    register dockable window:
      NppTbData { hClient=Handle, pszName="MarkdownViewer++", dlgID=commandId,
                  uMask=DWS_ICONTAB|DWS_ICONBAR|DWS_DF_CONT_RIGHT,
                  hIconTab=toolbarIconHandle, pszModuleName="MarkdownViewerPlusPlus" }
      SendMessage(NPPM_DMMREGASDCKDLG, 0, ref tbData)
    IsOutlookInstalled() -> show/hide mail buttons

  override void WndProc(ref Message m)
    if m.Msg == WM_NOTIFY (0x4E):
      nmhdr = (NMHDR)Marshal.PtrToStructure(m.LParam, typeof(NMHDR))
      if nmhdr.code == DMN_CLOSE:
        markdownViewer.ToggleToolbarIcon(false)

  virtual void Render(string text, FileInformation fileInfo)
    RawText = text
    FileInfo = fileInfo
    ConvertedText = Markdown.ToHtml(text, markdownPipeline)

  string BuildHtml(string html, string title="MarkdownViewer++")
    css = getCSS()
    return "<!DOCTYPE html><html><head><title>{title}</title><style>{css}</style></head><body>{html}</body></html>"

  string getCSS()
    extract @import statements from htmlCssStyle (regex)
    return imports + Resources.MarkdownViewerHTML + custom_css_without_imports

  int MilimiterToPoint(int mm) -> (int)(mm * 2.834646)

  bool IsOutlookInstalled()
    Type.GetTypeFromProgID("Outlook.Application") != null

  abstract void ScrollByRatioVertically(double scrollRatio)

  // Export handlers:

  void exportAsHTMLMenuItem_Click()
    SaveFileDialog -> .html
    File.WriteAllText(path, BuildHtml(ConvertedText, fileName))
    optional: XDocument.Parse for pretty-printing
    if options.htmlOpenExport: Process.Start(path)

  void exportAsPDFMenuItem_Click()
    SaveFileDialog -> .pdf
    PdfGenerateConfig {
      PageOrientation = options.pdfOrientation
      PageSize = options.pdfPageSize
      MarginTop = MilimiterToPoint(margins[0])
      MarginRight = MilimiterToPoint(margins[1])
      MarginBottom = MilimiterToPoint(margins[2])
      MarginLeft = MilimiterToPoint(margins[3])
    }
    pdf = PdfGenerator.GeneratePdf(BuildHtml(), config, getCSS())
    pdf.Save(path)
    if options.pdfOpenExport: Process.Start(path)

  void sendToPrinter_Click()
    WebBrowser wb = new WebBrowser()
    wb.DocumentText = BuildHtml()
    wb.DocumentCompleted += (s,e) => wb.ShowPrintPreviewDialog()

  void sendToClipboard_Click()
    ClipboardHelper.CopyToClipboard(BuildHtml(), ConvertedText)

  void sendAsTextMail_Click() / sendAsHTMLMail_Click()
    Outlook.Application app = new Outlook.Application()
    MailItem mail = app.CreateItem(olMailItem)
    mail.Subject = FileInfo.FileName
    mail.Body / mail.HTMLBody = text/html
    mail.Display()
}
```

### MARKDOWN_VIEWER_RENDERER (Forms/MarkdownViewerRenderer.cs)
```
class MarkdownViewerRenderer : AbstractRenderer {
  MarkdownViewerHtmlPanel markdownViewerHtmlPanel

  void Init()
    base.Init()
    markdownViewerHtmlPanel = new MarkdownViewerHtmlPanel()
    markdownViewerHtmlPanel.ImageLoad += OnImageLoad
    Controls.Add(markdownViewerHtmlPanel)
    Controls.SetChildIndex(markdownViewerHtmlPanel, 0)

  override void Render(string text, FileInformation fileInfo)
    base.Render(text, fileInfo)                             // sets ConvertedText
    string fileName = fileInfo.FileName ?? "MarkdownViewer++"
    markdownViewerHtmlPanel.Text = BuildHtml(ConvertedText, fileName)

  override void ScrollByRatioVertically(double scrollRatio)
    markdownViewerHtmlPanel.ScrollByRatioVertically(scrollRatio)

  void OnImageLoad(object sender, HtmlImageLoadEventArgs evt)
    src = evt.Src
    if src.StartsWith("file://"):
      ThreadPool.QueueUserWorkItem(_ => LoadImageFromFile(src, evt))
    elif (src.StartsWith("http") || src.StartsWith("https")) && src.EndsWith(".svg"):
      WebClient wc = new WebClient()
      wc.DownloadDataCompleted += (s,e) => OnDownloadDataCompleted(e, evt)
      wc.DownloadDataAsync(new Uri(src))
    else:
      evt.Handled = true   // prevent error border; let HtmlRenderer handle

  void LoadImageFromFile(string src, HtmlImageLoadEventArgs evt)
    path = parse file:// URI
    if not absolute: path = Path.Combine(FileInfo.FileDirectory, path)
    if path.EndsWith(".svg"):
      SvgDocument doc = SvgDocument.Open(path)
      ConvertSvgToBitmap(doc, evt)
    else:
      Image img = Image.FromFile(path)
      evt.Callback(img)
      evt.Handled = true

  Bitmap ConvertSvgToBitmap(SvgDocument svgDoc, HtmlImageLoadEventArgs evt)
    Bitmap bmp = new Bitmap(svgDoc.Width, svgDoc.Height, PixelFormat.Format32bppArgb)
    svgDoc.Draw(bmp)
    evt.Callback(bmp)
    evt.Handled = true
    return bmp

  void OnDownloadDataCompleted(DownloadDataCompletedEventArgs dl, HtmlImageLoadEventArgs evt)
    MemoryStream ms = new MemoryStream(dl.Result)
    SvgDocument doc = SvgDocument.Open(ms)
    ConvertSvgToBitmap(doc, evt)
}
```

### MARKDOWN_VIEWER_HTML_PANEL (Forms/MarkdownViewerHtmlPanel.cs)
```
class MarkdownViewerHtmlPanel : HtmlPanel {
  // inherits: HtmlRenderer.WinForms.HtmlPanel

  constructor:
    AllowDrop = false
    Dock = DockStyle.Fill
    IsContextMenuEnabled = false
    AvoidImagesLateLoading = false

  override string Text {
    get { return _text }
    set {
      _text = value
      if !IsDisposed:
        _htmlContainer.SetHtml(_text, _baseCssData)
        Redraw()
    }
  }

  void ScrollByRatioVertically(double scrollRatio)
    int max = VerticalScroll.Maximum
    int largeChange = VerticalScroll.LargeChange
    VerticalScroll.Value = (int)((max - largeChange) * scrollRatio)
    Redraw()

  void Redraw()
    PerformLayout()
    Invalidate()
    InvokeMouseMove()
}
```

---

## MARKDIG_PIPELINE

```
markdownPipeline = new MarkdownPipelineBuilder()
  .UseAdvancedExtensions()   // enables all of:
  .Build()

UseAdvancedExtensions() enables:
  UseAbbreviations()
  UseAutoIdentifiers()
  UseCitations()
  UseCustomContainers()
  UseDefinitionLists()
  UseEmphasisExtras()
  UseFigures()
  UseFooters()
  UseFootnotes()
  UseGridTables()
  UseMathematics()
  UseMediaLinks()
  UsePipeTables()
  UseListExtras()
  UseTaskLists()
  UseEmojiAndSmiley()
  UseAutoLinks()
  UseYamlFrontMatter()
  UseGenericAttributes()
  UseSmartyPants()
  UseBootstrap()
```

---

## OPTIONS_DIALOG

### MARKDOWN_VIEWER_OPTIONS (Forms/MarkdownViewerOptions.cs)
```
class MarkdownViewerOptions : Form {
  MarkdownViewerConfiguration configuration
  Dictionary<string, AbstractOptionsPanel> optionPanels
  delegate void SaveHandler(ref Options options)
  delegate void LoadHandler(Options options)
  SaveHandler SaveEvent
  LoadHandler LoadEvent

  constructor(ref MarkdownViewerConfiguration config)
    stores config reference
    uses reflection to instantiate panels:
      ["General"] = new OptionsPanelGeneral()
      ["HTML"]    = new OptionsPanelHTML()
      ["PDF"]     = new OptionsPanelPDF()
    foreach panel: SaveEvent += panel.SaveOptions; LoadEvent += panel.LoadOptions
    splitOptions.Panel2.Controls.Add(optionPanels["General"])
    LoadEvent(config.options)

  treeOptions_AfterSelect:
    remove old panel from splitOptions.Panel2
    add optionPanels[selectedNode.Text]

  btnOptionsSave_Click:
    SaveEvent(ref configuration.options)
    configuration.Save()
    Close()
}
```

### ABSTRACT_OPTIONS_PANEL
```
abstract class AbstractOptionsPanel : UserControl {
  constructor: Anchor=Bottom|Right; Dock=Fill
  virtual void LoadOptions(Options options)
  virtual void SaveOptions(ref Options options)
}
```

### OPTIONS_PANEL_GENERAL
```
controls: txtFileExtensions (TextBox), chkBoxNewFiles (CheckBox)
regex for validation: "^([a-zA-Z,]*)$"
tooltip message: "Add comma-separated extensions..."
LoadOptions: chkBoxNewFiles.Checked=inclNewFiles; txtFileExtensions.Text=fileExtensions
SaveOptions: options.inclNewFiles=chkBoxNewFiles.Checked; options.fileExtensions=txtFileExtensions.Text
```

### OPTIONS_PANEL_HTML
```
controls: txtCssStyles (TextBox, maxLength=32767), chkOpenHTMLExport (CheckBox)
LoadOptions: txtCssStyles.Text=HtmlCssStyle; chkOpenHTMLExport.Checked=htmlOpenExport
SaveOptions: options.HtmlCssStyle=txtCssStyles.Text; options.htmlOpenExport=chkOpenHTMLExport.Checked
```

### OPTIONS_PANEL_PDF
```
controls: cmbPDFOrientation (ComboBox), cmbPDFPageSize (ComboBox),
          numMarginLeft/Top/Right/Bottom (NumericUpDown, unit=mm), chkOpenPDFExport (CheckBox)
LoadOptions:
  populate orientation from Enum.GetNames(PageOrientation)
  populate page size from Enum.GetNames(PageSize) minus "Undefined"
  set selected items; load margins array [top,right,bottom,left]
SaveOptions:
  Enum.TryParse orientation/pageSize
  margins = $"{left},{top},{right},{bottom}"
  options.pdfOpenExport = chkOpenPDFExport.Checked
```

---

## CLIPBOARD_HELPER (Helper/ClipboardHelper.cs)

```
CF_HTML format (Windows clipboard HTML):
  Header section (ASCII):
    "Version:0.9\r\n"
    "StartHTML:{offset:09}\r\n"        offset to <html> tag
    "EndHTML:{offset:09}\r\n"          offset after </html>
    "StartFragment:{offset:09}\r\n"    offset to content
    "EndFragment:{offset:09}\r\n"      offset after content
    "StartSelection:{offset:09}\r\n"   same as StartFragment
    "EndSelection:{offset:09}\r\n"     same as EndFragment
  Body:
    <!DOCTYPE HTML PUBLIC "-//W3C//DTD HTML 4.0 Transitional//EN">
    <html><body>
    <!--StartFragment-->
    {content}
    <!--EndFragment-->
    </body></html>

class ClipboardHelper {
  static void CopyToClipboard(string html, string plainText)
    DataObject data = CreateDataObject(html, plainText)
    Clipboard.SetDataObject(data, copy:true)

  static DataObject CreateDataObject(string html, string plainText)
    string cfHtml = GetHtmlDataString(html)
    byte[] bytes = Encoding.UTF8.GetBytes(cfHtml)
    DataObject data = new DataObject()
    data.SetData(DataFormats.Html, new MemoryStream(bytes))
    data.SetData(DataFormats.Text, plainText)
    data.SetData(DataFormats.UnicodeText, plainText)
    return data

  private static string GetHtmlDataString(string html)
    // builds CF_HTML header
    // calculates byte offsets for all markers
    // back-patches placeholder strings with actual offsets
    // handles: @import before html tag, fragment markers, html/body wrap if absent
}
```

---

## WINDOWS_MESSAGE (Windows/WindowsMessage.cs)

```
struct NMHDR {
  IntPtr hwndFrom
  IntPtr idFrom
  int code
}

enum WindowsMessage : uint {
  WM_NULL=0x0, WM_CREATE=0x1, WM_DESTROY=0x2,
  WM_NOTIFY=0x4E,   [key: dock close notification]
  WM_NCCREATE=0x81,
  // 200+ constants total
}

enum DockMgrMsg {
  DMN_CLOSE    // sent when dockable panel X button clicked
}
```

---

## EVENT_FLOW_SEQUENCES

### PLUGIN_LOAD
```
Notepad++ loads DLL
-> isUnicode() = true
-> setInfo(NppData)
     PluginBase.nppData = data
     CommandMenuInit()
       new MarkdownViewerConfiguration().Init()    [loads INI]
       PluginBase.SetCommand(0..5)
-> getFuncsArray(&n) = _funcItems.NativePointer
-> beNotified(NPPN_TBMODIFICATION)
     Main.SetToolBarIcon()
       bitmap = Resources.markdown_16x16_solid
       NPPM_ADDTOOLBARICON
```

### TOGGLE_RENDERER (Ctrl+Shift+M)
```
User: Ctrl+Shift+M
-> MarkdownViewerCommand()
   rendererVisible = !rendererVisible
   if visible:
     UpdateEditorInformation()
       GetCurrentScintilla() -> Editor handle
       GetCurrentFileName() -> FileInfo.FileName
       GetCurrentDirectory() -> FileInfo.FileDirectory
     Update(scroll=true, render=true)
       ValidateFileExtension()
       UpdateMarkdownViewer()
         Editor.GetText(len+1) -> text
         renderer.Render(text, FileInfo)
           Markdown.ToHtml(text, pipeline) -> html
           BuildHtml(html, name) -> fullDoc
           htmlPanel.Text = fullDoc
             _htmlContainer.SetHtml(fullDoc, baseCss)
             Redraw()
       UpdateScrollBar()
         GetScrollInfo(SB_VERT)
         calc ratio
         renderer.ScrollByRatioVertically(ratio)
           htmlPanel.ScrollByRatioVertically(ratio)
             VerticalScroll.Value = (max-large)*ratio
             Redraw()
     renderer.SetFocus()
   ToggleToolbarIcon(visible)
     NPPM_DMMSHOW or NPPM_DMMHIDE
     NPPM_SETMENUITEMCHECK
```

### TEXT_CHANGE_UPDATE
```
User types/pastes/deletes
-> beNotified(SCN_MODIFIED)
   n.ModificationType & SC_MOD_INSERTTEXT | SC_MOD_DELETETEXT
   updateRenderer = true

-> beNotified(SCN_UPDATEUI)
   if updateRenderer:
     Update(render=true)
       UpdateMarkdownViewer()
         [same as above: getText -> render -> htmlPanel.Text]
       updateRenderer = false
```

### FILE_SWITCH
```
User clicks tab
-> beNotified(NPPN_BUFFERACTIVATED)
   UpdateEditorInformation()    [update FileInfo for new file]
   Update(scroll=true, render=true)
     ValidateFileExtension()   [check if new file should render]
     UpdateMarkdownViewer()
     UpdateScrollBar()
```

### SCROLL_SYNC
```
User scrolls editor
-> beNotified(SCN_UPDATEUI, SC_UPDATE_V_SCROLL)
   Update(scroll=true)
     UpdateScrollBar()
       GetScrollInfo(SIF_RANGE|SIF_TRACKPOS|SIF_PAGE)
       nTrackPos, nMax, nPage
       if !GetEndAtLastLine():
         [edge case: extra space at bottom]
         nActualMax = nMax - (nPage - nPage * nMax / (nMax + ???))
         ratio = min(1.0, adjusted / range)
       else:
         ratio = nTrackPos / (nMax - nPage)
       renderer.ScrollByRatioVertically(ratio)
```

### DOCKABLE_PANEL_CLOSE (X button)
```
User clicks X on docked panel
-> WM_NOTIFY -> DMN_CLOSE
   AbstractRenderer.WndProc()
   markdownViewer.ToggleToolbarIcon(false)
     NPPM_DMMHIDE
     NPPM_SETMENUITEMCHECK(false)
```

---

## BUILD_SYSTEM

### CSPROJ_HIGHLIGHTS
```
AllowUnsafeBlocks: true
Platforms: x86, x64, AnyCPU
DefineConstants: TRACE; [DEBUG|RELEASE]; v3_5; CORE; CORE_WITH_GDI

PostBuild targets:
  1. DllExportTask: convert managed exports -> native DLL exports
  2. Copy output DLL -> %ProgramFiles%\Notepad++\plugins\ (if exists)
  3. Extract version from AssemblyInfo.cs via regex
  4. mkdir Releases/
  5. ILMerge all dependencies into single DLL
  6. Create Releases/MarkdownViewerPlusPlus-{version}-{platform}.zip
     contents: LICENSE.md, license/, output DLL, README.md
```

### ILMERGE_ORDER (ILMergeOrder.txt)
```
1. [Primary assembly]
2. HtmlRenderer.Core
3. HtmlRenderer.WinForms
4. HtmlRenderer.PdfSharp
5. Markdig
6. Svg
7. PdfSharp
```

### ASSEMBLY_BINDING (app.config)
```xml
<dependentAssembly>
  <assemblyIdentity name="PdfSharp" ... />
  <bindingRedirect oldVersion="0.0.0.0-1.50.4845.0" newVersion="1.50.4845.0"/>
</dependentAssembly>
```

---

## EMBEDDED_RESOURCES (Properties/Resources.resx)

```
Resources.markdown_16x16_solid    Bitmap   toolbar icon (hbitmap)
Resources.MarkdownViewerHTML       string   base CSS stylesheet
icon files (Font Awesome 16x16 PNG):
  fa-refresh.png
  fa-html5.png
  fa-file-pdf-o.png
  fa-print.png
  fa-clipboard.png
  fa-cog.png
  fa-info.png
```

---

## COMMAND_MENU_TABLE

| idx | name | handler | shortcut | checkable | initial_check |
|---|---|---|---|---|---|
| 0 | MarkdownViewer++ | MarkdownViewerCommand | Ctrl+Shift+M | no | false |
| 1 | --- | null | - | - | - |
| 2 | Synchronize scrolling | SynchronizeScrollingCommand | none | yes | options.synchronizeScrolling |
| 3 | --- | null | - | - | - |
| 4 | Options | OptionsCommand | none | no | false |
| 5 | About | AboutCommand | none | no | false |

---

## THREADING_PATTERNS

```
main_thread: all WinForms operations (guaranteed by Notepad++ plugin model)

async_SVG_file:
  ThreadPool.QueueUserWorkItem(state => LoadImageFromFile(src, evt))
  -> runs on thread pool
  -> calls evt.Callback(bitmap)  [HtmlRenderer marshals to UI thread]

async_SVG_web:
  WebClient.DownloadDataAsync(uri)
  WebClient.DownloadDataCompleted += OnDownloadDataCompleted
  -> runs on thread pool
  -> ConvertSvgToBitmap -> evt.Callback(bitmap)

no_BackgroundWorker: direct ThreadPool usage only
```

---

## NAMESPACE_HIERARCHY

```
Kbg.NppPluginNET
  Main                             plugin stub
  PluginInfrastructure
    PluginBase                     singleton gateway state
    UnmanagedExports               DLL entry points
    NotepadPPGateway               Notepad++ API abstraction
    ScintillaGateway               Scintilla API abstraction
    Win32                          P/Invoke declarations
    [enums/structs]                Msgs_h, Docking_h, GatewayDomain, etc.

com.insanitydesign.MarkdownViewerPlusPlus
  MarkdownViewer                   core plugin class
  MarkdownViewerConfiguration      INI config
  FileInformation                  file metadata struct
  Forms
    AbstractRenderer               base dockable form + export logic
    MarkdownViewerRenderer         concrete renderer with HtmlPanel
    MarkdownViewerHtmlPanel        HtmlPanel + scroll sync
    MarkdownViewerOptions          options dialog
    AbstractOptionsPanel           option tab base
    OptionsPanelGeneral            general tab
    OptionsPanelHTML               HTML/CSS tab
    OptionsPanelPDF                PDF settings tab
    AboutDialog                    about dialog
  Helper
    ClipboardHelper                CF_HTML clipboard format
  Windows
    WindowsMessage                 WM_* constants + NMHDR

NppPlugin.DllExport
  DllExportAttribute               marks methods for native export
```

---

## CRITICAL_INVARIANTS

```
1. text_retrieval: always GetLength() then GetText(length+1) — null terminator required
2. debounce: SCN_MODIFIED sets flag; SCN_UPDATEUI consumes flag — prevents render on every keystroke
3. extension_validation: empty extensions string = render all; empty ext + inclNewFiles = render "new X" files
4. scroll_ratio: clamped [0.0, 1.0]; edge case when !GetEndAtLastLine() needs adjusted calculation
5. ilmerge: all deps merged into single DLL — no separate assemblies shipped
6. dllexport: DllExportTask post-build step is required — without it managed exports are not native
7. css_import: @import statements must appear before other CSS; getCSS() extracts and prepends them
8. cf_html: byte offsets in CF_HTML header must be exact UTF-8 byte positions — not char positions
9. svg_async: SVG loads are async; evt.Handled=true prevents HtmlRenderer from showing error border
10. ini_reflection: Options struct fields are serialized by field name — field renames break existing configs
```

---

## LIVE_SESSION_CONTEXT_2026_04_27

Current active session log:

```text
SESSION_LOG_2026-04-27_1617_PDT.md
```

User standing instructions for the current Markdown++ native rewrite session:

```text
1. Keep the active session log updated as work progresses.
2. Keep this AI_UNDERSTANDING.md file updated after each user prompt with durable context.
3. After each assistant response, recommend the next strongest move.
4. Treat untracked native rewrite files as important project work.
5. Keep the legacy C# MarkdownViewerPlusPlus project until native Markdown++ reaches practical parity.
```

Current checkpoint:

```text
The side-by-side C++ x64 native Markdown++ plugin is built around WebView2,
cmark-gfm, offline Mermaid, local document virtual-host mapping, in-place
preview updates, source-line-based scroll sync, and a 140ms edit debounce that
the user confirmed works properly. Navigation clicks no longer cause false full
refreshes, the x64 Scintilla SCNotification layout is corrected, selection/caret
updates prefer caret-based preview sync, debounced text-edit renders anchor to
the editing/caret source line, and the latest alignment change carries the
caret's relative editor viewport position into WebView scroll requests so the
matching preview block is not always forced to the top. The newest tweak prevents
generic SCN_UPDATEUI events from doing top-anchor click sync, lets caret-preferred
sync bypass the editor scroll throttle, and includes anchor ratio in duplicate
detection. A preview EOF spacer now mimics Notepad++'s blank space after the
final source lines so end-of-document sync is not clamped by the WebView maximum
scroll position. User confirmed the EOF spacer works perfectly, so scroll sync is
good enough for this pass. The latest change renders Mermaid blocks independently
and shows visible inline parse/render errors with the original source when a
Mermaid block is malformed. User confirmed the malformed Mermaid test shows the
expected inline `Mermaid render error` panel with parse error text and source.
The latest change adds first-pass native options plumbing with persisted INI
settings and menu controls for Mermaid rendering, scroll sync, and render
debounce delay. User confirmed the native options pass is working. The plugin
install/copy automation through tools/install-native-plugin.ps1 and the -Install
path on tools/build-native.ps1 are working. User confirmed the installed plugin
is present and showed that the Options command still opened the temporary summary
message box. The native Options dialog was tested and rejected: it produced a
save-error message and the user preferred the previously working menu-selectable
options. The latest change removes the Options dialog and the Options menu item
entirely, keeps settings as direct plugin-menu toggles, adds `Open automatically
for Markdown files` as a checked menu option, restores the simpler pre-dialog INI
save path, and keeps auto-open/auto-close on `NPPN_READY` and
`NPPN_BUFFERACTIVATED` for `.md`, `.markdown`, `.mdown`, `.mkd`, and `.mkdn`.
User confirmed the menu-only options work great. The latest change replaces the
flat `Cycle render debounce delay` command with a dynamic `Render debounce`
submenu that uses `NPPM_ALLOCATECMDID`, offers `0`, `80`, `140`, `250`, and
`500` ms choices, handles submenu clicks in `messageProc(WM_COMMAND, ...)`, saves
the selected value, and checks the current debounce value. User asked what
debounce means, agreed `Preview update delay` is clearer, and the visible submenu
label was renamed to `Preview update delay` while internal debounce naming stayed.
User confirmed the renamed submenu works and the selected delay checkmark
carries/persists across restart. The latest change implements local Markdown
link clicks from the preview back into Notepad++: preview JS posts raw local link
hrefs to WebView2, WebViewHost forwards a link callback, PluginController
resolves relative/virtual-host/file URLs to canonical Windows paths, and existing
Markdown files are opened through `NPPM_DOOPEN`. A focused smoke test file was
added at `smoke-tests/smoke-test.md`. User confirmed validation passed for local
Markdown link click handling. The latest change extends the same click path so
`http://` and `https://` links open in the default browser through
`ShellExecuteW`, while local Markdown links continue opening in Notepad++.
`smoke-tests/smoke-test.md` now covers both local and external link behavior.
User reported external URL clicks opened both the intended site and another
browser instance under a different profile. The latest fix excludes
`markdownplusplus.document` / `markdownplusplus.local` virtual hosts from
external detection and handles/cancels WebView2 `NewWindowRequested`, leaving
native `ShellExecuteW` as the single external-open path. User confirmed all link
behavior is now working. The latest change implements local fragment-only anchor
scrolling in preview JS: `#heading` links find an `id`/`name` target, scroll it
into view, update the hash, and do not post to native code. User reported the
fragment did not jump because rendered headings did not have IDs. The latest fix
generates GitHub-style heading IDs in preview JS on initial load and content
replacement, and anchor lookup falls back to matching heading text slugs. User
then confirmed the fragment jumps but the Notepad++ editor does not sync. The
latest fix makes anchor jumps post one explicit preview-scroll message after
`scrollIntoView()`, while normal scroll events still obey suppression. User then
reported Notepad++ still does not scroll. The latest stronger fix adds a
`force` boolean to preview scroll messages, sends `force: true` plus the target
heading's `data-sourcepos` start line for anchor jumps, parses that through
WebViewHost, and makes PluginController bypass suppression/throttle/duplicate
checks only for forced anchor jumps.

Current anchor-link status: fixed and confirmed. `smoke-tests/smoke-test.md` ->
`Jump to expected behavior` now moves the WebView preview to
`## Expected Behavior` and scrolls the Notepad++ editor to the matching source
heading. Local Markdown links and external browser links remain accepted and
working.

Current confirmed anchor-sync path:

```text
preview.js scrollToAnchor()
  -> target.scrollIntoView()
  -> compute sourceLineForNode(target) before hash/history update
  -> try hash/history update without letting failures stop sync
  -> postScrollMessage(sourceLine, currentScrollRatio(), true) immediately
  -> retry the same forced post on requestAnimationFrame and after 80 ms
WebViewHost::HandleWebMessage()
  -> TryParsePreviewScrollMessage(... line, ratio, force)
  -> scrollCallback_(line, ratio, force)
PluginController::HandlePreviewScroll(line, ratio, force)
  -> bypass suppression/throttle/duplicate checks if force
  -> ScrollEditorToSourceLine(line) if line > 0
```

Resolved root cause:

```text
The diagnostic DLL loaded and WebView message handling registered, but no
forced anchor raw-message entries appeared. The working patch hardened the
preview-side anchor path so sync is posted before any hash/history issue can
abort delivery, then repeated after layout timing. Temporary diagnostics were
removed after user confirmation.
```
```

Current build command to give the user:

```powershell
cd Z:\home\Vibe-Projects\MarkdownViewerPlusPlus
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

Current manual test target:

```text
Z:\home\Vibe-Projects\MarkdownViewerPlusPlus\smoke-tests\markdownplusplus-smoke.md
```

Current next strongest move:

```text
Rebuild/install the cleaned native build on Windows and quickly retest
smoke-tests/smoke-test.md -> Jump to expected behavior once to verify the
confirmed anchor-sync behavior remains after diagnostic cleanup.
```

Latest prompt update:

```text
2026-04-28 00:32 PDT: User reported forced anchor-sync still does not scroll
Notepad++ and asked to pick it up in another session. Leave current state as:
preview anchor jumps work, editor sync does not. Next strongest move is
instrumenting preview.js -> WebViewHost -> PluginController -> Scintilla scroll
chain to find whether the issue is message delivery/parsing, missing source line,
or Scintilla scroll math.
```

```text
2026-04-28 00:33 PDT: Session handoff finalized. User wants to continue in a
new session. The next session should begin with temporary diagnostics, not blind
patches. Instrument preview.js anchor postMessage payload, WebViewHost raw/parsed
previewScroll handling, PluginController HandlePreviewScroll, and
ScrollEditorToSourceLine. Repro remains smoke-tests/smoke-test.md -> Jump to
expected behavior: preview jumps correctly, Notepad++ editor does not scroll.
After identifying the broken link in the chain, patch it and remove diagnostics.
```

```text
2026-04-29 21:54 PDT: User asked to understand the project and pick up where the
previous session left off. Re-read the handoff and implemented the requested
temporary diagnostics for the unresolved fragment anchor editor-sync bug. Anchor
previewScroll messages now include debugReason:"anchor", target id/text,
computed targetSourceLine, and targetSourcepos. WebViewHost now logs forced
anchor raw JSON and parsed scroll/link/line/ratio/force/hasScrollCallback to
OutputDebugStringW and %TEMP%\MarkdownPlusPlus-anchor-debug.log.
PluginController now logs forced HandlePreviewScroll state and
ScrollEditorToSourceLine Scintilla math, including sourceLine, documentLine,
targetVisibleLine, currentFirstVisibleLine, delta, and whether SCI_LINESCROLL
was sent. Validation performed: cmake --build build/cmake/linux-check --config
Release and trailing-whitespace scan on the touched files. Full native compile
still requires the Windows/WebView2/MSVC build path. Next strongest move:
rebuild/install on Windows, click smoke-tests/smoke-test.md -> Jump to expected
behavior, inspect %TEMP%\MarkdownPlusPlus-anchor-debug.log, patch the identified
break, and remove diagnostics.
```

```text
2026-04-30: User confirmed the hardened anchor path worked. Cleaned up temporary
JS previewDebug payloads and native %TEMP%/OutputDebugString diagnostics while
keeping the behavior fix: source line is computed before hash/history update,
hash/history update is try/catch guarded, and forced previewScroll posts are
sent immediately, on requestAnimationFrame, and after 80 ms. Validation after
cleanup: node --check preview.js, cmake --build build/cmake/linux-check
--config Release, no leftover diagnostic strings, no trailing whitespace.
```

---

## QUICK_RELOAD_CONTEXT_2026_04_30

Use this section first when resuming work so the project does not need to be
rediscovered from scratch.

Repository shape:

```text
root=/home/Vibe-Projects/MarkdownViewerPlusPlus
branch=master
working_tree=intentionally_dirty
important_warning=Native rewrite files are untracked but important project work.
```

Two implementation tracks:

```text
1. Legacy C# plugin
   path=MarkdownViewerPlusPlus/
   project=MarkdownViewerPlusPlus.csproj
   solution=MarkdownViewerPlusPlus.sln
   runtime=.NET Framework 4.0 WinForms Notepad++ plugin
   dependencies=Markdig, HtmlRenderer, PDFSharp, Svg, ILMerge, DllExport
   flow=UnmanagedExports.cs -> Main.cs -> MarkdownViewer.cs -> WinForms renderer
   features=preview, scroll sync, custom CSS, HTML/PDF export, clipboard, print, Outlook email

2. Native rewrite
   path=MarkdownPlusPlus.Native/
   build=CMake + Visual Studio 2026 x64
   plugin_name=Markdown++
   runtime=C++ Notepad++ DLL with direct native exports
   dependencies=WebView2 Evergreen runtime, WebView2 SDK, cmark-gfm, bundled Mermaid
   flow=PluginExports.cpp -> PluginController -> PreviewWindow -> WebViewHost -> assets/preview.js
   features=WebView2 preview, cmark-gfm GFM render, sourcepos scroll sync, Mermaid, local images,
            local Markdown links, external URL opening, menu options, preview update delay
```

Native build/install command:

```powershell
cd Z:\home\Vibe-Projects\MarkdownViewerPlusPlus
powershell -ExecutionPolicy Bypass -File .\tools\build-native.ps1 -Configuration Release -Install
```

Current manual smoke files:

```text
smoke-tests/markdownplusplus-smoke.md
smoke-tests/smoke-test.md
```

Current accepted native behavior:

```text
- Plugin loads as Markdown++.
- Dockable WebView2 preview opens.
- Auto-open for Markdown files works.
- cmark-gfm renders GFM features.
- Bundled Mermaid renders and malformed Mermaid shows visible inline errors.
- Local SVG/images render through the document virtual host.
- External HTTPS images render.
- Local Markdown links clicked in preview open/switch in Notepad++.
- External http/https links clicked in preview open once in the default browser.
- Preview update delay submenu persists 0/80/140/250/500 ms selection.
- Menu-only options are preferred; do not reintroduce the rejected Options dialog.
- General editor/preview scroll sync is good enough for this pass, including EOF spacer alignment.
- Fragment-only preview links scroll both the WebView preview and the Notepad++ editor to the target source heading.
- Copy HTML to clipboard works and uses rendered HTML fallback, not raw Markdown.
- Standalone HTML export works, including fragment-only anchor jumps.
- PDF export works as a stand-alone PDF without markdownplusplus.document/local .md permission prompts.
- Print opens the WebView2 system print UI and works.
```

Current active issue:

```text
No known active blocker. Native parity hardening is accepted for this pass after
Windows rebuild/install and manual validation of copy, HTML export, PDF export,
and print.
```

Current pending parity validation:

```text
MarkdownPlusPlus.Native/src/ClipboardUtil.cpp/.h
  New CF_HTML clipboard helper with UTF-8 byte offsets and CF_UNICODETEXT
  fallback. The text fallback should be the rendered HTML fragment, not raw
  Markdown, to match the legacy C# plugin.

MarkdownPlusPlus.Native/src/PluginController.cpp/.h
  New commands: Copy HTML to clipboard, Export HTML..., Export PDF..., Print...
  Copy/HTML export render directly from the current Scintilla buffer. PDF/print
  use the live WebView2 preview and ask the user to rerun after the preview
  loads if the document is not ready.

MarkdownPlusPlus.Native/src/MarkdownRenderer.cpp/.h
  New BuildStandaloneDocument() emits UTF-8 export HTML with embedded
  preview.css, file:// base URI for the current Markdown directory, and optional
  file:// Mermaid script reference. It always includes preview.js because that
  script assigns heading IDs and handles standalone fragment jumps even when
  Mermaid is disabled.

MarkdownPlusPlus.Native/src/WebViewHost.cpp/.h and PreviewWindow.cpp/.h
  New WebView2 PrintToPdf and ShowPrintUI wrappers. PrintToPdf now asks
  preview.js to temporarily strip preview-local/relative/file hrefs before PDF
  generation so exported PDFs do not contain markdownplusplus.document or local
  .md link annotations. Fragment and external HTTPS links remain.

MarkdownPlusPlus.Native/assets/preview.css
  New @media print override hides the EOF spacer for print/PDF so scroll-sync
  slack does not create trailing blank pages.
```

Fragment-link editor-sync fix:

```text
MarkdownPlusPlus.Native/assets/preview.js
  scrollToAnchor() computes sourceLineForNode(target) before hash/history update,
  wraps the hash/history update in try/catch, and posts forced previewScroll
  immediately, on requestAnimationFrame, and after 80 ms.

MarkdownPlusPlus.Native/src/WebViewHost.cpp / PluginController.cpp
  Existing forced previewScroll path is retained: WebViewHost parses force=true
  and PluginController bypasses normal suppression/throttle/duplicate checks for
  forced anchor jumps before calling ScrollEditorToSourceLine(sourceLine).

Temporary diagnostics status:
  Removed after user confirmed the fix worked. Do not expect
  %TEMP%\MarkdownPlusPlus-anchor-debug.log to be created in the cleaned build.
```

Next strongest move:

```text
Discuss and choose the next feature gap. Strong candidates: theme/custom CSS
controls, configurable Markdown file extensions, find-in-preview/context
commands, accessibility/keyboarding polish, or release packaging automation and
versioning. Keep menu-only settings; do not reintroduce the rejected Options
dialog.
```

Latest prompt update:

```text
2026-04-30: User asked to put the project understanding context into
AI_UNDERSTANDING.md so future sessions do not need to rediscover it. Added this
quick reload context capturing repo shape, legacy/native tracks, accepted native
behavior, the then-active fragment-link editor-sync bug, diagnostics,
build/test commands, and the next strongest move.

2026-04-30: User reported the expected anchor debug log file did not exist after
testing. Added heartbeat diagnostics at plugin/WebView initialization and preview
callback registration so the next build distinguishes wrong DLL/install from
missing anchor WebView messages.

2026-04-30: User provided heartbeat-only log. Added JS-side anchor diagnostics,
try/catch around hash/history update, immediate/retry forced anchor scroll
posts, and widened native raw-message logging.

2026-04-30: User confirmed "it worked". Removed temporary JS/native diagnostics
and kept the functional anchor-sync hardening. The cleaned build has not yet
been manually retested on Windows after diagnostic cleanup.

2026-04-30: User confirmed the cleaned build still works. Fragment-only anchor
links now remain accepted behavior after diagnostic cleanup: preview jumps and
Notepad++ editor scrolls to the target source heading.

2026-04-30: User asked to finish native parity hardening. Implemented native
menu commands for Copy HTML to clipboard, Export HTML, Export PDF, and Print;
added CF_HTML clipboard support, standalone HTML export, WebView2 PrintToPdf /
ShowPrintUI wrappers, and a print CSS override hiding the EOF spacer. Updated
MarkdownPlusPlus.Native/README.md and smoke-tests/markdownplusplus-smoke.md.
Validation performed: node --check preview.js, cmake --build
build/cmake/linux-check --config Release, and trailing-whitespace scan. Windows
MSVC/WebView2 build and manual smoke validation are still pending.

2026-04-30: User tested Copy HTML to clipboard and saw exact Markdown text in a
plain-text paste target. Checked legacy behavior: AbstractRenderer sends
BuildHtml(ConvertedText, ...) as CF_HTML and ConvertedText as the text fallback,
so plain-text fallback should be rendered HTML, not raw Markdown. Patched native
CopyRenderedHtml() to pass rendered.articleHtml as the text fallback and updated
the smoke expectation.

2026-04-30: User tested Export HTML and reported the generated file opened, but
`Jump to expected behavior` did nothing. Root cause: exported HTML has no
window.chrome.webview, and preview.js returned before handling fragment-only
links. Also standalone export loaded preview.js only when Mermaid was enabled.
Patched handleLinkClick() to process `#fragment` links before the WebView2
postMessage requirement, and changed BuildStandaloneDocument() to always include
preview.js while loading mermaid.min.js only when Mermaid is enabled. Added a
standalone exported-HTML anchor check to smoke-tests/markdownplusplus-smoke.md.

2026-04-30: User asked whether smoke-tests/smoke-test.pdf was valid
stand-alone because opening it prompted for .md permission. Inspected the PDF:
it is structurally valid PDF 1.4, one page, no OpenAction/Launch/GoToR/JS, but
not cleanly stand-alone because local Markdown links became PDF URI annotations
to https://markdownplusplus.document/markdownplusplus-smoke.md and
https://markdownplusplus.document/BUILDING_NATIVE.md. Patched PDF export to call
preview.js preparePdfExport() before WebView2 PrintToPdf, temporarily removing
preview-local/relative/file hrefs, then restorePdfExport() after completion or
failure. Updated smoke expectations.

2026-04-30: User confirmed the Print menu command worked. Treat WebView2 system
print UI parity as accepted. Remaining parity validation is regenerated PDF
stand-alone link cleanup, exported HTML standalone fragment jumps, and clipboard
fallback behavior after the copy patch.

2026-04-30: User rebuilt and tested the remaining parity fixes and reported
"works perfectly." Treat native parity hardening as accepted for this pass:
Copy HTML fallback is fixed, standalone HTML fragment jumps work, PDF export is
stand-alone without markdownplusplus.document/local .md permission prompts, and
Print works. Next strongest move is release-readiness cleanup unless the user
chooses another feature gap.

2026-04-30: User asked to go through release-readiness cleanup before discussing
feature gaps. Updated MarkdownPlusPlus.Native/README.md, BUILDING_NATIVE.md,
MarkdownPlusPlus.Native/DEPENDENCIES.md, root README.md; added
MarkdownPlusPlus.Native/SMOKE_CHECKLIST.md and
MarkdownPlusPlus.Native/RELEASE_NOTES.md for native test build 0.1.0.0.
Verified current package folder shape is MarkdownPlusPlus.dll plus assets
preview.css, preview.js, and mermaid/README.md + mermaid.min.js. Validation:
node --check preview.js, cmake --build build/cmake/linux-check --config Release,
and trailing-whitespace scan on touched release-readiness docs.

2026-04-30: User asked to put remaining strict parity items into a feature
backlog and prepare a plan to remove all legacy artifacts/code from Git. Added
MarkdownPlusPlus.Native/BACKLOG.md. Backlog priorities: native release-candidate
checkpoint, configurable rendered file extensions/new-file handling, custom
CSS/theme controls, export preferences, optional Outlook email export, and UI
polish. Cleanup plan target: native-only repository with MarkdownViewerPlusPlus/
C# code, old solution/AppVeyor/Jekyll files, legacy docs/screenshots, legacy
dependency licenses, generated smoke outputs, local caches, and generated
dependency installs removed or ignored. Do not delete the legacy tree until the
native baseline is checkpointed and smoke-tested.

2026-04-30: User reported a smoke-test regression where the sample external URL
opened another Edge profile. Patched WebView2 user navigation handling:
NavigationStarting now cancels user-initiated preview navigations and routes
routable http/https/file URLs through the plugin link callback; NewWindowRequested
now marks requests handled and routes the URI through the same callback; preview.js
now intercepts modified primary-clicks and middle-click auxclicks; native
external URL detection now excludes markdownplusplus.document/local internal
hosts. Validation passed: git diff --check for touched files, linux-check CMake
build, and Windows VS2026 UNC package build. Fresh package is at
build/cmake/vs2026-x64-unc/package/Release/MarkdownPlusPlus. Normal
tools/build-native.ps1 -Package failed from Codex because the session used UNC
paths while the existing vs2026-x64 cache was created from Z:\, and this
PowerShell session lacks the Z: drive mapping. Attempted install to
C:\Program Files\Notepad++\plugins\MarkdownPlusPlus failed because this
PowerShell session is not elevated; Notepad++ was not running. User then ran
the normal build/install command from their environment and confirmed the retest
worked. Treat the external-link Edge-profile regression as fixed and accepted.

2026-04-30: User asked to make the native baseline checkpoint. Checkpoint scope
is native C++ Markdown++ source/assets, CMake build config, helper scripts,
native docs/release notes/backlog/smoke checklist, source smoke fixtures, and
session/context notes. Generated/local files are intentionally excluded/ignored:
build/, packages/, third_party/cmark-gfm/, tools/nuget/, tools/__pycache__,
MarkdownPlusPlus.Native/assets/mermaid/mermaid.min.js, generated smoke
HTML/PDF/copies/Zone.Identifier, and local assistant files. Validation before
checkpoint commit passed: git diff --check -- . ':!third_party', cmake --build
build/cmake/linux-check --config Release, and node --check
MarkdownPlusPlus.Native/assets/preview.js. Next after checkpoint is native-only
legacy cleanup.
```
