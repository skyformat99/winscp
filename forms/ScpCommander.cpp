//---------------------------------------------------------------------------
#include <vcl.h>
#pragma hdrstop

#include "ScpCommander.h"

#include <Common.h>
#include <CoreMain.h>
#include <Interface.h>
#include <TextsWin.h>
#include <HelpWin.h>
#include <VCLCommon.h>
#include <GUITools.h>
#include <DragDrop.hpp>

#include "Glyphs.h"
#include "NonVisual.h"
#include "Tools.h"
#include "WinConfiguration.h"
#include "TerminalManager.h"
//---------------------------------------------------------------------------
#pragma package(smart_init)
#pragma link "CustomDirView"
#pragma link "CustomScpExplorer"
#pragma link "CustomUnixDirView"
#pragma link "IEListView"
#pragma link "NortonLikeListView"
#pragma link "UnixDirView"
#pragma link "DirView"
#pragma link "CustomPathComboBox"
#pragma link "IEComboBox"
#pragma link "IEPathComboBox"
#pragma link "PathLabel"
#pragma link "UnixPathComboBox"
#pragma link "HistoryComboBox"
#pragma link "CustomDriveView"
#pragma link "DriveView"
#pragma link "UnixDriveView"
#pragma link "TB2Dock"
#pragma link "TB2Item"
#pragma link "TB2Toolbar"
#pragma link "TBX"
#pragma link "TB2ExtItems"
#pragma link "TBXExtItems"
#pragma link "TBXLists"
#pragma link "TBXStatusBars"
#pragma link "TBXToolPals"
#pragma resource "*.dfm"
//---------------------------------------------------------------------------
__fastcall TScpCommanderForm::TScpCommanderForm(TComponent* Owner)
        : TCustomScpExplorerForm(Owner)
{
  FCurrentSide = osLocal;
  FLastLeftPanelWidth = LeftPanelWidth;
  FNormalPanelsWidth = -1;
  FSynchronisingBrowse = false;
  FFirstTerminal = true;
  FInternalDDDownloadList = new TStringList();

  LocalBackButton->LinkSubitems = HistoryMenu(osLocal, true)->Items;
  LocalForwardButton->LinkSubitems = HistoryMenu(osLocal, false)->Items;
  RemoteBackButton->LinkSubitems = HistoryMenu(osRemote, true)->Items;
  RemoteForwardButton->LinkSubitems = HistoryMenu(osRemote, false)->Items;

  TopDock->PopupMenu = NonVisualDataModule->CommanderBarPopup;
  StatusBar->PopupMenu = TopDock->PopupMenu;
  QueueDock->PopupMenu = TopDock->PopupMenu;
  BottomDock->PopupMenu = TopDock->PopupMenu;

  LocalTopDock->PopupMenu = NonVisualDataModule->LocalPanelPopup;
  LocalPathLabel->PopupMenu = LocalTopDock->PopupMenu;
  LocalStatusBar->PopupMenu = LocalTopDock->PopupMenu;
  LocalDriveView->PopupMenu = LocalTopDock->PopupMenu;
  LocalBottomDock->PopupMenu = LocalTopDock->PopupMenu;

  RemoteTopDock->PopupMenu = NonVisualDataModule->RemotePanelPopup;
  RemotePathLabel->PopupMenu = RemoteTopDock->PopupMenu;
  RemoteStatusBar->PopupMenu = RemoteTopDock->PopupMenu;
  RemoteDriveView->PopupMenu = RemoteTopDock->PopupMenu;
  RemoteBottomDock->PopupMenu = RemoteTopDock->PopupMenu;

  // set common norton shorcuts to our actions
  NonVisualDataModule->CommanderShortcuts();
  Splitter->ShowHint = True;
  reinterpret_cast<TLabel*>(Splitter)->OnDblClick = SplitterDblClick;
  reinterpret_cast<TLabel*>(LocalPanelSplitter)->OnDblClick = PanelSplitterDblClick;
  reinterpret_cast<TLabel*>(RemotePanelSplitter)->OnDblClick = PanelSplitterDblClick;
  RemotePathComboBox->TabStop = False;

  CommandLineLabel->FocusControl = CommandLineCombo;
  CommandLineCombo->Text = "";
  FCommandLineComboPopulated = false;

  for (int i = 0; i < ToolbarToolbar->Items->Count; i++)
  {
    TTBCustomItem * Item = ToolbarToolbar->Items->Items[i];
    Item->Caption =
      ShortCutToText(Item->ShortCut) + " " +
      Trim(StringReplace(StripHotkey(Item->Caption),
        "...", "", TReplaceFlags() << rfReplaceAll << rfIgnoreCase));
  }

  LocalDirView->Font = Screen->IconFont;
}
//---------------------------------------------------------------------------
__fastcall TScpCommanderForm::~TScpCommanderForm()
{
  delete FInternalDDDownloadList;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::RestoreFormParams()
{
  assert(WinConfiguration);
  TCustomScpExplorerForm::RestoreFormParams();
  RestoreForm(WinConfiguration->ScpCommander.WindowParams, this);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::RestoreParams()
{
  assert(Configuration);

  // IDE often looses this link
  LocalDirView->HeaderImages = GlyphsModule->ArrowImages;

  TCustomScpExplorerForm::RestoreParams();
  LeftPanelWidth = WinConfiguration->ScpCommander.LocalPanelWidth;
  LoadToolbarsLayoutStr(WinConfiguration->ScpCommander.ToolbarsLayout);
  SessionCombo->EditWidth = WinConfiguration->ScpCommander.SessionComboWidth;
  StatusBar->Visible = WinConfiguration->ScpCommander.StatusBar;

  CommandLinePanel->Visible = WinConfiguration->ScpCommander.CommandLine;

  FDirViewToSelect = (WinConfiguration->ScpCommander.CurrentPanel == osLocal ?
    (TCustomDirView *)LocalDirView : (TCustomDirView *)RemoteDirView);
  #define RESTORE_PANEL_PARAMS(PANEL) \
    PANEL ## DirView->ColProperties->ParamsStr = WinConfiguration->ScpCommander.PANEL ## Panel.DirViewParams; \
    PANEL ## StatusBar->Visible = WinConfiguration->ScpCommander.PANEL ## Panel.StatusBar; \
    PANEL ## DriveView->Visible = WinConfiguration->ScpCommander.PANEL ## Panel.DriveView; \
    PANEL ## DriveView->Height = WinConfiguration->ScpCommander.PANEL ## Panel.DriveViewHeight
  RESTORE_PANEL_PARAMS(Local);
  RESTORE_PANEL_PARAMS(Remote);
  #undef RESTORE_PANEL_PARAMS

  // just to make sure
  LocalDirView->DirColProperties->ExtVisible = false;
  RemoteDirView->UnixColProperties->ExtVisible = false;

  NonVisualDataModule->SynchronizeBrowsingAction->Checked = WinConfiguration->SynchronizeBrowsing;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::StoreParams()
{
  assert(WinConfiguration);

  WinConfiguration->BeginUpdate();
  try
  {
    WinConfiguration->ScpCommander.ToolbarsLayout = GetToolbarsLayoutStr();
    WinConfiguration->ScpCommander.SessionComboWidth = SessionCombo->EditWidth;
    WinConfiguration->ScpCommander.LocalPanelWidth = LeftPanelWidth;
    WinConfiguration->ScpCommander.StatusBar = StatusBar->Visible;

    WinConfiguration->ScpCommander.CommandLine = CommandLinePanel->Visible;
    SaveCommandLine();

    WinConfiguration->ScpCommander.CurrentPanel = FCurrentSide;

    #define STORE_PANEL_PARAMS(PANEL) \
      WinConfiguration->ScpCommander.PANEL ## Panel.DirViewParams = PANEL ## DirView->ColProperties->ParamsStr; \
      WinConfiguration->ScpCommander.PANEL ## Panel.StatusBar = PANEL ## StatusBar->Visible; \
      WinConfiguration->ScpCommander.PANEL ## Panel.DriveView = PANEL ## DriveView->Visible; \
      WinConfiguration->ScpCommander.PANEL ## Panel.DriveViewHeight = PANEL ## DriveView->Height
    STORE_PANEL_PARAMS(Local);
    STORE_PANEL_PARAMS(Remote);
    #undef RESTORE_PANEL_PARAMS

    WinConfiguration->ScpCommander.WindowParams = StoreForm(this);;

    WinConfiguration->SynchronizeBrowsing = NonVisualDataModule->SynchronizeBrowsingAction->Checked;

    TCustomScpExplorerForm::StoreParams();
  }
  __finally
  {
    WinConfiguration->EndUpdate();
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::UpdateTerminal(TTerminal * Terminal)
{
  TCustomScpExplorerForm::UpdateTerminal(Terminal);

  TManagedTerminal * ManagedTerminal = dynamic_cast<TManagedTerminal *>(Terminal);
  assert(ManagedTerminal != NULL);

  SAFE_DESTROY(ManagedTerminal->LocalExplorerState);

  if (WinConfiguration->PreservePanelState)
  {
    ManagedTerminal->LocalExplorerState = LocalDirView->SaveState();
    ManagedTerminal->SynchronizeBrowsing = NonVisualDataModule->SynchronizeBrowsingAction->Checked;
  }

  assert(LocalDirView != NULL);
  ManagedTerminal->LocalDirectory = LocalDirView->PathName;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::UpdateSessionData(TSessionData * Data)
{
  TCustomScpExplorerForm::UpdateSessionData(Data);

  assert(LocalDirView);
  Data->LocalDirectory = LocalDirView->PathName;
}
//---------------------------------------------------------------------------
bool __fastcall TScpCommanderForm::InternalDDDownload(AnsiString & TargetDirectory)
{
  assert(IsFileControl(FDDTargetControl, osLocal));

  bool Result = false;
  if (FDDTargetControl == LocalDirView)
  {
    if (LocalDirView->DropTarget)
    {
      // when drop target is not directory, it is probably file type, which have
      // associated drop handler (such as ZIP file in WinXP). in this case we
      // must leave drop handling to destination application.
      // ! this check is duplicated in LocalDirViewDDTargetHasDropHandler()
      // for shellex downloads
      if (LocalDirView->ItemIsDirectory(LocalDirView->DropTarget))
      {
        TargetDirectory = LocalDirView->ItemFullFileName(LocalDirView->DropTarget);
        Result = true;
      }
    }
    else
    {
      TargetDirectory = IncludeTrailingBackslash(LocalDirView->Path);
      Result = true;
    }
  }
  else if (FDDTargetControl == LocalDriveView)
  {
    assert(LocalDriveView->DropTarget != NULL);
    TargetDirectory = LocalDriveView->NodePathName(LocalDriveView->DropTarget);
    Result = true;
  }
  else
  {
    assert(false);
    Abort();
  }

  return Result;
}
//---------------------------------------------------------------------------
bool __fastcall TScpCommanderForm::CopyParamDialog(TTransferDirection Direction,
  TTransferType Type, bool Temp, TStrings * FileList, AnsiString & TargetDirectory,
  TGUICopyParamType & CopyParam, bool Confirm, bool DragDrop)
{
  bool Result = false;
  // Temp means d&d here so far, may change in future!
  if (Temp && (Direction == tdToLocal) &&
      IsFileControl(FDDTargetControl, osLocal))
  {
    Result = InternalDDDownload(TargetDirectory);
    if (Result)
    {
      assert(FileList->Count > 0);
      FInternalDDDownloadList->Assign(FileList);
    }
  }
  else if (!Temp && TargetDirectory.IsEmpty())
  {
    if (Direction == tdToLocal)
    {
      TargetDirectory = IncludeTrailingBackslash(LocalDirView->Path);
    }
    else
    {
      TargetDirectory = UnixIncludeTrailingBackslash(RemoteDirView->Path);
    }
  }

  if (!Result)
  {
    Result = TCustomScpExplorerForm::CopyParamDialog(Direction, Type, Temp,
      FileList, TargetDirectory, CopyParam, Confirm, DragDrop);
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::DoShow()
{
  TCustomScpExplorerForm::DoShow();

  assert(FDirViewToSelect);
  FDirViewToSelect->SetFocus();

  UpdateControls();
}
//---------------------------------------------------------------------------
Boolean __fastcall TScpCommanderForm::AllowedAction(TAction * Action, TActionAllowed Allowed)
{
  #define FLAG ((TActionFlag)(Action->Tag))
  return
    // always require Commander flag
    (FLAG & afCommander) &&
    // if action is execution or update, we don't require any other flag
    // if we check for shortcut, we require proper dirview to be selected
    ((Allowed != aaShortCut) ||
     ((FLAG & afLocal) && (FCurrentSide == osLocal)) ||
     ((FLAG & afRemote) && (FCurrentSide == osRemote))
    );
  #undef FLAG
}
//---------------------------------------------------------------------------
TCustomDirView * __fastcall TScpCommanderForm::DirView(TOperationSide Side)
{
  Side = GetSide(Side);
  if (Side == osLocal)
  {
    return LocalDirView;
  }
  else
  {
    return TCustomScpExplorerForm::DirView(Side);
  }
}
//---------------------------------------------------------------------------
bool __fastcall TScpCommanderForm::IsFileControl(TObject * Control,
  TOperationSide Side)
{
  return
    ((Side == osLocal) &&
     ((Control == LocalDirView) || (Control == LocalDriveView))) ||
    TCustomScpExplorerForm::IsFileControl(Control, Side);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::ReloadLocalDirectory(const AnsiString Directory)
{
  if (Directory.IsEmpty() || ComparePaths(Directory, LocalDirView->Path))
  {
    LocalDirView->ReloadDirectory();
    LocalDriveView->ValidateDirectory(LocalDriveView->Selected);
  }
  TCustomScpExplorerForm::ReloadLocalDirectory();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::BatchStart(void *& Storage)
{
  Storage = new bool;
  *static_cast<bool*>(Storage) = LocalDirView->WatchForChanges;
  LocalDirView->WatchForChanges = false;
  LocalDriveView->WatchDirectory = false;

  TCustomScpExplorerForm::BatchStart(Storage);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::BatchEnd(void * Storage)
{
  TCustomScpExplorerForm::BatchEnd(Storage);

  assert(Storage != NULL);

  LocalDirView->WatchForChanges = *static_cast<bool*>(Storage);
  LocalDriveView->WatchDirectory = LocalDirView->WatchForChanges;

  delete Storage;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::TerminalChanged()
{
  if (Terminal)
  {
    bool WasSynchronisingBrowsing = NonVisualDataModule->SynchronizeBrowsingAction->Checked;
    NonVisualDataModule->SynchronizeBrowsingAction->Checked = false;

    TCustomScpExplorerForm::TerminalChanged();

    // we will load completelly different directory, so particularly
    // do not attempt to select previously selected directory
    LocalDirView->ContinueSession(false);

    TManagedTerminal * ManagedTerminal = dynamic_cast<TManagedTerminal *>(Terminal);
    assert(ManagedTerminal != NULL);

    // reset home directory
    LocalDirView->HomeDirectory = "";

    if (FFirstTerminal || !WinConfiguration->ScpCommander.PreserveLocalDirectory)
    {
      AnsiString LocalDirectory = ManagedTerminal->LocalDirectory;
      bool DocumentsDir = LocalDirectory.IsEmpty();

      if (!DocumentsDir)
      {
        try
        {
          LocalDirView->Path = LocalDirectory;
        }
        catch(Exception & E)
        {
          DocumentsDir = true;
          Terminal->ShowExtendedException(&E);
        }
      }

      if (DocumentsDir)
      {
        try
        {
          LocalDirView->HomeDirectory = "";
          LocalDirView->ExecuteHomeDirectory();
        }
        catch(Exception & E)
        {
          Terminal->ShowExtendedException(&E);
          LocalDirView->Path = ExtractFilePath(Application->ExeName);
        }
      }
    }
    FFirstTerminal = false;

    if (WinConfiguration->DefaultDirIsHome &&
        !Terminal->SessionData->UpdateDirectories)
    {
      LocalDirView->HomeDirectory = Terminal->SessionData->LocalDirectory;
    }

    if (WinConfiguration->PreservePanelState &&
        (ManagedTerminal->LocalExplorerState != NULL) &&
        !WinConfiguration->ScpCommander.PreserveLocalDirectory)
    {
      LocalDirView->RestoreState(ManagedTerminal->LocalExplorerState);
      NonVisualDataModule->SynchronizeBrowsingAction->Checked = ManagedTerminal->SynchronizeBrowsing;
    }
    else if (WasSynchronisingBrowsing &&
        SameText(ExtractFileName(LocalDirView->PathName),
          UnixExtractFileName(RemoteDirView->PathName)))
    {
      NonVisualDataModule->SynchronizeBrowsingAction->Checked = true;
    }
  }
  else
  {
    TCustomScpExplorerForm::TerminalChanged();
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::ConfigurationChanged()
{
  TCustomScpExplorerForm::ConfigurationChanged();
  if (WinConfiguration->DefaultDirIsHome && Terminal &&
      !Terminal->SessionData->UpdateDirectories)
  {
    LocalDirView->HomeDirectory = Terminal->SessionData->LocalDirectory;
  }
  else
  {
    LocalDirView->HomeDirectory = "";
  }
  LocalDirView->DimmHiddenFiles = WinConfiguration->DimmHiddenFiles;
  LocalDriveView->DimmHiddenDirs = WinConfiguration->DimmHiddenFiles;
  LocalDirView->ShowHiddenFiles = WinConfiguration->ShowHiddenFiles;
  LocalDriveView->ShowHiddenDirs = WinConfiguration->ShowHiddenFiles;

  LocalDirView->NortonLike = WinConfiguration->ScpCommander.NortonLikeMode;
  RemoteDirView->NortonLike = WinConfiguration->ScpCommander.NortonLikeMode;
  bool RowSelectChange = (LocalDirView->RowSelect != WinConfiguration->ScpCommander.FullRowSelect);
  LocalDirView->RowSelect = WinConfiguration->ScpCommander.FullRowSelect;
  RemoteDirView->RowSelect = WinConfiguration->ScpCommander.FullRowSelect;
  if (RowSelectChange)
  {
    // selection is not redrawn automatically when RowSelect changes
    LocalDirView->Invalidate();
    RemoteDirView->Invalidate();
  }

  LocalDirView->DragDropFilesEx->ShellExtensions->DropHandler =
    !WinConfiguration->DDExtEnabled;
  LocalDriveView->DragDropFilesEx->ShellExtensions->DropHandler =
    !WinConfiguration->DDExtEnabled;

  if ((LocalPanel->Left > RemotePanel->Left) != WinConfiguration->ScpCommander.SwappedPanels)
  {
    int AWidth = ClientWidth;
    Panel(false)->Align = alClient;
    Panel(true)->Align = alLeft;
    TControl * ControlsOrder[] =
      { Panel(true), Splitter, Panel(false) };
    SetHorizontalControlsOrder(ControlsOrder, LENOF(ControlsOrder));
    Panel(true)->TabOrder = 0;
    Panel(false)->TabOrder = 1;
    ClientWidth = AWidth;
    LeftPanelWidth = FLastLeftPanelWidth;

    int LocalIndex = MenuToolbar->Items->IndexOf(LocalMenuButton);
    int RemoteIndex = MenuToolbar->Items->IndexOf(RemoteMenuButton);
    MenuToolbar->Items->Move(LocalIndex, RemoteIndex);
    RemoteIndex = MenuToolbar->Items->IndexOf(RemoteMenuButton);
    MenuToolbar->Items->Move(RemoteIndex, LocalIndex);
    SWAP(TShortCut, NonVisualDataModule->LocalChangePathAction->ShortCut,
      NonVisualDataModule->RemoteChangePathAction->ShortCut);
  }
}
//---------------------------------------------------------------------------
TPanel * __fastcall TScpCommanderForm::Panel(bool Left)
{
  return (WinConfiguration->ScpCommander.SwappedPanels == Left ?
    RemotePanel : LocalPanel);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SetLeftPanelWidth(float value)
{
  float Total = LocalPanel->Width + RemotePanel->Width;
  FLeftPanelWidth = value;
  if (value * Total != Panel(true)->Width)
  {
    Panel(true)->Width = value * Total;
    UpdateControls();
  }
}
//---------------------------------------------------------------------------
float __fastcall TScpCommanderForm::GetLeftPanelWidth()
{
  return FLeftPanelWidth;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SplitterMoved(TObject * /*Sender*/)
{
  float Left = Panel(true)->Width;
  float Total = LocalPanel->Width + RemotePanel->Width;
  FLeftPanelWidth = Left / Total;
  FLastLeftPanelWidth = LeftPanelWidth;
  UpdateControls();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SplitterCanResize(TObject * /*Sender*/,
      int &NewSize, bool & /*Accept*/)
{
  // When splitter is drag so far to right, that width contraint of remote panel would
  // be violated, it doesn't stop, but extend form width.
  // Following prevents this behaviour.
  if (ClientWidth - NewSize - Splitter->Width < Panel(false)->Constraints->MinWidth)
    NewSize = (ClientWidth - Panel(false)->Constraints->MinWidth - Splitter->Width);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SplitterDblClick(TObject * /*Sender*/)
{
  LeftPanelWidth = 0.5;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::PanelSplitterDblClick(TObject * Sender)
{
  TSplitter * Splitter = dynamic_cast<TSplitter *>(Sender);
  assert(Splitter != NULL);
  TCustomDriveView * DriveView;
  TCustomDriveView * OtherDriveView;
  if (Splitter == LocalPanelSplitter)
  {
    DriveView = LocalDriveView;
    OtherDriveView = RemoteDriveView;
  }
  else
  {
    DriveView = RemoteDriveView;
    OtherDriveView = LocalDriveView;
  }

  assert(DriveView->Visible);
  if (OtherDriveView->Visible)
  {
    DriveView->Height = OtherDriveView->Height;
  }
  else
  {
    OtherDriveView->Height = DriveView->Height;
    OtherDriveView->Visible = true;
  }

  FixControlsPlacement();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::UpdateControls()
{
  TCustomScpExplorerForm::UpdateControls();

  AnsiString SplitterLongHint = Splitter->Hint;
  SplitterLongHint.Delete(1, SplitterLongHint.Pos("|"));
  Splitter->Hint = FORMAT("%0.0f%%|%s", (LeftPanelWidth*100, SplitterLongHint));
  CommandLineLabel->UnixPath = (FCurrentSide == osRemote);
  CommandLineLabel->Caption = DirView(osCurrent)->PathName;
  CommandLinePromptLabel->Caption =
    (FCurrentSide == osRemote) ? "$" : ">";
  LocalDirView->Color = (SessionColor != 0 ? SessionColor : clWindow);
  LocalDriveView->Color = LocalDirView->Color;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::ChangePath(TOperationSide Side)
{
  assert((Side == osLocal) || (Side == osRemote));
  TCustomPathComboBox * PathComboBox;
  if (Side == osLocal) PathComboBox = LocalPathComboBox;
    else PathComboBox = RemotePathComboBox;
  assert(PathComboBox);
  PathComboBox->SetFocus();
  PathComboBox->DroppedDown = True;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::PathComboBoxCloseUp(TObject * /*Sender*/,
      bool /*Canceled*/)
{
  DirView(osCurrent)->SetFocus();
}
//---------------------------------------------------------------------------
TControl * __fastcall TScpCommanderForm::GetComponent(Byte Component)
{
  switch (Component) {
    case fcToolBar: return ToolbarToolbar;
    case fcStatusBar: return StatusBar;
    case fcLocalStatusBar: return LocalStatusBar;
    case fcRemoteStatusBar: return RemoteStatusBar;
    case fcSessionCombo: return reinterpret_cast<TControl*>(SessionCombo);
    case fcCommandLinePanel: return CommandLinePanel;
    case fcLocalTree: return LocalDriveView;
    case fcSessionToolbar: return SessionToolbar;
    case fcCustomCommandsBand: return CustomCommandsToolbar;
    case fcColorMenu: return reinterpret_cast<TControl*>(ColorMenuItem);
    case fcColorPalette: return reinterpret_cast<TControl*>(SessionColorPalette);
    case fcTransferDropDown: return reinterpret_cast<TControl*>(TransferDropDown);
    case fcTransferList: return reinterpret_cast<TControl*>(TransferList);
    case fcTransferLabel: return reinterpret_cast<TControl*>(TransferLabel);

    case fcCommanderMenuBand: return MenuToolbar;
    case fcCommanderSessionBand: return SessionToolbar;
    case fcCommanderPreferencesBand: return PreferencesToolbar;
    case fcCommanderSelectionBand: return SelectionToolbar;
    case fcCommanderToolbarBand: return CommandToolbar;
    case fcCommanderSortBand: return SortToolbar;
    case fcCommanderCommandsBand: return CommandsToolbar;
    case fcCommanderUpdatesBand: return UpdatesToolbar;
    case fcCommanderTransferBand: return TransferToolbar;
    case fcCommanderUploadDownloadBand: return UploadDownloadToolbar;
    case fcCommanderCustomCommandsBand: return CustomCommandsToolbar;
    case fcCommanderLocalHistoryBand: return LocalHistoryToolbar;
    case fcCommanderLocalNavigationBand: return LocalNavigationToolbar;
    case fcCommanderRemoteHistoryBand: return RemoteHistoryToolbar;
    case fcCommanderRemoteNavigationBand: return RemoteNavigationToolbar;
    default: return TCustomScpExplorerForm::GetComponent(Component);
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::FixControlsPlacement()
{
  TCustomScpExplorerForm::FixControlsPlacement();

  LocalPanelSplitter->Visible = LocalDriveView->Visible;

  TControl * ControlsOrder[] =
    { CommandLinePanel, BottomDock, QueueSplitter, QueuePanel, StatusBar };
  SetVerticalControlsOrder(ControlsOrder, LENOF(ControlsOrder));

  TControl * LocalControlsOrder[] =
    { LocalTopDock, LocalPathLabel, LocalDriveView, LocalPanelSplitter,
      LocalDirView, LocalBottomDock, LocalStatusBar };
  SetVerticalControlsOrder(LocalControlsOrder, LENOF(LocalControlsOrder));

  TControl * RemoteControlsOrder[] =
    { RemoteTopDock, RemotePathLabel, RemoteDriveView, RemotePanelSplitter,
      RemoteDirView, RemoteBottomDock, RemoteStatusBar };
  SetVerticalControlsOrder(RemoteControlsOrder, LENOF(RemoteControlsOrder));

  if (LocalDirView->ItemFocused != NULL)
  {
    LocalDirView->ItemFocused->MakeVisible(false);
  }
}
//---------------------------------------------------------------------------
bool __fastcall TScpCommanderForm::GetHasDirView(TOperationSide Side)
{
  return TCustomScpExplorerForm::GetHasDirView(Side) || (Side == osLocal);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::CompareDirectories()
{
  LocalDirView->CompareFiles(RemoteDirView, false,
    WinConfiguration->ScpCommander.CompareCriterias());
  RemoteDirView->CompareFiles(LocalDirView, false,
    WinConfiguration->ScpCommander.CompareCriterias());
  if (LocalDirView->SelCount + RemoteDirView->SelCount == 0)
  {
    MessageDialog(LoadStr(COMPARE_NO_DIFFERENCES), qtInformation, qaOK,
      HELP_COMPARE_NO_DIFFERENCES);
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SynchronizeDirectories()
{
  AnsiString LocalDirectory = LocalDirView->PathName;
  AnsiString RemoteDirectory = RemoteDirView->PathName;
  DoSynchronizeDirectories(LocalDirectory, RemoteDirectory, false);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::FullSynchronizeDirectories()
{
  AnsiString LocalDirectory = LocalDirView->PathName;
  AnsiString RemoteDirectory = RemoteDirView->PathName;
  bool SaveMode = !(GUIConfiguration->SynchronizeModeAuto < 0);
  TSynchronizeMode Mode =
    (SaveMode ? (TSynchronizeMode)GUIConfiguration->SynchronizeModeAuto :
      ((FCurrentSide == osLocal) ? smRemote : smLocal));
  if (DoFullSynchronizeDirectories(LocalDirectory, RemoteDirectory, Mode,
        SaveMode, false))
  {
    if (SaveMode)
    {
      GUIConfiguration->SynchronizeModeAuto = Mode;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::ExploreLocalDirectory()
{
  if ((int)ShellExecute(Application->Handle, "explore",
      (char*)LocalDirView->Path.data(), NULL, NULL, SW_SHOWNORMAL) <= 32)
  {
    throw Exception(FORMAT(EXPLORE_LOCAL_DIR_ERROR, (LocalDirView->Path)));
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalDirViewExecFile(TObject *Sender,
      TListItem *Item, bool &AllowExec)
{
  assert(Item);
  if ((UpperCase(PFileRec(Item->Data)->FileExt) == "LNK") &&
      DirectoryExists(ResolveFileShortCut(LocalDirView->ItemFullFileName(Item), true)))
  {
    AllowExec = true;
  }
  else
  {
    DoDirViewExecFile(Sender, Item, AllowExec);
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalFileControlDDDragEnter(TObject *Sender,
      IDataObject *DataObj, int grfKeyState, TPoint &Point, int &dwEffect,
      bool &Accept)
{
  // LocalDirViewDDDragEnter is duplication of
  // TCustomScpExplorerForm::DirViewDDDragEnter, but it differs in
  // literal type of 'DataObj' parameter.Actual type is however same
  FileControlDDDragEnter(Sender, DataObj, grfKeyState, Point, dwEffect, Accept);
}
//---------------------------------------------------------------------------
bool __fastcall TScpCommanderForm::PanelOperation(TOperationSide Side,
  bool DragDrop)
{
  return TCustomScpExplorerForm::PanelOperation(Side, DragDrop) ||
    (DropSourceControl == LocalDirView);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::FileOperationProgress(
  TFileOperationProgressType & ProgressData, TCancelStatus & Cancel)
{
  // Heuristic: When operation finishes and DD targed is local dir view,
  // we suppose that drag&drop download finished, so local dir view should be
  // reloaded
  if (!ProgressData.InProgress && FProgressForm &&
      IsFileControl(FDDTargetControl, osLocal) &&
      ((ProgressData.Operation == ::foCopy) || (ProgressData.Operation == ::foMove)))
  {
    ReloadLocalDirectory();
  }
  TCustomScpExplorerForm::FileOperationProgress(ProgressData, Cancel);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::DoDirViewLoaded(TCustomDirView * ADirView)
{
  TCustomScpExplorerForm::DoDirViewLoaded(ADirView);

  UpdateControls();

  bool PrevNoTransferPresetAutoSelect = FNoTransferPresetAutoSelect;
  FNoTransferPresetAutoSelect = true;
  try
  {
    AnsiString PrevPath = FPrevPath[ADirView == LocalDirView];
    AnsiString FullPrevPath =
      (ADirView == LocalDirView) ? IncludeTrailingBackslash(PrevPath) :
        UnixIncludeTrailingBackslash(PrevPath);
    FPrevPath[ADirView == LocalDirView] = ADirView->Path;

    if (!FSynchronisingBrowse && NonVisualDataModule->SynchronizeBrowsingAction->Checked &&
        !PrevPath.IsEmpty() && PrevPath != ADirView->Path)
    {
      FSynchronisingBrowse = true;
      if (ADirView == LocalDirView)
      {
        Terminal->ExceptionOnFail = true;
        try
        {
          if (LocalDirView->Path.SubString(1, FullPrevPath.Length()) == FullPrevPath)
          {
            RemoteDirView->Path = UnixIncludeTrailingBackslash(RemoteDirView->Path) +
              ToUnixPath(LocalDirView->Path.SubString(FullPrevPath.Length() + 1,
                LocalDirView->Path.Length() - FullPrevPath.Length()));
          }
          else if (FullPrevPath.SubString(1, LocalDirView->Path.Length()) == LocalDirView->Path)
          {
            AnsiString RemotePath = RemoteDirView->Path;
            while (!ComparePaths(FullPrevPath, LocalDirView->Path))
            {
              if (RemotePath == UnixExcludeTrailingBackslash(RemotePath))
              {
                Abort();
              }
              RemotePath = UnixExtractFilePath(UnixExcludeTrailingBackslash(RemotePath));
              FullPrevPath = ExtractFilePath(ExcludeTrailingBackslash(FullPrevPath));
            }
            RemoteDirView->Path = RemotePath;
          }
          else
          {
            Abort();
          }
        }
        __finally
        {
          Terminal->ExceptionOnFail = false;
        }
      }
      else
      {
        if (RemoteDirView->Path.SubString(1, FullPrevPath.Length()) == FullPrevPath)
        {
          LocalDirView->Path = IncludeTrailingBackslash(LocalDirView->Path) +
            FromUnixPath(RemoteDirView->Path.SubString(FullPrevPath.Length() + 1,
              RemoteDirView->Path.Length() - FullPrevPath.Length()));
        }
        else if (FullPrevPath.SubString(1, RemoteDirView->Path.Length()) == RemoteDirView->Path)
        {
          AnsiString NewLocalPath;
          AnsiString LocalPath = ExcludeTrailingBackslash(LocalDirView->Path);
          while (!UnixComparePaths(FullPrevPath, RemoteDirView->Path))
          {
            NewLocalPath = ExcludeTrailingBackslash(ExtractFileDir(LocalPath));
            if (NewLocalPath == LocalPath)
            {
              Abort();
            }
            LocalPath = NewLocalPath;
            FullPrevPath = UnixExtractFilePath(UnixExcludeTrailingBackslash(FullPrevPath));
          }
          LocalDirView->Path = LocalPath;
        }
        else
        {
          Abort();
        }
      }
      FSynchronisingBrowse = false;
    }
    FNoTransferPresetAutoSelect = PrevNoTransferPresetAutoSelect;
  }
  catch(Exception & E)
  {
    FNoTransferPresetAutoSelect = PrevNoTransferPresetAutoSelect;
    FSynchronisingBrowse = false;
    NonVisualDataModule->SynchronizeBrowsingAction->Checked = false;
    if (!Application->Terminated)
    {
      Terminal->ShowExtendedException(&E);
      MessageDialog(LoadStr(SYNC_DIR_BROWSE_ERROR), qtInformation, qaOK,
        HELP_SYNC_DIR_BROWSE_ERROR);
    }
    else
    {
      throw;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::AddEditLink(bool Add)
{
  if (FCurrentSide == osLocal)
  {
    bool Edit = false;
    AnsiString FileName;
    AnsiString PointTo;
    bool SymbolicLink = true;

    if (LocalDirView->ItemFocused)
    {
      assert(LocalDirView->ItemFocused->Data);
      PFileRec FileRec = (PFileRec)LocalDirView->ItemFocused->Data;

      Edit = !Add && (UpperCase(FileRec->FileExt) == "LNK");
      if (Edit)
      {
        AnsiString FullName = LocalDirView->ItemFullFileName(LocalDirView->ItemFocused);
        FileName = FullName;
        PointTo = ResolveFileShortCut(FullName, false);
        if (PointTo.IsEmpty())
        {
          throw Exception(FMTLOAD(RESOLVE_SHORTCUT_ERROR, (FullName)));
        }
      }
      else
      {
        PointTo = FileRec->FileName;
      }
    }

    if (DoSymlinkDialog(FileName, PointTo, osLocal, SymbolicLink, Edit, false))
    {
      assert(SymbolicLink);
      assert(!FileName.IsEmpty());
      assert(!PointTo.IsEmpty());

      if (ExtractFileDrive(FileName) == "" && FileName[1] != '\\')
      {
        FileName = IncludeTrailingBackslash(LocalDirView->PathName) + FileName;
      }
      if (ExtractFileDrive(PointTo) == "" && PointTo[1] != '\\')
      {
        PointTo = IncludeTrailingBackslash(LocalDirView->PathName) + PointTo;
      }
      if (ExtractFileExt(FileName) == "")
      {
        FileName = FileName + ".lnk";
      }

      if (Edit && !DeleteFile(FileName))
      {
        throw Exception(FMTLOAD(DELETE_LOCAL_FILE_ERROR, (FileName)));
      }
      if (!CreateFileShortCut(PointTo, FileName, ""))
      {
        throw Exception(CREATE_SHORTCUT_ERROR);
      }
    }
  }
  else
  {
    TCustomScpExplorerForm::AddEditLink(Add);
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::DoOpenDirectoryDialog(TOpenDirectoryMode Mode,
  TOperationSide Side)
{
  bool UseLocationProfiles;
  do
  {
    UseLocationProfiles = WinConfiguration->UseLocationProfiles;
    if (UseLocationProfiles)
    {
      TStrings * LocalDirectories = NULL;
      TStrings * RemoteDirectories = NULL;
      try
      {
        LocalDirectories = CreateVisitedDirectories(osLocal);
        RemoteDirectories = CreateVisitedDirectories(osRemote);

        AnsiString Local = LocalDirView->PathName;
        AnsiString Remote = RemoteDirView->PathName;

        if (LocationProfilesDialog(Mode, Side, Local, Remote, LocalDirectories,
              RemoteDirectories, Terminal))
        {
          // make sure that whatever path is valid it is opened first and only
          // after that an eventual error is reported
          try
          {
            if (!Local.IsEmpty())
            {
              LocalDirView->Path = Local;
            }
          }
          __finally
          {
            if (!Remote.IsEmpty())
            {
              RemoteDirView->Path = Remote;
            }
          }
        }
      }
      __finally
      {
        delete LocalDirectories;
        delete RemoteDirectories;
      }
    }
    else
    {
      TCustomScpExplorerForm::DoOpenDirectoryDialog(Mode, Side);
    }
  }
  while (UseLocationProfiles != WinConfiguration->UseLocationProfiles);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalDirViewDDTargetHasDropHandler(
  TObject * /*Sender*/, TListItem * Item, int & /*Effect*/, bool & DropHandler)
{
  // when drop target is not directory, it is probably file type, which have
  // associated drop handler (such as ZIP file in WinXP). in this case we
  // cannot allow downloading when using shellex.
  // ! this check is duplicated in InternalDDDownload() for non-shellex downloads
  if ((FDDExtMapFile != NULL) &&
      !LocalDirView->ItemIsDirectory(Item))
  {
    DropHandler = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalFileControlDDDragOver(TObject * /*Sender*/,
  int grfKeyState, TPoint & /*Point*/, int & dwEffect)
{
  if (IsFileControl(DropSourceControl, osRemote))
  {
    if (((grfKeyState & (MK_CONTROL | MK_SHIFT)) == 0) ||
        (((grfKeyState & (MK_CONTROL | MK_SHIFT)) == MK_SHIFT) &&
          !WinConfiguration->DDAllowMoveInit))
    {
      dwEffect = DROPEFFECT_Copy;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::DDGetTarget(AnsiString & Directory)
{
  if (!FDDExtTarget.IsEmpty())
  {
    Directory = FDDExtTarget;
    FDDExtTarget = "";
  }
  else
  {
    TCustomScpExplorerForm::DDGetTarget(Directory);
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::DDExtInitDrag(TFileList * FileList,
  bool & Created)
{
  FDDExtTarget = "";
  TCustomScpExplorerForm::DDExtInitDrag(FileList, Created);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalFileControlDDFileOperation(
  TObject * /*Sender*/, int dwEffect, AnsiString SourcePath,
  AnsiString TargetPath, bool & DoOperation)
{
  if (IsFileControl(DropSourceControl, osRemote))
  {
    AnsiString TargetDirectory;
    if (InternalDDDownload(TargetDirectory))
    {
      if (FDDExtMapFile != NULL)
      {
        FDDExtTarget = TargetDirectory;
      }
      else
      {
        assert(FInternalDDDownloadList->Count > 0);
        assert(dwEffect == DROPEFFECT_Copy || dwEffect == DROPEFFECT_Move);
        TGUICopyParamType CopyParams = GUIConfiguration->CurrentCopyParam;
        TTransferType TransferType = dwEffect == DROPEFFECT_Copy ? ttCopy : ttMove;
        if (FDDMoveSlipped)
        {
          TransferType = ttMove;
        }
        if (CopyParamDialog(tdToLocal, TransferType,
              false, FInternalDDDownloadList, TargetDirectory, CopyParams,
              WinConfiguration->DDTransferConfirmation, true))
        {
          int Params =
            (TransferType == ttMove ? cpDelete : 0) |
            (CopyParams.NewerOnly ? cpNewerOnly : 0);
          DDDownload(FInternalDDDownloadList, TargetDirectory,
            &CopyParams, Params);
          FInternalDDDownloadList->Clear();
        }
      }
      DoOperation = false;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::RemoteFileControlDDFileOperationExecuted(
  TObject * /*Sender*/, int dwEffect, AnsiString /*SourcePath*/,
  AnsiString /*TargetPath*/)
{
  if ((dwEffect == DROPEFFECT_Move) &&
      IsFileControl(DropSourceControl, osLocal))
  {
    ReloadLocalDirectory();
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalDirViewEnter(TObject * /*Sender*/)
{
  SideEnter(osLocal);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalDriveViewEnter(TObject * /*Sender*/)
{
  MakeNextInTabOrder(LocalDirView, LocalDriveView);
  SideEnter(osLocal);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SideEnter(TOperationSide Side)
{
  if (FCurrentSide != Side)
  {
    CommandLineCombo->Items->Clear();
    FCommandLineComboPopulated = false;
  }
  TCustomScpExplorerForm::SideEnter(Side);
  UpdateControls();
  LocalPathLabel->UpdateStatus();
  RemotePathLabel->UpdateStatus();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::OpenConsole(AnsiString Command)
{
  SaveCommandLine();
  try
  {
    TCustomScpExplorerForm::OpenConsole(Command);
  }
  __finally
  {
    FCommandLineComboPopulated = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::CommandLineComboKeyDown(TObject * /*Sender*/,
  WORD & Key, TShiftState /*Shift*/)
{
  if (Key == VK_RETURN)
  {
    Key = 0;
    ExecuteCommandLine();
  }
  else if ((Key == VK_ESCAPE) && !CommandLineCombo->DroppedDown)
  {
    Key = 0;
    CommandLineCombo->Text = "";
  }
  else if ((Key == VK_UP) || (Key == VK_DOWN))
  {
    CommandLinePopulate();
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SaveCommandLine()
{
  if (FCommandLineComboPopulated)
  {
    CustomWinConfiguration->History[
      FCurrentSide == osRemote ? "Commands" : "LocalCommands"] =
        CommandLineCombo->Items;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::ExecuteCommandLine()
{
  if (!CommandLineCombo->Text.Trim().IsEmpty() &&
      ((FCurrentSide != osRemote) ||
       (Terminal->AllowedAnyCommand(CommandLineCombo->Text) &&
        EnsureCommandSessionFallback(fcAnyCommand))))
  {
    CommandLinePopulate();
    CommandLineCombo->SaveToHistory();
    AnsiString Command = CommandLineCombo->Text;
    CommandLineCombo->Text = "";
    if (FCurrentSide == osRemote)
    {
      OpenConsole(Command);
    }
    else
    {
      AnsiString Program, Params, Dir;
      SplitCommand(Command, Program, Params, Dir);
      if (!ExecuteShell(Program, Params))
      {
        throw Exception(FMTLOAD(EXECUTE_APP_ERROR, (Program)));
      }
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::CommandLineComboDropDown(
  TObject * /*Sender*/)
{
  CommandLinePopulate();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::CommandLinePopulate()
{
  if (!FCommandLineComboPopulated)
  {
    TStrings * CommandsHistory;
    CommandsHistory = CustomWinConfiguration->History[
      FCurrentSide == osRemote ? "Commands" : "LocalCommands"];
    if ((CommandsHistory != NULL) && (CommandsHistory->Count > 0))
    {
      CommandLineCombo->Items = CommandsHistory;
    }
    else
    {
      CommandLineCombo->Items->Clear();
    }
    FCommandLineComboPopulated = true;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::GoToCommandLine()
{
  ComponentVisible[fcCommandLinePanel] = true;
  if (CommandLineCombo->Enabled)
  {
    CommandLineCombo->SetFocus();
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::GoToTree()
{
  if (FCurrentSide == osLocal)
  {
    ComponentVisible[fcLocalTree] = true;
    LocalDriveView->SetFocus();
  }
  else
  {
    TCustomScpExplorerForm::GoToTree();
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::CommandLineComboEnter(TObject * /*Sender*/)
{
  KeyPreview = false;
  TPanel * LastPanel = FCurrentSide == osLocal ? LocalPanel : RemotePanel;
  MakeNextInTabOrder(LastPanel, CommandLinePanel);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::CommandLineComboExit(TObject * /*Sender*/)
{
  KeyPreview = true;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::PanelExportStore(TOperationSide Side,
  TPanelExport Export, TPanelExportDestination Destination,
  TStringList * ExportData)
{
  if (Destination == pedCommandLine)
  {
    ComponentVisible[fcCommandLinePanel] = true;

    AnsiString Buf;
    for (int Index = 0; Index < ExportData->Count; Index++)
    {
      Buf += ExportData->Strings[Index] + " ";
    }

    if (CommandLineCombo->Focused())
    {
      CommandLineCombo->SelText = Buf;
    }
    else
    {
      CommandLineCombo->Text = CommandLineCombo->Text + Buf;
    }
  }
  else
  {
    TCustomScpExplorerForm::PanelExportStore(Side, Export, Destination, ExportData);
  }
}
//---------------------------------------------------------------------------
int __fastcall TScpCommanderForm::GetStaticComponentsHeight()
{
  return TCustomScpExplorerForm::GetStaticComponentsHeight() +
    (CommandLinePanel->Visible ? CommandLinePanel->Height : 0) +
    (BottomDock->Visible ? BottomDock->Height : 0) +
    (StatusBar->Visible ? StatusBar->Height : 0);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::SysResizing(unsigned int Cmd)
{
  TCustomScpExplorerForm::SysResizing(Cmd);

  if ((Cmd == SC_MAXIMIZE) || (Cmd == 61490) ||
      ((Cmd == SC_DEFAULT) && (WindowState != wsMaximized)))
  {
    FNormalPanelsWidth = LocalPanel->Width + RemotePanel->Width;
  }
  else if ((Cmd == SC_RESTORE) || (Cmd == 61730) ||
    ((Cmd == SC_DEFAULT) && (WindowState == wsMaximized)))
  {
    if (FNormalPanelsWidth >= 0)
    {
      Panel(true)->Width = FLeftPanelWidth * FNormalPanelsWidth;
      FNormalPanelsWidth = -1;
    }
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::Resize()
{
  TCustomScpExplorerForm::Resize();

  LeftPanelWidth = FLastLeftPanelWidth;
  UpdateControls();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalFileControlDDMenuPopup(TObject * /*Sender*/,
  HMENU AMenu, IDataObject * /*DataObj*/, int /*AMinCustCmd*/, int /*grfKeyState*/,
  TPoint & /*pt*/)
{
  if (IsFileControl(DropSourceControl, osRemote) &&
      !WinConfiguration->DDAllowMoveInit)
  {
    // index of copy item
    int Index = GetMenuDefaultItem(AMenu, TRUE, 0);
    assert(Index >= 0);

    AnsiString Caption = Dragdrop_MIMoveStr;

    MENUITEMINFO MI;
    memset(&MI, 0, sizeof(MI));
    MI.cbSize = sizeof(MI);
    MI.fMask = MIIM_TYPE | MIIM_ID | MIIM_STATE;
    MI.fType = MFT_STRING;
    MI.wID = 1 /*DragDrop::CmdMove*/;
    MI.dwTypeData = Caption.c_str();
    MI.cch = Caption.Length();
    MI.fState = MFS_ENABLED;
    InsertMenuItem(AMenu, Index, TRUE, &MI);

    if (FDDMoveSlipped)
    {
      SetMenuDefaultItem(AMenu, Index, TRUE);
    }

    FDDMoveSlipped = false;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::PathLabelDblClick(TObject * Sender)
{
  OpenDirectory(Sender == LocalPathLabel ? osLocal : osRemote);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalPathLabelGetStatus(
  TCustomPathLabel * /*Sender*/, bool & Active)
{
  // this strange form is here to make borland compiler work :-)
  Active = Active || LocalDriveView->Focused();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::RemotePathLabelGetStatus(
  TCustomPathLabel * /*Sender*/, bool & Active)
{
  // this strange form is here to make borland compiler work :-)
  Active = Active || RemoteDriveView->Focused();
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalPathLabelPathClick(
  TCustomPathLabel * /*Sender*/, AnsiString Path)
{
  if (ComparePaths(Path, LocalDirView->Path))
  {
    OpenDirectory(osLocal);
  }
  else
  {
    LocalDirView->Path = Path;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::RemotePathLabelPathClick(
  TCustomPathLabel * /*Sender*/, AnsiString Path)
{
  if (UnixComparePaths(Path, RemoteDirView->Path))
  {
    OpenDirectory(osRemote);
  }
  else
  {
    RemoteDirView->Path = Path;
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalDirViewFileIconForName(
  TObject * /*Sender*/, TListItem * /*Item*/, AnsiString & FileName)
{
  AnsiString PartialExt = Configuration->PartialExt;
  if (SameText(ExtractFileExt(FileName), PartialExt))
  {
    FileName.SetLength(FileName.Length() - PartialExt.Length());
  }
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalDirViewUpdateStatusBar(
  TObject * /*Sender*/, const TStatusFileInfo & FileInfo)
{
  UpdateFileStatusBar(LocalStatusBar, FileInfo, 0);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::RemoteDirViewUpdateStatusBar(
  TObject * /*Sender*/, const TStatusFileInfo & FileInfo)
{
  UpdateFileStatusBar(RemoteStatusBar, FileInfo, 0);
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::LocalStatusBarClick(TObject * /*Sender*/)
{
  LocalDirView->SetFocus();
}
//---------------------------------------------------------------------------
AnsiString __fastcall TScpCommanderForm::PathForCaption()
{
  AnsiString Result;
  if (FCurrentSide == osLocal)
  {
    // for consistency do not show even local path when there is no terminal
    if (Terminal != NULL)
    {
      switch (WinConfiguration->PathInCaption)
      {
        case picShort:
          {
            Result = ExtractFileName(LocalDirView->PathName);
            if (Result.IsEmpty())
            {
              Result = LocalDirView->PathName;
            }
          }
          break;

        case picFull:
          Result = LocalDirView->PathName;
          break;
      }
    }
  }
  else
  {
    Result = TCustomScpExplorerForm::PathForCaption();
  }
  return Result;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::GetTransferPresetAutoSelectData(
  TCopyParamRuleData & Data)
{
  TCustomScpExplorerForm::GetTransferPresetAutoSelectData(Data);
  Data.LocalDirectory = LocalDirView->PathName;
}
//---------------------------------------------------------------------------
void __fastcall TScpCommanderForm::BeforeAction()
{
  TCustomScpExplorerForm::BeforeAction();

  if (LocalDirView->ItemFocused != NULL)
  {
    LocalDirView->ItemFocused->CancelEdit();
  }
}
//---------------------------------------------------------------------------
