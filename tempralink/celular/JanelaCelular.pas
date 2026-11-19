unit JanelaCelular;

{ A janela do Tempra Link no celular. Antes de tudo, a tela de conectar (TelaConectar): o rádio pelo Bluetooth ou a
  demonstração. Depois, quatro abas embaixo:
    Rádio    o controle remoto (TelaRadio), com a chave "Tela do rádio" na barra de cima; ele só fica ligado nesta aba
             (a leitura e a gravação precisam do rádio só para elas), e a tela do celular fica acesa enquanto isso;
    Canais   os canais salvos e os bancos, e cada canal campo a campo;
    Ajustes  as pastas do menu do rádio, com os textos, os VFOs e os códigos DTMF onde o Tempra Link do Windows os põe;
    Mais     ler, gravar, desfazer, os backups, o aviso de recepção, trocar de rádio e o registro.
  As páginas das abas são listas (ListaCelular) montadas de novo a cada mudança; cada aba guarda a pilha dela, e o
  voltar (o da barra ou a tecla do Android) desempilha. Os textos se editam numa caixa do sistema (InputQuery) e as
  opções numa página de escolha. A memória e a conversa com o rádio são do RadioCelular. }

interface

uses
  System.SysUtils, System.Types, System.UITypes, System.Classes, System.IOUtils, System.Math, System.StrUtils,
  System.DateUtils, System.IniFiles, FMX.Types, FMX.Controls, FMX.Forms, FMX.Graphics, FMX.Objects, FMX.Layouts,
  FMX.DialogService, FMX.Media, System.Notification, Transporte, Bluetooth, Remoto, BJ8300, Ajustes, TemaCelular, ModeloRadio,
  ListaCelular, RadioCelular, TelaConectar, TelaRadio, Melodia;

type
  TAba = (abRadio, abCanais, abAjustes, abMais);

  TPagina = record
    Titulo: string;
    Monta: TFunc<TArray<TLinha>>;
  end;

  PCanal = ^TCanal;

  TJanela = class(TForm)
    procedure FormCreate(Sender: TObject);
    procedure FormKeyUp(Sender: TObject; var Key: Word; var KeyChar: Char; Shift: TShiftState);
  private
    FNucleo: TRadioCelular;
    // a barra de cima
    FBarra: TRectangle;
    FVolta: TRectangle;
    FTitulo, FSubtitulo: TText;
    FChave: TRectangle;
    FPilula: TRectangle;
    FBola: TCircle;
    FGravar: TRectangle;
    // o meio e as abas
    FCorpo: TLayout;
    FConectar: TTelaConectar;
    FRadio: TTelaRadio;
    FLista: TListaCelular;
    FNav: TRectangle;
    FGrade: TGridPanelLayout;
    FBotoes: array[TAba] of TRectangle;
    FIcones: array[TAba] of TPath;
    FRotulos: array[TAba] of TText;
    FAba: TAba;
    FNoApp: Boolean;
    FPilhas: array[TAba] of TArray<TPagina>;
    // o véu da leitura e da gravação
    FVeu: TRectangle;
    FVeuTitulo, FVeuTexto: TText;
    FVeuBarra: TRectangle;
    FVeuCheio: TRectangle;
    // o aviso de recepção
    FNotificacoes: TNotificationCenter;
    FAvisa, FAvisoSom: Boolean;
    FAvisoLado: string;      // '' qualquer, ou 'A', 'B', 'C'
    FUltimoRX: Integer;
    FPerguntaAplica: TProc<TArray<string>>;   // o que fazer com a resposta da caixa aberta (só há uma de cada vez)
    FSom: TMediaPlayer;      // o ROGER CUSTOM tocado no celular
    procedure CriaBarra;
    procedure CriaNav;
    procedure CriaVeu;
    procedure MarcaNav;
    procedure NavClick(Sender: TObject);
    procedure MostraConectar;
    procedure EntraNoApp;
    procedure VaiAba(A: TAba);
    procedure Empilha(const Titulo: string; const Monta: TFunc<TArray<TLinha>>);
    procedure Desempilha;
    procedure Atualiza(Topo: Boolean = False);
    procedure Volta(Sender: TObject);
    procedure ChaveClick(Sender: TObject);
    procedure GravarClick(Sender: TObject);
    procedure CancelaClick(Sender: TObject);
    procedure PerguntaFechou(Sender: TObject; const R: TModalResult; const Valores: array of string);
    procedure MarcaChave;
    procedure AoEstado(E: TEstadoRadio);
    // as páginas
    function PaginaVazia: TArray<TLinha>;
    function PaginaCanais: TArray<TLinha>;
    function PaginaCanal(N: Integer): TArray<TLinha>;
    function PaginaBancos: TArray<TLinha>;
    function PaginaBancosDoCanal(N: Integer): TArray<TLinha>;
    function PaginaAjustes: TArray<TLinha>;
    function PaginaPasta(P: TPasta): TArray<TLinha>;
    function PaginaMais: TArray<TLinha>;
    function PaginaBackups: TArray<TLinha>;
    function PaginaRegistro: TArray<TLinha>;
    function PaginaRoger: TArray<TLinha>;
    function PaginaNota(K: Integer): TArray<TLinha>;
    function AcaoNota(K: Integer): TProc;
    procedure PerguntaNota(K: Integer; Inserir: Boolean);
    procedure PoeNotas(const N: TArray<TNota>);
    procedure Toca(const N: TArray<TNota>);
    // as ações (fábricas: cada uma guarda os próprios valores, e não a variável do laço que a montou)
    function AcaoCanal(N: Integer): TProc;
    function AcaoItem(It: TItem): TProc;
    function AcaoPasta(P: TPasta): TProc;
    function AcaoBanco(K: Integer): TProc;
    function AcaoBancoDoCanal(N, K: Integer): TProc;
    function AcaoBackup(const Arquivo: string): TProc;
    function AcaoLinhaRegistro(const S: string): TProc;
    function AcaoTexto(const Nome: string; Maximo: Integer): TProc;
    function AcaoFrequenciaVfo(K: Integer): TProc;
    function AcaoOffsetVfo(K: Integer): TProc;
    function AcaoGrupoDtmf(K: Integer): TProc;
    function AcaoEscolha(V: Integer; const Aplica: TProc<Integer>): TProc;
    procedure Escolhe(const Titulo: string; const Opcoes: TArray<string>; Atual: Integer; const Aplica: TProc<Integer>);
    procedure Pergunta(const Titulo, Atual: string; const Aplica: TProc<string>);
    procedure PerguntaVarios(const Titulo: string; const Rotulos, Atuais: TArray<string>;
      const Aplica: TProc<TArray<string>>);
    procedure Confirma(const Msg: string; const Sim: TProc);
    procedure Mostra(const Msg: string);
    procedure Muda(const Mudanca: TProc);
    procedure MudaCanal(N: Integer; const Mudanca: TProc<PCanal>);
    procedure NovoCanal;
    procedure Ler;
    procedure Gravar;
    procedure Tarefa(const Titulo: string; Ler_: Boolean);
    procedure Descarta(const Depois: TProc);
    procedure Avisa(const E: TEstadoRadio);
    procedure LigaAviso(Liga: Boolean);
    procedure AvisoPronto;
    procedure CanalDeAviso(const Id, Titulo: string; Importancia: TImportance);
    procedure LeConfig;
    procedure GuardaConfig;
  public
    destructor Destroy; override;
  end;

var
  Janela: TJanela;

implementation

{$R *.fmx}

uses
{$IFDEF ANDROID}
  System.Permissions, Androidapi.Helpers, Androidapi.JNI.GraphicsContentViewText, FMX.Helpers.Android,
{$ENDIF}
  RegistroCelular;

const
  ICONE_VOLTA = 'M15.4 4.6 L14 3.2 L5.2 12 L14 20.8 L15.4 19.4 L8 12 Z';
  NOMES_ABAS: array[TAba] of string = ('Rádio', 'Canais', 'Ajustes', 'Mais');
  POTENCIAS: array[TPotencia] of string = ('Alta', 'Média', 'Baixa');
  CANAL_AVISO = 'recebendo';
  CANAL_AVISO_MUDO = 'recebendo-mudo';

// A tela do celular acesa enquanto o controle remoto está aberto. A janela do Android só se mexe na thread de UI do
// Java, que não é a do FMX.
procedure TelaDoCelularAcesa(Liga: Boolean);
begin
{$IFDEF ANDROID}
  CallInUIThread(
    procedure
    begin
      if Liga then
        TAndroidHelper.Activity.getWindow.addFlags(TJWindowManager_LayoutParams.JavaClass.FLAG_KEEP_SCREEN_ON)
      else
        TAndroidHelper.Activity.getWindow.clearFlags(TJWindowManager_LayoutParams.JavaClass.FLAG_KEEP_SCREEN_ON);
    end);
{$ENDIF}
end;

