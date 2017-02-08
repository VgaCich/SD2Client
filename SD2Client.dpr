program SD2Client;

{$R *.res}
{$R Icons.res}

{.$DEFINE SLOW} //Emulate COM-port slowness
{.$DEFINE RONLY} //Disable FS writing
{$DEFINE USEMAGIC} //Use RSFS with Magic

//TODO: Errors checking
//TODO: Drag'n'drop in tree ?
//TODO: Operation cancelling
//TODO: Audio converter
//TODO: Lazy sectors map reading ?
//TODO: Tab not working. WTF?
//TODO: Handle "no free space" exception (on folder writing, etc)
//TODO: Hardlinks?

uses SysSfIni, Windows, Messages, CommCtrl, AvL, avlUtils, ShellAPI, avlSplitter,
  avlListViewEx, avlTreeViewEx, avlCOMPort;

const
  SD2SectorSize = 512;
  RSFSSectorSize = 508;
  RSFSLastSector = Integer($FFFFFFFF);
  RSFSNameLen = 89;
  RSFSAttrNoFile = Integer($FFFFFFFF);
  {$IFDEF USEMAGIC}
  RSFSMagic = Integer($53465352);
  RSFSFirstSector = 32;
  {$ELSE}
  RSFSFirstSector = 0;
  {$ENDIF}

type
  TOnProgress = procedure(Sender: TObject; Progress: Integer) of object;
  TRSFS = class;
  TSD2Device = class
  private
    FName: string;
    FCOMPort: TCOMPort;
    FSectorsCount: Integer;
    FDummyData: TFileStream;
    FFS: TRSFS;
    function CalcChecksum(const Data; Count: Integer): Word;
    function SendCommand(const Cmd: string; Arg: Cardinal): Integer;
  public
    constructor Create(Port: Integer; OnProgress: TOnProgress); overload;
    constructor Create(const DummyFile: string; OnProgress: TOnProgress); overload;
    destructor Destroy; override;
    procedure ReadSector(Sector: Integer; out Data);
    procedure WriteSector(Sector: Integer; const Data);
    function GetNextSector(Sector: Integer): Integer;
    function GetSectorsCount: Integer;
    property FS: TRSFS read FFS;
    property Name: string read FName;
  end;
  TRSFSStream = class;
  TRSFSDirectory = class;
  TRSFS = class
  private
    FDevice: TSD2Device;
    FSectorsCount, FLastAllocated: Integer;
    FSectorsMap: array of Byte;
    FUpdateMap: array of Boolean;
    FRoot: TRSFSDirectory;
    FOnProgress: TOnProgress;
    FOnWriteUpdates: TOnEvent;
    procedure MarkSector(Sector: Integer);
    procedure Progress(Progress: Integer);
    function SectorsMapSize: Integer;
  public
    constructor Create(Device: TSD2Device; OnProgress: TOnProgress = nil);
    destructor Destroy; override;
    function AllocateSector: Integer;
    procedure FreeSector(Sector: Integer);
    procedure WriteUpdates;
    procedure Format;
    function AllocateStream: TRSFSStream;
    procedure CalcSpace(out Total, Free, Occupied: Integer);
    procedure RebuildSectorsMap;
    property Device: TSD2Device read FDevice;
    property Root: TRSFSDirectory read FRoot;
    property OnProgress: TOnProgress read FOnProgress write FOnProgress;
    property OnWriteUpdates: TOnEvent read FOnWriteUpdates write FOnWriteUpdates;
  end;
  TRSFSStream = class(TStream)
  private
    FFS: TRSFS;
    FFirstSector, FCurSector: Integer;
    FSize, FPosition: Integer;
    FSector: packed record
      Data: packed array[0..RSFSSectorSize - 1] of Byte;
      NextSector: Integer;
    end;
    function GetSector(Offset: Integer): Integer;
    function SizeInSectors(SizeInBytes: Integer): Integer;
    function NewSector: Integer;
    procedure ReadSector(Sector: Integer);
    procedure WriteSector;
  protected
    function GetPosition: Integer; override;
    function GetSize: Integer; override;
    procedure SetSize(Value: Integer); override;
  public
    constructor Create(FS: TRSFS; FirstSector: Integer; Size: Integer = -1);
    function Read(var Buffer; Count: Integer): Integer; override;
    function Write(const Buffer; Count: Integer): Integer; override;
    function Seek(Offset: Integer; Origin: Word): Integer; override;
    property FirstSector: Integer read FFirstSector;
  end;
  TRSFSName = packed array[0..RSFSNameLen] of Char;
  PRSFSEntry = ^TRSFSEntry;
  TRSFSEntry = packed record
    Name: TRSFSName;
    FirstSector, Size, Attr: Integer;
    CreateTime, AccessTime, WriteTime: TFileTime;
    Icon: array[0..127] of Byte;
  end;
  TRSFSDirectory = class
  private
    FFS: TRSFS;
    FParent: TRSFSDirectory;
    FData: TRSFSStream;
    FEntries: TList;
    FTag: Integer;
    function GetCount: Integer;
    function GetEntry(Index: Integer): PRSFSEntry;
    function GetFirstSector: Integer;
    procedure Progress(Progress: Integer);
  public
    constructor Create(FS: TRSFS; Parent: TRSFSDirectory; FirstSector: Integer);
    destructor Destroy; override;
    procedure Reload;
    procedure Save;
    procedure Clear;
    procedure DeleteEntry(Index: Integer);
    function NewEntry: Integer;
    function FindEntry(Name: string; IsDir: Boolean): Integer; overload;
    function FindEntry(Entry: PRSFSEntry): Integer; overload;
    function FindEntry(FirstSector: Integer; IsDir: Boolean): Integer; overload;
    function IsDirectory(Index: Integer): Boolean;
    property Count: Integer read GetCount;
    property Entry[Index: Integer]: PRSFSEntry read GetEntry; default;
    property FirstSector: Integer read GetFirstSector;
    property Parent: TRSFSDirectory read FParent;
    property Tag: Integer read FTag write FTag;
  end;
  TMainForm = class(TForm)
  private
    TVFolders: TTreeViewEx;
    LVFiles: TListViewEx;
    TB: TToolBar;
    SB: TStatusBar;
    Splitter: TSplitter;
    PB: TProgressBar;
    MenuView, MenuPorts, MenuUpload, MenuFolders, MenuFiles: TMenu;
    TBImages, Folders, Icons, SmallIcons: TImageList;
    AccelTable: HAccel;
    COMPorts: TCOMPorts;
    SD2Device: TSD2Device;
    FStatusCnt: Integer;
    function UniqueName(Dir: TRSFSDirectory; const Name: string; IsDir: Boolean): string;
    procedure SetName(var Name: TRSFSName; const NewName: string);
    function FormClose(Sender: TObject): Boolean;
    function FormProcessMsg(var Msg: TMsg): Boolean;
    procedure AdjustColumns(Sender: TObject);
    procedure SplitterMove(Sender: TObject);
    procedure Progress(Sender: TObject; Progress: Integer);
    procedure UpdateSpace(Sender: TObject);
    procedure Status(const Msg: string; ControlsEnabled: Boolean);
    procedure SetDevButtons(Enabled: Boolean);
    procedure Connect(Port: Integer);
    procedure ClearTree(Node: Integer);
    procedure RefreshTree(Node: Integer);
    function FillNode(Node: Integer): Integer;
    procedure FillList(Dir: TRSFSDirectory);
    procedure DeleteFile(Entry: PRSFSEntry);
    procedure DeleteDir(Dir: TRSFSDirectory);
    procedure UploadFile(Dir: TRSFSDirectory; const Name: string);
    procedure UploadDir(Dir: TRSFSDirectory; const Path: string);
    procedure DownloadFile(Entry: PRSFSEntry; const Name: string);
    procedure DownloadDirectory(Dir: TRSFSDirectory; const Path: string);
    procedure ConnectClick;
    procedure DisconnectClick;
    procedure FormatClick;
    procedure RepairClick;
    procedure NewFolderClick;
    procedure UploadClick;
    procedure UploadFilesClick;
    procedure UploadDirectoryClick;
    procedure DownloadClick;
    procedure DeleteClick;
    procedure PropertiesClick;
    procedure ViewClick;
    procedure ShowAbout;
  public
    constructor Create;
    destructor Destroy; override;
    procedure WMCommand(var Msg: TWMCommand); message WM_COMMAND;
    procedure WMSize(var Msg: TWMSize); message WM_SIZE;
    procedure WMNotify(var Msg: TWMNotify); message WM_NOTIFY;
    procedure WMDropFiles(var Msg: TWMDropFiles); message WM_DROPFILES;
    procedure WMContextMenu(var Msg: TWMContextMenu); message WM_CONTEXTMENU;
  end;

type
  TTBButtons = (tbConnect, tbDisconnect, tbFormat, tbRepair, tbRefresh, tbNewFolder, tbUpload, tbDownload, tbDelete, tbProperties, tbView, tbAbout);

