unit GravadorBJ;

{ A gravação do firmware do BJ-8300 (o .anc) pelo cabo, como o gravador da fabricante a faz: o BinTe ANC Bootloader
  (a janela BinTalk V1.1, o KDH_Bootloader.BootHelper do .NET em tools/anc-bootloader), lido no CIL dele
  (docs/engenharia-reversa.md, seção 42). Sem nada de janela, para dar para testar.

  O rádio fica no modo UPDATE (ligado segurando o PTT2 e o atalho). O pacote, nos dois sentidos:
    AA 55 n(2) carga(n) crc(2)       n e o CRC com o byte baixo primeiro; o CRC16-XMODEM de tudo antes dele
  O primeiro byte da carga é o comando.
    PC -> rádio   06                 começa (o botão Update do BinTalk)
                  01 velocidade(4)   10000 duas vezes, e depois a de trabalho (115200)
                  02 endereço(4) n(4) e os n bytes do .anc sem a camada KDH, a partir do endereço
                  03                 o fim
    rádio -> PC   01                 pronto: anda um passo (abaixo)
                  02 endereço(4) n(4) pede n bytes do arquivo a partir do endereço
                  03                 terminou
                  04                 pede o 03
  O PC abre a serial a 9600 8N1, com o DTR e o RTS ligados, e manda o 06. A cada 01 do rádio:
    passo 0  o modo UPDATE respondeu: manda 01 10000 e passa a serial a 10000
    passo 1  a velocidade valeu, e o rádio pede o carregador (02): manda 01 10000 de novo
    passo 2  o rádio pulou para o carregador: manda 01 115200 e, 20 ms depois, passa a serial a 115200
    passo 3  só o andamento; manda de novo a última carga montada
  Quem decide o que gravar é o rádio, que pede os pedaços do arquivo (o carregador e uma das 6 imagens: uns 1/6 do
  arquivo, que é a conta do andamento do BinTalk). Sem um pacote certo em 2 s, o último pacote vai de novo, até 5
  vezes; depois disso, a gravação falhou, e o rádio volta a esperar no modo UPDATE.

  Antes de gravar, o ConfereAnc confere o arquivo inteiro, como o jlfw.py do repositório: o tamanho do .anc do BJ-8300,
  a camada KDH, o cabeçalho UFW e a tabela de entradas com os CRCs deles, o CRC de cada entrada (as 6 imagens de flash
  em claro, as outras em SFC com a chave do chip), e no app.bin a marca do hardware do BJ-8300 (V01T32) e a versão. O
  de fábrica (V0.12) passa igual. }

interface

uses
  System.SysUtils, Winapi.Windows, Transporte;

const
  TAMANHO_ANC = 4063840;      // o .anc do BJ-8300, o de fábrica e os deste projeto (o build exige o tamanho de fábrica)
  VELOCIDADE_INICIO = 9600;
  VELOCIDADE_TROCA = 10000;
  VELOCIDADE_TRABALHO = 115200;
  ESPERA_PACOTE = 2000;       // ms: o temporizador do BinTalk
  TENTATIVAS = 5;
  LIMITE_MS = 15 * 60 * 1000; // a gravação inteira (o BinTalk não tem limite)

type
  // A serial da gravação: espera tudo sair no Escreve, e troca a velocidade no meio.
  IPortaGravacao = interface
    procedure Escreve(const B: TBytes);
    function Le: TBytes;  // o que chegou, esperando até uns 20 ms pelo primeiro byte
    procedure Velocidade(Baud: Cardinal);
    function Agora: UInt64;  // ms
    procedure Dorme(Ms: Cardinal);
  end;

  // A serial do cabo como o BinTalk a abre: 9600 8N1, sem controle de fluxo, com o DTR e o RTS ligados.
  TPortaGravacao = class(TInterfacedObject, IPortaGravacao)
  private
    FH: THandle;
    FNome: string;
    procedure Erro(const Oque: string);
  public
    constructor Create(const Nome: string);
    destructor Destroy; override;
    procedure Escreve(const B: TBytes);
    function Le: TBytes;
    procedure Velocidade(Baud: Cardinal);
    function Agora: UInt64;
    procedure Dorme(Ms: Cardinal);
  end;

  TAnc = record
    Claro: TBytes;    // sem a camada KDH: o que vai para o rádio
    Versao: string;   // a do item VERSION, de dentro do app.bin: V0.12 na fábrica, V3.7T...
    Fabrica: Boolean; // o V0.12 de fábrica
  end;

  TGravadorBJ = class
  private
    procedure Registra(const Msg: string);
  public
    Cancelar: Boolean;       // só vale antes de o rádio responder: depois, parar no meio deixa o carregador esperando
    Espera: Cardinal;        // ms sem pacote certo antes de repetir (os testes o encurtam)
    OnRegistro: TProc<string>;
    OnProgresso: TProc<Integer>;  // a porcentagem, como o BinTalk a conta
    constructor Create;
    procedure Grava(const P: IPortaGravacao; const A: TAnc);
  end;