function SimNao(B: Boolean): string;
begin
  if B then
    Result := 'Sim'
  else
    Result := 'Não';
end;

function TextoBancos(B: Word): string;
var
  K: Integer;
begin
  Result := '';
  for K := 0 to BANCOS - 1 do
    if B shr K and 1 <> 0 then
      if Result = '' then
        Result := IntToStr(K + 1)
      else
        Result := Result + ', ' + IntToStr(K + 1);
end;

// "0.6", "5", "5,000" -> kHz (o do Tempra Link do Windows)
function TextoOffset(const S: string; out KHz: Cardinal): Boolean;
var
  F: Cardinal;
begin
  Result := TextoFrequencia(S, F) and (F mod 100 = 0) and (F div 100 < 1000000);
  if Result then
    KHz := F div 100;
end;

// os dígitos que o rádio guarda num código DTMF
function SoDtmf(const S: string; Max: Integer): string;
var
  Ch: Char;
begin
  Result := S.ToUpper;
  for Ch in Result do
    if (Ch = ' ') or (Pos(Ch, DIGITOS_DTMF) = 0) then
      raise EConvertError.Create('O DTMF só tem 0-9, A-D, * e #.');
  if Length(Result) > Max then
    raise EConvertError.CreateFmt('Até %d dígitos.', [Max]);
end;

function TextoOffsetVfo(KHz: Cardinal): string;
begin
  Result := Format('%d.%.3d', [KHz div 1000, KHz mod 1000]);
end;

// O nome do backup que a leitura guarda, BJ8300-aaaammdd-hhnnss, em data.
function DataDoBackup(const Nome: string; out Quando: TDateTime): Boolean;
var
  A, M, D, H, N, S: Integer;
begin
  Result := (Length(Nome) = 22) and Nome.StartsWith('BJ8300-') and
            TryStrToInt(Copy(Nome, 8, 4), A) and TryStrToInt(Copy(Nome, 12, 2), M) and
            TryStrToInt(Copy(Nome, 14, 2), D) and TryStrToInt(Copy(Nome, 17, 2), H) and
            TryStrToInt(Copy(Nome, 19, 2), N) and TryStrToInt(Copy(Nome, 21, 2), S) and
            TryEncodeDateTime(A, M, D, H, N, S, 0, Quando);
end;

{ TJanela }

procedure TJanela.FormCreate(Sender: TObject);
begin
  Fill.Kind := TBrushKind.Solid;
  Fill.Color := COR_FUNDO;
  FNucleo := TRadioCelular.Create;
  FUltimoRX := -1;
  LeConfig;
  CriaBarra;
  CriaNav;
  FCorpo := TLayout.Create(Self);
  FCorpo.Parent := Self;
  FCorpo.Align := TAlignLayout.Client;
  FRadio := TTelaRadio.Create(FCorpo);
  FRadio.OnEstado := AoEstado;
  FRadio.OnCaiu :=
    procedure(Msg: string)
    begin
      FConectar.Avisa(Msg, COR_VERMELHO);
    end;
  FRadio.OnTentaDeNovo :=
    procedure
    begin
      if FNoApp and (FAba = abRadio) then
        FRadio.Liga(FNucleo.Abre, True);
    end;
  FLista := TListaCelular.Create(FCorpo);
  FConectar := TTelaConectar.Create(FCorpo);
  FConectar.OnRadio :=
    procedure(R: TRadioBLE)
    begin
      FNucleo.UsaAparelho(R);
      Anota('Rádio escolhido: ' + R.Nome + ' (' + R.Endereco + ')');
      EntraNoApp;
    end;
  FConectar.OnDemonstracao :=
    procedure
    begin
      FNucleo.UsaDemonstracao;
      Anota('Demonstração');
      EntraNoApp;
    end;
  CriaVeu;
  SetLength(FPilhas[abCanais], 1);
  FPilhas[abCanais][0].Titulo := 'Canais';
  FPilhas[abCanais][0].Monta := PaginaCanais;
  SetLength(FPilhas[abAjustes], 1);
  FPilhas[abAjustes][0].Titulo := 'Ajustes';
  FPilhas[abAjustes][0].Monta := PaginaAjustes;
  SetLength(FPilhas[abMais], 1);
  FPilhas[abMais][0].Titulo := 'Mais';
  FPilhas[abMais][0].Monta := PaginaMais;
  MostraConectar;
end;

destructor TJanela.Destroy;
begin
  // antes dos componentes: o Desliga do TTelaRadio ainda mexe nas teclas dele
  FRadio.Free;
  FConectar.Free;
  FLista.Free;
  FNucleo.Free;
  inherited;
end;

procedure TJanela.CriaBarra;
var
  Textos: TLayout;
  T: TText;
  I: TControl;
begin
  FBarra := Caixa(Self, COR_NAV, 0);
  FBarra.Align := TAlignLayout.Top;
  FBarra.Height := 60;
  FBarra.HitTest := False;

  FVolta := TRectangle.Create(FBarra);
  FVolta.Parent := FBarra;
  FVolta.Align := TAlignLayout.Left;
  FVolta.Width := 52;
  FVolta.Fill.Kind := TBrushKind.None;
  FVolta.Stroke.Kind := TBrushKind.None;
  FVolta.OnClick := Volta;
  I := Icone(FVolta, ICONE_VOLTA, COR_TEXTO);
  I.Align := TAlignLayout.Center;

  // a chave da tela do rádio: o rótulo e a pílula, que acende verde com a tela acesa
  FChave := TRectangle.Create(FBarra);
  FChave.Parent := FBarra;
  FChave.Align := TAlignLayout.Right;
  FChave.Width := 128;
  FChave.Fill.Kind := TBrushKind.None;
  FChave.Stroke.Kind := TBrushKind.None;
  FChave.OnClick := ChaveClick;
  T := Texto(FChave, 'Tela do' + sLineBreak + 'rádio', 11, COR_TEXTO_FRACO, False, TTextAlign.Trailing);
  T.SetBounds(0, 14, 66, 32);
  T.TextSettings.WordWrap := True;
  FPilula := Caixa(FChave, COR_VERDE, 14);
  FPilula.HitTest := False;
  FPilula.SetBounds(72, 16, 46, 28);
  FBola := TCircle.Create(FPilula);
  FBola.Parent := FPilula;
  FBola.HitTest := False;
  FBola.Stroke.Kind := TBrushKind.None;
  FBola.Width := 22;
  FBola.Height := 22;

  // o atalho da gravação, com mudanças por gravar
  FGravar := Caixa(FBarra, COR_AMBAR, 16);
  FGravar.Align := TAlignLayout.Right;
  FGravar.Width := 92;
  FGravar.Margins.Rect := RectF(0, 12, 12, 12);
  FGravar.OnClick := GravarClick;
  T := Texto(FGravar, 'Gravar', 14, COR_NAV, True);
  T.Align := TAlignLayout.Client;

  Textos := TLayout.Create(FBarra);
  Textos.Parent := FBarra;
  Textos.Align := TAlignLayout.Client;
  Textos.HitTest := False;
  FTitulo := Texto(Textos, '', 18, COR_TEXTO, True, TTextAlign.Leading);
  FTitulo.Align := TAlignLayout.Top;
  FTitulo.Height := 26;
  FTitulo.Margins.Top := 8;
  FSubtitulo := Texto(Textos, '', 12, COR_TEXTO_FRACO, False, TTextAlign.Leading);
  FSubtitulo.Align := TAlignLayout.Top;
  FSubtitulo.Position.Y := 100;
  FSubtitulo.Height := 18;
end;

// As abas numa grade de 4 colunas de 25%: ela se distribui sozinha, sem depender de ouvir o tamanho.
procedure TJanela.CriaNav;
const
  ICONES: array[TAba] of string = (ICONE_RADIO, ICONE_CANAIS, ICONE_AJUSTES, ICONE_MAIS);
var
  A: TAba;
  B: TRectangle;
  K: Integer;
  Coluna: TGridPanelLayout.TColumnItem;
  Linha_: TGridPanelLayout.TRowItem;