const
  RCGet = 'Get';
  RCPut = 'Put';
  RCPar = 'Par';
  RCNes = 'Nes';
  RADat = 'Dat'#10;
  RARdy = 'Rdy'#10;
  RAErr = 'Err'#10;
  RAErC = 'ErC'#10;
  RaECS = 'ECS'#10;
  CmdRetries = 8;
  DataRetries = 3; 
  CCapt = 'VgaSoft SD2 Client 1.0'{$IFDEF RONLY}+' (Read-only)'{$ENDIF}{$IFDEF SLOW}+' (Slow fileop)'{$ENDIF};
  AboutText = CCapt + #13#10 +
            'Copyright '#169'VgaSoft, 2016';
  SFilter = 'All files|*.*';
  SNewFolder = 'New Folder';
  SFreeSpace = '%s/%s free';
  SReady = 'Ready';
  SReadingFS = 'Reading filesystem...';
  SWritingFS = 'Writing filesystem...';
  SFormatting = 'Formatting...';
  SRepairing = 'Repairing FS...';
  SCantOpenDevice = 'Can''t open device: ';
  SNotFormatted = 'Connected device is not formatted';
  SOpenDeviceConfirm = 'Opening physical device is dangerous!'#13#10'Open "%s"?';
  SFormatConfirm = 'Formatting device will destroy all data on it!'#13#10'Continue?';
  SRepairConfirm = 'Repairing filesystem on device.'#13#10'Continue?';
  SDeletingDirectory = 'Deleting directory...';
  SDeleteDirConfirm = 'Deleting directory "%s" and its contents.'#13#10'Continue?';
  SDeletingFiles = 'Deleting files...';
  SDeleteFileConfirm = 'Deleting file "%s".'#13#10'Continue?';
  SDeleteFilesConfirm = 'Deleting selected files.'#13#10'Continue?';
  SUploadingFile = 'Uploading file "%s"...';
  SUploadingDirectory = 'Uploading directory...';
  SNotEnoughSpace = 'No enough space to upload file "%s"';
  SFileAlreadyExists = 'File "%s" already exists.'#13#10'Rename?';
  SDownloadingFile = 'Downloading file "%s"...';
  SDownloadingDirectory = 'Downloading directory...';
  SFileInfo = '%s: [%s] [%s] [%s]';
  SFolderSize = 'Files: %d; Total size: %s';
  SOverwriteConfirm = 'File "%s" already exists.'#13#10'Overwrite?';
  SAlreadyExists = 'Entry "%s" already exists.'#13#10'Rename?';
  SFileProperties = 'File: %s'#13#10'Size: %s'#13#10'Attributes: %s (0x%08x)'#13#10;
  SDirProperties = 'Directory: %s'#13#10'Entries: %d'#13#10'Total size: %s'#13#10'Attributes: %s (0x%08x)'#13#10;
  SDates = 'Created: %s'#13#10'Modified: %s'#13#10'Opened: %s';
  tbUploadItem = Integer(tbUpload) + 1;
  tbViewItem = Integer(tbView) + 2;
  TBButtons: array[0..13] of record Caption: string; ImageIndex: Integer; end = (
    (Caption: 'Connect (F3)'; ImageIndex: 0),
    (Caption: 'Disconnect (F4)'; ImageIndex: 1),
    (Caption: 'Format'; ImageIndex: 6),
    (Caption: 'Repair'; ImageIndex: 11),
    (Caption: 'Refresh (F5)'; ImageIndex: 2),
    (Caption: '-'; ImageIndex: -1),
    (Caption: 'New folder (F7)'; ImageIndex: 9),
    (Caption: 'Upload'; ImageIndex: 3),
    (Caption: 'Download (Ctrl-S)'; ImageIndex: 4),
    (Caption: 'Delete (Del)'; ImageIndex: 5),
    (Caption: 'Properties (Alt-Enter)'; ImageIndex: 10),
    (Caption: '-'; ImageIndex: -1),
    (Caption: 'View'; ImageIndex: 7),
    (Caption: 'About (F1)'; ImageIndex: 8));
  DevButtons: array[0..7] of TTBButtons = (tbFormat, tbRepair, tbRefresh, tbNewFolder, tbUpload, tbDownload, tbDelete, tbProperties);
  ColSize = 1;
  ColTime = 2;
  ColAttr = 3;
  LVFilesColumns: array[0..3] of record Caption: string; Width: Integer end = (
    (Caption: 'Name'; Width: 150),
    (Caption: 'Size'; Width: 80),
    (Caption: 'Time'; Width: 120),
    (Caption: 'Attr.'; Width: 40));
  ViewReport = 1001;
  ViewList = 1002;
  ViewSmallIcons = 1003;
  ViewIcons = 1004;
  MenuViewTemplate: array[0..4] of PChar = ('1001',
    '&Report',
    '&List',
    '&Small icons',
    '&Icons');
  PortsDummy = 2000;
  MenuPortsTemplate: array[0..1] of PChar = ('2000',
    '&File...');
  UploadFiles = 3001;
  UploadDirectory = 3002;
  MenuUploadTemplate: array[0..2] of PChar = ('3001',
    '&Files...'#9'Ctrl-O',
    '&Directory...'#9'Ctrl-D');
  FoldersRefresh = 10001;
  FoldersRename = FoldersRefresh + 2;
  FoldersNewFolder = FoldersRename + 1;
  FoldersUpload = FoldersNewFolder + 1;
  FoldersDownload = FoldersUpload + 1;
  FoldersDelete = FoldersDownload + 1;
  FoldersProperties = FoldersDelete + 1;
  MenuFoldersTemplate: array[0..8] of PChar = ('10001',
    'Re&fresh'#9'F5',
    '-',
    '&Rename'#9'F2',
    '&New folder'#9'F7',
    '&Upload',
    '&Download'#9'Ctrl-S',
    'D&elete'#9'Del',
    '&Properties'#9'Alt-Enter');
  FilesRefresh = 11001;
  FilesRename = FilesRefresh + 2;
  FilesUpload = FilesRename + 1;
  FilesDownload = FilesUpload + 1;
  FilesDelete = FilesDownload + 1;
  FilesProperties = FilesDelete + 1;
  MenuFilesTemplate: array[0..7] of PChar = ('11001',
    'Re&fresh'#9'F5',
    '-',
    '&Rename'#9'F2',
    '&Upload'#9'Ctrl-O',
    '&Download'#9'Ctrl-S',
    'D&elete'#9'Del',
    '&Properties'#9'Alt-Enter');
  MainConnect = 9001;
  MainDisconnect = MainConnect + 1;
  MainRefresh = MainDisconnect + 1;
  MainRename = MainRefresh + 1;
  MainNewFolder = MainRename + 1;
  MainDownload = MainNewFolder + 1;
  MainDelete = MainDownload + 1;
  MainProperties = MainDelete + 1;
  MainAbout = MainProperties + 1;
  SBPartSpace = 0;
  SBPartProgress = 1;
  SBPartProgressText = 2;
  SBPartInfo = 3;
  SBParts: array[0..3] of Integer = (150, 250, 280, -1);

var
  Accels: array[0..10] of TAccel = (
    (fVirt: FVIRTKEY; Key: VK_F3; Cmd: MainConnect),
    (fVirt: FVIRTKEY; Key: VK_F4; Cmd: MainDisconnect),
    (fVirt: FVIRTKEY; Key: VK_F2; Cmd: MainRename),
    (fVirt: FCONTROL or FVIRTKEY; Key: Ord('O'); Cmd: UploadFiles),
    (fVirt: FCONTROL or FVIRTKEY; Key: Ord('D'); Cmd: UploadDirectory),
    (fVirt: FCONTROL or FVIRTKEY; Key: Ord('S'); Cmd: MainDownload),
    (fVirt: FVIRTKEY; Key: VK_DELETE; Cmd: MainDelete),
    (fVirt: FALT or FVIRTKEY; Key: VK_RETURN; Cmd: MainProperties),
    (fVirt: FVIRTKEY; Key: VK_F1; Cmd: MainAbout),
    (fVirt: FVIRTKEY; Key: VK_F5; Cmd: MainRefresh),
    (fVirt: FVIRTKEY; Key: VK_F7; Cmd: MainNewFolder));

function ImageList_GetIcon(ImageList: HIMAGELIST; Index: Integer; Flags: DWORD): HICON; stdcall; external 'comctl32.dll';

const
  IOCTL_DISK_GET_DRIVE_GEOMETRY = $00000007 shl 16;
  IOCTL_STORAGE_CHECK_VERIFY = ($0000002D shl 16) or ($0001 shl 14) or ($0200 shl 2);

type
  TDiskGeometry = record
    Cylinders: TLargeInteger;
    MediaType: Integer;
    TracksPerCylinder, SectorsPerTrack, BytesPerSector: DWORD;
  end;

function GetDirInformation(const Path: string; var DirInfo: TByHandleFileInformation): Boolean;
var
  Dir: THandle;
begin
  Result := false;
  Dir := CreateFile(PChar(Path), GENERIC_READ, FILE_SHARE_READ or FILE_SHARE_WRITE, nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL or FILE_FLAG_BACKUP_SEMANTICS, 0);
  if Dir <> INVALID_HANDLE_VALUE then
  begin
    Result := GetFileInformationByHandle(Dir, DirInfo);
    CloseHandle(Dir);
  end;
end;

function SetDirTime(const Path: string; const CreateTime, AccessTime, WriteTime: TFileTime): Boolean;
var
  Dir: THandle;
begin
  Result := false;
  Dir := CreateFile(PChar(Path), GENERIC_WRITE, FILE_SHARE_READ or FILE_SHARE_WRITE, nil, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL or FILE_FLAG_BACKUP_SEMANTICS, 0);
  if Dir <> INVALID_HANDLE_VALUE then
  begin
    Result := SetFileTime(Dir, @CreateTime, @AccessTime, @WriteTime);
    CloseHandle(Dir);
  end;
end;

procedure EnumRemovableDevices(var List: TCOMPorts);
var
  i: Integer;
  DriveHandle: THandle;
  BytesReturned: Cardinal;
  Geom: TDiskGeometry;
begin
  for i := 0 to 63 do
  begin
    DriveHandle := FileOpen('\\.\PhysicalDrive' + IntToStr(i), fmOpenRead or fmShareDenyNone);
    if DriveHandle <> INVALID_HANDLE_VALUE then
    try
      if not DeviceIoControl(DriveHandle, IOCTL_STORAGE_CHECK_VERIFY, nil, 0, nil, 0, BytesReturned, nil) or
         not DeviceIoControl(DriveHandle, IOCTL_DISK_GET_DRIVE_GEOMETRY, nil, 0, @Geom, SizeOf(Geom), BytesReturned, nil) or
         (Geom.MediaType <> 11) or (Geom.BytesPerSector <> SD2SectorSize) then Continue;
      SetLength(List, Length(List) + 1);
      List[High(List)].Name := Format('Physical Drive %d (%s)', [i, SizeToStr(Geom.Cylinders * Geom.TracksPerCylinder * Geom.SectorsPerTrack * Geom.BytesPerSector)]);
      List[High(List)].Index := -i - 1;
    finally
      CloseHandle(DriveHandle);
    end;
  end;
end;

var
  MainForm: TMainForm;

{ TSD2Device }

constructor TSD2Device.Create(Port: Integer; OnProgress: TOnProgress);
begin
  FCOMPort := TCOMPort.Create(Port, 4096);
  try
    with FCOMPort do
    begin
      BitRate := 115200;
      DataBits := 8;
      FlowControl := fcNone;
      Parity := NOPARITY;
      StopBits := ONESTOPBIT;
    end;
    FName := 'SD2 at COM' + IntToStr(Port);
    Assert(GetSectorsCount > 1);
    FFS := TRSFS.Create(Self, OnProgress);
  except
    FAN(FCOMPort);
    raise;
  end;
end;

constructor TSD2Device.Create(const DummyFile: string; OnProgress: TOnProgress);
var
  BytesReturned: Cardinal;
  Geom: TDiskGeometry;
begin
  FDummyData := TFileStream.Create(DummyFile, fmOpenReadWrite or fmShareDenyNone);
  if DeviceIoControl(FDummyData.Handle, IOCTL_DISK_GET_DRIVE_GEOMETRY, nil, 0, @Geom, SizeOf(Geom), BytesReturned, nil) then
  begin
    FSectorsCount := Geom.Cylinders * Geom.TracksPerCylinder * Geom.SectorsPerTrack;
    FName := 'Device: ' + ExtractFileName(DummyFile);
  end
  else begin
    FSectorsCount := FDummyData.Size div SD2SectorSize;
    FName := 'File: ' + ExtractFileName(DummyFile);
  end;
  FFS := TRSFS.Create(Self, OnProgress);
end;

destructor TSD2Device.Destroy;
begin
  FAN(FFS);
  FAN(FCOMPort);
  FAN(FDummyData);
  inherited;
end;

procedure TSD2Device.ReadSector(Sector: Integer; out Data);
var
  Checksum: Word;
  Dummy: Byte;
  i: Integer;
begin
  if Assigned(FCOMPort) then
  begin
    for i := 0 to DataRetries - 1 do
    begin
      SendCommand(RCGet, SD2SectorSize * Sector);
      if FCOMPort.Read(Data, SD2SectorSize) = SD2SectorSize then
      begin
        FCOMPort.Read(Checksum, SizeOf(Checksum));
        FCOMPort.Read(Dummy, 1);
        if (Dummy = 10) and (Checksum = CalcChecksum(Data, SD2SectorSize)) then Exit;
      end;
      Sleep(500);
    end;
    if Checksum = CalcChecksum(Data, SD2SectorSize) then
      raise Exception.CreateFmt('SD2 Error: sector 0x%08x checksum wrong', [Sector])
    else
      raise Exception.Create('SD2 Error: incomplete sector data');
  end;
  if Assigned(FDummyData) then
  begin
    Assert(Sector < GetSectorsCount);
    FDummyData.Seek(SD2SectorSize * Sector, soFromBeginning);
    FDummyData.Read(Data, SD2SectorSize);
    {$IFDEF SLOW}Sleep(50);{$ENDIF}
  end;
end;

procedure TSD2Device.WriteSector(Sector: Integer; const Data);
var
  Checksum: Word;
  RespBuf: string;
  i: Integer;
begin
  if Assigned(FCOMPort) then
  begin
    {$IFDEF RONLY}Exit;{$ENDIF}
    Checksum := CalcChecksum(Data, SD2SectorSize);
    for i := 0 to DataRetries - 1 do
    begin
      SendCommand(RCPut, SD2SectorSize * Sector);
      FCOMPort.Write(Data, SD2SectorSize);
      FCOMPort.Write(Checksum, SizeOf(Checksum));
      FCOMPort.Flush;
      SetLength(RespBuf, 4);
      FCOMPort.Read(RespBuf[1], 4);
      if RespBuf = RARdy then Exit;
      Sleep(500);
    end;
    raise Exception.CreateFmt('SD2 Error %s at writing sector 0x%08x', [Copy(RespBuf, 1, 3), Sector]);
  end;
  if Assigned(FDummyData) then
  begin
    Assert(Sector < GetSectorsCount);
    FDummyData.Seek(SD2SectorSize * Sector, soFromBeginning);
    FDummyData.Write(Data, SD2SectorSize);
    {$IFDEF SLOW}Sleep(50);{$ENDIF}
  end;
end;

function TSD2Device.GetNextSector(Sector: Integer): Integer;
var
  Buf: array[0..SD2SectorSize div 4 - 1] of Integer;
begin
  Assert((Sector <> 0) and (Sector <> RSFSLastSector));
  if Assigned(FCOMPort) then
  begin
    Result := SendCommand(RCNes, Sector);
  end;
  if Assigned(FDummyData) then
  begin
    Assert(Sector < GetSectorsCount);
    FDummyData.Seek(SD2SectorSize * Sector, soFromBeginning);
    FDummyData.Read(Buf, SizeOf(Buf));
    Result := Buf[High(Buf)];
    {$IFDEF SLOW}Sleep(15);{$ENDIF}
  end;
end;

function TSD2Device.GetSectorsCount: Integer;
begin
  if Assigned(FCOMPort) then
  begin
    Result := SendCommand(RCPar, 0);
  end;
  if Assigned(FDummyData) then
  begin
    Result := FSectorsCount;
    {$IFDEF SLOW}Sleep(15);{$ENDIF}
  end;
end;

function TSD2Device.CalcChecksum(const Data; Count: Integer): Word;
var
  i, j: Integer;
  C, P: Byte;
begin
  Result := 0;
  for i := 0 to Count - 1 do
  begin
    C := PByteArray(@Data)[i];
    for j := 0 to 7 do
    begin
      P := Hi(Result);
      Result := Result shl 1;
      if (P xor C) and $80 <> 0 then
        Result := Result xor $1021;
      C := C shl 1;
    end;
  end;
  Result := System.Swap(Result);
end;

function TSD2Device.SendCommand(const Cmd: string; Arg: Cardinal): Integer;
var
  ArgBuf: string;
  i: Integer;
begin
  if not Assigned(FCOMPort) then Exit;
  for i := 0 to CmdRetries - 1 do
  begin
    FCOMPort.Purge(true, true);
    FCOMPort.Write(Cmd[1], Length(Cmd) + 1);
    ArgBuf := IntToHex(Integer(Arg), 8);
    while (Length(ArgBuf) > 1) and (ArgBuf[1] = '0') do
      ArgBuf := Copy(ArgBuf, 2, MaxInt);
    FCOMPort.Write(ArgBuf[1], Length(ArgBuf) + 1);
    FCOMPort.Flush;
    SetLength(ArgBuf, 4);
    SetLength(ArgBuf, FCOMPort.Read(ArgBuf[1], 4));
    if ArgBuf = RADat then
    begin
      if (Cmd <> RCGet) and (Cmd <> RCPut) then
      begin
        SetLength(ArgBuf, 9);
        FCOMPort.Read(ArgBuf[1], 9);
        Result := StrToInt('$' + Trim(ArgBuf));
      end
        else Result := 0;
      Exit;
    end;
    Sleep(100);
  end;
  if ArgBuf <> '' then
    raise Exception.CreateFmt('SD2 Error %s at command %s(0x%08x)', [Copy(ArgBuf, 1, 3), Copy(Cmd, 1, 3), Arg])
  else
    raise Exception.Create('No responce from SD2');
end;

{ TRSFS }

constructor TRSFS.Create(Device: TSD2Device; OnProgress: TOnProgress = nil);
var
  i: Integer;
  {$IFDEF USEMAGIC}Unformatted: Boolean;{$ENDIF}
begin
  {$IFDEF USEMAGIC}Unformatted := false;{$ENDIF}
  FDevice := Device;
  FOnProgress := OnProgress;
  FSectorsCount := Device.GetSectorsCount;
  FLastAllocated := RSFSFirstSector;
  Assert(FSectorsCount > RSFSFirstSector + 1);
  SetLength(FSectorsMap, SD2SectorSize * SectorsMapSize);
  SetLength(FUpdateMap, SectorsMapSize);
  for i := 0 to High(FUpdateMap) do
  begin
    Progress(100 * i div Length(FUpdateMap));
    Device.ReadSector(i, FSectorsMap[SD2SectorSize * i]);
    {$IFDEF USEMAGIC}
    if (i = 0) and (PInteger(@FSectorsMap[0])^ <> RSFSMagic) then
      Unformatted := true;
    {$ENDIF}
    FUpdateMap[i] := false;
  end;
  {$IFDEF USEMAGIC}if not Unformatted then{$ENDIF}
    FRoot := TRSFSDirectory.Create(Self, nil, SectorsMapSize);
end;

destructor TRSFS.Destroy;
begin
  if Assigned(FRoot) then
  begin
    FRoot.Save;
    WriteUpdates;
  end;
  FAN(FRoot);
  Finalize(FSectorsMap);
  Finalize(FUpdateMap);
  inherited;
end;

function TRSFS.AllocateSector: Integer;
var
  i, j: Integer;
begin
  if not Assigned(FRoot) then
    raise Exception.Create('SD2 Error: operation on unformatted FS');
  for i := FLastAllocated div 8 to Min(FSectorsCount div 8, High(FSectorsMap)) do
    if FSectorsMap[i] <> $FF then
      for j := 0 to 7 do
        if (FSectorsMap[i] and (1 shl j) = 0) and (8 * i + j < FSectorsCount) then
        begin
          Result := 8 * i + j;
          MarkSector(Result);
          FLastAllocated := Result;
          Exit;
        end;
  if FLastAllocated <> RSFSFirstSector then
  begin
    FLastAllocated := RSFSFirstSector;
    Result := AllocateSector;
  end
    else Result := RSFSLastSector;
end;

procedure TRSFS.FreeSector(Sector: Integer);
begin
  if not Assigned(FRoot) then
    raise Exception.Create('SD2 Error: operation on unformatted FS');
  if (Sector < RSFSFirstSector) or (Sector >= FSectorsCount) then Exit;
  FSectorsMap[Sector div 8] := FSectorsMap[Sector div 8] and not (1 shl (Sector mod 8));
  FUpdateMap[Sector div (8 * SD2SectorSize)] := true;
  if FLastAllocated > Sector then
    FLastAllocated := Sector;
end;

procedure TRSFS.WriteUpdates;
var
  i: Integer;
begin
  if not Assigned(FRoot) then
    raise Exception.Create('SD2 Error: operation on unformatted FS');
  for i := 0 to High(FUpdateMap) do
  begin
    Progress(100 * i div Length(FUpdateMap));
    if FUpdateMap[i] then
    begin
      Device.WriteSector(i, FSectorsMap[SD2SectorSize * i]);
      FUpdateMap[i] := false;
    end;
  end;
  if Assigned(FOnWriteUpdates) then
    FOnWriteUpdates(Self);
end;

procedure TRSFS.Format;
var
  i: Integer;
  Buf: array[0..SD2SectorSize - 1] of Byte;
begin
  for i := 0 to High(FSectorsMap) do
    if i < Length(FUpdateMap) div 8 then
      FSectorsMap[i] := $FF
    else
      FSectorsMap[i] := 0;
  for i := 0 to Length(FUpdateMap) mod 8 - 1 do
    MarkSector(8 * (Length(FUpdateMap) div 8) + i);
  MarkSector(SectorsMapSize);
  for i := 0 to High(FUpdateMap) do
    FUpdateMap[i] := true;
  {$IFDEF USEMAGIC}PInteger(@FSectorsMap[0])^ := RSFSMagic;{$ENDIF}
  FLastAllocated := RSFSFirstSector;
  if not Assigned(FRoot) then
  begin
    FillChar(Buf, SD2SectorSize, $FF);
    FDevice.WriteSector(SectorsMapSize, Buf);
    FRoot := TRSFSDirectory.Create(Self, nil, SectorsMapSize);
  end
    else FRoot.Clear;
  FRoot.Save;
  WriteUpdates;
end;

function TRSFS.AllocateStream: TRSFSStream;
var
  Sector: Integer;
begin
  Sector := AllocateSector;
  if Sector = RSFSLastSector then
    raise Exception.Create('No free space');
  Result := TRSFSStream.Create(Self, Sector, 0);
  Result.FSector.NextSector := RSFSLastSector;
  Result.Write(Self, 0);
end;

procedure TRSFS.CalcSpace(out Total, Free, Occupied: Integer);
var
  i, j: Integer;
begin
  Total := (FSectorsCount - Length(FUpdateMap)) * RSFSSectorSize;
  Free := 0;
  for i := RSFSFirstSector div 8 to Min(FSectorsCount div 8, High(FSectorsMap)) do
    if FSectorsMap[i] = 0 then
      Inc(Free, 8 * RSFSSectorSize)
    else if FSectorsMap[i] <> $FF then
      for j := 0 to 7 do
        if (FSectorsMap[i] and (1 shl j) = 0) and (8 * i + j < FSectorsCount) then
          Inc(Free, RSFSSectorSize);
  Occupied := Total - Free;
end;

procedure TRSFS.RebuildSectorsMap;
var
  TotalSize, TraversedSize: Integer;
  SectorsMap: array of Byte;
  Files: array of record
    FirstSector, Size: Integer;
  end;

  procedure MarkSector(Sector: Integer);
  begin
    SectorsMap[Sector div 8] := SectorsMap[Sector div 8] or (1 shl (Sector mod 8));
  end;

  function TraverseStream(Sector: Integer; Size: Integer = -1): Integer;
  var
    Traversed: Integer;
  begin
    Traversed := 0;
    while (Sector <> 0) and (Sector <> RSFSLastSector) do
    begin
      Inc(Traversed, RSFSSectorSize);
      if Size > 0 then
        Progress(Round(100 * ((TraversedSize + Traversed) / TotalSize)));
      MarkSector(Sector);
      Sector := Device.GetNextSector(Sector);
    end;
  end;

  procedure LoadDirectory(Sector: Integer);
  var
    Data: TRSFSStream;
    Entry: TRSFSEntry;
  begin
    TraverseStream(Sector);
    Data := TRSFSStream.Create(Self, Sector);
    while Data.Position < Data.Size do
    begin
      Data.Read(Entry, SizeOf(Entry));
      if (Entry.Attr = RSFSAttrNoFile) or (Entry.Name[0] = #0) then
        Continue
      else if Entry.Attr and FILE_ATTRIBUTE_DIRECTORY <> 0 then
        LoadDirectory(Entry.FirstSector)
      else begin
        SetLength(Files, Length(Files) + 1);
        Files[High(Files)].FirstSector := Entry.FirstSector;
        Files[High(Files)].Size := Entry.Size;
        Inc(TotalSize, Entry.Size);
      end;
    end;
  end;

var
  i: Integer;
begin
  if not Assigned(FRoot) then
    raise Exception.Create('SD2 Error: operation on unformatted FS');
  SetLength(SectorsMap, SD2SectorSize * SectorsMapSize);
  SetLength(Files, 0);
  try
    TotalSize := 0;
    TraversedSize := 0;
    for i := 0 to High(SectorsMap) do
      if i < Length(FUpdateMap) div 8 then
        SectorsMap[i] := $FF
      else
        SectorsMap[i] := 0;
    for i := 0 to Length(FUpdateMap) mod 8 - 1 do
      MarkSector(8 * (Length(FUpdateMap) div 8) + i);
    MarkSector(SectorsMapSize);
    {$IFDEF USEMAGIC}PInteger(@SectorsMap[0])^ := RSFSMagic;{$ENDIF}
    LoadDirectory(SectorsMapSize);
    for i := 0 to High(Files) do
    begin
      TraverseStream(Files[i].FirstSector, Files[i].Size);
      Inc(TraversedSize, Files[i].Size);
    end;
    for i := 0 to High(FSectorsMap) do
      FSectorsMap[i] := SectorsMap[i];
    for i := 0 to High(FUpdateMap) do
      FUpdateMap[i] := true;
    FLastAllocated := RSFSFirstSector;
    WriteUpdates;
  finally
    Finalize(SectorsMap);
    Finalize(Files);
  end;
end;

procedure TRSFS.MarkSector(Sector: Integer);
begin
  FSectorsMap[Sector div 8] := FSectorsMap[Sector div 8] or (1 shl (Sector mod 8));
  FUpdateMap[Sector div (8 * SD2SectorSize)] := true;
end;

procedure TRSFS.Progress(Progress: Integer);
begin
  if Assigned(FOnProgress) then
    FOnProgress(Self, Progress);
end;

function TRSFS.SectorsMapSize: Integer;
const
  BitsInSector = 8 * SD2SectorSize;
begin
  Result := Max(RSFSFirstSector, (FSectorsCount + BitsInSector - 1) div BitsInSector);
end;

{ TRSFSStream }

constructor TRSFSStream.Create(FS: TRSFS; FirstSector: Integer; Size: Integer = -1);
var
  CurSector: Integer;
begin
  inherited Create;
  FFS := FS;
  FFirstSector := FirstSector;
  ReadSector(FirstSector);
  if Size < 0 then
  begin
    FSize := 0;
    CurSector := FirstSector;
    while (CurSector <> RSFSLastSector) and (CurSector <> 0) do
    begin
      Inc(FSize, RSFSSectorSize);
      CurSector := FFS.Device.GetNextSector(CurSector);
    end;
  end
    else FSize := Size;
end;

function TRSFSStream.Read(var Buffer; Count: Integer): Integer;
var
  MoveSize: Integer;
  P: Pointer;
begin
  Result := 0;
  P := @Buffer;
  while (Count > 0) and (FPosition < FSize) do
  begin
    MoveSize := Min(Count, RSFSSectorSize - FPosition mod RSFSSectorSize);
    Move(FSector.Data[FPosition mod RSFSSectorSize], P^, MoveSize);
    Inc(FPosition, MoveSize);
    Inc(Result, MoveSize);
    Inc(Cardinal(P), MoveSize);
    Dec(Count, MoveSize);
    if (FPosition mod RSFSSectorSize = 0) and (MoveSize > 0) then
    begin
      if (FSector.NextSector = RSFSLastSector) or (FSector.NextSector = 0) then Exit;
      ReadSector(FSector.NextSector);
    end;
  end;
end;

function TRSFSStream.Write(const Buffer; Count: Integer): Integer;
var
  MoveSize, NextSector: Integer;
  P: Pointer;
begin
  Result := 0;
  P := @Buffer;
  while Count > 0 do
  begin
    if (FPosition = FSize) and (FPosition div RSFSSectorSize > 0) and (FPosition mod RSFSSectorSize = 0) then
      if NewSector = RSFSLastSector then Exit;
    MoveSize := Min(Count, RSFSSectorSize - FPosition mod RSFSSectorSize);
    Move(P^, FSector.Data[FPosition mod RSFSSectorSize], MoveSize);
    Inc(FPosition, MoveSize);
    FSize := Max(FSize, FPosition);
    Inc(Result, MoveSize);
    Inc(Cardinal(P), MoveSize);
    Dec(Count, MoveSize);
    if (FPosition < FSize) and (FPosition mod RSFSSectorSize = 0) then
    begin
      WriteSector;
      if Count > RSFSSectorSize then
      begin
        FCurSector := FSector.NextSector;
        FSector.NextSector := FFS.Device.GetNextSector(FCurSector)
      end
        else ReadSector(FSector.NextSector);
    end;
  end;
  if FPosition = FSize then WriteSector;
end;

function TRSFSStream.Seek(Offset: Integer; Origin: Word): Integer;
var
  Sector: Integer;
begin
  case Origin of
    soFromBeginning: Result := Offset;
    soFromCurrent: Result := FPosition + Offset;
    soFromEnd: Result := FSize + Offset;
    else begin
      Result := FPosition;
      Exit;
    end;
  end;
  Result := Max(0, Min(Result, FSize));
  if GetSector(Result) < GetSector(FPosition) then
  begin
    ReadSector(FFirstSector);
    FPosition := 0;
  end;
  Sector := FCurSector;
  while GetSector(FPosition) < GetSector(Result) do
  begin
    Sector := FFS.Device.GetNextSector(Sector);
    Inc(FPosition, RSFSSectorSize);
  end;
  ReadSector(Sector);
  FPosition := Result;
end;

function TRSFSStream.GetPosition: Integer;
begin
  Result := FPosition;
end;

function TRSFSStream.GetSize: Integer;
begin
  Result := FSize;
end;

procedure TRSFSStream.SetSize(Value: Integer);
var
  CurSector: Integer;
begin
  if Value > FSize then
  begin
    while SizeInSectors(Value) > SizeInSectors(FSize) do
    begin
      if NewSector = RSFSLastSector then Exit;
      Inc(FSize, RSFSSectorSize);
    end;
    FSize := Value;
  end
  else if Value < FSize then
  begin
    if GetSector(FPosition) < SizeInSectors(Value) then
    begin
      CurSector := FCurSector;
      FSize := SizeInSectors(FPosition) * RSFSSectorSize;
    end
    else begin
      CurSector := FFirstSector;
      FSize := RSFSSectorSize;
    end;
    while FSize < Value do
    begin
      CurSector := FFS.Device.GetNextSector(CurSector);
      Inc(FSize, RSFSSectorSize);
    end;
    ReadSector(CurSector);
    CurSector := FSector.NextSector;
    FSector.NextSector := RSFSLastSector;
    WriteSector;
    while (CurSector <> RSFSLastSector) and (CurSector <> 0) do
    begin
      FFS.FreeSector(CurSector);
      CurSector := FFS.Device.GetNextSector(CurSector);
    end;
  end;
  Seek(FPosition, soFromBeginning);
end;

function TRSFSStream.GetSector(Offset: Integer): Integer;
begin
  if Offset < FSize then
    Result := Offset div RSFSSectorSize
  else
    Result := SizeInSectors(FSize) - 1;
end;

function TRSFSStream.SizeInSectors(SizeInBytes: Integer): Integer;
begin
  Result := (SizeInBytes + RSFSSectorSize - 1) div RSFSSectorSize;
end;

function TRSFSStream.NewSector: Integer;
begin
  Result := FFS.AllocateSector;
  FSector.NextSector := Result;
  WriteSector;
  if Result = RSFSLastSector then Exit;
  ZeroMemory(@FSector.Data, RSFSSectorSize);
  FSector.NextSector := RSFSLastSector;
  FCurSector := Result;
end;

procedure TRSFSStream.ReadSector(Sector: Integer);
begin
  Assert((Sector <> 0) and (Sector <> RSFSLastSector));
  if Sector <> FCurSector then
  begin
    FFS.Device.ReadSector(Sector, FSector);
    FCurSector := Sector;
  end;
end;

procedure TRSFSStream.WriteSector;
begin
  Assert((FCurSector <> 0) and (FCurSector <> RSFSLastSector));
  FFS.Device.WriteSector(FCurSector, FSector);
end;

{ TRSFSDirectory }

constructor TRSFSDirectory.Create(FS: TRSFS; Parent: TRSFSDirectory; FirstSector: Integer);
begin
  inherited Create;
  FFS := FS;
  FParent := Parent;
  FEntries := TList.Create;
  if FirstSector > 0 then
  begin
    FData := TRSFSStream.Create(FFS, FirstSector);
    Reload;
  end
  else
    FData := FFS.AllocateStream;
end;

destructor TRSFSDirectory.Destroy;
begin
  Clear;
  FAN(FEntries);
  FAN(FData);
  inherited;
end;

procedure TRSFSDirectory.Reload;
var
  Idx: Integer;
begin
  Clear;
  FData.Position := 0;
  while FData.Position < FData.Size do
  begin
    Progress(Round(100 * (FData.Position / FData.Size)));
    Idx := NewEntry;
    FData.Read(Entry[Idx]^, SizeOf(TRSFSEntry));
    if (Entry[Idx].Attr = RSFSAttrNoFile) or (Entry[Idx].Name[0] = #0) then
      DeleteEntry(Idx);
  end;
end;

procedure TRSFSDirectory.Save;
var
  i: Integer;
  DummyEntry: TRSFSEntry;
begin
  FData.Position := 0;
  for i := 0 to Count - 1 do
  begin
    Progress(100 * i div Count);
    FData.Write(Entry[i]^, SizeOf(TRSFSEntry));
  end;
  FillChar(DummyEntry, SizeOf(DummyEntry), $FF); 
  while (FData.Position < RSFSSectorSize) or (FData.Position mod RSFSSectorSize <> 0) do
    FData.Write(DummyEntry, SizeOf(DummyEntry));
  FData.Size := FData.Position;
end;

procedure TRSFSDirectory.Clear;
var
  i: Integer;
begin
  for i := 0 to FEntries.Count - 1 do
    Dispose(PRSFSEntry(FEntries[i]));
  FEntries.Clear;
end;

procedure TRSFSDirectory.DeleteEntry(Index: Integer);
begin
  if (Index < 0) or (Index >= Count) then Exit;
  Dispose(PRSFSEntry(FEntries[Index]));
  FEntries.Delete(Index);
end;

function TRSFSDirectory.NewEntry: Integer;
var
  Entry: PRSFSEntry;
begin
  New(Entry);
  Entry.Name[RSFSNameLen] := #0;
  FillChar(Entry.Icon, SizeOf(Entry.Icon), $FF);
  Result := FEntries.Add(Entry);
end;

function TRSFSDirectory.FindEntry(Name: string; IsDir: Boolean): Integer;
begin
  Name := UpperCase(Name);
  for Result := 0 to Count - 1 do
    if (UpperCase(string(Entry[Result].Name)) = Name) and (IsDirectory(Result) = IsDir) then Exit;
  Result := -1;
end;

function TRSFSDirectory.FindEntry(Entry: PRSFSEntry): Integer;
begin
  Result := FEntries.IndexOf(Entry);
end;

function TRSFSDirectory.FindEntry(FirstSector: Integer; IsDir: Boolean): Integer;
begin
  for Result := 0 to Count - 1 do
    if (IsDirectory(Result) = IsDir) and (Entry[Result].FirstSector = FirstSector) then
      Exit;
  Result := -1;
end;

function TRSFSDirectory.IsDirectory(Index: Integer): Boolean;
begin
  Result := Assigned(Entry[Index]) and (Entry[Index].Attr and FILE_ATTRIBUTE_DIRECTORY = FILE_ATTRIBUTE_DIRECTORY);
end;

function TRSFSDirectory.GetCount: Integer;
begin
  Result := FEntries.Count;
end;

function TRSFSDirectory.GetEntry(Index: Integer): PRSFSEntry;
begin
  Result := FEntries[Index];
end;

function TRSFSDirectory.GetFirstSector: Integer;
begin
  Result := FData.FirstSector;
end;

procedure TRSFSDirectory.Progress(Progress: Integer);
begin
  if Assigned(FFS.FOnProgress) then
    FFS.FOnProgress(Self, Progress);
end;

{ TMainForm }

constructor TMainForm.Create;
var
  i: Integer;
  Rect: TRect;
begin
  inherited Create(nil, CCapt);
  ExStyle := ExStyle or WS_EX_ACCEPTFILES;
  CanvasInit;
  SetSize(640, 480);
  Position := poScreenCenter;
  OnClose := FormClose;
  OnProcessMsg := FormProcessMsg;
  AccelTable := CreateAcceleratorTable(Accels, Length(Accels));
  Icons := TImageList.Create;
  Icons.LoadSystemIcons(false);
  SmallIcons := TImageList.Create;
  SmallIcons.LoadSystemIcons(true);
  TBImages := TImageList.Create;
  TBImages.AddMasked(LoadImage(hInstance, 'TB', IMAGE_BITMAP, 0, 0, 0), clFuchsia);
  TB := TToolBar.Create(Self, true);
  TB.Style := TB.Style or TBSTYLE_TOOLTIPS or CCS_TOP;
  TB.ExStyle := TB.ExStyle or TBSTYLE_EX_MIXEDBUTTONS;
  TB.Perform(TB_SETMAXTEXTROWS, 0, 0);
  TB.Perform(TB_AUTOSIZE, 0, 0);
  TB.Images := TBImages;
  for i := Low(TBButtons) to High(TBButtons) do
    TB.ButtonAdd(TBButtons[i].Caption, TBButtons[i].ImageIndex);
  SB := TStatusBar.Create(Self, '');
  SB.SetParts(Length(SBParts), SBParts);
  SB.Perform(SB_GETRECT, SBPartProgress, Integer(@Rect));
  SetDevButtons(false);
  MenuView := TMenu.Create(Self, false, MenuViewTemplate);
  MenuPorts := TMenu.Create(Self, false, MenuPortsTemplate);
  MenuUpload := TMenu.Create(Self, false, MenuUploadTemplate);
  MenuFolders := TMenu.Create(Self, false, MenuFoldersTemplate);
  ModifyMenu(MenuFolders.Handle, FoldersUpload, MF_BYCOMMAND or MF_POPUP, MenuUpload.Handle, MenuFoldersTemplate[FoldersUpload - FoldersRefresh + 1]);
  MenuFiles := TMenu.Create(Self, false, MenuFilesTemplate);
  PB := TProgressBar.Create(SB);
  PB.Style := PB.Style or PBS_SMOOTH;
  with Rect do
    PB.SetBounds(Left, Top, Right - Left, Bottom - Top);
  PB.Position := 0;
  PB.Min := 0;
  PB.Max := 100;
  Folders := TImageList.Create;
  Folders.AddMasked(LoadImage(hInstance, 'FOLDER', IMAGE_BITMAP, 0, 0, 0), clFuchsia);
  Splitter := TSplitter.Create(Self, true);
  Splitter.SetBounds(200, TB.Height, Splitter.Width, ClientHeight - TB.Height - SB.Height);
  Splitter.OnMove := SplitterMove;
  TVFolders := TTreeViewEx.Create(Self);
  TVFolders.Images := Folders;
  TVFolders.StateImages := Folders;
  TVFolders.SetBounds(0, TB.Height, Splitter.Left, Splitter.Height);
  LVFiles := TListViewEx.Create(Self);
  LVFiles.Style := LVFiles.Style and not LVS_SINGLESEL or LVS_SHOWSELALWAYS or LVS_EDITLABELS or LVS_NOSORTHEADER or LVS_SORTASCENDING;
  LVFiles.ViewStyle := LVS_REPORT;
  LVFiles.OptionsEx := LVFiles.OptionsEx or LVS_EX_FULLROWSELECT or LVS_EX_GRIDLINES or LVS_EX_INFOTIP;
  LVFiles.LargeImages := Icons;
  LVFiles.SmallImages := SmallIcons;
  for i := 0 to High(LVFilesColumns) do
    with LVFilesColumns[i] do
      LVFiles.ColumnAdd(Caption, Width);
  LVFiles.OnResize := AdjustColumns;
  LVFiles.SetBounds(Splitter.Right, TB.Height, ClientWidth - Splitter.Right, Splitter.Height);
  Splitter.BringToFront;
end;

destructor TMainForm.Destroy;
begin
  DestroyAcceleratorTable(AccelTable);
  Finalize(COMPorts);
  FAN(TBImages);
  FAN(Folders);
  FAN(Icons);
  FAN(SmallIcons);
  FAN(MenuView);
  FAN(MenuPorts);
  FAN(MenuUpload);
  FAN(MenuFolders);
  FAN(MenuFiles);
  inherited;
end;

procedure TMainForm.WMCommand(var Msg: TWMCommand);
begin
  inherited;
  if Assigned(TB) and (Msg.Ctl = TB.Handle) then
    case TTBButtons(Msg.ItemID) of
      tbConnect: ConnectClick;
      tbDisconnect: DisconnectClick;
      tbFormat: FormatClick;
      tbRepair: RepairClick;
      tbRefresh: RefreshTree(TVFolders.Selected);
      tbNewFolder: NewFolderClick;
      tbUpload: UploadClick;
      tbDownload: DownloadClick;
      tbDelete: DeleteClick;
      tbProperties: PropertiesClick;
      tbView: ViewClick;
      tbAbout: ShowAbout;
    end;
  if (Msg.Ctl = 0) and (Msg.NotifyCode in [0, 1]) and (FStatusCnt = 0) then
    case Msg.ItemID of
      ViewReport: LVFiles.ViewStyle := LVS_REPORT;
      ViewList: LVFiles.ViewStyle := LVS_LIST;
      ViewSmallIcons: LVFiles.ViewStyle := LVS_SMALLICON;
      ViewIcons: LVFiles.ViewStyle := LVS_ICON;
      PortsDummy .. PortsDummy + 256: Connect(Msg.ItemID - PortsDummy);
      MainConnect: ConnectClick;
      MainDisconnect: DisconnectClick;
      MainRename:
        if GetFocus = TVFolders.Handle then
          TVFolders.Perform(TVM_EDITLABEL, 0, TVFolders.Selected)
        else if GetFocus = LVFiles.Handle then
          LVFiles.Perform(LVM_EDITLABEL, LVFiles.SelectedIndex, 0);
      MainAbout: ShowAbout;
      UploadFiles, FilesUpload: UploadFilesClick;
      UploadDirectory: UploadDirectoryClick;
      MainRefresh, FoldersRefresh, FilesRefresh: RefreshTree(TVFolders.Selected);
      FoldersRename: TVFolders.Perform(TVM_EDITLABEL, 0, TVFolders.Selected);
      FilesRename: LVFiles.Perform(LVM_EDITLABEL, LVFiles.SelectedIndex, 0);
      MainNewFolder, FoldersNewFolder: NewFolderClick;
      MainDownload, FoldersDownload, FilesDownload: DownloadClick;
      MainDelete, FoldersDelete, FilesDelete: DeleteClick;
      MainProperties, FoldersProperties, FilesProperties: PropertiesClick;
    end;
end;

procedure TMainForm.WMSize(var Msg: TWMSize);
var
  Rect: TRect;
begin
  inherited;
  if not (Assigned(TB) and Assigned(Splitter) and Assigned(TVFolders) and Assigned(LVFiles) and Assigned(SB) and Assigned(PB)) then Exit;
  TB.Width := ClientWidth;
  Splitter.Height := ClientHeight - TB.Height - SB.Height;
  if ClientWidth > 0 then
    Splitter.MaxPos := ClientWidth - Splitter.Width;
  TVFolders.Height := Splitter.Height;
  LVFiles.SetSize(ClientWidth - Splitter.Right, Splitter.Height);
  SB.Perform(SB_GETRECT, SBPartProgress, Integer(@Rect));
  with Rect do
    PB.SetBounds(Left, Top, Right - Left, Bottom - Top);
end;

procedure TMainForm.WMNotify(var Msg: TWMNotify);
const
  ImageIndex: array[Boolean, Boolean] of Integer = ((0, 1), (2, 2));
var
  i: Integer;
  Dir: TRSFSDirectory;
  Name: string;
begin
  inherited;
  if not Assigned(SD2Device) then Exit;
  if Assigned(TVFolders) and (Msg.NMHdr.hwndFrom = TVFolders.Handle) then
    if Msg.NMHdr.code = TVN_GETDISPINFO then
      with PTVDispInfo(Msg.NMHdr).item do
      begin
        if mask and TVIF_IMAGE <> 0 then
          iImage := ImageIndex[Pointer(lParam) = SD2Device.FS.Root, state and TVIS_EXPANDED = TVIS_EXPANDED];
        if mask and TVIF_SELECTEDIMAGE <> 0 then
          iSelectedImage := iImage;
      end
    else if not Assigned(SD2Device.FS.Root) then Exit
    else if Msg.NMHdr.code = TVN_SELCHANGED then
    begin
      Dir := TRSFSDirectory(PNMTreeView(Msg.NMHdr).itemNew.lParam);
      if Dir.Tag = 0 then FillNode(Integer(PNMTreeView(Msg.NMHdr).itemNew.hItem));
      FillList(Dir);
    end
    else if (Msg.NMHdr.code = TVN_ITEMEXPANDING) and (PNMTreeView(Msg.NMHdr).action = TVE_EXPAND) then
    begin
      Dir := TRSFSDirectory(PNMTreeView(Msg.NMHdr).itemNew.lParam);
      if Dir.Tag = 0 then FillNode(Integer(PNMTreeView(Msg.NMHdr).itemNew.hItem));
    end
    else if Msg.NMHdr.code = TVN_BEGINLABELEDIT then
    begin
      Dir := TRSFSDirectory(PTVDispInfo(Msg.NMHdr).item.lParam);
      Msg.Result := Byte((Dir = SD2Device.FS.Root) or not Assigned(Dir));
      SendMessage(TVFolders.Perform(TVM_GETEDITCONTROL, 0, 0), EM_SETLIMITTEXT, RSFSNameLen, 0);
    end
    else if Msg.NMHdr.code = TVN_ENDLABELEDIT then
    begin
      with PTVDispInfo(Msg.NMHdr)^ do
        if Assigned(item.pszText) and (item.lParam <> 0) and (StrLen(item.pszText) > 0) then
        begin
          Dir := TRSFSDirectory(item.lParam);
          if not Assigned(Dir.Parent) then Exit;
          i := Dir.Parent.FindEntry(Dir.FirstSector, true);
          Name := item.pszText;
          if Assigned(Dir.Parent[i]) then
            with Dir.Parent do
            begin
              if (FindEntry(Name, true) >= 0) and
                 (FindEntry(Name, true) <> i) then
                if MessageBox(Handle, PChar(Format(SAlreadyExists, [Name])), PChar(Caption), MB_ICONERROR or MB_YESNO or MB_DEFBUTTON1) = ID_YES then
                  Name := UniqueName(Dir.Parent, Name, true)
                else
                  Exit;
              Status(SWritingFS, false);
              try
                SetName(Entry[i].Name, Name);
                Save;
              finally
                Status(SReady, true);
              end;
            end;
          Msg.Result := 1;
          TVFolders.SetFocus;
        end
    end;
  if Assigned(LVFiles) and (PNMHdr(Msg.NMHdr).hwndFrom = LVFiles.Handle) then
    if (Msg.NMHdr.code = LVN_ITEMCHANGED) and (PNMListView(Msg.NMHdr).lParam <> 0) and (LVFiles.Tag = 0) then
    begin
      with PRSFSEntry(PNMListView(Msg.NMHdr).lParam)^ do
        SB.SetPartText(SBPartInfo, 0, Format(SFileInfo, [string(Name), SizeToStr(Size), DateTimeToStrShort(FileTimeToDateTime(WriteTime)), FileAttributesToStr(Attr)]));
    end
    else if Msg.NMHdr.code = LVN_BEGINLABELEDIT then
    begin
      Msg.Result := 0;
      SendMessage(LVFiles.Perform(LVM_GETEDITCONTROL, 0, 0), EM_SETLIMITTEXT, RSFSNameLen, 0);
    end
    else if Msg.NMHdr.code = LVN_ENDLABELEDIT then
    begin
      with PLVDispInfo(Msg.NMHdr)^ do
        if Assigned(item.pszText) and (item.lParam <> 0) and (StrLen(item.pszText) > 0) then
        begin
          Dir := TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory;
          if not Assigned(Dir) or (Dir.FindEntry(PRSFSEntry(item.lParam)) < 0) then Exit;
          Name := item.pszText;
          if (Dir.FindEntry(Name, false) >= 0) and
             (Dir.FindEntry(Name, false) <> Dir.FindEntry(PRSFSEntry(item.lParam))) then
            if MessageBox(Handle, PChar(Format(SAlreadyExists, [Name])), PChar(Caption), MB_ICONERROR or MB_YESNO or MB_DEFBUTTON1) = ID_YES then
              Name := UniqueName(Dir, Name, false)
            else
              Exit;
          Status(SWritingFS, false);
          try
            SetName(PRSFSEntry(item.lParam).Name, Name);
            Dir.Save;
          finally
            Status(SReady, true);
          end;
          Msg.Result := 1;
          LVFiles.SetFocus;
        end
    end;
end;

procedure TMainForm.WMDropFiles(var Msg: TWMDropFiles);
var
  FileName: array[0..MAX_PATH] of Char;
  Dir: TRSFSDirectory;
  i: Integer;
begin
  inherited;
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) then Exit;
  Dir := TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory;
  try
    for i := 0 to DragQueryFile(Msg.Drop, $FFFFFFFF, nil, 0) - 1 do
    begin
      DragQueryFile(Msg.Drop, i, FileName, MAX_PATH + 1);
      if FileExists(string(FileName)) then
        UploadFile(Dir, string(FileName))
      else if DirectoryExists(string(FileName)) then
        UploadDir(Dir, string(FileName));
    end;
  finally
    DragFinish(Msg.Drop);
    Dir.Save;
    SD2Device.FS.WriteUpdates;
    RefreshTree(TVFolders.Selected);
  end;
end;

procedure TMainForm.WMContextMenu(var Msg: TWMContextMenu);
var
  P: TPoint;
  R: TRect;

  procedure AdjustPos(X, Y: Integer);
  begin
    if (Msg.XPos = -1) and (Msg.YPos = -1) then
    begin
      P := Point(X, Y);
      ClientToScreen(Msg.hWnd, P);
      Msg.XPos := P.X;
      Msg.YPos := P.Y;
    end;
  end;

begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) then Exit;
  P := Point(Msg.XPos, Msg.YPos);
  ScreenToClient(Msg.hWnd, P);
  Windows.SetFocus(Msg.hWnd);
  if Msg.hWnd = TVFolders.Handle then
  begin
    if TVFolders.ItemAtPoint(P.X, P.Y) <> 0 then
      TVFolders.Selected := TVFolders.ItemAtPoint(P.X, P.Y);
    TreeView_GetItemRect(TVFolders.Handle, CommCtrl.HTreeItem(TVFolders.Selected), R, true);
    AdjustPos(R.Left, R.Bottom);
    MenuFolders.Popup(Msg.XPos, Msg.YPos);
  end
  else if Msg.hWnd = LVFiles.Handle then
  begin
    if LVFiles.ItemAtPoint(P.X, P.Y) <> -1 then
      LVFiles.SelectedIndex := LVFiles.ItemAtPoint(P.X, P.Y);
    ListView_GetItemRect(LVFiles.Handle, LVFiles.SelectedIndex, R, LVIR_SELECTBOUNDS);
    AdjustPos(R.Left, R.Bottom);
    MenuFiles.Popup(Msg.XPos, Msg.YPos);
  end;
end;

function TMainForm.UniqueName(Dir: TRSFSDirectory; const Name: string; IsDir: Boolean): string;
var
  i: Integer;
  BaseName, Ext: string;

  function NewName: string;
  begin
    Result := ' [' + IntToStr(i) + ']';
    if Length(BaseName) > RSFSNameLen - Length(Ext) - Length(Result) then
      SetLength(BaseName, RSFSNameLen - Length(Ext) - Length(Result));
    Result := BaseName + Result + Ext;
  end;

begin
  i := 1;
  if Dir.FindEntry(Name, IsDir) >= 0 then
  begin
    BaseName := ExtractFileNoExt(Name);
    Ext := ExtractFileExt(Name);
    while Dir.FindEntry(NewName, IsDir) >= 0 do
      Inc(i);
    Result := NewName;
  end
    else Result := Name;
end;

procedure TMainForm.SetName(var Name: TRSFSName; const NewName: string);
begin
  ZeroMemory(@Name, SizeOf(Name));
  Move(NewName[1], Name[0], Min(Length(NewName), RSFSNameLen));
end;

function TMainForm.FormClose(Sender: TObject): Boolean;
begin
  ClearTree(Integer(TVI_ROOT));
  FAN(SD2Device);
  Result := true;
end;

function TMainForm.FormProcessMsg(var Msg: TMsg): Boolean;
begin
  Result := TranslateAccelerator(Handle, AccelTable, Msg) <> 0;
  Result := false;
end;

procedure TMainForm.AdjustColumns(Sender: TObject);
var
  Width: Integer;
  i: Integer;
begin
  if LVFiles.ViewStyle <> LVS_REPORT then Exit;
  Width := LVFiles.ClientWidth;
  for i := 1 to LVFiles.ColumnCount - 1 do
    Dec(Width, LVFiles.Perform(LVM_GETCOLUMNWIDTH, i, 0));
  LVFiles.Perform(LVM_SETCOLUMNWIDTH, 0, Max(Width, LVFilesColumns[0].Width));
end;

procedure TMainForm.SplitterMove(Sender: TObject);
begin
  TVFolders.Width := Splitter.Left;
  LVFiles.SetBounds(Splitter.Right, TB.Height, ClientWidth - Splitter.Right, LVFiles.Height);
end;

procedure TMainForm.Progress(Sender: TObject; Progress: Integer);
begin
  if PB.Position <> Progress then
  begin
    PB.Position := Progress;
    SB.SetPartText(SBPartProgressText, 0, IntToStr(Progress) + '%');
  end;
  ProcessMessages;
end;

procedure TMainForm.UpdateSpace(Sender: TObject);
var
  Total, Free, Occupied: Integer;
begin
  SD2Device.FS.CalcSpace(Total, Free, Occupied);
  SB.SetPartText(SBPartSpace, 0, Format(SFreeSpace, [SizeToStr(Free), SizeToStr(Total)]));
end;

procedure TMainForm.Status(const Msg: string; ControlsEnabled: Boolean);
const
  Delta: array[Boolean] of Integer = (1, -1);
begin
  FStatusCnt := Max(0, FStatusCnt + Delta[ControlsEnabled]);
  if ControlsEnabled and (FStatusCnt <> 0) then Exit; 
  SB.SetPartText(SBPartInfo, 0, Msg);
  PB.Position := 0;
  if ControlsEnabled then
    SB.SetPartText(SBPartProgressText, 0, '')
  else
    SB.SetPartText(SBPartProgressText, 0, '0%');
  LVFiles.Enabled := ControlsEnabled;
  Splitter.Enabled := ControlsEnabled;
  TB.Enabled := ControlsEnabled;
  TVFolders.Enabled := ControlsEnabled;
  ProcessMessages;
end;

procedure TMainForm.SetDevButtons(Enabled: Boolean);
const
  Conv: array[Boolean] of Integer = (0, 1);
var
  i: Integer;
begin
  TB.Perform(TB_ENABLEBUTTON, Integer(tbConnect), Conv[not Enabled]);
  TB.Perform(TB_ENABLEBUTTON, Integer(tbDisconnect), Conv[Enabled]);
  for i := 0 to High(DevButtons) do
    TB.Perform(TB_ENABLEBUTTON, Integer(DevButtons[i]), Conv[Enabled]);
  if not Enabled then
    SB.SetPartText(SBPartSpace, 0, '');
end;

procedure TMainForm.Connect(Port: Integer);
var
  FileName: string;
begin
  Status(SReadingFS, false);
  try
    if Port = 0 then
    begin
      FileName := '';
      if not OpenSaveDialog(Handle, true, '', '', SFilter, '', 0, OFN_FILEMUSTEXIST or OFN_NOREADONLYRETURN, FileName) then Exit;
      SD2Device := TSD2Device.Create(FileName, Progress);
    end
    else if COMPorts[Port - 1].Index < 0 then
      if MessageBox(Handle, PCHar(Format(SOpenDeviceConfirm, [COMPorts[Port - 1].Name])), PChar(Caption), MB_ICONQUESTION or MB_YESNO or MB_DEFBUTTON2) = IDYES then
        SD2Device := TSD2Device.Create('\\.\PhysicalDrive' + IntToStr(-COMPorts[Port - 1].Index - 1), Progress)
      else Exit
    else try
      SD2Device := TSD2Device.Create(COMPorts[Port - 1].Index, Progress);
    except
      MessageBox(Handle, PCHar(SCantOpenDevice + Exception(ExceptObject).Message), PChar(Caption), MB_ICONERROR);
      Exit;
    end;
    SetDevButtons(true);
    SD2Device.FS.OnWriteUpdates := UpdateSpace;
    UpdateSpace(Self);
    RefreshTree(Integer(TVI_ROOT));
    if not Assigned(SD2Device.FS.Root) then
      MessageBox(Handle, SNotFormatted, PChar(Caption), MB_ICONINFORMATION);
  finally
    Status(SReady, true);
  end;
end;

procedure TMainForm.ClearTree(Node: Integer);

  procedure ClearNode(Node: Integer);
  begin
    if (Node <> Integer(TVI_ROOT)) and (TVFolders.GetItemObject(Node) <> SD2Device.FS.Root) then
      TVFolders.GetItemObject(Node).Free;
    Node := TVFolders.Perform(TVM_GETNEXTITEM, TVGN_CHILD, Node);
    while Node <> 0 do
    begin
      ClearNode(Node);
      Node := TVFolders.Perform(TVM_GETNEXTITEM, TVGN_NEXT, Node);
    end;
  end;

begin
  TVFolders.BeginUpdate;
  try
    ClearNode(Node);
    TVFolders.DeleteItem(Node);
  finally
    TVFolders.EndUpdate;
  end;
end;

procedure TMainForm.RefreshTree(Node: Integer);
var
  Child: Integer;
  Dir: TRSFSDirectory;
begin
  if not Assigned(SD2Device) then Exit;
  TVFolders.BeginUpdate;
  try
    Child := TVFolders.Perform(TVM_GETNEXTITEM, TVGN_CHILD, Node);
    while Child <> 0 do
    begin
      ClearTree(Child);
      Child := TVFolders.Perform(TVM_GETNEXTITEM, TVGN_CHILD, Node);
    end;
    if Node = Integer(TVI_ROOT) then
    begin
      TVFolders.Selected := FillNode(TVFolders.ItemInsert(Integer(TVI_ROOT), Integer(TVI_LAST), SD2Device.Name, SD2Device.FS.Root));
      TVFolders.ExpandItem(TVFolders.Selected, emExpand);
    end
    else begin
      LVFiles.Clear;
      Dir := TRSFSDirectory(TVFolders.GetItemObject(Node));
      Status(SReadingFS, false);
      try
        Dir.Reload;
        FillNode(Node);
        FillList(Dir);
      finally
        Status(SReady, true);
      end;
    end;
  finally
    TVFolders.EndUpdate;
  end;
end;

function TMainForm.FillNode(Node: Integer): Integer;
var
  i, j: Integer;
  Dir, CurDir: TRSFSDirectory;
  TVI: TTVItem;
begin
  Result := Node;
  ZeroMemory(@TVI, SizeOf(TVI));
  TVI.mask := TVIF_HANDLE or TVIF_CHILDREN;
  TVI.cChildren := 1;
  Dir := TVFolders.GetItemObject(Node) as TRSFSDirectory;
  if not Assigned(Dir) then Exit;
  Status(SReadingFS, false);
  try
    TVFolders.BeginUpdate;
    for i := 0 to Dir.Count - 1 do
      if Dir.IsDirectory(i) then
      begin
        CurDir := TRSFSDirectory.Create(SD2Device.FS, Dir, Dir[i].FirstSector);
        TVI.hItem := HTreeItem(TVFolders.ItemInsert(Node, Integer(TVI_SORT), string(Dir[i].Name), CurDir));
        for j := 0 to CurDir.Count - 1 do
          if CurDir.IsDirectory(j) then
          begin
            TVFolders.Perform(TVM_SETITEM, 0, Integer(@TVI));
            Break;
          end;
      end;
    (TVFolders.GetItemObject(Node) as TRSFSDirectory).Tag := 1;
  finally
    TVFolders.EndUpdate;
    Status(SReady, true);
  end;
end;

procedure TMainForm.FillList(Dir: TRSFSDirectory);
const
  IconSize: array[Boolean] of Cardinal = (SHGFI_SMALLICON, SHGFI_LARGEICON);
var
  i, Item, FilesSize: Integer;
  SelItem: PRSFSEntry;
  SFI: TShFileInfo;
begin
  SelItem := PRSFSEntry(LVFiles.ItemObject[LVFiles.SelectedIndex]);
  LVFiles.BeginUpdate;
  try
    LVFiles.Tag := 1;
    LVFiles.Clear;
    FilesSize := 0;
    for i := 0 to Dir.Count - 1 do
      if not Dir.IsDirectory(i) then
      begin
        Inc(FilesSize, Dir[i].Size);
        Item := LVFiles.ItemAdd(string(Dir[i].Name));
        LVFiles.Items[Item, ColSize] := SizeToStr(Dir[i].Size);
        LVFiles.Items[Item, ColTime] := DateTimeToStrShort(FileTimeToDateTime(Dir[i].WriteTime));
        LVFiles.Items[Item, ColAttr] := FileAttributesToStr(Dir[i].Attr);
        LVFiles.ItemObject[Item] := TObject(Dir[i]);
        SHGetFileInfo(PChar(string(Dir[i].Name)), Dir[i].Attr, SFI, SizeOf(SFI),
          SHGFI_USEFILEATTRIBUTES or SHGFI_ICON or SHGFI_SYSICONINDEX or IconSize[LVFiles.ViewStyle = LVS_ICON]);
        LVFiles.ItemImageIndex[Item] := SFI.iIcon;
        if Dir[i] = SelItem then LVFiles.SelectedIndex := Item;
      end;
  finally
    SB.SetPartText(SBPartInfo, 0, Format(SFolderSize, [LVFiles.ItemCount, SizeToStr(FilesSize)]));
    LVFiles.Tag := 0;
    LVFiles.EndUpdate;
  end;
end;

procedure TMainForm.DeleteFile(Entry: PRSFSEntry);
var
  F: TRSFSStream;
begin
  F := TRSFSStream.Create(SD2Device.FS, Entry.FirstSector, Entry.Size);
  try
    F.Size := 0;
    SD2Device.FS.FreeSector(F.FirstSector);
  finally
    FAN(F);
  end;
end;

procedure TMainForm.DeleteDir(Dir: TRSFSDirectory);
var
  i: Integer;
begin
  for i := 0 to Dir.Count - 1 do
    if Dir.IsDirectory(i) then
      DeleteDir(TRSFSDirectory.Create(SD2Device.FS, Dir, Dir[i].FirstSector))
    else
      DeleteFile(Dir[i]);
  Dir.Clear;
  Dir.Save;
  SD2Device.FS.FreeSector(Dir.FirstSector);
  Dir.Free;
  SD2Device.FS.WriteUpdates;
end;

procedure TMainForm.UploadFile(Dir: TRSFSDirectory; const Name: string);

  function NewFile(const FileName: string; var Entry: Integer): TRSFSStream;
  begin
    Result := SD2Device.FS.AllocateStream;
    Entry := Dir.NewEntry;
      with Dir[Entry]^ do
      begin
        SetName(Name, FileName);
        FirstSector := Result.FirstSector;
        Size := -1;
      end;
  end;

var
  Src: TFileStream;
  Dst: TRSFSStream;
  Buf: array[0..$FFF] of Byte;
  Entry: Integer;
  FileName: string;
  FileInfo: TByHandleFileInformation;
  Total, Free, Occupied: Integer;
begin
  FileName := ExtractFileName(Name);
  Status(Format(SUploadingFile, [FileName]), false);
  try
    if not FileExists(Name) then Exit;
    SD2Device.FS.CalcSpace(Total, Free, Occupied);
    if FlSize(Name) > Free then
    begin
      MessageBox(Handle, PChar(Format(SNotEnoughSpace, [FileName])), PChar(Caption), MB_ICONERROR);
      Exit;
    end;
    if Length(FileName) > RSFSNameLen then
      SetLength(FileName, RSFSNameLen);
    Dst := nil;
    if Dir.FindEntry(FileName, false) >= 0 then
      case MessageBox(Handle, PChar(Format(SFileAlreadyExists, [FileName])), PChar(Caption), MB_ICONQUESTION or MB_YESNOCANCEL or MB_DEFBUTTON1) of
        IDYES: begin
            FileName := UniqueName(Dir, FileName, false);
            Dst := NewFile(FileName, Entry);
          end;
        IDNO: begin
            Entry := Dir.FindEntry(FileName, false);
            Dst := TRSFSStream.Create(SD2Device.FS, Dir[Entry].FirstSector, Dir[Entry].Size);
            Dst.Size := 0;
          end;
        IDCANCEL: Exit;
      end;
    if not Assigned(Dst) then
      Dst := NewFile(FileName, Entry);
    try
      Src := TFileStream.Create(Name, fmOpenRead or fmShareDenyWrite);
      try
        while Src.Position < Src.Size do
        begin
          Progress(Self, Round((Src.Position / Src.Size) * 100));
          Dst.Write(Buf, Src.Read(Buf, Min(Src.Size - Src.Position, SizeOf(Buf))));
        end;
        with Dir[Entry]^ do
        begin
          Size := Dst.Size;
          if GetFileInformationByHandle(Src.Handle, FileInfo) then
            with FileInfo do
            begin
              Attr := dwFileAttributes and not FILE_ATTRIBUTE_DIRECTORY;
              CreateTime := ftCreationTime;
              AccessTime := ftLastAccessTime;
              WriteTime := ftLastWriteTime;
            end
          else
            Attr := 0;
        end;
      finally
        FAN(Src);
      end;
    finally
      FAN(Dst);
    end;
  finally
    Status(SReady, true);
  end;
end;

procedure TMainForm.UploadDir(Dir: TRSFSDirectory; const Path: string);
var
  DirName: string;
  SR: TSearchRec;
  DirInfo: TByHandleFileInformation;
begin
  Status(SUploadingDirectory, false);
  try
    DirName := ExtractFileName(ExcludeTrailingBackslash(Path));
    if Length(DirName) > RSFSNameLen then
      SetLength(DirName, RSFSNameLen);  
    if Dir.FindEntry(DirName, true) >= 0 then
      Dir := TRSFSDirectory.Create(SD2Device.FS, Dir, Dir[Dir.FindEntry(DirName, true)].FirstSector)
    else
      with Dir[Dir.NewEntry]^ do
      begin
        Dir := TRSFSDirectory.Create(SD2Device.FS, Dir, 0);
        SetName(Name, DirName);
        FirstSector := Dir.FirstSector;
        Size := 0;
        if GetDirInformation(Path, DirInfo) then
          with DirInfo do
          begin
            Attr := dwFileAttributes or FILE_ATTRIBUTE_DIRECTORY;
            CreateTime := ftCreationTime;
            AccessTime := ftLastAccessTime;
            WriteTime := ftLastWriteTime;
          end
        else
          Attr := FILE_ATTRIBUTE_DIRECTORY;
      end;
    try
      if FindFirst(AddTrailingBackslash(Path) + '*', faAnyFile and not faDirectory, SR) = 0 then
        repeat
          UploadFile(Dir, AddTrailingBackslash(Path) + SR.Name);
        until FindNext(SR) <> 0;
      FindClose(SR);
      if FindFirst(AddTrailingBackslash(Path) + '*', faDirectory or faHidden, SR) = 0 then
        repeat
          if ((SR.Attr and faDirectory) = faDirectory) and (SR.Name <> '.') and (SR.Name <> '..') then
            UploadDir(Dir, AddTrailingBackslash(Path) + SR.Name);
        until FindNext(SR) <> 0;
      FindClose(SR);
    finally
      Dir.Save;
      FAN(Dir);
    end;
  finally
    Status(SReady, true);
  end;
end;

procedure TMainForm.DownloadFile(Entry: PRSFSEntry; const Name: string);
var
  Src: TRSFSStream;
  Dst: TFileStream;
  Buf: array[0..$FFF] of Byte;
begin
  Status(Format(SDownloadingFile, [string(ExtractFileName(Name))]), false);
  try
    ForceDirectories(ExtractFilePath(Name));
    if FileExists(Name) and (MessageBox(Handle, PChar(Format(SOverwriteConfirm, [Name])), PChar(Caption),
      MB_ICONWARNING or MB_YESNO or MB_DEFBUTTON2) = IDNO) then Exit;
    Dst := TFileStream.Create(Name, fmCreate);
    try
      Src := TRSFSStream.Create(SD2Device.FS, Entry.FirstSector, Entry.Size);
      try
        while Src.Position < Src.Size do
        begin
          Progress(Self, Round((Src.Position / Src.Size) * 100));
          Dst.Write(Buf, Src.Read(Buf, Min(Src.Size - Src.Position, SizeOf(Buf))));
        end;
        with Entry^ do
          SetFileTime(Dst.Handle, @CreateTime, @AccessTime, @WriteTime);
      finally
        FAN(Src);
      end;
      SetFileAttributes(PChar(Name), Entry.Attr);
    finally
      FAN(Dst);
    end;
  finally
    Status(SReady, true);
  end;
end;

procedure TMainForm.DownloadDirectory(Dir: TRSFSDirectory; const Path: string);
var
  i: Integer;
  CurDir: TRSFSDirectory;
begin
  Status(SDownloadingDirectory, false);
  try
    if not DirectoryExists(Path) then
    begin
      ForceDirectories(Path);
      if Assigned(Dir.Parent) then
        with Dir.Parent[Dir.Parent.FindEntry(Dir.FirstSector, true)]^ do
        begin
          SetFileAttributes(PChar(Path), Attr);
          SetDirTime(Path, CreateTime, AccessTime, WriteTime);
        end;
    end;
    for i := 0 to Dir.Count - 1 do
      if not Dir.IsDirectory(i) then
        DownloadFile(Dir[i], Path + string(Dir[i].Name));
    for i := 0 to Dir.Count - 1 do
      if Dir.IsDirectory(i) then
      begin
        CurDir := TRSFSDirectory.Create(SD2Device.FS, Dir, Dir[i].FirstSector);
        try
          DownloadDirectory(CurDir, AddTrailingBackslash(Path + string(Dir[i].Name)));
        finally
          FAN(CurDir);
        end;
      end;
  finally
    Status(SReady, true);
  end;
end;

procedure TMainForm.ConnectClick;
var
  i: Integer;
  Rect: TRect;
begin
  if Assigned(SD2Device) then Exit;
  while DeleteMenu(MenuPorts.Handle, 1, MF_BYPOSITION) do;
  EnumCOMPorts(COMPorts);
  EnumRemovableDevices(COMPorts);
  for i := 0 to High(COMPorts) do
    MenuPorts.AddItem(COMPorts[i].Name, i + 2);
  TB.Perform(TB_GETITEMRECT, Integer(tbConnect), Integer(@Rect));
  Rect.Top := Rect.Bottom;
  ClientToScreen(Handle, Rect.TopLeft);
  MenuPorts.Popup(Rect.Left, Rect.Top);
end;

procedure TMainForm.DisconnectClick;
begin
  if not Assigned(SD2Device) then Exit;
  LVFiles.Clear;
  ClearTree(Integer(TVI_ROOT));
  Status(SWritingFS, false);
  try
    FAN(SD2Device);
  finally
    Status(SReady, true);
  end;
  SetDevButtons(false);
end;

procedure TMainForm.FormatClick;
begin
  if not Assigned(SD2Device) or
    (MessageBox(Handle, SFormatConfirm, PChar(Caption), MB_YESNO or MB_DEFBUTTON2 or MB_ICONEXCLAMATION) = IDNO) then Exit;
  Status(SFormatting, false);
  try
    SD2Device.FS.Format;
    RefreshTree(Integer(TVI_ROOT));
  finally
    Status(SReady, true);
  end;
end;

procedure TMainForm.RepairClick;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) or
    (MessageBox(Handle, SRepairConfirm, PChar(Caption), MB_YESNO or MB_DEFBUTTON2 or MB_ICONEXCLAMATION) = IDNO) then Exit;
  Status(SRepairing, false);
  try
    SD2Device.FS.RebuildSectorsMap;
    UpdateSpace(Self);
    RefreshTree(Integer(TVI_ROOT));
  finally
    Status(SReady, true);
  end;
end;

procedure TMainForm.NewFolderClick;
var
  Dir, NewDir: TRSFSDirectory;
  DirName: string;
  Node: Integer;
  Time: TSystemTime;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) then Exit;
  Status(SWritingFS, false);
  try
    Dir := TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory;
    NewDir := TRSFSDirectory.Create(SD2Device.FS, Dir, 0);
    DirName := UniqueName(Dir, SNewFolder, true);
    with Dir[Dir.NewEntry]^ do
    begin
      SetName(Name, DirName);
      FirstSector := NewDir.FirstSector;
      Size := 0;
      Attr := FILE_ATTRIBUTE_DIRECTORY;
      GetSystemTime(Time);
      SystemTimeToFileTime(Time, CreateTime);
      SystemTimeToFileTime(Time, AccessTime);
      SystemTimeToFileTime(Time, WriteTime);
    end;
    Dir.Save;
    NewDir.Save;
    SD2Device.FS.WriteUpdates;
    Node := TVFolders.ItemInsert(TVFolders.Selected, Integer(TVI_SORT), DirName, NewDir);
    TVFolders.Selected := Node;
  finally
    Status(SReady, true);
  end;
  TVFolders.Perform(TVM_EDITLABEL, 0, Node);
end;

procedure TMainForm.UploadClick;
var
  Rect: TRect;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) then Exit;
  TB.Perform(TB_GETITEMRECT, tbUploadItem, Integer(@Rect));
  Rect.Top := Rect.Bottom;
  ClientToScreen(Handle, Rect.TopLeft);
  MenuUpload.Popup(Rect.Left, Rect.Top);
