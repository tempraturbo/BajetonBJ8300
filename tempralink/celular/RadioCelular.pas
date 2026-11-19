unit RadioCelular;

{ O que o app do celular sabe do rádio, fora das telas: o menu (menu-bj8300.txt, no pacote), a memória lida ou aberta
  (o TMemoriaBJ8300, o mesmo do Tempra Link do Windows), por onde se fala com ele (o Bluetooth do aparelho escolhido,
  ou o BJ-8300 simulado da demonstração), a leitura e a gravação numa thread e os backups na pasta do app.

  A memória começa vazia; a leitura cria uma nova e só a põe no lugar quando termina bem, com o backup dela guardado,
  como no Windows. A gravação vai com as regras do rádio (PreparaGravacao) e confere o que gravou. Depois de uma
  sessão, o rádio só a fecha quando param de chegar bytes: o Abre espera 1,5 s antes de deixar o controle remoto
  voltar a falar com ele. }

interface

uses
  System.SysUtils, System.Classes, System.IOUtils, System.Types, System.Generics.Collections,
  System.Generics.Defaults, Transporte, Bluetooth, CaboBJ, BJ8300, Ajustes, Compatibilidade;

type
  TFimTarefa = reference to procedure(Ok: Boolean; const Mensagem: string);

  TRadioCelular = class
  private
    FMenu: TMenu;
    FMemoria: TMemoriaBJ8300;
    FDemo: ITransporte;
    FAparelho: TRadioBLE;
    FEhDemo: Boolean;
    FCabo: TCaboBJ;          // a sessão em curso, para o Cancela
    FFimSessao: UInt64;      // quando a última terminou
    FOcupado: Boolean;
    procedure Tarefa(const Esperar: TFunc<Boolean>; const Progresso: TProc<Integer, Integer>; const Corpo: TProc<TCaboBJ>;
      const Depois: TFunc<Boolean, string, string>; const Fim: TFimTarefa);
  public
    Origem: string;          // de onde veio a memória, para a janela mostrar
    // a leitura ou a gravação parou porque o firmware do rádio não é um com que o app trabalha (CaboBJ.FAIXA); na
    // thread da janela, antes do Fim
    OnIncompativel: TProc<TIncompativel, string>;
    DoArquivo: Boolean;      // veio de um backup: a gravação substitui o que o rádio tem (o Restaurar)
    constructor Create;
    destructor Destroy; override;
    procedure UsaDemonstracao;
    procedure UsaAparelho(const A: TRadioBLE);
    function Nome: string;
    property Demonstracao: Boolean read FEhDemo;
    // Por onde falar com o rádio agora: um Bluetooth novo, ou o simulado de sempre (da thread de quem vai usar).
    function Abre: ITransporte;
    // Numa thread: Esperar (o controle remoto soltar o rádio) e a sessão; Progresso e Fim na thread da janela.
    procedure Le(const Esperar: TFunc<Boolean>; const Progresso: TProc<Integer, Integer>; const Fim: TFimTarefa);
    procedure Grava(const Esperar: TFunc<Boolean>; const Progresso: TProc<Integer, Integer>; const Fim: TFimTarefa);
    procedure Cancela;
    property Ocupado: Boolean read FOcupado;
    property Menu: TMenu read FMenu;
    property Memoria: TMemoriaBJ8300 read FMemoria;
    // os backups, na pasta do app
    function Backups: TArray<string>;   // o mais novo primeiro
    procedure AbreBackup(const Arquivo: string);
    function SalvaBackup: string;
  end;

// Um arquivo que vai no pacote (o .deployproj o põe em assets/internal, e o System.StartUpCopy o copia para a pasta
// do app); no Windows, o da pasta do Tempra Link, para o app rodar no PC nos testes.
function ArquivoDoPacote(const Nome: string): string;

implementation

uses
  RegistroCelular, Demonstracao;

const
  ARQUIVO_MENU = 'menu-bj8300.txt';
  ARQUIVO_RETRATO = 'retrato-demonstracao.bin';
  ARQUIVO_AREAS = 'radio-demonstracao.bin';

function ArquivoDoPacote(const Nome: string): string;
{$IFDEF ANDROID}
begin
  Result := TPath.Combine(TPath.GetDocumentsPath, Nome);