begin
  FNav := Caixa(Self, COR_NAV, 0);
  FNav.Align := TAlignLayout.Bottom;
  FNav.Height := 62;
  FNav.HitTest := False;
  FGrade := TGridPanelLayout.Create(FNav);
  FGrade.Parent := FNav;
  FGrade.Align := TAlignLayout.Client;
  FGrade.ColumnCollection.BeginUpdate;
  FGrade.ColumnCollection.Clear;
  for K := 1 to 4 do
  begin
    Coluna := FGrade.ColumnCollection.Add;
    Coluna.SizeStyle := TGridPanelLayout.TSizeStyle.Percent;
    Coluna.Value := 25;
  end;
  FGrade.ColumnCollection.EndUpdate;
  FGrade.RowCollection.BeginUpdate;
  FGrade.RowCollection.Clear;
  Linha_ := FGrade.RowCollection.Add;
  Linha_.SizeStyle := TGridPanelLayout.TSizeStyle.Percent;
  Linha_.Value := 100;
  FGrade.RowCollection.EndUpdate;
  for A := Low(TAba) to High(TAba) do
  begin
    B := TRectangle.Create(FGrade);
    B.Parent := FGrade;
    B.Align := TAlignLayout.Client;
    B.Fill.Kind := TBrushKind.None;
    B.Stroke.Kind := TBrushKind.None;
    B.Tag := Ord(A);
    B.OnClick := NavClick;
    FIcones[A] := Icone(B, ICONES[A], COR_TEXTO_FRACO);
    FIcones[A].Align := TAlignLayout.Top;   // o Fit do caminho o centra na largura
    FIcones[A].Height := 24;
    FIcones[A].Margins.Top := 10;
    FRotulos[A] := Texto(B, NOMES_ABAS[A], 11, COR_TEXTO_FRACO);
    FRotulos[A].Align := TAlignLayout.Bottom;
    FRotulos[A].Height := 18;
    FRotulos[A].Margins.Bottom := 8;
    FBotoes[A] := B;
  end;
end;

procedure TJanela.MarcaNav;
var
  A: TAba;
  Cor: TAlphaColor;
begin
  for A := Low(TAba) to High(TAba) do
  begin
    if A = FAba then
      Cor := COR_VERDE
    else
      Cor := COR_TEXTO_FRACO;
    FIcones[A].Fill.Color := Cor;
    FRotulos[A].TextSettings.FontColor := Cor;
  end;
end;

procedure TJanela.CriaVeu;
var
  Cartao, Cancela: TRectangle;
  T: TText;
begin
  FVeu := TRectangle.Create(Self);
  FVeu.Parent := Self;
  FVeu.Align := TAlignLayout.Contents;
  FVeu.Fill.Color := $C0000000;
  FVeu.Stroke.Kind := TBrushKind.None;
  FVeu.Visible := False;
  Cartao := Caixa(FVeu, COR_SUPERFICIE, 18, COR_DIVISA);
  Cartao.Align := TAlignLayout.VertCenter;
  Cartao.Height := 190;
  Cartao.Margins.Rect := RectF(24, 0, 24, 0);
  Cartao.Padding.Rect := RectF(20, 18, 20, 18);
  FVeuTitulo := Texto(Cartao, '', 18, COR_TEXTO, True, TTextAlign.Leading);
  FVeuTitulo.Align := TAlignLayout.Top;
  FVeuTitulo.Height := 28;
  FVeuTexto := Texto(Cartao, '', 13, COR_TEXTO_FRACO, False, TTextAlign.Leading);
  FVeuTexto.Align := TAlignLayout.Top;
  FVeuTexto.Position.Y := 100;
  FVeuTexto.Height := 40;
  FVeuTexto.TextSettings.WordWrap := True;
  FVeuBarra := Caixa(Cartao, COR_SUPERFICIE_ALTA, 5);
  FVeuBarra.Align := TAlignLayout.Top;
  FVeuBarra.Position.Y := 200;
  FVeuBarra.Height := 10;
  FVeuBarra.Margins.Top := 6;
  FVeuCheio := Caixa(FVeuBarra, COR_VERDE, 5);
  FVeuCheio.Align := TAlignLayout.Left;
  FVeuCheio.Width := 0;
  Cancela := Caixa(Cartao, COR_SUPERFICIE_ALTA, 14, COR_DIVISA);
  Cancela.Align := TAlignLayout.Bottom;
  Cancela.Height := 44;
  Cancela.OnClick := CancelaClick;
  T := Texto(Cancela, 'Cancelar', 15, COR_TEXTO, True);
  T.Align := TAlignLayout.Client;
end;

{ A navegação }

procedure TJanela.MostraConectar;
begin
  FRadio.Desliga;
  TelaDoCelularAcesa(False);
  FNoApp := False;
  FRadio.Raiz.Visible := False;
  FLista.Raiz.Visible := False;
  FNav.Visible := False;
  FConectar.Raiz.Visible := True;
  FVolta.Visible := False;
  FChave.Visible := False;
  FGravar.Visible := False;
  FTitulo.Text := 'Tempra Link';
  FTitulo.Margins.Left := 16;
  FSubtitulo.Text := RADIO_MARCA + ' ' + RADIO_NOME;
  FSubtitulo.Margins.Left := 16;
end;

procedure TJanela.EntraNoApp;
begin
  FNoApp := True;
  FConectar.Raiz.Visible := False;
  FNav.Visible := True;
  FTitulo.Margins.Left := 0;
  FSubtitulo.Margins.Left := 0;
  FAba := abMais;   // para a troca abaixo ligar o controle remoto
  VaiAba(abRadio);
end;

procedure TJanela.NavClick(Sender: TObject);
begin
  if not FVeu.Visible then
    VaiAba(TAba((Sender as TFmxObject).Tag));
end;

procedure TJanela.VaiAba(A: TAba);
begin
  if (A = FAba) and (Length(FPilhas[A]) > 1) then
    SetLength(FPilhas[A], 1);   // a aba de novo: volta ao começo dela
  if A <> FAba then
  begin
    if FAba = abRadio then
    begin
      FRadio.Desliga;
      TelaDoCelularAcesa(False);
    end;
    FAba := A;
    if A = abRadio then
    begin
      FUltimoRX := -1;
      FRadio.Liga(FNucleo.Abre, True);
      TelaDoCelularAcesa(True);
    end;
  end;
  FRadio.Raiz.Visible := A = abRadio;
  FLista.Raiz.Visible := A <> abRadio;
  MarcaNav;
  Atualiza(True);
  // escondida, a frente do rádio não ouve o tamanho: arruma depois de a janela alinhar o que apareceu
  TThread.ForceQueue(nil,
    procedure
    begin
      FRadio.Rearruma;
    end);
end;

procedure TJanela.Empilha(const Titulo: string; const Monta: TFunc<TArray<TLinha>>);
var
  P: TPagina;
begin
  P.Titulo := Titulo;
  P.Monta := Monta;
  FPilhas[FAba] := FPilhas[FAba] + [P];
  Atualiza(True);
end;

procedure TJanela.Desempilha;
begin
  if Length(FPilhas[FAba]) > 1 then
  begin
    SetLength(FPilhas[FAba], Length(FPilhas[FAba]) - 1);
    Atualiza(True);
  end;
end;

// A barra e a página de agora, montadas de novo (depois de uma mudança, sem mexer na rolagem).
procedure TJanela.Atualiza(Topo: Boolean);
var
  P: TPagina;
begin
  if not FNoApp then
    Exit;
  FVolta.Visible := True;
  FChave.Visible := FAba = abRadio;
  FGravar.Visible := (FAba <> abRadio) and FNucleo.Memoria.Mudou;
  if FAba = abRadio then
  begin
    FTitulo.Text := RADIO_MARCA + ' ' + RADIO_NOME;
    FSubtitulo.Text := FNucleo.Nome;
    MarcaChave;
    Exit;
  end;
  P := FPilhas[FAba][High(FPilhas[FAba])];
  FTitulo.Text := P.Titulo;
  if FNucleo.Memoria.Vazia then
    FSubtitulo.Text := FNucleo.Nome + ' · nada lido ainda'
  else if FNucleo.Memoria.Mudou then
    FSubtitulo.Text := FNucleo.Origem + ' · com mudanças'
  else
    FSubtitulo.Text := FNucleo.Origem;
  FLista.Mostra(P.Monta(), Topo);
end;

procedure TJanela.Volta(Sender: TObject);
begin
  if FVeu.Visible then
    Exit;
  if FAba <> abRadio then
  begin
    if Length(FPilhas[FAba]) > 1 then
      Desempilha
    else
      VaiAba(abRadio);
  end
  else
    MostraConectar;
end;

procedure TJanela.FormKeyUp(Sender: TObject; var Key: Word; var KeyChar: Char; Shift: TShiftState);
begin
  if (Key = vkHardwareBack) and FNoApp then
  begin
    Key := 0;
    Volta(nil);
  end;
end;

{ O controle remoto }

procedure TJanela.ChaveClick(Sender: TObject);
begin
  FRadio.ApagaTela(not FRadio.TelaApagada);
  MarcaChave;
end;

procedure TJanela.MarcaChave;
begin
  if FRadio.TelaApagada then
  begin
    FPilula.Fill.Color := COR_SUPERFICIE_ALTA;
    FBola.Fill.Color := COR_TEXTO_FRACO;
    FBola.Position.Point := PointF(3, 3);
  end
  else
  begin
    FPilula.Fill.Color := COR_VERDE;
    FBola.Fill.Color := TAlphaColors.White;
    FBola.Position.Point := PointF(FPilula.Width - 25, 3);
  end;