function Crc16(const B: TBytes; Inicio, N: Integer): Word;
function Empacota(const Carga: TBytes): TBytes;
// A camada KDH do .anc: tira e põe (é a mesma conta, BootHelper.Decrypt).
procedure Kdh(var B: TBytes);
// '' quando o arquivo é um .anc do BJ-8300 inteiro; senão, o que está errado.
function ConfereAnc(const Dados: TBytes; out A: TAnc): string;

implementation

uses
  Idioma;

const
  POS_VERSAO = $5FC10;  // 0x01E5FD10, o texto do VERSION, menos o começo do app.bin (0x01E00100)

function Crc16(const B: TBytes; Inicio, N: Integer): Word;
var
  I, J: Integer;
  C: Cardinal;
begin
  C := 0;
  for I := Inicio to Inicio + N - 1 do
  begin
    C := C xor (Cardinal(B[I]) shl 8);
    for J := 1 to 8 do
      if C and $8000 <> 0 then
        C := ((C shl 1) xor $1021) and $FFFF
      else
        C := (C shl 1) and $FFFF;
  end;
  Result := Word(C);
end;

function Empacota(const Carga: TBytes): TBytes;
var
  N: Integer;
  C: Word;
begin
  N := Length(Carga);
  SetLength(Result, N + 6);
  Result[0] := $AA;
  Result[1] := $55;
  Result[2] := Byte(N);
  Result[3] := Byte(N shr 8);
  if N > 0 then
    Move(Carga[0], Result[4], N);
  C := Crc16(Result, 0, N + 4);
  Result[N + 4] := Byte(C);
  Result[N + 5] := Byte(C shr 8);
end;

{ Blocos de 1 KB, menos os 2 primeiros e os 2 últimos: o bloco b com b mod 3 = 1 vai em XOR com KDHT, o com 2 com RBGI
  (o 0 fica), byte a byte, pulando os bytes 0x00 e 0xFF, o da chave e o inverso dele; a posição na chave anda a cada
  byte dos blocos cifrados e não volta ao começo de cada bloco. }
procedure Kdh(var B: TBytes);
const
  CHAVES: array[1..2] of array[0..3] of Byte = ((Ord('K'), Ord('D'), Ord('H'), Ord('T')),
                                                 (Ord('R'), Ord('B'), Ord('G'), Ord('I')));
var
  Blocos, Bloco, I, Fim, K: Integer;
  V, C: Byte;
begin
  Blocos := (Length(B) + 1023) div 1024;
  K := 0;
  for Bloco := 2 to Blocos - 3 do
  begin
    if Bloco mod 3 = 0 then
      Continue;
    Fim := Bloco * 1024 + 1024;
    if Fim > Length(B) then
      Fim := Length(B);
    for I := Bloco * 1024 to Fim - 1 do
    begin
      V := B[I];
      C := CHAVES[Bloco mod 3][K];
      if (V <> 0) and (V <> $FF) and (V <> C) and (V <> C xor $FF) then
        B[I] := V xor C;
      K := (K + 1) and 3;
    end;
  end;
end;

{ ---------- a conferência do .anc (as contas do firmware/scripts/jlfw.py) ---------- }