end;

procedure TMainForm.UploadFilesClick;
var
  i: Integer;
  FileName: string;
  Files: TStringList;
  Dir: TRSFSDirectory;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) then Exit;
  FileName := '';
  if not OpenSaveDialog(Handle, true, '', '', SFilter, '', 0, OFN_ALLOWMULTISELECT or OFN_FILEMUSTEXIST, FileName) then Exit;
  Files := TStringList.Create;
  try
    Files.Text := FileName;
    Dir := TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory;
    for i := 0 to Files.Count - 1 do
      UploadFile(Dir, Files[i]);
  finally
    Dir.Save;
    SD2Device.FS.WriteUpdates;
    RefreshTree(TVFolders.Selected);
    FAN(Files);
  end;
end;

procedure TMainForm.UploadDirectoryClick;
var
  Path: string;
  Dir: TRSFSDirectory;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) then Exit;
  Path := '';
  if not OpenDirDialog(Handle, '', false, Path) then Exit;
  try
    Dir := TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory;
    UploadDir(Dir, Path);
  finally
    Dir.Save;
    SD2Device.FS.WriteUpdates;
    RefreshTree(TVFolders.Selected);
  end;
end;

procedure TMainForm.DownloadClick;
var
  DownloadFolder: Boolean;
  i: Integer;
  Path: string;
  Files: TStringList;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) or
     not ((GetFocus = TVFolders.Handle) or (GetFocus = LVFiles.Handle)) then Exit;
  DownloadFolder := GetFocus = TVFolders.Handle;
  Path := '';
  if (DownloadFolder or (LVFiles.SelCount > 1)) and
    not OpenDirDialog(Handle, '', true, Path) then Exit;
  AddTrailingBackslashV(Path);
  if not DownloadFolder then
  begin
    if LVFiles.SelCount = 1 then
    begin
      Path := LVFiles.SelectedCaption;
      if not OpenSaveDialog(Handle, false, '', '', SFilter, '', 0, OFN_NOREADONLYRETURN, Path) then Exit;
      DownloadFile(PRSFSEntry(LVFiles.ItemObject[LVFiles.SelectedIndex]), Path);
    end
    else if LVFiles.SelCount > 1 then
    begin
      for i := 0 to LVFiles.SelCount - 1 do
        DownloadFile(PRSFSEntry(LVFiles.ItemObject[LVFiles.Selected[i]]),
          Path + LVFiles.Items[LVFiles.Selected[i], 0]);
    end;
  end
    else DownloadDirectory(TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory, Path);