end;

procedure TJanela.AoEstado(E: TEstadoRadio);
begin
  MarcaChave;
  if (E.RX = 4) and (FUltimoRX >= 0) and (FUltimoRX <> 4) and FAvisa and
     ((FAvisoLado = '') or (FAvisoLado = E.Letra)) then
    Avisa(E);
  FUltimoRX := E.RX;
end;

{ O aviso de recepção: uma notificação quando o rádio começa a receber (no lado escolhido, ou em qualquer), com o
  controle remoto aberto. Com o som, o canal de notificação de importância alta (o som e o aviso no alto da tela); sem,
  o de importância baixa. }

procedure TJanela.Avisa(const E: TEstadoRadio);
var
  N: TNotification;
begin
  if FNotificacoes = nil then
    Exit;
  N := FNotificacoes.CreateNotification;
  try
    N.Name := CANAL_AVISO;
    N.Title := Format('%s recebendo no %s', [RADIO_NOME, E.Letra]);
    N.AlertBody := 'Sinal ' + TextoSinal(E.Sinal);
    N.EnableSound := FAvisoSom;
    if FAvisoSom then
      N.ChannelId := CANAL_AVISO
    else
      N.ChannelId := CANAL_AVISO_MUDO;
    FNotificacoes.PresentNotification(N);
  finally
    N.Free;
  end;
  Anota('Aviso de recepção: ' + E.Letra + ', ' + TextoSinal(E.Sinal));
end;

procedure TJanela.CanalDeAviso(const Id, Titulo: string; Importancia: TImportance);
var
  C: TChannel;
begin
  C := FNotificacoes.CreateChannel;
  try
    C.Id := Id;
    C.Title := Titulo;
    C.Importance := Importancia;
    FNotificacoes.CreateOrUpdateChannel(C);
  finally
    C.Free;
  end;
end;

procedure TJanela.AvisoPronto;
begin
  if FNotificacoes = nil then
  begin
    FNotificacoes := TNotificationCenter.Create(Self);
    CanalDeAviso(CANAL_AVISO, 'Rádio recebendo', TImportance.High);
    CanalDeAviso(CANAL_AVISO_MUDO, 'Rádio recebendo, sem som', TImportance.Low);
  end;
  FAvisa := True;
  Atualiza;
end;

procedure TJanela.LigaAviso(Liga: Boolean);
{$IFDEF ANDROID}
var
  P: TArray<string>;
{$ENDIF}
begin
  if not Liga then
  begin
    FAvisa := False;
    Atualiza;
    Exit;
  end;
{$IFDEF ANDROID}
  if TOSVersion.Check(13) then
  begin
    P := ['android.permission.POST_NOTIFICATIONS'];
    PermissionsService.RequestPermissions(P,
      procedure(const Permissoes: TClassicStringDynArray; const Respostas: TClassicPermissionStatusDynArray)
      begin
        if (Length(Respostas) = 1) and (Respostas[0] = TPermissionStatus.Granted) then
          AvisoPronto
        else
          Mostra('Sem a permissão de notificações o aviso não aparece. Ela se dá em Configurações › Apps › ' +
                 'Tempra Link › Notificações.');
      end);
    Exit;
  end;
{$ENDIF}
  AvisoPronto;
end;

// O lado e o som do aviso ficam de uma vez para a outra; o aviso começa desligado (ligá-lo pede a permissão).
procedure TJanela.LeConfig;
var
  Ini: TMemIniFile;
begin
  Ini := TMemIniFile.Create(System.IOUtils.TPath.Combine(PastaDoApp, 'tempralink.ini'), TEncoding.UTF8);
  try
    FAvisoSom := Ini.ReadBool('aviso', 'som', True);
    FAvisoLado := Ini.ReadString('aviso', 'lado', '');
  finally
    Ini.Free;
  end;
end;

procedure TJanela.GuardaConfig;
var
  Ini: TMemIniFile;
begin
  try
    Ini := TMemIniFile.Create(System.IOUtils.TPath.Combine(PastaDoApp, 'tempralink.ini'), TEncoding.UTF8);
    try
      Ini.WriteBool('aviso', 'som', FAvisoSom);
      Ini.WriteString('aviso', 'lado', FAvisoLado);
      Ini.UpdateFile;
    finally
      Ini.Free;
    end;
  except
    on E: Exception do
      Anota('A configuração não foi guardada: ' + E.Message);
  end;
end;

{ As caixas do sistema }

procedure TJanela.Mostra(const Msg: string);
begin
  TDialogService.ShowMessage(Msg);
end;

procedure TJanela.Confirma(const Msg: string; const Sim: TProc);
begin
  TDialogService.MessageDialog(Msg, TMsgDlgType.mtConfirmation, [TMsgDlgBtn.mbYes, TMsgDlgBtn.mbNo],
    TMsgDlgBtn.mbNo, 0,
    procedure(const R: TModalResult)
    begin
      if R = mrYes then
        Sim();
    end);
end;

// A caixa de texto do sistema. A resposta vem pelo evento de método: o Delphi não casa um método anônimo com o array
// aberto do TInputCloseQueryProc.
procedure TJanela.Pergunta(const Titulo, Atual: string; const Aplica: TProc<string>);
begin
  PerguntaVarios(Titulo, [''], [Atual],
    procedure(V: TArray<string>)
    begin
      Aplica(V[0]);
    end);
end;

procedure TJanela.PerguntaVarios(const Titulo: string; const Rotulos, Atuais: TArray<string>;
  const Aplica: TProc<TArray<string>>);
begin
  FPerguntaAplica := Aplica;
  TDialogService.InputQuery(Titulo, Rotulos, Atuais, PerguntaFechou);
end;

procedure TJanela.PerguntaFechou(Sender: TObject; const R: TModalResult; const Valores: array of string);
var
  Aplica: TProc<TArray<string>>;
  V: TArray<string>;
  K: Integer;
begin
  Aplica := FPerguntaAplica;
  FPerguntaAplica := nil;
  if (R <> mrOk) or not Assigned(Aplica) then
    Exit;
  SetLength(V, Length(Valores));
  for K := 0 to High(Valores) do
    V[K] := Valores[K];
  Aplica(V);
end;

// Uma mudança na memória, com o desfazer guardado antes; a que não serve (EConvertError) volta e se explica.
procedure TJanela.Muda(const Mudanca: TProc);
begin
  FNucleo.Memoria.Guarda;
  try
    Mudanca();
  except
    on E: EConvertError do
    begin
      FNucleo.Memoria.Desfaz;
      Mostra(E.Message);
    end;
  end;
  Atualiza;
end;

procedure TJanela.MudaCanal(N: Integer; const Mudanca: TProc<PCanal>);
begin
  Muda(
    procedure
    var
      C: TCanal;
    begin
      C := FNucleo.Memoria.Canal(N);
      Mudanca(@C);
      FNucleo.Memoria.PoeCanal(C);
    end);
end;

function TJanela.AcaoEscolha(V: Integer; const Aplica: TProc<Integer>): TProc;
begin
  Result :=
    procedure
    begin
      Desempilha;
      Aplica(V);
    end;
end;

// A página de escolha: as opções, a de agora marcada; um toque aplica e volta.
procedure TJanela.Escolhe(const Titulo: string; const Opcoes: TArray<string>; Atual: Integer;
  const Aplica: TProc<Integer>);
var
  Linhas: TArray<TLinha>;
  K: Integer;
begin
  SetLength(Linhas, Length(Opcoes));
  for K := 0 to High(Opcoes) do
  begin
    Linhas[K] := Linha(Opcoes[K], '', IfThen(K = Atual, '✓', ''), AcaoEscolha(K, Aplica));
    if K = Atual then
      Linhas[K].Cor := COR_VERDE;
  end;
  Empilha(Titulo,
    function: TArray<TLinha>
    begin
      Result := Linhas;
    end);
end;

{ As páginas }

function TJanela.PaginaVazia: TArray<TLinha>;
begin
  Result := [Secao('Nada lido ainda'),
             Linha('Ler do rádio', 'os canais, os bancos e os ajustes, pelo ' + IfThen(FNucleo.Demonstracao,
                   'rádio simulado', 'Bluetooth'), '', Ler),
             Linha('Abrir um backup', 'um dos guardados neste celular', '',
                   procedure
                   begin
                     Empilha('Backups', PaginaBackups);
                   end)];
end;

function TJanela.AcaoCanal(N: Integer): TProc;
begin
  Result :=
    procedure
    begin
      Empilha(Format('Canal %d', [N]),
        function: TArray<TLinha>
        begin
          Result := PaginaCanal(N);
        end);
    end;
end;