function ProximaChave(V: Word): Word; inline;
begin
  if V and $8000 <> 0 then
    Result := Word((V shl 1) xor $1021)
  else
    Result := Word(V shl 1);
end;

// ENC: XOR com a sequência da chave, que anda como o CRC.
function Enc(const B: TBytes; Inicio, N: Integer; Chave: Word): TBytes;
var
  I: Integer;
begin
  SetLength(Result, N);
  for I := 0 to N - 1 do
  begin
    Result[I] := B[Inicio + I] xor Byte(Chave);
    Chave := ProximaChave(Chave);
  end;
end;

// SFC: blocos de 32 B alinhados pelo endereço (Base é o do primeiro byte), cada um em ENC com a chave xor (o endereço
// do bloco / 4).
function Sfc(const B: TBytes; Inicio, N: Integer; Chave: Word; Base: Cardinal): TBytes;
var
  I, J: Integer;
  A, Bloco: Cardinal;
  K: Word;
begin
  SetLength(Result, N);
  I := 0;
  while I < N do
  begin
    A := Base + Cardinal(I);
    Bloco := A and not Cardinal(31);
    K := Chave xor Word((Bloco shr 2) and $FFFF);
    for J := 1 to Integer(A - Bloco) do
      K := ProximaChave(K);
    J := Integer(A - Bloco);
    while (J < 32) and (I < N) do
    begin
      Result[I] := B[Inicio + I] xor Byte(K);
      K := ProximaChave(K);
      Inc(I);
      Inc(J);
    end;
  end;
end;

function Le16(const B: TBytes; P: Integer): Word; inline;
begin
  Result := B[P] or B[P + 1] shl 8;
end;

function Le32(const B: TBytes; P: Integer): Cardinal; inline;
begin
  Result := Cardinal(B[P]) or Cardinal(B[P + 1]) shl 8 or Cardinal(B[P + 2]) shl 16 or Cardinal(B[P + 3]) shl 24;
end;

function Texto(const B: TBytes; P, N: Integer): string;
var
  I: Integer;
begin
  Result := '';
  for I := P to P + N - 1 do
  begin
    if B[I] = 0 then
      Break;
    Result := Result + Char(B[I]);
  end;
end;

type
  TJlfs = record
    Ok: Boolean;
    CrcDados: Word;
    Desloc, Tam: Cardinal;
    Nome: string;
  end;

// Uma entrada JLFS de 32 B em claro: crc16 dos bytes 2..31, o CRC dos dados, o deslocamento, o tamanho e o nome.
function Jlfs(const E: TBytes): TJlfs;
begin
  Result.Ok := Crc16(E, 2, 30) = Le16(E, 0);
  Result.CrcDados := Le16(E, 2);
  Result.Desloc := Le32(E, 4);
  Result.Tam := Le32(E, 8);
  Result.Nome := Texto(E, 16, 16);
end;

// A posição do app_dir_head na imagem: a tabela JLFS do começo (em 0x20, ENC 0xFFFF). -1 se não houver.
function DirApp(const Img: TBytes): Integer;
var
  Pos: Integer;
  E: TJlfs;
begin
  Result := -1;
  Pos := 0;
  while $20 + Pos + 32 <= Length(Img) do
  begin
    E := Jlfs(Enc(Img, $20 + Pos, 32, $FFFF));
    if not E.Ok then
      Exit;
    if E.Nome = 'app_dir_head' then
      Exit(Integer(E.Desloc));
    Inc(Pos, 32);
  end;
end;

// A chave do chip: a que faz a primeira entrada do app_dir_head ter o CRC certo e se chamar app_area_head.
function ChaveDoChip(const Img: TBytes; Base: Integer; out Chave: Word): Boolean;
var
  K: Integer;
  E: TJlfs;
begin
  if (Base < 0) or (Base + 32 > Length(Img)) then
    Exit(False);
  for K := 0 to $FFFF do
  begin
    E := Jlfs(Enc(Img, Base, 32, Word(K)));
    if E.Ok and (E.Nome = 'app_area_head') then
    begin
      Chave := Word(K);
      Exit(True);
    end;
  end;
  Result := False;
end;