end;

procedure TMainForm.DeleteClick;
var
  i, Sel, FS: Integer;
  Dir: TRSFSDirectory;
  Entry: PRSFSEntry;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) or
     not ((GetFocus = TVFolders.Handle) or (GetFocus = LVFiles.Handle)) then Exit;
  if GetFocus = TVFolders.Handle then
  begin
    if MessageBox(Handle, PChar(Format(SDeleteDirConfirm, [TVFolders.GetItemText(TVFolders.Selected)])),
      PChar(Caption), MB_ICONQUESTION or MB_YESNO or MB_DEFBUTTON2) = IDNO then Exit;
    Sel := TVFolders.Selected;
    Dir := TVFolders.GetItemObject(Sel) as TRSFSDirectory;
    if Dir = SD2Device.FS.Root then Exit;
    TVFolders.Selected := TVFolders.GetItemParent(Sel);
    Status(SDeletingDirectory, false);
    try
      FS := Dir.FirstSector;
      Dir := Dir.Parent;
      ClearTree(Sel);
      Dir.DeleteEntry(Dir.FindEntry(FS, true));
      Dir.Save;
      DeleteDir(TRSFSDirectory.Create(SD2Device.FS, nil, FS));
      SD2Device.FS.WriteUpdates;
      RefreshTree(TVFolders.Selected);
    finally
      Status(SReady, true);
    end;
  end;
  if GetFocus = LVFiles.Handle then
  begin
    if (LVFiles.SelCount < 1) or ((LVFiles.SelCount = 1) and (MessageBox(Handle, PChar(Format(SDeleteFileConfirm, [LVFiles.SelectedCaption])),
      PChar(Caption), MB_ICONQUESTION or MB_YESNO or MB_DEFBUTTON2) = IDNO)) or
      ((LVFiles.SelCount > 1) and (MessageBox(Handle, SDeleteFilesConfirm, PChar(Caption), MB_ICONQUESTION or MB_YESNO or MB_DEFBUTTON2) = IDNO)) then Exit;
    Status(SDeletingFiles, false);
    try
      Dir := TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory;
      try
        for i := 0 to LVFiles.SelCount - 1 do
        begin
          Progress(Self, 100 * i div LVFiles.SelCount);
          Entry := PRSFSEntry(LVFiles.ItemObject[LVFiles.Selected[i]]);
          if Dir.FindEntry(Entry) >= 0 then
          begin
            DeleteFile(Entry);
            Dir.DeleteEntry(Dir.FindEntry(Entry));
          end;
        end;
      finally
        Status(SWritingFS, false);
        Dir.Save;
        SD2Device.FS.WriteUpdates;
      end;
      FillList(Dir);
    finally
      Status('', true);
      Status(SReady, true);
    end;
  end;