function TJanela.PaginaCanais: TArray<TLinha>;
var
  M: TMemoriaBJ8300;
  N, Ligados, K: Integer;
  C: TCanal;
  D: string;
begin
  M := FNucleo.Memoria;
  if M.Vazia then
    Exit(PaginaVazia);
  Ligados := 0;
  for K := 0 to BANCOS - 1 do
    if M.BancoAtivo(K) then
      Inc(Ligados);
  Result := [Linha('Novo canal', 'no primeiro número livre', '+', NovoCanal),
             Linha('Bancos', Format('%d de %d ligados', [Ligados, BANCOS]), '',
                   procedure
                   begin
                     Empilha('Bancos', PaginaBancos);
                   end),
             Secao(Format('%d canais salvos', [M.Salvos]))];
  for N := 1 to CANAIS do
  begin
    C := M.Canal(N);
    if not C.Existe then
      Continue;
    if C.TX = 0 then
      D := 'sem TX'
    else if C.TX = C.RX then
      D := 'simplex'
    else
      D := 'TX ' + FrequenciaTexto(C.TX);
    if C.TomTX <> 0 then
      D := D + ' · tom ' + TomTexto(C.TomTX);
    D := D + ' · ' + POTENCIAS[C.Potencia];
    if C.Estreito then
      D := D + ' · estreito';
    if C.AM then
      D := D + ' · AM';
    if C.Bancos <> 0 then
      D := D + ' · banco ' + TextoBancos(C.Bancos);
    Result := Result + [Linha(Format('%d   %s', [N, C.Nome]), D, FrequenciaTexto(C.RX), AcaoCanal(N))];
  end;
end;

procedure TJanela.NovoCanal;
var
  N: Integer;
begin
  N := 1;
  while (N <= CANAIS) and FNucleo.Memoria.Canal(N).Existe do
    Inc(N);
  if N > CANAIS then
  begin
    Mostra('Os 960 canais estão ocupados.');
    Exit;
  end;
  Pergunta(Format('Canal %d: a frequência de RX, em MHz', [N]), '',
    procedure(V: string)
    var
      F: Cardinal;
    begin
      if not TextoFrequencia(V, F) or not FaixaRX(F) then
      begin
        Mostra('A frequência de RX vai em MHz, de 18 a 1000.');
        Exit;
      end;
      Muda(
        procedure
        var
          C: TCanal;
        begin
          // um canal novo, como o SAVE do rádio o grava: alta, larga, na varredura, sem tom e sem banco
          C := Default(TCanal);
          C.Numero := N;
          C.Varre := True;
          C.RX := F;
          C.TX := F;
          FNucleo.Memoria.PoeCanal(C);
        end);
      AcaoCanal(N)();
    end);
end;

function TJanela.PaginaCanal(N: Integer): TArray<TLinha>;
var
  C: TCanal;
  K, ScrambleAtual: Integer;
  Tons, TextosScramble, Grupos, PttIds: TArray<string>;
begin
  C := FNucleo.Memoria.Canal(N);
  if not C.Existe then
  begin
    Result := [Secao('Canal apagado')];
    Exit;
  end;
  Tons := ListaDeTons;
  SetLength(TextosScramble, Length(SCRAMBLES));
  ScrambleAtual := 0;
  for K := 0 to High(SCRAMBLES) do
  begin
    TextosScramble[K] := TextoScramble(SCRAMBLES[K]);
    if SCRAMBLES[K] = C.Scramble then
      ScrambleAtual := K;
  end;
  PttIds := ['OFF', 'BOT', 'EOT', 'BOTH'];
  SetLength(Grupos, GRUPOS_DTMF);
  for K := 0 to GRUPOS_DTMF - 1 do
    Grupos[K] := IntToStr(K + 1);
  Result := [
    Linha('Nome', '', IfThen(C.Nome = '', '—', C.Nome),
      procedure
      begin
        Pergunta('Nome do canal (até 12)', FNucleo.Memoria.Canal(N).Nome,
          procedure(V: string)
          begin
            MudaCanal(N, procedure(P: PCanal) begin P.Nome := NomeValido(UpperCase(V)); end);
          end);
      end),
    Linha('Frequência de RX', '', FrequenciaTexto(C.RX),
      procedure
      begin
        Pergunta('RX, em MHz', FrequenciaTexto(FNucleo.Memoria.Canal(N).RX),
          procedure(V: string)
          begin
            MudaCanal(N,
              procedure(P: PCanal)
              var
                F: Cardinal;
              begin
                if not TextoFrequencia(V, F) or not FaixaRX(F) then
                  raise EConvertError.Create('A frequência de RX vai em MHz, de 18 a 1000.');
                if P.TX = P.RX then
                  P.TX := F;
                P.RX := F;
              end);
          end);
      end),
    Linha('Frequência de TX', 'vazia: o canal fica sem TX', IfThen(C.TX = 0, 'sem TX', FrequenciaTexto(C.TX)),
      procedure
      var
        Atual: Cardinal;
      begin
        Atual := FNucleo.Memoria.Canal(N).TX;
        Pergunta('TX, em MHz (vazia: sem TX)', IfThen(Atual = 0, '', FrequenciaTexto(Atual)),
          procedure(V: string)
          begin
            MudaCanal(N,
              procedure(P: PCanal)
              var
                F: Cardinal;
              begin
                if (V.Trim = '') or (V.Trim = '-') then
                  P.TX := 0
                else if TextoFrequencia(V, F) and FaixaRX(F) then
                  P.TX := F
                else
                  raise EConvertError.Create('A frequência de TX vai em MHz, de 18 a 1000; vazia, o canal fica ' +
                                             'sem TX.');
              end);
          end);
      end),
    Linha('Tom de RX', 'CTCSS ou DCS', TomTexto(C.TomRX),
      procedure
      begin
        Escolhe('Tom de RX', Tons, Max(0, IndexStr(TomTexto(FNucleo.Memoria.Canal(N).TomRX), Tons)),
          procedure(V: Integer)
          var
            T: Word;
          begin
            if TomDeTexto(Tons[V], T) then
              MudaCanal(N, procedure(P: PCanal) begin P.TomRX := T; end);
          end);
      end),
    Linha('Tom de TX', 'CTCSS ou DCS', TomTexto(C.TomTX),
      procedure
      begin
        Escolhe('Tom de TX', Tons, Max(0, IndexStr(TomTexto(FNucleo.Memoria.Canal(N).TomTX), Tons)),
          procedure(V: Integer)
          var
            T: Word;
          begin
            if TomDeTexto(Tons[V], T) then
              MudaCanal(N, procedure(P: PCanal) begin P.TomTX := T; end);
          end);
      end),
    Linha('Potência', '', POTENCIAS[C.Potencia],
      procedure
      begin
        Escolhe('Potência', ['Alta', 'Média', 'Baixa'], Ord(FNucleo.Memoria.Canal(N).Potencia),
          procedure(V: Integer)
          begin
            MudaCanal(N, procedure(P: PCanal) begin P.Potencia := TPotencia(V); end);
          end);
      end),
    Linha('Largura', 'um toque troca', IfThen(C.Estreito, 'Estreita', 'Larga'),
      procedure
      begin
        MudaCanal(N, procedure(P: PCanal) begin P.Estreito := not P.Estreito; end);
      end),
    Linha('Modulação', 'um toque troca', IfThen(C.AM, 'AM', 'FM'),
      procedure
      begin
        MudaCanal(N, procedure(P: PCanal) begin P.AM := not P.AM; end);
      end),
    Linha('Na varredura', 'um toque troca', SimNao(C.Varre),
      procedure
      begin
        MudaCanal(N, procedure(P: PCanal) begin P.Varre := not P.Varre; end);
      end),
    Linha('BCL', 'não transmite com o canal ocupado', SimNao(C.BCL),
      procedure
      begin
        MudaCanal(N, procedure(P: PCanal) begin P.BCL := not P.BCL; end);
      end),
    Linha('Scramble', '', TextoScramble(C.Scramble),
      procedure
      begin
        Escolhe('Scramble', TextosScramble, ScrambleAtual,
          procedure(V: Integer)
          begin
            MudaCanal(N, procedure(P: PCanal) begin P.Scramble := SCRAMBLES[V]; end);
          end);
      end),
    Linha('ENCRYPT', '', TextoEncrypt(C.Encrypt),
      procedure
      begin
        Escolhe('ENCRYPT', [TextoEncrypt(0), TextoEncrypt(1), TextoEncrypt(2), TextoEncrypt(3)],
          FNucleo.Memoria.Canal(N).Encrypt,
          procedure(V: Integer)
          begin
            MudaCanal(N, procedure(P: PCanal) begin P.Encrypt := V; end);
          end);
      end),
    Linha('Grupo DTMF', '', IntToStr(C.Scode + 1),
      procedure
      begin
        Escolhe('Grupo DTMF', Grupos, FNucleo.Memoria.Canal(N).Scode,
          procedure(V: Integer)
          begin
            MudaCanal(N, procedure(P: PCanal) begin P.Scode := V; end);
          end);
      end),
    Linha('PTT-ID', '', PttIds[C.PttId and 3],
      procedure
      begin
        Escolhe('PTT-ID', PttIds, FNucleo.Memoria.Canal(N).PttId and 3,
          procedure(V: Integer)
          begin
            MudaCanal(N, procedure(P: PCanal) begin P.PttId := V; end);
          end);
      end),
    Linha('Bancos', 'em quais dos 16 o canal entra', IfThen(C.Bancos = 0, 'nenhum', TextoBancos(C.Bancos)),
      procedure
      begin
        Empilha(Format('Canal %d: bancos', [N]),
          function: TArray<TLinha>
          begin
            Result := PaginaBancosDoCanal(N);
          end);
      end),
    Secao(''),
    Linha('Apagar o canal', '', '',
      procedure
      begin
        Confirma(Format('Apagar o canal %d?', [N]),
          procedure
          begin
            Desempilha;
            Muda(procedure begin FNucleo.Memoria.ApagaCanal(N); end);
          end);
      end)];
  Result[High(Result)].Cor := COR_VERMELHO;