end;
{$ELSE}
var
  Tempralink: string;
begin
  // o .exe fica em tempralink\celular\<plataforma>\<config>
  Tempralink := TPath.GetFullPath(TPath.Combine(ExtractFilePath(ParamStr(0)), '..\..\..'));
  if Nome = ARQUIVO_RETRATO then
    Result := TPath.Combine(Tempralink, 'testes\retrato-1-a-em-evidencia-recebendo.bin')
  else if Nome = ARQUIVO_AREAS then
    Result := TPath.Combine(Tempralink, 'testes\radio.bin')
  else
    Result := TPath.Combine(Tempralink, Nome);
end;
{$ENDIF}

function LeDoPacote(const Nome: string): TBytes;
var
  Arq: string;
begin
  Arq := ArquivoDoPacote(Nome);
  if TFile.Exists(Arq) then
    Result := TFile.ReadAllBytes(Arq)
  else
    Result := nil;
end;

function PastaDosBackups: string;
begin
  Result := TPath.Combine(PastaDoApp, 'backups');
end;

constructor TRadioCelular.Create;
begin
  FMenu := TMenu.Create(TFile.ReadAllText(ArquivoDoPacote(ARQUIVO_MENU), TEncoding.UTF8));
  FMemoria := TMemoriaBJ8300.Create(FMenu);
end;

destructor TRadioCelular.Destroy;
begin
  FMemoria.Free;
  FMenu.Free;
  inherited;
end;

procedure TRadioCelular.UsaDemonstracao;
begin
  FEhDemo := True;
  if FDemo = nil then
    FDemo := TDemonstracao.Create(LeDoPacote(ARQUIVO_RETRATO), LeDoPacote(ARQUIVO_AREAS));
end;

procedure TRadioCelular.UsaAparelho(const A: TRadioBLE);
begin
  FEhDemo := False;
  FAparelho := A;
end;

function TRadioCelular.Nome: string;
begin
  if FEhDemo then
    Result := 'Demonstração, sem rádio'
  else
    Result := 'Bluetooth · ' + FAparelho.Nome;
end;

function TRadioCelular.Abre: ITransporte;
var
  Falta: Int64;
  A: TRadioBLE;
begin
  Falta := Int64(FFimSessao + SESSAO_FECHA_MS) - Int64(TThread.GetTickCount64);
  if (FFimSessao > 0) and (Falta > 0) then
    TThread.Sleep(Falta);
  if FEhDemo then
    Exit(FDemo);
  A := FAparelho;
  Result := TTransporteBLE.Create(A.Aparelho,
    procedure(S: string)
    begin
      Anota(S);
    end);
end;

procedure TRadioCelular.Tarefa(const Esperar: TFunc<Boolean>; const Progresso: TProc<Integer, Integer>;
  const Corpo: TProc<TCaboBJ>; const Depois: TFunc<Boolean, string, string>; const Fim: TFimTarefa);
var
  C: TCaboBJ;
begin
  FOcupado := True;
  C := TCaboBJ.Create;
  C.OnRegistro :=
    procedure(S: string)
    begin
      Anota(S);
    end;
  C.OnProgresso :=
    procedure(Feito, Total: Integer)
    begin
      TThread.Queue(nil,
        procedure
        begin
          Progresso(Feito, Total);
        end);
    end;
  FCabo := C;
  TThread.CreateAnonymousThread(
    procedure
    var
      Ok, Incompativel: Boolean;
      Motivo: TIncompativel;
      Msg: string;
    begin
      Ok := False;
      Incompativel := False;
      Motivo := inFabrica;
      Msg := '';
      try
        if not Esperar() then
          raise EFalha.Create('O controle remoto não soltou a ligação com o rádio.');
        Corpo(C);   // abre o transporte, e ele fecha no fim do Corpo
        Ok := True;
      except
        on E: EIncompativel do
        begin
          Msg := E.Message;
          Incompativel := True;
          Motivo := E.Motivo;
        end;
        on E: Exception do
          Msg := E.Message;
      end;
      FFimSessao := TThread.GetTickCount64;
      TThread.Queue(nil,
        procedure
        begin
          FCabo := nil;
          C.Free;
          FOcupado := False;
          if Incompativel and Assigned(OnIncompativel) then
            OnIncompativel(Motivo, Msg);
          Msg := Depois(Ok, Msg);
          Anota(Msg);
          Fim(Ok, Msg);
        end);
    end).Start;