// O app.bin da imagem, decifrado e com o CRC conferido; nil se não achar ou não conferir.
function AppBin(const Img: TBytes; Base: Integer; Chave: Word): TBytes;
var
  Pos: Integer;
  E: TJlfs;
begin
  Result := nil;
  Pos := 0;
  while Base + Pos + 32 <= Length(Img) do
  begin
    E := Jlfs(Sfc(Img, Base + Pos, 32, Chave, Cardinal(Pos)));
    if not E.Ok then
      Exit;
    if E.Nome = 'app.bin' then
    begin
      if (E.Tam = 0) or (Int64(Base) + E.Desloc + E.Tam > Length(Img)) then
        Exit;
      Result := Sfc(Img, Base + Integer(E.Desloc), Integer(E.Tam), Chave, E.Desloc);
      if Crc16(Result, 0, Length(Result)) <> E.CrcDados then
        Result := nil;
      Exit;
    end;
    Inc(Pos, 32);
  end;
end;

function Contem(const B: TBytes; const S: RawByteString): Boolean;
var
  I: Integer;
begin
  for I := 0 to Length(B) - Length(S) do
    if CompareMem(@B[I], PAnsiChar(S), Length(S)) then
      Exit(True);
  Result := False;
end;

function ConfereAnc(const Dados: TBytes; out A: TAnc): string;
const
  IMAGENS = [$00, $20, $21, $22, $23, $24];  // as 6 imagens de flash, em claro dentro do UFW
var
  Cab, E, Img, App: TBytes;
  N, I, Quantas: Integer;
  Tipo: Byte;
  CrcEntrada: Word;
  Desloc, Tam: Cardinal;
  Nome: string;
  Chave: Word;
  TemChave: Boolean;
begin
  A := Default(TAnc);
  if Length(Dados) <> TAMANHO_ANC then
    Exit(Format(Tr('Não é um firmware do BJ-8300: o arquivo tem %d bytes, e o .anc do BJ-8300 tem %d.',
                   'Not a BJ-8300 firmware: the file has %d bytes, and the BJ-8300 .anc has %d.'),
                [Length(Dados), TAMANHO_ANC]));
  A.Claro := Copy(Dados);
  Kdh(A.Claro);

  Cab := Enc(A.Claro, 0, $40, $FFFF);
  if Crc16(Cab, 2, $3E) <> Le16(Cab, 0) then
    Exit(Tr('O arquivo está estragado: o CRC do cabeçalho não confere.',
            'The file is damaged: the header CRC does not match.'));
  if Texto(Cab, $10, 16) <> 'AC695X' then
    Exit(Format(Tr('Não é um firmware do BJ-8300: o chip é %s.', 'Not a BJ-8300 firmware: the chip is %s.'),
                [Texto(Cab, $10, 16)]));
  N := Le16(Cab, 8);
  if (N < 1) or (N > 32) or ($40 + N * $50 > Length(A.Claro)) then
    Exit(Tr('O arquivo está estragado: a tabela de entradas não cabe nele.',
            'The file is damaged: its entry table does not fit in it.'));
  if Crc16(A.Claro, $40, N * $50) <> Le16(Cab, 2) then
    Exit(Tr('O arquivo está estragado: o CRC da tabela de entradas não confere.',
            'The file is damaged: the entry table CRC does not match.'));

  // a chave do chip, da primeira imagem de flash
  TemChave := False;
  Chave := 0;
  Quantas := 0;
  for I := 0 to N - 1 do
  begin
    E := Enc(A.Claro, $40 + I * $50, $50, $FFFF);
    Desloc := Le32(E, 8);
    Tam := Le32(E, 12);
    if Int64(Desloc) + Tam > Length(A.Claro) then
      Exit(Tr('O arquivo está estragado: uma entrada passa do fim dele.',
              'The file is damaged: an entry runs past its end.'));
    if E[0] in IMAGENS then
    begin
      Inc(Quantas);
      if not TemChave and (E[0] = 0) then
      begin
        Img := Copy(A.Claro, Desloc, Tam);
        TemChave := ChaveDoChip(Img, DirApp(Img), Chave);
        if TemChave then
        begin
          App := AppBin(Img, DirApp(Img), Chave);
          if App = nil then
            Exit(Tr('O arquivo está estragado: o app.bin da primeira imagem não confere.',
                    'The file is damaged: the first image''s app.bin does not match.'));
        end;
      end;
    end;
  end;
  if Quantas <> 6 then
    Exit(Format(Tr('Não é um firmware do BJ-8300: tem %d imagens de flash, e o do BJ-8300 tem 6.',
                   'Not a BJ-8300 firmware: it has %d flash images, and the BJ-8300 one has 6.'), [Quantas]));
  if not TemChave then
    Exit(Tr('O arquivo está estragado: não achei a chave do chip na primeira imagem.',
            'The file is damaged: could not find the chip key in the first image.'));

  // o CRC de cada entrada: as imagens em claro; as outras como estão, ou em SFC com a chave do chip
  for I := 0 to N - 1 do
  begin
    E := Enc(A.Claro, $40 + I * $50, $50, $FFFF);
    Tipo := E[0];
    CrcEntrada := Le16(E, 4);
    Desloc := Le32(E, 8);
    Tam := Le32(E, 12);
    Nome := Texto(E, $40, 16);
    if Tam = 0 then
      Continue;
    if (Crc16(A.Claro, Desloc, Tam) <> CrcEntrada) and ((Tipo in IMAGENS) or
       (Crc16(Sfc(A.Claro, Desloc, Tam, Chave, Desloc), 0, Tam) <> CrcEntrada)) then
      Exit(Format(Tr('O arquivo está estragado: o CRC de %s não confere.', 'The file is damaged: the CRC of %s does not match.'),
                  [Nome]));
  end;

  if not Contem(App, 'V01T32') then
    Exit(Tr('Não é um firmware do BJ-8300: falta a marca do hardware dele (V01T32).',
            'Not a BJ-8300 firmware: the radio''s hardware tag (V01T32) is missing.'));
  if Length(App) > POS_VERSAO + 6 then
    A.Versao := Texto(App, POS_VERSAO, 6);
  A.Fabrica := A.Versao = 'V0.12';
  Result := '';