end;

function TJanela.AcaoBancoDoCanal(N, K: Integer): TProc;
begin
  Result :=
    procedure
    begin
      MudaCanal(N, procedure(P: PCanal) begin P.Bancos := P.Bancos xor (1 shl K); end);
    end;
end;

function TJanela.PaginaBancosDoCanal(N: Integer): TArray<TLinha>;
var
  K: Integer;
  B: Word;
  Nome: string;
begin
  B := FNucleo.Memoria.Canal(N).Bancos;
  Result := [Secao('Toque para pôr ou tirar')];
  for K := 0 to BANCOS - 1 do
  begin
    Nome := FNucleo.Memoria.BancoNome(K);
    if Nome = '' then
      Nome := Format('BANK %.2d', [K + 1]);
    Result := Result + [Linha(Format('%d   %s', [K + 1, Nome]), '', IfThen(B shr K and 1 <> 0, '✓', ''),
                              AcaoBancoDoCanal(N, K))];
  end;
end;

function TJanela.AcaoBanco(K: Integer): TProc;
begin
  Result :=
    procedure
    begin
      Empilha(Format('Banco %d', [K + 1]),
        function: TArray<TLinha>
        var
          Nome: string;
        begin
          Nome := FNucleo.Memoria.BancoNome(K);
          Result := [
            Linha('Nome', 'vazio: o de fábrica, BANK nn', IfThen(Nome = '', Format('BANK %.2d', [K + 1]), Nome),
              procedure
              begin
                Pergunta(Format('Nome do banco %d (até %d)', [K + 1, BANCO_NOME]), FNucleo.Memoria.BancoNome(K),
                  procedure(V: string)
                  begin
                    Muda(procedure begin FNucleo.Memoria.PoeBancoNome(K, NomeValido(UpperCase(V))); end);
                  end);
              end),
            Linha('Ligado', 'na lista do BANKS > ACTIVE do rádio; um toque troca',
                  SimNao(FNucleo.Memoria.BancoAtivo(K)),
              procedure
              begin
                Muda(procedure begin FNucleo.Memoria.PoeBancoAtivo(K, not FNucleo.Memoria.BancoAtivo(K)); end);
              end),
            Linha('Canais nele', 'eles se põem no banco pela página de cada canal',
                  IntToStr(FNucleo.Memoria.CanaisNoBanco(K)))];
        end);
    end;
end;

function TJanela.PaginaBancos: TArray<TLinha>;
var
  K: Integer;
  Nome: string;
  M: TMemoriaBJ8300;
begin
  M := FNucleo.Memoria;
  Result := nil;
  for K := 0 to BANCOS - 1 do
  begin
    Nome := M.BancoNome(K);
    if Nome = '' then
      Nome := Format('BANK %.2d', [K + 1]);
    Result := Result + [Linha(Format('%d   %s', [K + 1, Nome]), Format('%d canais', [M.CanaisNoBanco(K)]),
                              IfThen(M.BancoAtivo(K), 'ligado', 'desligado'), AcaoBanco(K))];
    if not M.BancoAtivo(K) then
      Result[High(Result)].CorDireita := COR_TEXTO_FRACO;
  end;
end;

{ Os ajustes }

function TJanela.AcaoPasta(P: TPasta): TProc;
begin
  Result :=
    procedure
    begin
      Empilha(P.Nome,
        function: TArray<TLinha>
        begin
          Result := PaginaPasta(P);
        end);
    end;
end;

function TJanela.PaginaAjustes: TArray<TLinha>;
var
  P: TPasta;
begin
  if FNucleo.Memoria.Vazia then
    Exit(PaginaVazia);
  Result := [Secao('O menu do rádio')];
  for P in FNucleo.Menu.Pastas do
    Result := Result + [Linha(P.Nome, Format('%d itens', [P.Itens.Count]), '', AcaoPasta(P))];
end;

function TJanela.AcaoItem(It: TItem): TProc;
begin
  Result :=
    procedure
    var
      Opcoes: TArray<string>;
      K: Integer;
    begin
      SetLength(Opcoes, Length(It.Valores));
      for K := 0 to High(It.Valores) do
        Opcoes[K] := It.Valores[K].Texto;
      Escolhe(It.Rotulo, Opcoes, FNucleo.Memoria.Valor(It),
        procedure(V: Integer)
        begin
          Muda(procedure begin FNucleo.Memoria.PoeValor_(It, V); end);
        end);
    end;
end;

function TJanela.AcaoTexto(const Nome: string; Maximo: Integer): TProc;
begin
  Result :=
    procedure
    begin
      Pergunta(Format('%s (até %d)', [Nome, Maximo]), FNucleo.Memoria.Texto(Nome),
        procedure(V: string)
        begin
          Muda(procedure begin FNucleo.Memoria.PoeTexto(Nome, TextoDoCampo(Nome, V, Maximo)); end);
        end);
    end;
end;

function TJanela.AcaoFrequenciaVfo(K: Integer): TProc;
begin
  Result :=
    procedure
    begin
      Pergunta(Format('VFO %s, em MHz', [Chr(Ord('A') + K)]), FrequenciaTexto(FNucleo.Memoria.Frequencia(K)),
        procedure(V: string)
        begin
          Muda(
            procedure
            var
              F: Cardinal;
            begin
              if not TextoFrequencia(V, F) or not FaixaRX(F) then
                raise EConvertError.Create('A frequência vai em MHz, de 18 a 1000.');
              FNucleo.Memoria.PoeFrequencia(K, F);
            end);
        end);
    end;
end;

function TJanela.AcaoOffsetVfo(K: Integer): TProc;
begin
  Result :=
    procedure
    begin
      Pergunta(Format('Offset do VFO %s, em MHz', [Chr(Ord('A') + K)]), TextoOffsetVfo(FNucleo.Memoria.Offset(K)),
        procedure(V: string)
        begin
          Muda(
            procedure
            var
              F: Cardinal;
            begin
              if not TextoOffset(V, F) then
                raise EConvertError.Create('O offset vai em MHz, até 999.999, de kHz em kHz.');
              FNucleo.Memoria.PoeOffset(K, F);
            end);
        end);
    end;
end;

function TJanela.AcaoGrupoDtmf(K: Integer): TProc;
begin
  Result :=
    procedure
    begin
      Pergunta(Format('Código do grupo %d (até %d dígitos)', [K + 1, DIGITOS_GRUPO]), FNucleo.Memoria.GrupoDtmf(K),
        procedure(V: string)
        begin
          Muda(procedure begin FNucleo.Memoria.PoeGrupoDtmf(K, SoDtmf(V, DIGITOS_GRUPO)); end);
        end);
    end;
end;

function TJanela.PaginaPasta(P: TPasta): TArray<TLinha>;
var
  M: TMemoriaBJ8300;
  It: TItem;
  T: TTextoDesc;
  V, K: Integer;
  L: TLinha;