end;

procedure TMainForm.PropertiesClick;

  function GetDirSize(Dir: TRSFSDirectory): Integer;
  var
    i: Integer;
    CurDir: TRSFSDirectory;
  begin
    Result := 0;
    for i := 0 to Dir.Count - 1 do
      if Dir.IsDirectory(i) then
      begin
        CurDir := TRSFSDirectory.Create(SD2Device.FS, Dir, Dir[i].FirstSector);
        try
          Inc(Result, GetDirSize(CurDir));
        finally
          FAN(CurDir);
        end;
      end
      else
        Inc(Result, Dir[i].Size);
  end;

var
  Props: string;
  Size: Integer;
  Dir: TRSFSDirectory;
  Entry: PRSFSEntry;
begin
  if not Assigned(SD2Device) or not Assigned(SD2Device.FS.Root) or
     not ((GetFocus = TVFolders.Handle) or (GetFocus = LVFiles.Handle)) then Exit;
  Dir := TVFolders.GetItemObject(TVFolders.Selected) as TRSFSDirectory;
  if GetFocus = TVFolders.Handle then
  begin
    Status(SReadingFS, false);
    try
      Size := GetDirSize(Dir);
    finally
      Status(SReady, true);
    end;
    if Dir = SD2Device.FS.Root then
    begin
      Entry := nil;
      Props := Format(SDirProperties, [SD2Device.Name, Dir.Count, SizeToStr(Size), FileAttributesToStr(0), 0]);
    end
    else begin
      Entry := Dir.Parent[Dir.Parent.FindEntry(Dir.FirstSector, true)];
      if not Assigned(Entry) then Exit;
      Props := Format(SDirProperties, [string(Entry.Name), Dir.Count, SizeToStr(Size), FileAttributesToStr(Entry.Attr), Entry.Attr]);
    end;
  end
  else begin
    Entry := PRSFSEntry(LVFiles.ItemObject[LVFiles.SelectedIndex]);
    if not Assigned(Entry) then Exit;
    Props := Format(SFileProperties, [string(Entry.Name), SizeToStr(Entry.Size), FileAttributesToStr(Entry.Attr), Entry.Attr]);
  end;
  if Assigned(Entry) then
    Props := Props + Format(SDates, [
      DateTimeToStrShort(FileTimeToDateTime(Entry.CreateTime)),
      DateTimeToStrShort(FileTimeToDateTime(Entry.WriteTime)),
      DateTimeToStrShort(FileTimeToDateTime(Entry.AccessTime))]);
  MessageBox(Handle, PChar(Props), PChar(Caption), MB_ICONINFORMATION);
