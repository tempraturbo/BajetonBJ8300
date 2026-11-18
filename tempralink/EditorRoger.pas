unit EditorRoger;

{ O editor das notas do ROGER CUSTOM, com o som saindo pelo PC:
  - TTeclado: um piano com os tons que o rádio toca (de 311 a 2960 Hz, os semitons de D#4 a F#7). Clicar numa tecla
    toca o tom; dois cliques, ou arrastar a tecla até a linha do tempo, põem a nota.
  - TLinhaDoTempo: as notas em blocos, o tom na altura e a duração no comprimento, nos 3 s que o rádio aceita. Arrastar
    a nota para cima e para baixo muda o tom (tocando cada um), pela ponta direita muda a duração, e para os lados muda
    a ordem. Pelo teclado: as setas escolhem (com Ctrl, mudam a ordem) e sobem ou descem o tom (com Shift, a oitava),
    + e - mudam a duração, Delete apaga e o espaço toca e para.
  O som é o do Melodia.SomDasNotas (um tom puro por vez, como o rádio), tocado pelo PlaySound; o cursor anda junto. }

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Classes, System.Types, Vcl.Controls, Vcl.Graphics,
  Vcl.ExtCtrls, BJ8300;

const
  NOTA_MIN = 63;   // D#4, 311 Hz: o primeiro semitom acima dos 300 Hz do rádio
  NOTA_MAX = 102;  // F#7, 2960 Hz

type
  TArrasto = (arNada, arMove, arDuracao);

  TLinhaDoTempo = class(TCustomControl)
  private
    FNotas: TArray<TNota>;
    FEscolhida: Integer;
    FArrasto: TArrasto;
    FMudouNoArrasto: Boolean;
    FInsercao: Integer;           // onde a tecla arrastada entra (-1 nenhum)
    FCursor: Integer;             // o ms que está tocando, -1 parado
    FDesde, FAte: Integer;        // o trecho que está tocando, em ms
    FComecou: UInt64;             // quando o primeiro som do trecho sai
    FRelogio: TTimer;
    FOnAntes, FOnMudou, FOnEscolheu, FOnFimDoSom: TNotifyEvent;
    FOnTom: TProc<Integer>;
    procedure SetNotas(const Valor: TArray<TNota>);
    procedure SetEscolhida(Valor: Integer);
    procedure Anda(Sender: TObject);
    procedure WMGetDlgCode(var Message: TWMGetDlgCode); message WM_GETDLGCODE;
    function X0: Integer;
    function X1: Integer;
    function PorMs: Double;
    function XDoMs(Ms: Integer): Integer;
    function YDaNota(Nota: Integer): Integer;
    function NotaDoY(Y: Integer): Integer;
    function Bloco(K: Integer): TRect;
    function NaPosicao(X, Y: Integer): Integer;
    function IndiceDoX(X: Integer): Integer;
    procedure Antes;
    procedure Mudou;
    procedure Troca(A, B: Integer);
  protected
    procedure Paint; override;
    procedure MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure MouseMove(Shift: TShiftState; X, Y: Integer); override;
    procedure MouseUp(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure KeyDown(var Key: Word; Shift: TShiftState); override;
    procedure DragOver(Source: TObject; X, Y: Integer; State: TDragState; var Accept: Boolean); override;
    procedure DoExit; override;
  public
    DuracaoNova: Integer;         // os ms das notas que entram arrastadas do teclado
    constructor Create(AOwner: TComponent); override;
    procedure DragDrop(Source: TObject; X, Y: Integer); override;
    function Total: Integer;
    function Inicio(K: Integer): Integer;
    // a nota em K (depois da última, no fim), cortada para caber nos 3 s; falso se não cabe nem 10 ms
    function Insere(K: Integer; N: TNota): Boolean;
    procedure Apaga(K: Integer);
    procedure Toca(DesdeK: Integer);
    procedure Para;
    function Tocando: Boolean;
    property Notas: TArray<TNota> read FNotas write SetNotas;
    property Escolhida: Integer read FEscolhida write SetEscolhida;
    property OnAntes: TNotifyEvent read FOnAntes write FOnAntes;     // antes de uma mudança: o desfazer guarda
    property OnMudou: TNotifyEvent read FOnMudou write FOnMudou;
    property OnEscolheu: TNotifyEvent read FOnEscolheu write FOnEscolheu;
    property OnFimDoSom: TNotifyEvent read FOnFimDoSom write FOnFimDoSom;
    property OnTom: TProc<Integer> read FOnTom write FOnTom;         // um tom para ouvir, em Hz
  end;

  TTeclado = class(TCustomControl)
  private
    FMarcada, FApertada, FArrastada: Integer;
    FOnToca, FOnPoe: TProc<Integer>;
    function Brancas: Integer;
    function TeclaBranca(I: Integer): TRect;
    function TeclaPreta(Nota: Integer): TRect;
    function NotaEm(X, Y: Integer): Integer;
    procedure SetMarcada(Valor: Integer);
  protected
    procedure Paint; override;
    procedure MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure MouseUp(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure DoEndDrag(Target: TObject; X, Y: Integer); override;
  public
    constructor Create(AOwner: TComponent); override;
    property Marcada: Integer read FMarcada write SetMarcada;  // a nota da nota escolhida na linha do tempo
    property Arrastada: Integer read FArrastada;                // a que está sendo arrastada para a linha do tempo
    property OnToca: TProc<Integer> read FOnToca write FOnToca; // a nota do MIDI, para ouvir
    property OnPoe: TProc<Integer> read FOnPoe write FOnPoe;    // dois cliques: para pôr na linha do tempo
  end;

function HzDaNota(Nota: Integer): Integer;
function NotaDoHz(Hz: Integer): Integer;
function NomeDaNota(Nota: Integer): string;
// Toca no PC as notas (ou um tom só); o que estava tocando para.
procedure TocaNotas(const N: TArray<TNota>; SilencioMs: Integer);
procedure TocaTom(Hz, Ms: Integer);
procedure CalaSom;

implementation

uses
  Winapi.MMSystem, System.Math, Visual, Melodia, Idioma;

const
  NOMES: array[0..11] of string = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B');
  // as teclas do piano vão de D4 a G7, para o D#4 e o F#7 serem pretas entre brancas; as das pontas ficam apagadas
  PIANO_DE = 62;
  PIANO_ATE = 103;

var
  SomAtual: TBytes;  // o WAV que o PlaySound está lendo: vive até o próximo

function HzDaNota(Nota: Integer): Integer;
begin
  Result := Round(440 * Power(2, (Nota - 69) / 12));
end;

function NotaDoHz(Hz: Integer): Integer;
begin
  Result := Round(69 + 12 * Log2(Max(Hz, 1) / 440));
end;

function NomeDaNota(Nota: Integer): string;
begin
  Result := NOMES[Nota mod 12] + IntToStr(Nota div 12 - 1);
end;

function Preta(Nota: Integer): Boolean;
begin
  Result := Nota mod 12 in [1, 3, 6, 8, 10];
end;

procedure CalaSom;
begin
  PlaySound(nil, 0, 0);
end;

procedure TocaNotas(const N: TArray<TNota>; SilencioMs: Integer);
begin
  CalaSom;
  SomAtual := SomDasNotas(N, SilencioMs);
  PlaySound(PChar(@SomAtual[0]), 0, SND_MEMORY or SND_ASYNC or SND_NODEFAULT);
end;

procedure TocaTom(Hz, Ms: Integer);
var
  N: TArray<TNota>;
begin
  SetLength(N, 1);
  N[0].Hz := Hz;
  N[0].Ms := Ms;
  TocaNotas(N, 60);
end;

function E(C: TControl; X: Integer): Integer;
begin
  Result := MulDiv(X, C.CurrentPPI, 96);
end;

{ TLinhaDoTempo }

const
  SILENCIO_DO_TOQUE = 120;  // ms antes do som, para a placa de som acordar

constructor TLinhaDoTempo.Create(AOwner: TComponent);
begin
  inherited;
  ControlStyle := ControlStyle + [csOpaque];
  DoubleBuffered := True;
  TabStop := True;
  FEscolhida := -1;
  FInsercao := -1;
  FCursor := -1;
  DuracaoNova := 100;
  FRelogio := TTimer.Create(Self);
  FRelogio.Enabled := False;
  FRelogio.Interval := 25;
  FRelogio.OnTimer := Anda;
end;

procedure TLinhaDoTempo.WMGetDlgCode(var Message: TWMGetDlgCode);
begin
  Message.Result := DLGC_WANTARROWS or DLGC_WANTCHARS;
end;

function TLinhaDoTempo.X0: Integer;
begin
  Result := E(Self, 38);
end;

function TLinhaDoTempo.X1: Integer;
begin
  Result := ClientWidth - E(Self, 12);
end;

function TLinhaDoTempo.PorMs: Double;
begin
  Result := (X1 - X0) / ROGER_TOTAL_MS;
end;

function TLinhaDoTempo.XDoMs(Ms: Integer): Integer;
begin
  Result := X0 + Round(Ms * PorMs);
end;

// o tom na faixa de cima (as pausas ficam numa faixa embaixo)
function TLinhaDoTempo.YDaNota(Nota: Integer): Integer;
var
  Alto, Baixo: Integer;
begin
  Alto := E(Self, 24);
  Baixo := ClientHeight - E(Self, 34);
  Result := Baixo - Round((EnsureRange(Nota, NOTA_MIN, NOTA_MAX) - NOTA_MIN) * (Baixo - Alto) / (NOTA_MAX - NOTA_MIN));
end;

function TLinhaDoTempo.NotaDoY(Y: Integer): Integer;
var
  Alto, Baixo: Integer;
begin
  Alto := E(Self, 24);
  Baixo := ClientHeight - E(Self, 34);
  Result := EnsureRange(NOTA_MIN + Round((Baixo - Y) * (NOTA_MAX - NOTA_MIN) / Max(1, Baixo - Alto)), NOTA_MIN, NOTA_MAX);
end;

function TLinhaDoTempo.Total: Integer;
var
  N: TNota;
begin
  Result := 0;
  for N in FNotas do
    Inc(Result, N.Ms);
end;

function TLinhaDoTempo.Inicio(K: Integer): Integer;
var
  I: Integer;
begin
  Result := 0;
  for I := 0 to Min(K, Length(FNotas)) - 1 do
    Inc(Result, FNotas[I].Ms);
end;

function TLinhaDoTempo.Bloco(K: Integer): TRect;
var
  Ini, Y: Integer;
begin
  Ini := Inicio(K);
  Result.Left := XDoMs(Ini);
  Result.Right := Max(XDoMs(Ini + FNotas[K].Ms), Result.Left + E(Self, 4));
  if FNotas[K].Hz = 0 then
  begin
    Result.Bottom := ClientHeight - E(Self, 6);
    Result.Top := Result.Bottom - E(Self, 16);
  end
  else
  begin
    Y := YDaNota(NotaDoHz(FNotas[K].Hz));
    Result.Top := Y - E(Self, 6);
    Result.Bottom := Y + E(Self, 6);
  end;
end;

function TLinhaDoTempo.NaPosicao(X, Y: Integer): Integer;
var
  K: Integer;
  R: TRect;
begin
  for K := 0 to High(FNotas) do
  begin
    R := Bloco(K);
    R.Inflate(0, E(Self, 4));
    if R.Contains(Point(X, Y)) then
      Exit(K);
  end;
  Result := -1;
end;

// onde uma nota solta em X entra: antes da primeira cujo meio fica depois de X
function TLinhaDoTempo.IndiceDoX(X: Integer): Integer;
var
  Ms: Double;
  K, Ini: Integer;
begin
  Ms := (X - X0) / PorMs;
  Ini := 0;
  for K := 0 to High(FNotas) do
  begin
    if Ms < Ini + FNotas[K].Ms / 2 then
      Exit(K);
    Inc(Ini, FNotas[K].Ms);
  end;
  Result := Length(FNotas);
end;

procedure TLinhaDoTempo.SetNotas(const Valor: TArray<TNota>);
begin
  FNotas := Copy(Valor);
  if FEscolhida > High(FNotas) then
    FEscolhida := High(FNotas);
  Invalidate;
end;

procedure TLinhaDoTempo.SetEscolhida(Valor: Integer);
begin
  if Valor > High(FNotas) then
    Valor := High(FNotas);
  if Valor <> FEscolhida then
  begin
    FEscolhida := Valor;
    Invalidate;
    if Assigned(FOnEscolheu) then
      FOnEscolheu(Self);
  end;
end;

procedure TLinhaDoTempo.Antes;
begin
  if not FMudouNoArrasto and Assigned(FOnAntes) then
    FOnAntes(Self);
  FMudouNoArrasto := FArrasto <> arNada;
end;

procedure TLinhaDoTempo.Mudou;
begin
  Invalidate;
  if Assigned(FOnMudou) then
    FOnMudou(Self);
end;

procedure TLinhaDoTempo.Troca(A, B: Integer);
var
  N: TNota;
begin
  N := FNotas[A];
  FNotas[A] := FNotas[B];
  FNotas[B] := N;
end;

function TLinhaDoTempo.Insere(K: Integer; N: TNota): Boolean;
begin
  N.Ms := Min(N.Ms, ROGER_TOTAL_MS - Total) div 10 * 10;
  Result := (N.Ms >= 10) and (Length(FNotas) < ROGER_NOTAS);
  if not Result then
    Exit;
  Antes;
  Insert(N, FNotas, EnsureRange(K, 0, Length(FNotas)));
  FEscolhida := EnsureRange(K, 0, High(FNotas));
  Mudou;
  if Assigned(FOnEscolheu) then
    FOnEscolheu(Self);
end;

procedure TLinhaDoTempo.Apaga(K: Integer);
begin
  if (K < 0) or (K > High(FNotas)) then
    Exit;
  Antes;
  Delete(FNotas, K, 1);
  FEscolhida := Min(K, High(FNotas));
  Mudou;
  if Assigned(FOnEscolheu) then
    FOnEscolheu(Self);
end;

procedure TLinhaDoTempo.Toca(DesdeK: Integer);
begin
  if FNotas = nil then
    Exit;
  DesdeK := EnsureRange(DesdeK, 0, High(FNotas));
  TocaNotas(Copy(FNotas, DesdeK, MaxInt), SILENCIO_DO_TOQUE);
  FDesde := Inicio(DesdeK);
  FAte := Total;
  FComecou := GetTickCount64 + SILENCIO_DO_TOQUE;
  FCursor := FDesde;
  FRelogio.Enabled := True;
  Invalidate;
end;

procedure TLinhaDoTempo.Para;
begin
  CalaSom;
  if FRelogio.Enabled then
  begin
    FRelogio.Enabled := False;
    FCursor := -1;
    Invalidate;
    if Assigned(FOnFimDoSom) then
      FOnFimDoSom(Self);
  end;
end;

function TLinhaDoTempo.Tocando: Boolean;
begin
  Result := FRelogio.Enabled;
end;

procedure TLinhaDoTempo.Anda(Sender: TObject);
var
  Agora: Int64;
begin
  Agora := Int64(GetTickCount64) - Int64(FComecou);
  if FDesde + Agora > FAte then
  begin
    FRelogio.Enabled := False;
    FCursor := -1;
    Invalidate;
    if Assigned(FOnFimDoSom) then
      FOnFimDoSom(Self);
    Exit;
  end;
  FCursor := FDesde + Max(0, Agora);
  Invalidate;
end;

procedure TLinhaDoTempo.Paint;
var
  C: TCanvas;
  R: TRect;
  K, Ms, X, Nota: Integer;
  Fundo: TColor;
  Texto: string;
begin
  C := Canvas;
  C.Brush.Color := COR_FUNDO;
  C.FillRect(ClientRect);
  Arredondado(C, ClientRect, E(Self, 10), COR_SUPERFICIE, IfThen(Focused, $00343B2E, COR_DIVISA));
  C.Font.Name := 'Segoe UI';
  C.Font.Height := -E(Self, 10);
  C.Brush.Style := bsClear;
  // as oitavas (os C) com o nome à esquerda
  for Nota := NOTA_MIN to NOTA_MAX do
    if Nota mod 12 = 0 then
    begin
      C.Pen.Color := COR_DIVISA;
      C.MoveTo(X0, YDaNota(Nota));
      C.LineTo(X1, YDaNota(Nota));
      C.Font.Color := COR_TEXTO_FRACO;
      C.TextOut(E(Self, 8), YDaNota(Nota) - C.TextHeight('C') div 2, NomeDaNota(Nota));
    end;
  // o tempo: a cada 100 ms uma risca fraca, a cada 500 o número; o fim dos 3 s em âmbar
  Ms := 0;
  while Ms <= ROGER_TOTAL_MS do
  begin
    X := XDoMs(Ms);
    if Ms mod 500 = 0 then
    begin
      C.Pen.Color := IfThen(Ms = ROGER_TOTAL_MS, COR_AMBAR, $00343B2E);
      C.Font.Color := IfThen(Ms = ROGER_TOTAL_MS, COR_AMBAR, COR_TEXTO_FRACO);
      Texto := FormatFloat('0.#', Ms / 1000) + ' s';
      C.TextOut(EnsureRange(X - C.TextWidth(Texto) div 2, X0, X1 - C.TextWidth(Texto)), E(Self, 4), Texto);
    end
    else
      C.Pen.Color := $00202820;
    C.MoveTo(X, E(Self, 20));
    C.LineTo(X, ClientHeight - E(Self, 4));
    Inc(Ms, 100);
  end;
  if FNotas = nil then
  begin
    C.Font.Color := COR_TEXTO_FRACO;
    C.Font.Height := -E(Self, 12);
    Texto := Tr('Clique numa tecla do piano para ouvir; dois cliques, ou arrastar a tecla até aqui, põem a nota.',
                'Click a piano key to hear it; double-click it, or drag it here, to add the note.');
    C.TextOut((ClientWidth - C.TextWidth(Texto)) div 2, (ClientHeight - C.TextHeight(Texto)) div 2, Texto);
  end;
  // as notas e as pausas
  C.Font.Height := -E(Self, 10);
  for K := 0 to High(FNotas) do
  begin
    R := Bloco(K);
    if FNotas[K].Hz = 0 then
      Fundo := IfThen(K = FEscolhida, $00505A4A, $00343B2E)
    else
      Fundo := IfThen(K = FEscolhida, $00B3E35B, $0080A52A);
    Arredondado(C, R, E(Self, 4), Fundo, IfThen(K = FEscolhida, COR_TEXTO, clNone), IfThen(K = FEscolhida, 2, 1));
    if FNotas[K].Hz = 0 then
      Texto := Tr('pausa', 'pause')
    else
      Texto := NomeDaNota(NotaDoHz(FNotas[K].Hz));
    if R.Width > C.TextWidth(Texto) + E(Self, 6) then
    begin
      C.Font.Color := IfThen(FNotas[K].Hz = 0, COR_TEXTO, $00162205);
      C.TextOut(R.Left + (R.Width - C.TextWidth(Texto)) div 2, R.Top + (R.Height - C.TextHeight(Texto)) div 2, Texto);
    end;
  end;
  // onde a tecla arrastada vai entrar
  if FInsercao >= 0 then
  begin
    X := XDoMs(Inicio(FInsercao));
    C.Pen.Color := COR_VERDE;
    C.Pen.Width := E(Self, 2);
    C.MoveTo(X, E(Self, 20));
    C.LineTo(X, ClientHeight - E(Self, 4));
    C.Pen.Width := 1;
  end;
  // o cursor do som
  if FCursor >= 0 then
  begin
    X := XDoMs(FCursor);
    C.Pen.Color := COR_AMBAR;
    C.Pen.Width := E(Self, 2);
    C.MoveTo(X, E(Self, 18));
    C.LineTo(X, ClientHeight - E(Self, 4));
    C.Pen.Width := 1;
  end;
end;

procedure TLinhaDoTempo.MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
var
  K: Integer;
begin
  inherited;
  if Button <> mbLeft then
    Exit;
  if CanFocus then
    SetFocus;
  K := NaPosicao(X, Y);
  FMudouNoArrasto := False;
  if K < 0 then
  begin
    Escolhida := -1;
    Exit;
  end;
  Escolhida := K;
  if X >= Bloco(K).Right - E(Self, 6) then
    FArrasto := arDuracao
  else
  begin
    FArrasto := arMove;
    if (FNotas[K].Hz > 0) and Assigned(FOnTom) then
      FOnTom(FNotas[K].Hz);
  end;
end;

procedure TLinhaDoTempo.MouseMove(Shift: TShiftState; X, Y: Integer);
var
  K, Ms, Nota: Integer;
  R: TRect;
begin
  inherited;
  K := FEscolhida;
  case FArrasto of
    arDuracao:
      begin
        Ms := Round((X - XDoMs(Inicio(K))) / PorMs / 10) * 10;
        Ms := EnsureRange(Ms, 10, Min(1000, ROGER_TOTAL_MS - (Total - FNotas[K].Ms)));
        if Ms <> FNotas[K].Ms then
        begin
          Antes;
          FNotas[K].Ms := Ms;
          Mudou;
        end;
      end;
    arMove:
      begin
        // para cima e para baixo, o tom (tocando cada um)
        if FNotas[K].Hz > 0 then
        begin
          Nota := NotaDoY(Y);
          if Nota <> NotaDoHz(FNotas[K].Hz) then
          begin
            Antes;
            FNotas[K].Hz := HzDaNota(Nota);
            Mudou;
            if Assigned(FOnTom) then
              FOnTom(FNotas[K].Hz);
          end;
        end;
        // para os lados, a ordem: passando do meio da vizinha, troca com ela
        if (K > 0) and (X < XDoMs(Inicio(K - 1) + FNotas[K - 1].Ms div 2)) then
        begin
          Antes;
          Troca(K, K - 1);
          FEscolhida := K - 1;
          Mudou;
        end
        else if (K < High(FNotas)) and (X > XDoMs(Inicio(K + 1) + FNotas[K + 1].Ms div 2)) then
        begin
          Antes;
          Troca(K, K + 1);
          FEscolhida := K + 1;
          Mudou;
        end;
      end;
  else
    // o cursor diz o que o arrasto faz
    K := NaPosicao(X, Y);
    if K < 0 then
      Cursor := crDefault
    else
    begin
      R := Bloco(K);
      if X >= R.Right - E(Self, 6) then
        Cursor := crSizeWE
      else
        Cursor := crSizeAll;
    end;
  end;
end;

procedure TLinhaDoTempo.MouseUp(Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  inherited;
  FArrasto := arNada;
  FMudouNoArrasto := False;
end;

procedure TLinhaDoTempo.DoExit;
begin
  inherited;
  Invalidate;
end;

procedure TLinhaDoTempo.KeyDown(var Key: Word; Shift: TShiftState);
var
  K, Nota, Passo: Integer;
begin
  inherited;
  K := FEscolhida;
  case Key of
    VK_SPACE:
      if Tocando then
        Para
      else
        Toca(Max(0, K));
    VK_DELETE, VK_BACK:
      Apaga(K);
    VK_LEFT, VK_RIGHT:
      if (ssCtrl in Shift) and (K >= 0) then
      begin
        Passo := IfThen(Key = VK_LEFT, -1, 1);
        if (K + Passo >= 0) and (K + Passo <= High(FNotas)) then
        begin
          Antes;
          Troca(K, K + Passo);
          FEscolhida := K + Passo;
          Mudou;
        end;
      end
      else if FNotas <> nil then
        Escolhida := EnsureRange(K + IfThen(Key = VK_LEFT, -1, 1), 0, High(FNotas));
    VK_UP, VK_DOWN:
      if (K >= 0) and (FNotas[K].Hz > 0) then
      begin
        Nota := EnsureRange(NotaDoHz(FNotas[K].Hz) + IfThen(Key = VK_UP, 1, -1) * IfThen(ssShift in Shift, 12, 1),
                            NOTA_MIN, NOTA_MAX);
        Antes;
        FNotas[K].Hz := HzDaNota(Nota);
        Mudou;
        if Assigned(FOnTom) then
          FOnTom(FNotas[K].Hz);
      end;
    VK_ADD, VK_OEM_PLUS, VK_SUBTRACT, VK_OEM_MINUS:
      if K >= 0 then
      begin
        Passo := IfThen(Key in [VK_ADD, VK_OEM_PLUS], 1, -1) * IfThen(ssShift in Shift, 100, 10);
        Passo := EnsureRange(FNotas[K].Ms + Passo, 10, Min(1000, ROGER_TOTAL_MS - (Total - FNotas[K].Ms)));
        if Passo <> FNotas[K].Ms then
        begin
          Antes;
          FNotas[K].Ms := Passo;
          Mudou;
        end;
      end;
  else
    Exit;
  end;
  Key := 0;
end;

procedure TLinhaDoTempo.DragOver(Source: TObject; X, Y: Integer; State: TDragState; var Accept: Boolean);
begin
  Accept := (Source is TTeclado) and (TTeclado(Source).Arrastada >= 0);
  if Accept and (State <> dsDragLeave) then
    FInsercao := IndiceDoX(X)
  else
    FInsercao := -1;
  Invalidate;
end;

procedure TLinhaDoTempo.DragDrop(Source: TObject; X, Y: Integer);
var
  N: TNota;
begin
  FInsercao := -1;
  if (Source is TTeclado) and (TTeclado(Source).Arrastada >= 0) then
  begin
    N.Hz := HzDaNota(TTeclado(Source).Arrastada);
    N.Ms := DuracaoNova;
    Insere(IndiceDoX(X), N);
  end;
  Invalidate;
end;

{ TTeclado }

constructor TTeclado.Create(AOwner: TComponent);
begin
  inherited;
  ControlStyle := ControlStyle + [csOpaque];
  DoubleBuffered := True;
  FMarcada := -1;
  FApertada := -1;
  FArrastada := -1;
  DragCursor := crDrag;
end;

function TTeclado.Brancas: Integer;
var
  Nota: Integer;
begin
  Result := 0;
  for Nota := PIANO_DE to PIANO_ATE do
    if not Preta(Nota) then
      Inc(Result);
end;

function TTeclado.TeclaBranca(I: Integer): TRect;
var
  L: Double;
begin
  L := ClientWidth / Brancas;
  Result := Rect(Round(I * L), 0, Round((I + 1) * L) - 1, ClientHeight);
end;

function TTeclado.TeclaPreta(Nota: Integer): TRect;
var
  I, N: Integer;
  L: Double;
begin
  // a preta fica sobre a divisa entre a branca de antes e a de depois
  I := 0;
  for N := PIANO_DE to Nota - 1 do
    if not Preta(N) then
      Inc(I);
  L := ClientWidth / Brancas;
  Result := Rect(Round(I * L - L * 0.3), 0, Round(I * L + L * 0.3), Round(ClientHeight * 0.6));
end;

function TTeclado.NotaEm(X, Y: Integer): Integer;
var
  Nota, I: Integer;
begin
  for Nota := PIANO_DE to PIANO_ATE do
    if Preta(Nota) and TeclaPreta(Nota).Contains(Point(X, Y)) then
      Exit(IfThen((Nota >= NOTA_MIN) and (Nota <= NOTA_MAX), Nota, -1));
  I := 0;
  for Nota := PIANO_DE to PIANO_ATE do
    if not Preta(Nota) then
    begin
      if TeclaBranca(I).Contains(Point(X, Y)) then
        Exit(IfThen((Nota >= NOTA_MIN) and (Nota <= NOTA_MAX), Nota, -1));
      Inc(I);
    end;
  Result := -1;
end;

procedure TTeclado.SetMarcada(Valor: Integer);
begin
  if Valor <> FMarcada then
  begin
    FMarcada := Valor;
    Invalidate;
  end;
end;

procedure TTeclado.Paint;
var
  C: TCanvas;
  R: TRect;
  Nota, I: Integer;
  Fora: Boolean;
begin
  C := Canvas;
  C.Brush.Color := COR_FUNDO;
  C.FillRect(ClientRect);
  C.Font.Name := 'Segoe UI';
  C.Font.Height := -E(Self, 9);
  // as brancas, com o nome dos C embaixo
  I := 0;
  for Nota := PIANO_DE to PIANO_ATE do
    if not Preta(Nota) then
    begin
      R := TeclaBranca(I);
      Inc(I);
      Fora := (Nota < NOTA_MIN) or (Nota > NOTA_MAX);
      if Fora then
        C.Brush.Color := $00303530
      else if (Nota = FApertada) or (Nota = FMarcada) then
        C.Brush.Color := COR_VERDE
      else
        C.Brush.Color := $00E8EEE8;
      C.Pen.Color := COR_FUNDO;
      C.RoundRect(R.Left, R.Top - E(Self, 6), R.Right, R.Bottom, E(Self, 6), E(Self, 6));
      if Nota mod 12 = 0 then
      begin
        C.Brush.Style := bsClear;
        C.Font.Color := $00505A4A;
        C.TextOut(R.Left + (R.Width - C.TextWidth(NomeDaNota(Nota))) div 2, R.Bottom - C.TextHeight('C') - E(Self, 3),
                  NomeDaNota(Nota));
        C.Brush.Style := bsSolid;
      end;
    end;
  // as pretas por cima
  for Nota := PIANO_DE to PIANO_ATE do
    if Preta(Nota) then
    begin
      R := TeclaPreta(Nota);
      if (Nota = FApertada) or (Nota = FMarcada) then
        C.Brush.Color := COR_VERDE
      else
        C.Brush.Color := $00181C17;
      C.Pen.Color := COR_FUNDO;
      C.RoundRect(R.Left, R.Top - E(Self, 4), R.Right, R.Bottom, E(Self, 4), E(Self, 4));
    end;
end;

procedure TTeclado.MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
var
  Nota: Integer;
begin
  inherited;
  if Button <> mbLeft then
    Exit;
  Nota := NotaEm(X, Y);
  if Nota < 0 then
    Exit;
  FApertada := Nota;
  FArrastada := Nota;
  Invalidate;
  if (ssDouble in Shift) and Assigned(FOnPoe) then
    FOnPoe(Nota)
  else
  begin
    if Assigned(FOnToca) then
      FOnToca(Nota);
    BeginDrag(False, E(Self, 8));
  end;
end;

procedure TTeclado.MouseUp(Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  inherited;
  FApertada := -1;
  Invalidate;
end;

// a tecla arrastada até a linha do tempo se apaga quando o arrasto acaba (o MouseUp não vem)
procedure TTeclado.DoEndDrag(Target: TObject; X, Y: Integer);
begin
  inherited;
  FApertada := -1;
  Invalidate;
end;

end.