begin
  M := FNucleo.Memoria;
  Result := nil;
  for It in P.Itens do
  begin
    V := M.Valor(It);
    L := Linha(It.Rotulo, '', It.Valores[V].Texto, AcaoItem(It));
    if V <> M.ValorOriginal(It) then
      L.CorDireita := COR_AMBAR;   // mudou desde a leitura
    Result := Result + [L];
  end;
  // os campos que o Tempra Link do Windows põe nas mesmas pastas
  for T in FNucleo.Menu.Textos do
    if (P.Nome = 'APRS (SMS)') and (T.Nome <> 'MORSE MESSAGE') or (P.Nome = 'MORSE') and (T.Nome = 'MORSE MESSAGE') then
      Result := Result + [Linha(T.Nome, '', IfThen(M.Texto(T.Nome) = '', '—', M.Texto(T.Nome)),
                                AcaoTexto(T.Nome, T.Maximo))];
  if P.Nome = 'CHANNEL' then
    for K := 0 to 2 do
      Result := Result + [Linha(Format('FREQUENCY (VFO %s)', [Chr(Ord('A') + K)]), '',
                                FrequenciaTexto(M.Frequencia(K)), AcaoFrequenciaVfo(K))];
  if P.Nome = 'REPEATER' then
    for K := 0 to 2 do
      Result := Result + [Linha(Format('OFFSET (VFO %s)', [Chr(Ord('A') + K)]), '', TextoOffsetVfo(M.Offset(K)),
                                AcaoOffsetVfo(K))];
  if P.Nome = 'DTMF' then
    for K := 0 to GRUPOS_DTMF - 1 do
      Result := Result + [Linha(Format('GROUP %d (código)', [K + 1]), '',
                                IfThen(M.GrupoDtmf(K) = '', '—', M.GrupoDtmf(K)), AcaoGrupoDtmf(K))];
  if P.Nome = 'TX' then
    Result := Result + [Secao('ROGER CUSTOM'),
      Linha('As notas do CUSTOM', 'o ROGER em CUSTOM toca estas (firmware v164)', IntToStr(Length(M.Notas)),
        procedure
        begin
          Empilha('ROGER CUSTOM', PaginaRoger);
        end)];
end;

{ O ROGER CUSTOM: até 32 notas, cada uma de 0 (pausa) ou 300 a 3000 Hz e de 10 a 1000 ms, até 3 s ao todo (o
  NotasValidas). Uma mudança que passaria disso não entra, e diz por quê. }

// a nota do Hz, como no piano do Tempra Link do Windows: o lá de 440 Hz é o A4
function NomeDaNota(Hz: Integer): string;
const
  NOMES: array[0..11] of string = ('C', 'C#', 'D', 'D#', 'E', 'F', 'F#', 'G', 'G#', 'A', 'A#', 'B');
var
  N: Integer;
begin
  if Hz <= 0 then
    Exit('pausa');
  N := Round(69 + 12 * Log2(Hz / 440));
  Result := NOMES[N mod 12] + IntToStr(N div 12 - 1);
end;

function TextoNota(const N: TNota): string;
begin
  if N.Hz = 0 then
    Result := 'pausa'
  else
    Result := Format('%d Hz  (%s)', [N.Hz, NomeDaNota(N.Hz)]);
end;

procedure TJanela.PoeNotas(const N: TArray<TNota>);
var
  Erro: string;
begin
  if (N <> nil) and not NotasValidas(N, Erro) then
  begin
    Mostra(Erro);
    Exit;
  end;
  Muda(
    procedure
    begin
      FNucleo.Memoria.PoeNotas(N);
    end);
end;

procedure TJanela.Toca(const N: TArray<TNota>);
var
  Arq: string;
begin
  if N = nil then
    Exit;
  try
    Arq := System.IOUtils.TPath.Combine(PastaDoApp, 'roger.wav');
    TFile.WriteAllBytes(Arq, SomDasNotas(N, 150));
    if FSom = nil then
      FSom := TMediaPlayer.Create(Self);
    FSom.Stop;
    FSom.Clear;
    FSom.FileName := Arq;
    FSom.Play;
  except
    on E: Exception do
      Mostra('O som não tocou: ' + E.Message);
  end;
end;

// a nota K: mudada (Inserir falso) ou uma nova no lugar K (as de lá em diante andam uma casa)
procedure TJanela.PerguntaNota(K: Integer; Inserir: Boolean);
var
  N: TArray<TNota>;
  Hz, Ms: string;
begin
  N := FNucleo.Memoria.Notas;
  if Inserir then
  begin
    Hz := '880';
    Ms := '100';
  end
  else
  begin
    Hz := IntToStr(N[K].Hz);
    Ms := IntToStr(N[K].Ms);
  end;
  PerguntaVarios(IfThen(Inserir, Format('Nota nova (a %dª)', [K + 1]), Format('Nota %d', [K + 1])),
    ['Hz (0 = pausa, ou de 300 a 3000)', 'ms (de 10 a 1000)'], [Hz, Ms],
    procedure(V: TArray<string>)
    var
      Nova: TNota;
      H, D: Integer;
      M: TArray<TNota>;
    begin
      if not TryStrToInt(Trim(V[0]), H) or not TryStrToInt(Trim(V[1]), D) or (H < 0) or (H > 65535) or (D < 0) or
         (D > 65535) then
      begin
        Mostra('O Hz e os ms vão em números inteiros.');
        Exit;
      end;
      Nova.Hz := H;
      Nova.Ms := D;
      M := Copy(FNucleo.Memoria.Notas);
      if Inserir then
        Insert(Nova, M, K)
      else
        M[K] := Nova;
      PoeNotas(M);
    end);
end;

function TJanela.AcaoNota(K: Integer): TProc;
begin
  Result :=
    procedure
    begin
      Empilha(Format('Nota %d', [K + 1]),
        function: TArray<TLinha>
        begin
          Result := PaginaNota(K);
        end);
    end;
end;

function TJanela.PaginaNota(K: Integer): TArray<TLinha>;
var
  N: TArray<TNota>;
begin
  N := FNucleo.Memoria.Notas;
  if K > High(N) then
  begin
    Result := [Secao('Nota apagada')];
    Exit;
  end;
  Result := [
    Linha('Mudar', Format('%s, %d ms', [TextoNota(N[K]), N[K].Ms]), '',
      procedure
      begin
        PerguntaNota(K, False);
      end),
    Linha('Ouvir esta', '', '▶',
      procedure
      begin
        Toca([FNucleo.Memoria.Notas[K]]);
      end),
    Linha('Pôr uma antes desta', '', '',
      procedure
      begin
        PerguntaNota(K, True);
      end),
    Linha('Pôr uma depois desta', '', '',
      procedure
      begin
        PerguntaNota(K + 1, True);
      end),
    Linha('Apagar esta nota', '', '',
      procedure
      var
        M: TArray<TNota>;
      begin
        M := Copy(FNucleo.Memoria.Notas);
        Delete(M, K, 1);
        Desempilha;
        PoeNotas(M);
      end)];
  Result[High(Result)].Cor := COR_VERMELHO;
end;

function TJanela.PaginaRoger: TArray<TLinha>;
var
  N: TArray<TNota>;
  K, Total: Integer;
  Erro: string;
begin
  N := FNucleo.Memoria.Notas;
  Total := 0;
  for K := 0 to High(N) do
    Inc(Total, N[K].Ms);
  if N = nil then
    Result := [Secao('Nenhuma nota: o CUSTOM fica calado')]
  else if NotasValidas(N, Erro) then
    Result := [Secao(Format('%d de %d notas · %s s de %d s', [Length(N), ROGER_NOTAS, FormatFloat('0.00', Total / 1000),
                            ROGER_TOTAL_MS div 1000]))]
  else
    Result := [Secao(Erro)];
  if N <> nil then
    Result := Result + [Linha('Ouvir no celular', 'como o rádio toca: um tom puro por vez', '▶',
      procedure
      begin
        Toca(FNucleo.Memoria.Notas);
      end)];
  Result := Result + [Linha('Acrescentar uma nota', 'no fim', '+',
    procedure
    begin
      PerguntaNota(Length(FNucleo.Memoria.Notas), True);
    end)];
  if N <> nil then
  begin
    Result := Result + [Secao('As notas, na ordem')];
    for K := 0 to High(N) do
      Result := Result + [Linha(Format('%d   %s', [K + 1, TextoNota(N[K])]), '', Format('%d ms', [N[K].Ms]),
                                AcaoNota(K))];
    Result := Result + [Secao(''), Linha('Apagar todas', 'o CUSTOM fica calado', '',
      procedure
      begin
        Confirma('Apagar todas as notas do ROGER CUSTOM?',
          procedure
          begin
            PoeNotas(nil);
          end);
      end)];
    Result[High(Result)].Cor := COR_VERMELHO;
  end;
end;

{ Mais }

function TJanela.PaginaMais: TArray<TLinha>;
var
  M: TMemoriaBJ8300;
  Lado: string;
  L: TLinha;