end;

{ TPortaGravacao }

constructor TPortaGravacao.Create(const Nome: string);
var
  D: TDCB;
  T: TCommTimeouts;
begin
  FNome := Nome;
  FH := CreateFile(PChar('\\.\' + Nome), GENERIC_READ or GENERIC_WRITE, 0, nil, OPEN_EXISTING, 0, 0);
  if FH = INVALID_HANDLE_VALUE then
    Erro(Tr('abrir', 'open'));
  D.DCBlength := SizeOf(D);
  if not GetCommState(FH, D) then
    Erro(Tr('configurar', 'configure'));
  D.BaudRate := VELOCIDADE_INICIO;
  D.Flags := 1 or $10 or $1000;  // o fBinary, o DTR_CONTROL_ENABLE e o RTS_CONTROL_ENABLE: o DtrEnable e o RtsEnable do BinTalk
  D.ByteSize := 8;
  D.Parity := NOPARITY;
  D.StopBits := ONESTOPBIT;
  if not SetCommState(FH, D) then
    Erro(Tr('configurar', 'configure'));
  T := Default(TCommTimeouts);
  T.ReadIntervalTimeout := MAXDWORD;
  T.ReadTotalTimeoutMultiplier := MAXDWORD;
  T.ReadTotalTimeoutConstant := 20;
  T.WriteTotalTimeoutConstant := 10000;
  if not SetCommTimeouts(FH, T) then
    Erro(Tr('configurar', 'configure'));
  PurgeComm(FH, PURGE_RXCLEAR or PURGE_TXCLEAR);
end;

destructor TPortaGravacao.Destroy;
begin
  if (FH <> 0) and (FH <> INVALID_HANDLE_VALUE) then
    CloseHandle(FH);
  inherited;
end;

procedure TPortaGravacao.Erro(const Oque: string);
begin
  raise EInOutError.CreateFmt(Tr('Não deu para %s a %s: %s', 'Could not %s %s: %s'),
                              [Oque, FNome, SysErrorMessage(GetLastError)]);
end;

procedure TPortaGravacao.Escreve(const B: TBytes);
var
  N: DWORD;
begin
  if (Length(B) > 0) and (not WriteFile(FH, B[0], Length(B), N, nil) or (Integer(N) <> Length(B))) then
    Erro(Tr('escrever na', 'write to'));
  FlushFileBuffers(FH);  // a velocidade só pode mudar depois que o pacote saiu
end;

function TPortaGravacao.Le: TBytes;
var
  N: DWORD;
begin
  SetLength(Result, 4096);
  if not ReadFile(FH, Result[0], Length(Result), N, nil) then
    Erro(Tr('ler da', 'read from'));
  SetLength(Result, N);
end;

procedure TPortaGravacao.Velocidade(Baud: Cardinal);
var
  D: TDCB;
begin
  D.DCBlength := SizeOf(D);
  if not GetCommState(FH, D) then
    Erro(Tr('configurar', 'configure'));
  D.BaudRate := Baud;
  if not SetCommState(FH, D) then
    Erro(Tr('configurar', 'configure'));
  PurgeComm(FH, PURGE_RXCLEAR);
end;

function TPortaGravacao.Agora: UInt64;
begin
  Result := GetTickCount64;
end;

procedure TPortaGravacao.Dorme(Ms: Cardinal);
begin
  Sleep(Ms);
end;

{ TGravadorBJ }

constructor TGravadorBJ.Create;
begin
  inherited;
  Espera := ESPERA_PACOTE;
end;

procedure TGravadorBJ.Registra(const Msg: string);
begin
  if Assigned(OnRegistro) then
    OnRegistro(Msg);
end;

procedure Poe32(var B: TBytes; P: Integer; V: Cardinal);
begin
  B[P] := Byte(V);
  B[P + 1] := Byte(V shr 8);
  B[P + 2] := Byte(V shr 16);
  B[P + 3] := Byte(V shr 24);
end;

{ O laço do BootHelper.BootLoading com o botão Update (flagPTTPress): os estados com os números dele (8 começa, 10 a
  velocidade, 13 os dados, 15 o fim, 17 espera o AA, 18 o resto do cabeçalho, 19 o corpo), o passo (local3), a última
  carga montada (local0, que o passo 3 manda de novo), o último pacote (o que se repete) e o temporizador de 2 s: ele
  corre desde o começo, o envio só o liga se estava parado, o AA o recomeça e o pacote certo o para. }
procedure TGravadorBJ.Grava(const P: IPortaGravacao; const A: TAnc);
const
  COMECA = 8;
  VELOCIDADE = 10;
  DADOS = 13;
  FIM = 15;
  ESPERA_AA = 17;
  CABECALHO = 18;
  CORPO = 19;
var
  Estado, Passo, Restam, Tam, N, Porcento, UltimaPorcento, Falta: Integer;
  Carga, Pacote, Rx, Fila, Chegou: TBytes;
  Prazo, Limite: UInt64;
  Ligado, Repete, Respondeu, AvisouCancelar: Boolean;
  Endereco, Pedido: Cardinal;
  Enviado, Sexto: Int64;

  procedure Liga;  // o Timer.Start: só se estava parado
  begin
    if not Ligado then
    begin
      Ligado := True;
      Prazo := P.Agora + Espera;
    end;
  end;

  procedure Envia;
  begin
    Pacote := Empacota(Carga);
    P.Escreve(Pacote);
    Estado := ESPERA_AA;
    Liga;
    Rx := nil;
  end;

  function Tira(K: Integer): TBytes;
  begin
    Result := Copy(Fila, 0, K);
    Delete(Fila, 0, K);
  end;

  procedure Andamento;
  begin
    if Sexto <= 0 then
      Exit;
    Porcento := Enviado * 100 div Sexto;
    if Porcento > 100 then
      Porcento := 100;
    if (Porcento <> UltimaPorcento) and Assigned(OnProgresso) then
      OnProgresso(Porcento);
    UltimaPorcento := Porcento;
  end;

  // Um pacote certo que o BinTalk não sabe tratar o deixa esperando para sempre, com o temporizador parado: aqui o
  // temporizador continua, e sem outro pacote o último vai de novo, como na falta de resposta.
  procedure Desconhecido;
  begin
    Estado := ESPERA_AA;
    Liga;
  end;

  // um pacote certo do rádio; True no fim da gravação
  function Trata: Boolean;
  var
    Cmd: Byte;
  begin
    Result := False;
    Restam := TENTATIVAS;
    Ligado := False;
    Respondeu := True;
    Cmd := Rx[4];
    case Cmd of
      1:
        case Passo of
          0:
            begin
              Registra(Tr('O rádio respondeu. Preparando a gravação: não desligue o rádio nem tire o cabo.',
                          'The radio answered. Getting ready to flash: do not turn the radio off or pull the cable.'));
              Estado := VELOCIDADE;
            end;
          1:
            begin
              Registra(Tr('Mandando ao rádio o programa que grava o firmware (leva uns 30 segundos)...',
                          'Sending the radio the program that flashes the firmware (takes about 30 seconds)...'));
              Estado := VELOCIDADE;
            end;
          2:
            begin
              Registra(Tr('Gravando o firmware no rádio. Não desligue o rádio nem tire o cabo até terminar.',
                          'Flashing the firmware to the radio. Do not turn the radio off or pull the cable until it ' +
                          'finishes.'));
              Estado := VELOCIDADE;
            end;
          3:
            begin
              Andamento;
              Estado := VELOCIDADE;
            end;
        else
          Desconhecido;
        end;
      2:
        if Length(Rx) >= 4 + 9 then
        begin
          Endereco := Le32(Rx, 5);
          Pedido := Le32(Rx, 9);
          Estado := DADOS;
          Andamento;
        end
        else
          Desconhecido;
      3:
        begin
          if Assigned(OnProgresso) then
            OnProgresso(100);
          Registra(Tr('O rádio terminou a gravação.', 'The radio finished flashing.'));
          Result := True;
        end;
      4:
        Estado := FIM;
    else
      Desconhecido;
    end;
  end;

begin
  Estado := COMECA;
  Passo := 0;
  Carga := TBytes.Create(0);
  Pacote := nil;
  Fila := nil;
  Rx := nil;
  Restam := TENTATIVAS;
  Repete := False;
  Respondeu := False;
  AvisouCancelar := False;
  Enviado := 0;
  Sexto := Length(A.Claro) div 6;
  UltimaPorcento := -1;
  Tam := 0;
  Ligado := True;  // o temporizador do BinTalk nasce ligado
  Prazo := P.Agora + Espera;
  Registra(Format(Tr('Gravando o firmware %s: chamando o rádio no modo UPDATE...',
                     'Flashing firmware %s: calling the radio in UPDATE mode...'), [A.Versao]));
  Limite := P.Agora + LIMITE_MS;
  while True do
  begin
    if Cancelar and not Respondeu then
      raise ECancelado.Create(Tr('Cancelado.', 'Cancelled.'));
    if Cancelar and not AvisouCancelar then
    begin
      AvisouCancelar := True;
      Registra(Tr('O rádio já está gravando: parar agora pode deixá-lo sem firmware, e a gravação segue até o fim.',
                  'The radio is already flashing: stopping now could leave it without firmware, so flashing goes on ' +
                  'to the end.'));
    end;
    if P.Agora > Limite then
      raise EFalha.Create(Tr('A gravação passou de 15 minutos sem terminar. Desligue o rádio, ponha-o de novo no modo ' +
                             'UPDATE e grave outra vez.',
                             'Flashing went past 15 minutes without finishing. Turn the radio off, put it in UPDATE ' +
                             'mode again and flash again.'));
    // lê só quando o estado espera bytes que ainda não chegaram: com o pacote do rádio inteiro na fila, cada leitura a
    // mais esperaria 20 ms à toa (três por pacote, com o rádio parado esperando a resposta)
    case Estado of
      ESPERA_AA: Falta := 1;
      CABECALHO: Falta := 3;
      CORPO: Falta := Tam + 2;
    else
      Falta := 0;
    end;
    if Length(Fila) < Falta then
    begin
      Chegou := P.Le;
      if Length(Chegou) > 0 then
        Fila := Fila + Chegou;
    end;
    if Ligado and (P.Agora >= Prazo) then
    begin
      Repete := True;
      Prazo := P.Agora + Espera;
    end;

    case Estado of
      COMECA:
        begin
          Carga := TBytes.Create(6);
          Envia;
        end;
      VELOCIDADE:
        begin
          case Passo of
            0, 1: Carga := TBytes.Create(1, Byte(VELOCIDADE_TROCA), Byte(VELOCIDADE_TROCA shr 8), 0, 0);
            2:
              begin
                SetLength(Carga, 5);
                Carga[0] := 1;
                Poe32(Carga, 1, VELOCIDADE_TRABALHO);
              end;
          end;
          Envia;
          case Passo of
            0:
              begin
                P.Velocidade(VELOCIDADE_TROCA);
                P.Dorme(20);
                Passo := 1;
              end;
            1:
              Passo := 2;
            2:
              begin
                P.Dorme(20);
                P.Velocidade(VELOCIDADE_TRABALHO);
                P.Dorme(10);
                Passo := 3;
              end;
          end;
        end;
      DADOS:
        begin
          if Pedido > 65535 - 9 then
            raise EFalha.CreateFmt(Tr('O rádio pediu %d bytes de uma vez, mais do que cabe num pacote.',
                                      'The radio asked for %d bytes at once, more than a packet holds.'), [Pedido]);
          SetLength(Carga, 9 + Integer(Pedido));
          FillChar(Carga[0], Length(Carga), 0);
          N := 0;
          if Endereco < Cardinal(Length(A.Claro)) then
          begin
            N := Length(A.Claro) - Integer(Endereco);
            if N > Integer(Pedido) then
              N := Pedido;
            Move(A.Claro[Endereco], Carga[9], N);
          end;
          Carga[0] := 2;
          Poe32(Carga, 1, Endereco);
          Poe32(Carga, 5, Cardinal(N));
          Envia;
          Inc(Enviado, N);
        end;
      FIM:
        begin
          Carga := TBytes.Create(3);
          Envia;
        end;
      ESPERA_AA:
        if Length(Fila) >= 1 then
        begin
          Rx := Tira(1);
          if Rx[0] = $AA then
          begin
            Estado := CABECALHO;
            Restam := TENTATIVAS;
            Ligado := False;
            Liga;
          end;
        end;
      CABECALHO:
        if Length(Fila) >= 3 then
        begin
          Rx := Rx + Tira(3);
          Tam := Rx[2] or Rx[3] shl 8;
          Estado := CORPO;
        end;
      CORPO:
        if Length(Fila) >= Tam + 2 then
        begin
          Rx := Copy(Rx, 0, 4) + Tira(Length(Fila));
          if (Tam >= 1) and (Crc16(Rx, 0, Tam + 4) = Le16(Rx, Tam + 4)) and Trata then
            Exit;
        end;
    end;

    if Repete then
    begin
      Repete := False;
      if Restam <= 0 then
      begin
        if Respondeu then
          raise EFalha.Create(Tr('O rádio parou de responder no meio da gravação. Desligue-o, ponha-o de novo no modo ' +
                                 'UPDATE e grave outra vez.',
                                 'The radio stopped answering in the middle of flashing. Turn it off, put it in ' +
                                 'UPDATE mode again and flash again.'));
        raise ESemRadio.Create(Tr('O rádio não respondeu. Ele está no modo UPDATE (ligado segurando o PTT2 e o atalho, ' +
                                  'com UPDATE na tela)? A porta é a do cabo?',
                                  'The radio did not answer. Is it in UPDATE mode (turned on holding PTT2 and the ' +
                                  'shortcut key, with UPDATE on the screen)? Is the port the cable''s?'));
      end;
      Dec(Restam);
      Estado := ESPERA_AA;
      if Pacote <> nil then
        P.Escreve(Pacote);
      Rx := nil;
      Registra(Format(Tr('O rádio não respondeu: tentando de novo (%d de %d).', 'The radio did not answer: trying again ' +
                         '(%d of %d).'), [TENTATIVAS - Restam, TENTATIVAS]));
    end;
  end;
end;

end.