end;

procedure TMainForm.ViewClick;
const
  MenuCheck: array[Boolean] of UINT = (MF_UNCHECKED, MF_CHECKED);
var
  Rect: TRect;
begin
  CheckMenuItem(MenuView.Handle, ViewReport, MenuCheck[LVFiles.ViewStyle = LVS_REPORT] or MF_BYCOMMAND);
  CheckMenuItem(MenuView.Handle, ViewList, MenuCheck[LVFiles.ViewStyle = LVS_LIST] or MF_BYCOMMAND);
  CheckMenuItem(MenuView.Handle, ViewSmallIcons, MenuCheck[LVFiles.ViewStyle = LVS_SMALLICON] or MF_BYCOMMAND);
  CheckMenuItem(MenuView.Handle, ViewIcons, MenuCheck[LVFiles.ViewStyle = LVS_ICON] or MF_BYCOMMAND);
  TB.Perform(TB_GETITEMRECT, tbViewItem, Integer(@Rect));
  Rect.Top := Rect.Bottom;
  ClientToScreen(Handle, Rect.TopLeft);
  MenuView.Popup(Rect.Left, Rect.Top);
end;

procedure TMainForm.ShowAbout;
const
  AboutCaption = 'About';
  AboutIcon = 'MAINICON';
var
  Version: TOSVersionInfo;
  MsgBoxParamsW: TMsgBoxParamsW;
  MsgBoxParamsA: TMsgBoxParamsA;