begin
  M := FNucleo.Memoria;
  Result := [Secao('O rádio'),
             Linha('Ler do rádio', IfThen(M.Vazia, 'os canais, os bancos e os ajustes', FNucleo.Origem), '', Ler)];
  if not M.Vazia then
  begin
    L := Linha('Gravar no rádio', IfThen(M.Mudou, 'as mudanças desta memória', 'nada mudou desde a leitura'), '',
               Gravar);
    if M.Mudou then
      L.Cor := COR_AMBAR;
    Result := Result + [L];
    if M.PodeDesfazer then
      Result := Result + [Linha('Desfazer a última mudança', '', '',
        procedure
        begin
          FNucleo.Memoria.Desfaz;
          Atualiza;
        end)];
  end;

  Result := Result + [Secao('Backups')];
  if not M.Vazia then
    Result := Result + [Linha('Guardar um backup agora', 'a memória de agora, com as mudanças', '',
      procedure
      var
        Arq: string;
      begin
        try
          Arq := FNucleo.SalvaBackup;
          Mostra('Backup guardado: ' + System.IOUtils.TPath.GetFileName(Arq));
        except
          on E: Exception do
            Mostra('O backup não foi guardado: ' + E.Message);
        end;
        Atualiza;
      end)];
  Result := Result + [Linha('Abrir um backup', IfThen(Length(FNucleo.Backups) = 1, '1 guardado neste celular',
                             Format('%d guardados neste celular', [Length(FNucleo.Backups)])), '',
    procedure
    begin
      Empilha('Backups', PaginaBackups);
    end)];

  Result := Result + [Secao('Aviso de recepção')];
  L := Linha('Avisar quando o rádio receber', 'notificação no celular, com o controle remoto aberto',
             IfThen(FAvisa, 'Ligado', 'Desligado'),
    procedure
    begin
      LigaAviso(not FAvisa);
    end);
  if not FAvisa then
    L.CorDireita := COR_TEXTO_FRACO;
  if FAvisoLado = '' then
    Lado := 'qualquer'
  else
    Lado := FAvisoLado;
  Result := Result + [L,
    Linha('Lado', 'de quem receber', Lado,
      procedure
      begin
        Escolhe('Avisar do lado', ['Qualquer', 'A', 'B', 'C'], Max(0, Pos(FAvisoLado, 'ABC')),
          procedure(V: Integer)
          begin
            if V = 0 then
              FAvisoLado := ''
            else
              FAvisoLado := Chr(Ord('A') + V - 1);
            GuardaConfig;
            Atualiza;
          end);
      end),
    Linha('Som do aviso', 'um toque troca', IfThen(FAvisoSom, 'Ligado', 'Desligado'),
      procedure
      begin
        FAvisoSom := not FAvisoSom;
        GuardaConfig;
        Atualiza;
      end)];

  Result := Result + [Secao('Ligação'),
    Linha('Trocar de rádio', FNucleo.Nome, '', MostraConectar),
    Linha('Registro', 'o que aconteceu na ligação com o rádio', '',
      procedure
      begin
        Empilha('Registro', PaginaRegistro);
      end),
    Secao('Sobre'),
    Linha('Tempra Link do ' + RADIO_NOME, IfThen(M.Versao = '', 'versão 0.1 do app',
          'versão 0.1 do app · firmware ' + M.Versao), '')];
end;

function TJanela.AcaoBackup(const Arquivo: string): TProc;
begin
  Result :=
    procedure
    begin
      Descarta(
        procedure
        begin
          try
            FNucleo.AbreBackup(Arquivo);
            SetLength(FPilhas[abCanais], 1);
            SetLength(FPilhas[abAjustes], 1);
            Desempilha;
            Mostra(Format('Backup aberto: %d canais salvos. Gravar no rádio põe o que ele tem no rádio.',
                          [FNucleo.Memoria.Salvos]));
          except
            on E: Exception do
              Mostra('O backup não abriu: ' + E.Message);
          end;
          Atualiza(True);
        end);
    end;
end;

function TJanela.PaginaBackups: TArray<TLinha>;
var
  Arq, Nome: string;
  Quando: TDateTime;
begin
  Result := nil;
  for Arq in FNucleo.Backups do
  begin
    Nome := System.IOUtils.TPath.GetFileNameWithoutExtension(Arq);
    if DataDoBackup(Nome, Quando) then
      Result := Result + [Linha(FormatDateTime('dd/mm/yyyy hh:nn', Quando), Nome, '', AcaoBackup(Arq))]
    else
      Result := Result + [Linha(Nome, '', '', AcaoBackup(Arq))];
  end;
  if Result = nil then
    Result := [Secao('Nenhum backup guardado'),
               Linha('Cada leitura guarda um', 'e também o Guardar um backup agora, na aba Mais')];
end;

function TJanela.AcaoLinhaRegistro(const S: string): TProc;
begin
  Result :=
    procedure
    begin
      Mostra(S);
    end;
end;

function TJanela.PaginaRegistro: TArray<TLinha>;
var
  S: string;
begin
  Result := [Linha('Atualizar', ArquivoDoRegistro, '',
               procedure
               begin
                 Atualiza(True);
               end),
             Secao('O mais novo primeiro')];
  // a linha é "dd/mm hh:nn:ss.zzz  texto": o texto no título, a hora embaixo, e um toque mostra tudo
  for S in UltimasLinhas do
    Result := Result + [Linha(Copy(S, 21, MaxInt), Copy(S, 1, 18), '', AcaoLinhaRegistro(S))];
end;

{ Ler e gravar }

// Com mudanças não gravadas, pergunta antes de perdê-las.
procedure TJanela.Descarta(const Depois: TProc);
begin
  if FNucleo.Memoria.Mudou then
    Confirma('Há mudanças que não foram gravadas no rádio. Descartar?', Depois)
  else
    Depois();
end;

procedure TJanela.Ler;
begin
  Descarta(
    procedure
    begin
      Tarefa('Lendo o rádio', True);
    end);
end;

procedure TJanela.GravarClick(Sender: TObject);
begin
  Gravar;
end;

procedure TJanela.CancelaClick(Sender: TObject);
begin
  FNucleo.Cancela;
  FVeuTexto.Text := 'Cancelando...';
end;

procedure TJanela.Gravar;
var
  M: TMemoriaBJ8300;
  N: Integer;
  Msg: string;
begin
  M := FNucleo.Memoria;
  if M.Vazia or FVeu.Visible then
    Exit;
  M.Guarda;
  M.PreparaGravacao;
  N := Length(M.Mudancas);
  if N = 0 then
  begin
    M.Desfaz;
    Mostra('Não há o que gravar: o rádio já tem esta memória.');
    Exit;
  end;
  Msg := Format('Gravar no rádio: %d mudanças (blocos de canais, ajustes, bancos e textos)?', [N]);
  if FNucleo.DoArquivo then
    Msg := Msg + sLineBreak + sLineBreak + 'É um backup aberto: o que está nele substitui o que o rádio tem nesses ' +
           'lugares.';
  Msg := Msg + sLineBreak + sLineBreak + 'O rádio fica mudo enquanto grava';
  if M.MudouBancos then
    Msg := Msg + ', e depois é preciso desligar e ligar o rádio para ele montar os bancos';
  TDialogService.MessageDialog(Msg + '.', TMsgDlgType.mtConfirmation, [TMsgDlgBtn.mbYes, TMsgDlgBtn.mbNo], TMsgDlgBtn.mbNo, 0,
    procedure(const R: TModalResult)
    begin
      if R = mrYes then
        Tarefa('Gravando no rádio', False)
      else
      begin
        FNucleo.Memoria.Desfaz;
        Atualiza;
      end;
    end);
end;

// A leitura ou a gravação, com o véu por cima. O controle remoto já está desligado (a aba não é a do rádio), mas a
// thread dele pode estar saindo: a tarefa espera ela soltar o rádio.
procedure TJanela.Tarefa(const Titulo: string; Ler_: Boolean);
var
  Esperar: TFunc<Boolean>;
  Progresso: TProc<Integer, Integer>;
  Fim: TFimTarefa;
begin
  FRadio.Desliga;
  FVeuTitulo.Text := Titulo;
  FVeuTexto.Text := 'Conectando ao rádio...';
  FVeuCheio.Width := 0;
  FVeu.Visible := True;
  FVeu.BringToFront;
  Esperar :=
    function: Boolean
    begin
      Result := FRadio.EsperaParar(15000);
    end;
  Progresso :=
    procedure(Feito, Total: Integer)
    begin
      if Total <= 0 then
        Exit;
      FVeuCheio.Width := FVeuBarra.Width * Min(1, Feito / Total);
      FVeuTexto.Text := Format('%d de %d blocos', [Feito, Total]);
    end;
  Fim :=
    procedure(Ok: Boolean; const Mensagem: string)
    begin
      FVeu.Visible := False;
      if Ok and Ler_ then
      begin
        SetLength(FPilhas[abCanais], 1);
        SetLength(FPilhas[abAjustes], 1);
      end;
      Atualiza(Ok and Ler_);
      Mostra(Mensagem);
    end;
  if Ler_ then
    FNucleo.Le(Esperar, Progresso, Fim)
  else
    FNucleo.Grava(Esperar, Progresso, Fim);
end;

end.