end;

procedure TRadioCelular.Le(const Esperar: TFunc<Boolean>; const Progresso: TProc<Integer, Integer>;
  const Fim: TFimTarefa);
var
  M: TMemoriaBJ8300;
begin
  if FOcupado then
    Exit;
  Anota('Lendo o rádio (' + Nome + ')...');
  M := TMemoriaBJ8300.Create(FMenu);
  Tarefa(Esperar, Progresso,
    procedure(C: TCaboBJ)
    var
      T: ITransporte;
    begin
      C.Total := BlocosDaLeitura;
      C.Feito := 0;
      T := Abre;
      C.Sessao(T,
        procedure
        begin
          M.LeDoRadio(C);
        end);
    end,
    function(Ok: Boolean; Msg: string): string
    begin
      if not Ok then
      begin
        M.Free;
        Exit('A leitura parou: ' + Msg);
      end;
      FMemoria.Free;
      FMemoria := M;
      DoArquivo := False;
      Origem := 'lido do rádio em ' + FormatDateTime('dd/mm hh:nn', M.Lida);
      try
        SalvaBackup;
        Result := Format('Rádio lido: %d canais salvos. O backup ficou guardado.', [M.Salvos]);
      except
        on E: Exception do
          Result := 'Rádio lido, mas o backup não foi guardado: ' + E.Message;
      end;
    end, Fim);
end;

procedure TRadioCelular.Grava(const Esperar: TFunc<Boolean>; const Progresso: TProc<Integer, Integer>;
  const Fim: TFimTarefa);
var
  M: TMemoriaBJ8300;
  Restaurar, Bancos: Boolean;
begin
  if FOcupado then
    Exit;
  Anota('Gravando no rádio (' + Nome + ')...');
  M := FMemoria;
  Restaurar := DoArquivo;
  Bancos := M.MudouBancos;
  Tarefa(Esperar, Progresso,
    procedure(C: TCaboBJ)
    var
      T: ITransporte;
    begin
      T := Abre;
      C.Sessao(T,
        procedure
        begin
          M.GravaNoRadio(C, Restaurar);
        end);
    end,
    function(Ok: Boolean; Msg: string): string
    begin
      if not Ok then
        Exit('A gravação parou: ' + Msg);
      M.Gravou;
      DoArquivo := False;
      Origem := 'gravado no rádio em ' + FormatDateTime('dd/mm hh:nn', Now);
      if Bancos then
        Result := 'Gravado e conferido. Desligue e ligue o rádio: ele monta os bancos na partida.'
      else
        Result := 'Gravado e conferido.';
    end, Fim);
end;

procedure TRadioCelular.Cancela;
begin
  if FCabo <> nil then
    FCabo.Cancelar := True;
end;

function TRadioCelular.Backups: TArray<string>;
begin
  if not TDirectory.Exists(PastaDosBackups) then
    Exit(nil);
  Result := TDirectory.GetFiles(PastaDosBackups, '*' + EXTENSAO);
  TArray.Sort<string>(Result, TComparer<string>.Construct(
    function(const A, B: string): Integer
    begin
      Result := CompareStr(B, A);   // o nome leva a data: o mais novo primeiro
    end));
end;

procedure TRadioCelular.AbreBackup(const Arquivo: string);
var
  M: TMemoriaBJ8300;
begin
  M := TMemoriaBJ8300.Create(FMenu);
  try
    M.Abre(Arquivo);
  except
    M.Free;
    raise;
  end;
  FMemoria.Free;
  FMemoria := M;
  DoArquivo := True;
  Origem := 'backup ' + TPath.GetFileNameWithoutExtension(Arquivo);
  Anota('Backup aberto: ' + Arquivo);
end;

function TRadioCelular.SalvaBackup: string;
var
  Quando: TDateTime;
begin
  TDirectory.CreateDirectory(PastaDosBackups);
  Quando := FMemoria.Lida;
  if Quando = 0 then
    Quando := Now;
  Result := TPath.Combine(PastaDosBackups, 'BJ8300-' + FormatDateTime('yyyymmdd-hhnnss', Quando) + EXTENSAO);
  FMemoria.Salva(Result);
end;

end.
