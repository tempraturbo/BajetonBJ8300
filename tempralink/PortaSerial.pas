unit PortaSerial;

{ A serial do cabo pela API do Windows, e a lista das portas com o chip de cada uma. }

interface

uses
  System.SysUtils, Winapi.Windows, Transporte;

type
  TPorta = record
    Nome: string;       // COM4
    Descricao: string;  // USB-SERIAL CH340 (COM4)
    Ch340: Boolean;     // o chip dos cabos de programação (VID 1A86)
  end;

  TPortaSerial = class(TInterfacedObject, ITransporte)
  private
    FH: THandle;
    FNome: string;
    procedure Erro(const Oque: string);
  public
    constructor Create(const Nome: string);
    destructor Destroy; override;
    procedure Escreve(const B: TBytes);
    function Le: TBytes;
    procedure LimpaEntrada;
    function Agora: UInt64;
    procedure Dorme(Ms: Cardinal);
  end;

function ListaPortas: TArray<TPorta>;

implementation

uses
  System.Generics.Collections, System.Generics.Defaults, Idioma;

{ TPortaSerial }

constructor TPortaSerial.Create(const Nome: string);
var
  D: TDCB;
  T: TCommTimeouts;
begin
  FNome := Nome;
  FH := CreateFile(PChar('\\.\' + Nome), GENERIC_READ or GENERIC_WRITE, 0, nil, OPEN_EXISTING, 0, 0);
  if FH = INVALID_HANDLE_VALUE then
    Erro(Tr('abrir', 'open'));
  // DTR e RTS baixos: com eles altos o rádio fica mudo (medido em 26/10), e o PTT é o mesmo fio do RX da serial.
  D.DCBlength := SizeOf(D);
  if not GetCommState(FH, D) then
    Erro(Tr('configurar', 'configure'));
  D.BaudRate := 115200;
  D.Flags := 1;  // só o fBinary: sem paridade, sem controle de fluxo, DTR_CONTROL_DISABLE e RTS_CONTROL_DISABLE
  D.ByteSize := 8;
  D.Parity := NOPARITY;
  D.StopBits := ONESTOPBIT;
  if not SetCommState(FH, D) then
    Erro(Tr('configurar', 'configure'));
  // A leitura volta logo com o que já chegou, ou espera até 20 ms pelo primeiro byte.
  T := Default(TCommTimeouts);
  T.ReadIntervalTimeout := MAXDWORD;
  T.ReadTotalTimeoutMultiplier := MAXDWORD;
  T.ReadTotalTimeoutConstant := 20;
  T.WriteTotalTimeoutConstant := 3000;
  if not SetCommTimeouts(FH, T) then
    Erro(Tr('configurar', 'configure'));
  PurgeComm(FH, PURGE_RXCLEAR or PURGE_TXCLEAR);
end;

destructor TPortaSerial.Destroy;
begin
  if (FH <> 0) and (FH <> INVALID_HANDLE_VALUE) then
    CloseHandle(FH);
  inherited;
end;

procedure TPortaSerial.Erro(const Oque: string);
begin
  raise EInOutError.CreateFmt(Tr('Não deu para %s a %s: %s', 'Could not %s %s: %s'),
                              [Oque, FNome, SysErrorMessage(GetLastError)]);
end;

procedure TPortaSerial.Escreve(const B: TBytes);
var
  N: DWORD;
begin
  if (Length(B) > 0) and (not WriteFile(FH, B[0], Length(B), N, nil) or (Integer(N) <> Length(B))) then
    Erro(Tr('escrever na', 'write to'));
end;

function TPortaSerial.Le: TBytes;
var
  N: DWORD;
begin
  SetLength(Result, 2048);
  if not ReadFile(FH, Result[0], Length(Result), N, nil) then
    Erro(Tr('ler da', 'read from'));
  SetLength(Result, N);
end;

procedure TPortaSerial.LimpaEntrada;
begin
  PurgeComm(FH, PURGE_RXCLEAR);
end;

function TPortaSerial.Agora: UInt64;
begin
  Result := GetTickCount64;
end;

procedure TPortaSerial.Dorme(Ms: Cardinal);
begin
  Sleep(Ms);
end;


{ As portas da classe Ports do Gerenciador de Dispositivos, pela SetupAPI. }

type
  HDEVINFO = THandle;
  SP_DEVINFO_DATA = record
    cbSize: DWORD;
    ClassGuid: TGUID;
    DevInst: DWORD;
    Reserved: ULONG_PTR;
  end;

const
  GUID_DEVCLASS_PORTS: TGUID = '{4D36E978-E325-11CE-BFC1-08002BE10318}';
  DIGCF_PRESENT = 2;
  SPDRP_HARDWAREID = 1;
  SPDRP_FRIENDLYNAME = $C;
  DICS_FLAG_GLOBAL = 1;
  DIREG_DEV = 1;

function SetupDiGetClassDevsW(ClassGuid: PGUID; Enumerator: PWideChar; Pai: HWND; Flags: DWORD): HDEVINFO;
  stdcall; external 'setupapi.dll';
function SetupDiEnumDeviceInfo(Lista: HDEVINFO; Indice: DWORD; var Info: SP_DEVINFO_DATA): BOOL;
  stdcall; external 'setupapi.dll';
function SetupDiGetDeviceRegistryPropertyW(Lista: HDEVINFO; var Info: SP_DEVINFO_DATA; Propriedade: DWORD;
  Tipo: PDWORD; Buffer: PByte; Tamanho: DWORD; Precisa: PDWORD): BOOL; stdcall; external 'setupapi.dll';
function SetupDiOpenDevRegKey(Lista: HDEVINFO; var Info: SP_DEVINFO_DATA; Escopo, Perfil, Tipo: DWORD;
  Acesso: REGSAM): HKEY; stdcall; external 'setupapi.dll';
function SetupDiDestroyDeviceInfoList(Lista: HDEVINFO): BOOL; stdcall; external 'setupapi.dll';

// Texto de uma propriedade; num REG_MULTI_SZ, as cadeias separadas por #0.
function Propriedade(Lista: HDEVINFO; var Info: SP_DEVINFO_DATA; Qual: DWORD): string;
var
  Buf: array[0..1023] of WideChar;
begin
  FillChar(Buf, SizeOf(Buf), 0);
  if SetupDiGetDeviceRegistryPropertyW(Lista, Info, Qual, nil, @Buf, SizeOf(Buf) - SizeOf(WideChar), nil) then
    SetString(Result, Buf, Length(Buf))
  else
    Result := '';
  Result := TrimRight(Result);  // os #0 do fim
end;

function NomeDaPorta(Lista: HDEVINFO; var Info: SP_DEVINFO_DATA): string;
var
  K: HKEY;
  Buf: array[0..63] of WideChar;
  Tam: DWORD;
begin
  Result := '';
  K := SetupDiOpenDevRegKey(Lista, Info, DICS_FLAG_GLOBAL, 0, DIREG_DEV, KEY_READ);
  if K = INVALID_HANDLE_VALUE then
    Exit;
  FillChar(Buf, SizeOf(Buf), 0);
  Tam := SizeOf(Buf) - SizeOf(WideChar);
  if RegQueryValueEx(K, 'PortName', nil, nil, @Buf, @Tam) = ERROR_SUCCESS then
    Result := PWideChar(@Buf);
  RegCloseKey(K);
end;

function ListaPortas: TArray<TPorta>;
var
  Lista: HDEVINFO;
  Info: SP_DEVINFO_DATA;
  I: DWORD;
  P: TPorta;
  Portas: TList<TPorta>;
begin
  Portas := TList<TPorta>.Create;
  try
    Lista := SetupDiGetClassDevsW(@GUID_DEVCLASS_PORTS, nil, 0, DIGCF_PRESENT);
    if Lista <> INVALID_HANDLE_VALUE then
    try
      I := 0;
      Info.cbSize := SizeOf(Info);
      while SetupDiEnumDeviceInfo(Lista, I, Info) do
      begin
        P.Nome := NomeDaPorta(Lista, Info);
        if P.Nome.StartsWith('COM', True) then
        begin
          P.Descricao := Propriedade(Lista, Info, SPDRP_FRIENDLYNAME);
          if P.Descricao = '' then
            P.Descricao := P.Nome;
          P.Ch340 := Propriedade(Lista, Info, SPDRP_HARDWAREID).ToUpper.Contains('VID_1A86');
          Portas.Add(P);
        end;
        Inc(I);
      end;
    finally
      SetupDiDestroyDeviceInfoList(Lista);
    end;
    // COM3 antes de COM10
    Portas.Sort(TComparer<TPorta>.Construct(
      function(const A, B: TPorta): Integer
      begin
        Result := StrToIntDef(A.Nome.Substring(3), 0) - StrToIntDef(B.Nome.Substring(3), 0);
      end));
    Result := Portas.ToArray;
  finally
    Portas.Free;
  end;
end;

end.