begin
  Version.dwOSVersionInfoSize := SizeOf(TOSVersionInfo);
  GetVersionEx(Version);
  if Version.dwPlatformId = VER_PLATFORM_WIN32_NT then
  begin
    FillChar(MsgBoxParamsW, SizeOf(MsgBoxParamsW), #0);
    with MsgBoxParamsW do
    begin
      cbSize := SizeOf(MsgBoxParamsW);
      hwndOwner := Handle;
      hInstance := SysInit.hInstance;
      lpszText := AboutText;
      lpszCaption := AboutCaption;
      lpszIcon := AboutIcon;
      dwStyle := MB_USERICON;
    end;
    MessageBoxIndirectW(MsgBoxParamsW);
  end
  else begin
    FillChar(MsgBoxParamsA, SizeOf(MsgBoxParamsA), #0);
    with MsgBoxParamsA do
    begin
      cbSize := SizeOf(MsgBoxParamsA);
      hwndOwner := Handle;
      hInstance := SysInit.hInstance;
      lpszText := AboutText;
      lpszCaption := AboutCaption;
      lpszIcon := AboutIcon;
      dwStyle := MB_USERICON;
    end;
    MessageBoxIndirectA(MsgBoxParamsA);
  end;
end;

begin
  MainForm := TMainForm.Create;
  MainForm.Run;
  MainForm.Free;
end.

