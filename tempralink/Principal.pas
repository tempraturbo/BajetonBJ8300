unit Principal;

{ A janela do Tempra Link do BJ-8300: a navegação à esquerda (Canais, Bancos, Ajustes, Logo de abertura, Controle
  remoto, Firmware, Backup e Registro), o rádio, a porta e as ações no alto, e o andamento embaixo. O que fala com o
  rádio roda numa thread. As grades se editam na própria célula: o texto e a frequência num campo, as escolhas numa
  lista (o sim e o não também, numa lista SIM/NÃO), e os bancos numa lista de marcar; com várias linhas
  escolhidas, o valor vai para todas. A mesma janela do Tempra Link do UV-17L, sem o que o BJ não tem (a gravação do
  firmware pelo programa), e com o Bluetooth na lista das portas. O ROGER CUSTOM (firmware v164) se edita na pasta TX
  dos ajustes, com o editor do UV-17L. O controle remoto (v159) traz a tela do rádio pelo cabo; pelo Bluetooth, o
  estado. }

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Classes, System.Types, Vcl.Graphics, Vcl.Controls,
  Vcl.Forms, Vcl.StdCtrls, Vcl.ComCtrls, Vcl.ExtCtrls, Vcl.Dialogs, Vcl.Grids, Vcl.WinXPanels, Vcl.WinXCtrls,
  Vcl.CheckLst, Vcl.Menus, Transporte, CaboBJ, Logo, LogoFormato, Ajustes, BJ8300, PortaSerial, Bluetooth, Visual, Remoto,
  TelaBJ, EditorRoger;

const
  VERSAO_TEMPRA_LINK = '1.0';  // a mesma do Recursos.rc

type
  TFim = (fOk, fCancelado, fFalha, fSemRadio, fConflito, fErro);
  TAcao = (acLer, acGravar, acLerLogo, acGravarLogo, acProcurar, acRemoto);
  TTipoCelula = (tcNada, tcTexto, tcLista, tcBancos);

  // uma linha da grade dos ajustes
  TTipoAjuste = (taItem, taTexto, taFrequencia, taOffset, taGrupoDtmf);
  TLinhaAjuste = record
    Tipo: TTipoAjuste;
    Item: TItem;
    Nome: string;     // o do texto
    Tamanho: Integer; // o do texto
    K: Integer;       // o lado da frequência e do offset, o grupo DTMF
    Rotulo: string;
  end;

  // O caminho até o rádio (a porta ou o Bluetooth) aberto numa thread, com o trabalho dado; o fim volta pelo
  // OnTerminate.
  TTarefa = class(TThread)
  private
    FAbre: TFunc<ITransporte>;
    FCorpo: TProc<ITransporte>;
  protected
    procedure Execute; override;
  public
    Fim: TFim;
    Mensagem: string;
    constructor Create(Abre: TFunc<ITransporte>; Corpo: TProc<ITransporte>);
  end;

  TFormPrincipal = class(TForm)
    PainelNav: TPanel;
    PaintMarca: TPaintBox;
    LabelMarca: TLabel;
    LabelModelo: TLabel;
    ListaNav: TListBox;
    LabelVersao: TLabel;
    PainelConteudo: TPanel;
    PainelTopo: TPanel;
    LabelTitulo: TLabel;
    LabelRadio: TLabel;
    ComboPorta: TComboBox;
    BotaoAtualizar: TButton;
    BotaoLer: TButton;
    BotaoGravarRadio: TButton;
    BotaoDesfazer: TButton;
    PainelBase: TPanel;
    LabelEstado: TLabel;
    Barra: TProgressBar;
    BotaoCancelar: TButton;
    Cartoes: TCardPanel;
    CartaoCanais: TCard;
    PainelFiltro: TPanel;
    Busca: TSearchBox;
    CheckVazios: TCheckBox;
    BotaoNovoCanal: TButton;
    BotaoApagarCanal: TButton;
    BotaoImportarCsv: TButton;
    BotaoExportarCsv: TButton;
    LabelContagem: TLabel;
    Grade: TStringGrid;
    LabelVazio: TLabel;
    CartaoBancos: TCard;
    GradeBancos: TStringGrid;
    LabelBancosNota: TLabel;
    CartaoAjustes: TCard;
    PainelPastas: TPanel;
    BuscaAjuste: TSearchBox;
    ListaPastas: TListBox;
    PainelValores: TPanel;
    GradeAjustes: TStringGrid;
    PainelRoger: TPanel;
    LabelRoger: TLabel;
    LabelRogerInfo: TLabel;
    LabelRogerDica: TLabel;
    BotaoOuvir: TButton;
    BotaoTocarDaqui: TButton;
    LabelDuracaoNova: TLabel;
    ComboDuracaoNova: TComboBox;
    BotaoPausa: TButton;
    BotaoApagarNota: TButton;
    LabelNotaEscolhida: TLabel;
    EditHz: TEdit;
    LabelHz: TLabel;
    EditMs: TEdit;
    LabelMs: TLabel;
    LabelNomeNota: TLabel;
    BotaoImportarSom: TButton;
    LabelInicio: TLabel;
    EditInicio: TEdit;
    LabelDuracao: TLabel;
    EditDuracao: TEdit;
    ComboModelos: TComboBox;
    CartaoLogo: TCard;
    PainelPrevia: TPanel;
    Previa: TImage;
    BotaoImagem: TButton;
    RadioPreencher: TRadioButton;
    RadioInteira: TRadioButton;
    LabelLogoInfo: TLabel;
    BotaoLerLogo: TButton;
    BotaoSalvarLogo: TButton;
    BotaoGravarLogo: TButton;
    LabelLogoPassos: TLabel;
    CartaoControle: TCard;
    PainelVisor: TPanel;
    PaintVisor: TPaintBox;
    BotaoConectar: TButton;
    LabelRemoto: TLabel;
    LabelTelaRadio: TLabel;
    PainelTeclado: TPanel;
    LabelAvisoCabo: TLabel;
    LabelControleNota: TLabel;
    CartaoFirmware: TCard;
    LabelInfo: TLabel;
    LabelPassos: TLabel;
    CartaoBackup: TCard;
    LabelBackup: TLabel;
    BotaoAbrirBackup: TButton;
    BotaoSalvarBackup: TButton;
    BotaoPastaBackups: TButton;
    LabelBackupNota: TLabel;
    CartaoRegistro: TCard;
    MemoRegistro: TMemo;
    DialogoImagem: TOpenDialog;
    DialogoSalvar: TSaveDialog;
    DialogoBackup: TOpenDialog;
    DialogoSalvarBackup: TSaveDialog;
    DialogoCsv: TOpenDialog;
    DialogoSalvarCsv: TSaveDialog;
    DialogoSom: TOpenDialog;
    MenuCanais: TPopupMenu;
    ItemCopiar: TMenuItem;
    ItemColar: TMenuItem;
    ItemApagar: TMenuItem;
    procedure FormCreate(Sender: TObject);
    procedure FormDestroy(Sender: TObject);
    procedure FormCloseQuery(Sender: TObject; var CanClose: Boolean);
    procedure FormKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure ListaNavClick(Sender: TObject);
    procedure ListaNavDrawItem(Control: TWinControl; Index: Integer; Rect: TRect; State: TOwnerDrawState);
    procedure BotaoAtualizarClick(Sender: TObject);
    procedure ComboPortaChange(Sender: TObject);
    procedure BotaoLerClick(Sender: TObject);
    procedure BotaoGravarRadioClick(Sender: TObject);
    procedure BotaoDesfazerClick(Sender: TObject);
    procedure BotaoCancelarClick(Sender: TObject);
    procedure BuscaChange(Sender: TObject);
    procedure GradeDrawCell(Sender: TObject; ACol, ARow: Integer; Rect: TRect; State: TGridDrawState);
    procedure GradeDblClick(Sender: TObject);
    procedure GradeSelectCell(Sender: TObject; ACol, ARow: Integer; var CanSelect: Boolean);
    procedure GradeKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure GradeKeyPress(Sender: TObject; var Key: Char);
    procedure BotaoNovoCanalClick(Sender: TObject);
    procedure BotaoApagarCanalClick(Sender: TObject);
    procedure BotaoImportarCsvClick(Sender: TObject);
    procedure BotaoExportarCsvClick(Sender: TObject);
    procedure ItemCopiarClick(Sender: TObject);
    procedure ItemColarClick(Sender: TObject);
    procedure ListaPastasClick(Sender: TObject);
    procedure ListaPastasDrawItem(Control: TWinControl; Index: Integer; Rect: TRect; State: TOwnerDrawState);
    procedure BuscaAjusteChange(Sender: TObject);
    procedure BotaoOuvirClick(Sender: TObject);
    procedure BotaoTocarDaquiClick(Sender: TObject);
    procedure BotaoPausaClick(Sender: TObject);
    procedure BotaoApagarNotaClick(Sender: TObject);
    procedure ComboDuracaoNovaChange(Sender: TObject);
    procedure EditNotaChange(Sender: TObject);
    procedure ComboModelosChange(Sender: TObject);
    procedure BotaoImportarSomClick(Sender: TObject);
    procedure EditSomChange(Sender: TObject);
    procedure ChaveTelaClick(Sender: TObject);
    procedure BotaoImagemClick(Sender: TObject);
    procedure RadioAjusteClick(Sender: TObject);
    procedure BotaoLerLogoClick(Sender: TObject);
    procedure BotaoSalvarLogoClick(Sender: TObject);
    procedure BotaoGravarLogoClick(Sender: TObject);
    procedure PaintMarcaPaint(Sender: TObject);
    procedure BotaoAbrirBackupClick(Sender: TObject);
    procedure BotaoSalvarBackupClick(Sender: TObject);
    procedure BotaoPastaBackupsClick(Sender: TObject);
    procedure BotaoConectarClick(Sender: TObject);
    procedure PaintVisorPaint(Sender: TObject);
  private
    FMenu: TMenu;
    FPortas: TArray<TPorta>;
    FRadiosBle: TArray<TRadioBLE>;  // os achados pela última procura, que vêm na lista depois das portas
    FAchados: TArray<TRadioBLE>;    // o que a thread da procura achou
    FRemoto: TRemoto;
    FConectado: Boolean;            // o controle remoto já ouviu o rádio
    FEstadoRemoto: TEstadoRadio;    // o último estado que ele mandou
    FBotaoPtt: TTecla;
    FSegurado: TTimer;              // anda enquanto uma tecla ou o PTT está apertado: solta se o mouse soltou fora
    FTela: TBitmap;                 // a última tela do rádio que chegou
    FAvisoTela: string;             // o que falta nela (TelaBJ: o espectro, o APRS e o Morse), ou por que ela parou
    FTarefa: TTarefa;
    FAcao: TAcao;
    FCabo: TCaboBJ;
    FMemoria: TMemoriaBJ8300;   // o que se lê, se mostra, se edita e se grava; vazia até ler ou abrir um backup
    FChaveTela: TChave;         // a tela do rádio no controle remoto: Ligada, acesa (o pedido de apagar desligado)
    FNotas: TArray<TNota>;      // o que o editor do ROGER mostra (pode estar inválido enquanto se edita)
    FArquivoSom: string;        // o MIDI ou o som do ROGER, que se lê de novo quando o começo ou a duração mudam
    FLinhaRoger: TLinhaDoTempo;
    FTeclado: TTeclado;
    FMostrandoNota: Boolean;    // os campos da nota escolhida sendo preenchidos: o OnChange não muda nada
    FLida: TMemoriaBJ8300;      // a que a thread está lendo
    FOrigem: string;            // de onde veio a memória: o rádio ou o arquivo
    FDoArquivo: Boolean;        // veio de um backup: gravar é restaurar
    FBancosGravados: Boolean;   // a gravação de agora mexe nos bancos: o rádio só os monta na partida
    FLinhas: TArray<Integer>;   // os canais de cada linha da grade
    FNovo: Integer;             // um canal vazio que o Novo canal pôs na grade
    FAjustes: TArray<TLinhaAjuste>;
    FImagem: TWICImage;         // a imagem escolhida; nil quando o logo veio do rádio
    FLogo: TBytes;              // o logo da prévia, como vai para o rádio
    FLogoLido: TBytes;          // o que a thread leu do rádio
    FFechar: Boolean;
    // a edição na célula
    FEditor: TEdit;
    FLista: TComboBox;
    FPainelBancos: TPanel;
    FListaBancos: TCheckListBox;
    FEdGrade: TStringGrid;
    FEdCol, FEdLin: Integer;
    procedure Mostra(Cartao: Integer);
    procedure AbreImagem(const Caminho: string);
    procedure AbreBackup(const Caminho: string);
    procedure MostraLogo(const L: TBytes; const Info: string);
    procedure MostraMemoria;
    procedure MostraTopo;
    procedure PreencheGrade;
    procedure PreencheLinhaDoCanal(L, N: Integer);
    procedure PreencheBancos;
    procedure PreencheAjustes;
    procedure PreencheNotas;
    procedure MostraNotaEscolhida;
    procedure RogerAntes(Sender: TObject);
    procedure RogerMudou(Sender: TObject);
    procedure RogerEscolheu(Sender: TObject);
    procedure RogerFimDoSom(Sender: TObject);
    procedure PoeNoRoger(const N: TNota);
    procedure ImportaSom;
    procedure ListaAsPortas;
    procedure Habilita;
    procedure Registra(const Msg: string);
    procedure Avisa(const Msg: string);
    procedure NaJanela(P: TThreadProcedure);
    function Avisos: TProc<string>;
    function Andamento: TProc<Integer, Integer>;
    procedure Inicia(Acao: TAcao; Corpo: TProc<ITransporte>);
    function Confirma(const Pergunta: string): Boolean;
    function Porta: string;
    function Escolhida: Integer;
    function AbreTransporte: TFunc<ITransporte>;
    function AbreRemoto: TFunc<ITransporte>;
    function NovoCabo: TCaboBJ;
    function DescartaMudancas: Boolean;
    procedure Terminou(Sender: TObject);
    procedure TeclaDesce(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
    procedure TeclaSobe(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
    procedure SoltaTudo;
    procedure ConfereSegurado(Sender: TObject);
    procedure WMDropFiles(var Msg: TWMDropFiles); message WM_DROPFILES;
    // a edição na célula
    function Celula(G: TStringGrid; Col, Lin: Integer; out Opcoes: TArray<string>; out Atual: string): TTipoCelula;
    procedure Aplica(G: TStringGrid; Col, Lin: Integer; const Valor: string);
    procedure AplicaCanal(Col, N: Integer; const Valor: string);
    procedure IniciaEdicao(G: TStringGrid; Col, Lin: Integer; const Primeira: string = '');
    procedure ConfirmaEdicao(Sender: TObject);
    procedure CancelaEdicao;
    procedure EditorKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
    procedure EditorKeyPress(Sender: TObject; var Key: Char);
    procedure ListaCloseUp(Sender: TObject);
    procedure BancosOkClick(Sender: TObject);
    procedure LinhasEscolhidas(out Primeira, Ultima: Integer);
    procedure Mudou;
  protected
    procedure CreateWnd; override;
  end;

var
  FormPrincipal: TFormPrincipal;

implementation

{$R *.dfm}

uses
  Winapi.ShellAPI, System.IOUtils, System.Math, System.StrUtils, Vcl.Clipbrd, Vcl.Imaging.pngimage, Csv, Idioma,
  Winapi.GDIPOBJ, Melodia;

const
  CARTAO_CANAIS = 0;
  CARTAO_BANCOS = 1;
  CARTAO_AJUSTES = 2;
  CARTAO_LOGO = 3;
  CARTAO_CONTROLE = 4;
  CARTAO_FIRMWARE = 5;
  CARTAO_BACKUP = 6;
  CARTAO_REGISTRO = 7;

  COL_NOME = 1;
  COL_RX = 2;
  COL_TX = 3;
  COL_TOM_RX = 4;
  COL_TOM_TX = 5;
  COL_POTENCIA = 6;
  COL_LARGURA = 7;
  COL_MODULACAO = 8;
  COL_BCL = 9;
  COL_VARRE = 10;
  COL_SCRAMBLE = 11;
  COL_ENCRYPT = 12;
  COL_GRUPO = 13;
  COL_PTT_ID = 14;
  COL_BANCOS = 15;

  POTENCIAS: array[TPotencia] of array[Boolean] of string = (('High', 'Alta'), ('Mid', 'Média'), ('Low', 'Baixa'));
  PTT_IDS: array[0..3] of string = ('OFF', 'BOT', 'EOT', 'BOTH');
  // o último item da lista das portas: procura o rádio pelo Bluetooth LE
  PROCURA_BLE = -1;

function Potencia(P: TPotencia): string;
begin
  Result := Tr(POTENCIAS[P][True], POTENCIAS[P][False]);
end;

function SimNao(B: Boolean): string;
begin
  if B then
    Result := Tr('Sim', 'Yes')
  else
    Result := Tr('Não', 'No');
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

function BancosDeTexto(const S: string): Word;
var
  Parte: string;
  K: Integer;
begin
  Result := 0;
  for Parte in S.Split([',', ' ', ';'], TStringSplitOptions.ExcludeEmpty) do
    if TryStrToInt(Parte, K) and (K >= 1) and (K <= BANCOS) then
      Result := Result or 1 shl (K - 1);
end;

{ TTarefa }

constructor TTarefa.Create(Abre: TFunc<ITransporte>; Corpo: TProc<ITransporte>);
begin
  inherited Create(True);
  FreeOnTerminate := True;
  FAbre := Abre;
  FCorpo := Corpo;
end;

procedure TTarefa.Execute;
var
  T: ITransporte;
begin
  Fim := fErro;
  try
    if Assigned(FAbre) then
      T := FAbre();
    FCorpo(T);
    Fim := fOk;
  except
    on E: Exception do
    begin
      Mensagem := E.Message;
      if E is ECancelado then
        Fim := fCancelado
      else if E is EFalha then
        Fim := fFalha
      else if (E is ESemRadio) or (E is ENaoTempra) then
        Fim := fSemRadio
      else if E is EConflito then
        Fim := fConflito;
    end;
  end;
end;

{ TFormPrincipal }

function PastaDosBackups: string;
begin
  Result := TPath.Combine(GetEnvironmentVariable('LOCALAPPDATA'), 'TempraLink\backups');
end;

type
  TTeclaRadio = record
    Texto, Letras: string;
    Codigo: Byte;
    Tipo: TTipoTecla;
  end;

const
  // a linha de cima, os algarismos em 4 linhas de 3 e as laterais; o código é o cru da varredura (firmware/src/remoto.c)
  TECLAS_RADIO: array[0..17] of TTeclaRadio = (
    (Texto: 'MENU'; Letras: 'OK'; Codigo: TECLA_OK; Tipo: ttFuncao), (Texto: '▲'; Codigo: TECLA_CIMA; Tipo: ttFuncao),
    (Texto: '▼'; Codigo: TECLA_BAIXO; Tipo: ttFuncao), (Texto: 'EXIT'; Codigo: TECLA_EXIT; Tipo: ttFuncao),
    (Texto: '1'; Codigo: 6), (Texto: '2'; Letras: 'ABC'; Codigo: 7), (Texto: '3'; Letras: 'DEF'; Codigo: 8),
    (Texto: '4'; Letras: 'GHI'; Codigo: 10), (Texto: '5'; Letras: 'JKL'; Codigo: 11), (Texto: '6'; Letras: 'MNO'; Codigo: 12),
    (Texto: '7'; Letras: 'PQRS'; Codigo: 14), (Texto: '8'; Letras: 'TUV'; Codigo: 15), (Texto: '9'; Letras: 'WXYZ'; Codigo: 16),
    (Texto: '*'; Codigo: TECLA_ESTRELA), (Texto: '0'; Codigo: TECLA_ZERO), (Texto: '#'; Codigo: TECLA_CERQUILHA),
    (Texto: 'PF2'; Codigo: TECLA_PF2; Tipo: ttFuncao), (Texto: 'PF3'; Codigo: TECLA_PF3; Tipo: ttFuncao));
  // as durações das notas novas do ROGER, em ms
  DURACOES_NOVAS: array[0..7] of Integer = (30, 50, 80, 100, 150, 200, 300, 500);
  // em 96 dpi: o teclado tem a largura de 4 teclas de cima; os números, 3 por linha, na mesma largura
  TECLA_LARGURA = 72;
  TECLA_ALTURA = 46;
  TECLA_VAO = 8;
  TECLADO_LARGURA = 4 * TECLA_LARGURA + 3 * TECLA_VAO;
  DIGITO_LARGURA = (TECLADO_LARGURA - 2 * TECLA_VAO) div 3;

procedure TFormPrincipal.FormCreate(Sender: TObject);
const
  COLUNAS: array[0..15] of array[Boolean] of string = (
    ('No.', 'Nº'), ('Name', 'Nome'), ('RX (MHz)', 'RX (MHz)'), ('TX (MHz)', 'TX (MHz)'), ('RX tone', 'Tom RX'),
    ('TX tone', 'Tom TX'), ('Power', 'Potência'), ('Bandwidth', 'Largura'), ('RX mod.', 'Modulação'),
    ('Busy lock', 'BCL'), ('Scan', 'Varredura'), ('Scramble', 'Scramble'), ('Encrypt', 'Encrypt'),
    ('DTMF group', 'Grupo DTMF'), ('PTT-ID', 'PTT-ID'), ('Banks', 'Bancos'));
  LARGURAS: array[0..15] of Integer = (44, 110, 84, 84, 64, 64, 64, 72, 76, 44, 72, 76, 66, 80, 56, 90);
var
  I: Integer;
  B: TButton;
  T: TTecla;
begin
  AplicaTema(Self);
  KeyPreview := True;
  FMenu := TMenu.DoRecurso;
  FMemoria := TMemoriaBJ8300.Create(FMenu);
  Caption := 'Tempra Link';
  LabelMarca.Caption := 'Tempra Link';
  LabelModelo.Caption := 'Baojie BJ-8300';
  LabelVersao.Caption := Tr('versão ', 'version ') + VERSAO_TEMPRA_LINK;
  ListaNav.Items.Text := Tr('Canais'#13#10'Bancos'#13#10'Ajustes'#13#10'Logo de abertura'#13#10'Controle remoto'#13#10 +
                            'Firmware'#13#10'Backup'#13#10'Registro',
                            'Channels'#13#10'Banks'#13#10'Settings'#13#10'Boot logo'#13#10'Remote control'#13#10 +
                            'Firmware'#13#10'Backup'#13#10'Log');
  BotaoAtualizar.Caption := Tr('Atualizar', 'Refresh');
  BotaoLer.Caption := Tr('Ler do rádio', 'Read from radio');
  BotaoGravarRadio.Caption := Tr('Gravar no rádio', 'Write to radio');
  BotaoDesfazer.Caption := Tr('Desfazer', 'Undo');
  BotaoCancelar.Caption := Tr('Cancelar', 'Cancel');

  // os canais
  Busca.TextHint := Tr('Buscar por nome, frequência ou tom', 'Search by name, frequency or tone');
  CheckVazios.Caption := Tr('Mostrar os vazios', 'Show empty ones');
  BotaoNovoCanal.Caption := Tr('Novo canal', 'New channel');
  BotaoApagarCanal.Caption := Tr('Apagar', 'Delete');
  BotaoImportarCsv.Caption := Tr('Importar CSV...', 'Import CSV...');
  BotaoExportarCsv.Caption := Tr('Exportar CSV...', 'Export CSV...');
  ItemCopiar.Caption := Tr('Copiar', 'Copy') + #9'Ctrl+C';
  ItemColar.Caption := Tr('Colar', 'Paste') + #9'Ctrl+V';
  ItemApagar.Caption := Tr('Apagar', 'Delete') + #9'Del';
  ItemApagar.OnClick := BotaoApagarCanalClick;
  LabelVazio.Caption := Tr('Nenhum rádio lido ainda.'#13#10#13#10'Ligue o rádio normalmente, com o firmware TEMPRA (V3.7T ' +
                           'ou mais novo), conecte o cabo e clique em Ler do rádio. Ou abra um backup, na página Backup.',
                           'No radio read yet.'#13#10#13#10'Turn the radio on normally, with the TEMPRA firmware (V3.7T ' +
                           'or newer), connect the cable and click Read from radio. Or open a backup, on the Backup page.');
  Grade.ColCount := Length(COLUNAS);
  for I := 0 to High(COLUNAS) do
  begin
    Grade.Cells[I, 0] := Tr(COLUNAS[I][True], COLUNAS[I][False]);
    Grade.ColWidths[I] := MulDiv(LARGURAS[I], CurrentPPI, 96);
  end;
  DialogoCsv.Filter := Tr('CSV do CHIRP (*.csv)|*.csv', 'CHIRP CSV (*.csv)|*.csv');
  DialogoSalvarCsv.Filter := DialogoCsv.Filter;

  // os bancos
  GradeBancos.Cells[0, 0] := Tr('Nº', 'No.');
  GradeBancos.Cells[1, 0] := Tr('Nome', 'Name');
  GradeBancos.Cells[2, 0] := Tr('Ativo', 'Active');
  GradeBancos.Cells[3, 0] := Tr('Canais', 'Channels');
  GradeBancos.ColWidths[0] := MulDiv(44, CurrentPPI, 96);
  GradeBancos.ColWidths[1] := MulDiv(160, CurrentPPI, 96);
  GradeBancos.ColWidths[2] := MulDiv(70, CurrentPPI, 96);
  GradeBancos.ColWidths[3] := MulDiv(70, CurrentPPI, 96);
  LabelBancosNota.Caption := Tr(
    'Com bancos ativos, o rádio mostra só os canais deles; sem nenhum, mostra todos. Para pôr um canal num banco, use ' +
    'a coluna Bancos na página Canais. Sem nome, o rádio mostra BANK 01 a BANK 16. Um banco ativo sem canal o rádio ' +
    'recusa: ele sai desligado na gravação. Depois de gravar os bancos, desligue e ligue o rádio: ele monta os bancos ' +
    'na partida.',
    'With active banks, the radio shows only their channels; with none, it shows all. To put a channel in a bank, use ' +
    'the Banks column on the Channels page. Without a name, the radio shows BANK 01 to BANK 16. The radio refuses an ' +
    'active bank with no channel: it is written turned off. After writing the banks, turn the radio off and on: it ' +
    'builds the banks at power-up.');

  // os ajustes
  BuscaAjuste.TextHint := Tr('Buscar ajuste', 'Search setting');
  for var P in FMenu.Pastas do
    if P.Itens.Count > 0 then
      ListaPastas.Items.AddObject(P.Nome, P);
  GradeAjustes.Cells[0, 0] := Tr('Item do menu', 'Menu item');
  GradeAjustes.Cells[1, 0] := Tr('Valor', 'Value');
  GradeAjustes.ColWidths[0] := MulDiv(240, CurrentPPI, 96);
  GradeAjustes.ColWidths[1] := MulDiv(220, CurrentPPI, 96);
  // o ROGER CUSTOM (v164), na pasta TX: o editor do Tempra Link do UV-17L
  LabelRoger.Caption := Tr('ROGER CUSTOM  ·  escolha CUSTOM no ROGER, acima, para o rádio tocar',
                           'ROGER CUSTOM  ·  choose CUSTOM in ROGER, above, for the radio to play it');
  LabelRogerDica.Caption := Tr(
    'Clique numa tecla do piano para ouvir; dois cliques, ou arrastar a tecla para a linha, põem a nota. Na linha, ' +
    'arraste a nota para cima e para baixo (o tom), pela ponta direita (a duração) e para os lados (a ordem). ' +
    'Espaço toca; Delete apaga.',
    'Click a piano key to hear it; double-click it, or drag it to the line, to add the note. On the line, drag the ' +
    'note up and down (the tone), by its right end (the length) and sideways (the order). Space plays; Delete removes.');
  BotaoOuvir.Caption := Tr('▶  Tocar', '▶  Play');
  BotaoTocarDaqui.Caption := Tr('▶  Da escolhida', '▶  From selected');
  LabelDuracaoNova.Caption := Tr('Notas novas', 'New notes');
  for var D in DURACOES_NOVAS do
    ComboDuracaoNova.Items.Add(Format('%d ms', [D]));
  ComboDuracaoNova.ItemIndex := 3;
  BotaoPausa.Caption := Tr('+ Pausa', '+ Pause');
  BotaoApagarNota.Caption := Tr('Apagar', 'Delete');
  LabelNotaEscolhida.Caption := Tr('Nota escolhida', 'Selected note');
  FLinhaRoger := TLinhaDoTempo.Create(Self);
  FLinhaRoger.Parent := PainelRoger;
  FLinhaRoger.SetBounds(16, 98, PainelRoger.ClientWidth - 32, 150);
  FLinhaRoger.Anchors := [akLeft, akTop, akRight];
  FLinhaRoger.DuracaoNova := 100;
  FLinhaRoger.OnAntes := RogerAntes;
  FLinhaRoger.OnMudou := RogerMudou;
  FLinhaRoger.OnEscolheu := RogerEscolheu;
  FLinhaRoger.OnFimDoSom := RogerFimDoSom;
  FLinhaRoger.OnTom :=
    procedure(Hz: Integer)
    begin
      FLinhaRoger.Para;
      TocaTom(Hz, 200);
    end;
  FTeclado := TTeclado.Create(Self);
  FTeclado.Parent := PainelRoger;
  FTeclado.SetBounds(16, 256, PainelRoger.ClientWidth - 32, 66);
  FTeclado.Anchors := [akLeft, akTop, akRight];
  FTeclado.OnToca :=
    procedure(Nota: Integer)
    begin
      FLinhaRoger.Para;
      TocaTom(HzDaNota(Nota), 350);
    end;
  FTeclado.OnPoe :=
    procedure(Nota: Integer)
    var
      N: TNota;
    begin
      N.Hz := HzDaNota(Nota);
      N.Ms := FLinhaRoger.DuracaoNova;
      PoeNoRoger(N);
    end;
  ComboModelos.Items.Text := Tr('Modelos...'#13#10'Bipe curto'#13#10'Dois tons'#13#10'Três tons subindo'#13#10'K em Morse',
                                'Presets...'#13#10'Short beep'#13#10'Two tones'#13#10'Three rising tones'#13#10'K in Morse');
  ComboModelos.ItemIndex := 0;
  BotaoImportarSom.Caption := Tr('Importar MIDI ou som...', 'Import MIDI or sound...');
  LabelInicio.Caption := Tr('Começar em (s)', 'Start at (s)');
  LabelDuracao.Caption := Tr('Duração (s)', 'Length (s)');
  DialogoSom.Filter := Tr('MIDI e sons|*.mid;*.midi;*.mp3;*.wav;*.wma;*.m4a;*.aac;*.flac|Todos os arquivos (*.*)|*.*',
                          'MIDI and sounds|*.mid;*.midi;*.mp3;*.wav;*.wma;*.m4a;*.aac;*.flac|All files (*.*)|*.*');

  // o firmware: o gravador é o da BinTe, que o repositório traz
  LabelInfo.Caption := Tr('O firmware do BJ-8300 se grava pelo BinTe ANC Bootloader, que vem no repositório ' +
                          '(tools\anc-bootloader), com o arquivo .anc de firmware\releases.',
                          'The BJ-8300 firmware is flashed with the BinTe ANC Bootloader, which comes in the repository ' +
                          '(tools\anc-bootloader), with the .anc file from firmware\releases.');
  LabelPassos.Caption := Tr(
    '1. Ligue o cabo no PC e no rádio desligado.'#13#10 +
    '2. Abra o BinTe ANC Bootloader.exe, escolha a porta do cabo, 115200, e o arquivo BJ8300_BANK_v*.anc.'#13#10 +
    '3. Ligue o rádio segurando o PTT2 e o atalho até aparecer UPDATE na tela, e clique em Update.'#13#10 +
    '4. O registro mostra Update Mode, Baudrate OK, o andamento e Finish. Desligue e ligue o rádio: em SYSTEM > ' +
    'VERSION aparece a versão nova.'#13#10#13#10 +
    'Antes de gravar o firmware, leia o rádio aqui, para ter o backup dos canais e dos ajustes. O passo a passo está ' +
    'no README do repositório.',
    '1. Connect the cable to the PC and to the radio, switched off.'#13#10 +
    '2. Open BinTe ANC Bootloader.exe, choose the cable port, 115200, and the BJ8300_BANK_v*.anc file.'#13#10 +
    '3. Turn the radio on holding PTT2 and the shortcut key until UPDATE shows on screen, and click Update.'#13#10 +
    '4. The log shows Update Mode, Baudrate OK, the progress and Finish. Turn the radio off and on: SYSTEM > VERSION ' +
    'shows the new version.'#13#10#13#10 +
    'Before flashing the firmware, read the radio here, to have the backup of the channels and settings. The step by ' +
    'step is in the repository''s README.');

  // o logo
  BotaoImagem.Caption := Tr('Escolher imagem...', 'Choose image...');
  RadioPreencher.Caption := Tr('Preencher a tela (corta as sobras)', 'Fill the screen (crops the rest)');
  RadioInteira.Caption := Tr('Mostrar inteira (faixas pretas)', 'Show it whole (black bars)');
  BotaoLerLogo.Caption := Tr('Ler do rádio', 'Read from radio');
  BotaoSalvarLogo.Caption := Tr('Salvar como PNG...', 'Save as PNG...');
  BotaoGravarLogo.Caption := Tr('Gravar no rádio', 'Write to radio');
  LabelLogoInfo.Caption := Tr('Escolha uma imagem, ou leia o logo que está no rádio.',
                              'Choose an image, or read the logo that is on the radio.');
  LabelLogoPassos.Caption := Tr(
    'Ler do rádio traz o logo que está nele (160 x 138), para ver ou salvar em PNG. Gravar o logo pelo cabo depende do ' +
    'firmware: a V3.7T ainda não grava, e o Tempra Link confere lendo de volta e avisa. Imagens de 160 x 138 vão como ' +
    'estão; as outras se ajustam.',
    'Read from radio brings the logo it has (160 x 138), to view or save as PNG. Writing the logo over the cable ' +
    'depends on the firmware: V3.7T does not write it yet, and Tempra Link checks by reading back and says so. ' +
    '160 x 138 images go as they are; others are fitted.');
  DialogoImagem.Filter := Tr('Imagens|*.png;*.jpg;*.jpeg;*.bmp;*.gif|Todos os arquivos (*.*)|*.*',
                             'Images|*.png;*.jpg;*.jpeg;*.bmp;*.gif|All files (*.*)|*.*');
  DialogoSalvar.Filter := 'PNG (*.png)|*.png';

  // o backup
  BotaoAbrirBackup.Caption := Tr('Abrir backup...', 'Open backup...');
  BotaoSalvarBackup.Caption := Tr('Salvar backup...', 'Save backup...');
  BotaoPastaBackups.Caption := Tr('Abrir a pasta dos backups', 'Open the backups folder');
  LabelBackupNota.Caption := Format(Tr(
    'O backup guarda os canais, os ajustes, os bancos, os textos, o DTMF, o WATCH, o VFO C e a calibração do rádio, ' +
    'com as mudanças que ainda não foram para ele. Cada leitura do rádio guarda um sozinha, em %s. Abrir um backup e ' +
    'gravar no rádio o restaura. O backup do UV-17L não abre aqui: é de outro rádio.',
    'The backup keeps the radio''s channels, settings, banks, texts, DTMF, WATCH, VFO C and calibration, with the ' +
    'changes not yet written to it. Every read from the radio saves one by itself, in %s. Opening a backup and ' +
    'writing to the radio restores it. A UV-17L backup does not open here: it is from another radio.'),
    [PastaDosBackups]);
  DialogoBackup.Filter := Tr('Backup do Tempra Link (*.tempra)|*.tempra', 'Tempra Link backup (*.tempra)|*.tempra');
  DialogoSalvarBackup.Filter := DialogoBackup.Filter;

  // os editores da célula, um de cada tipo, que vão para a grade que está sendo editada
  FEditor := TEdit.Create(Self);
  FEditor.Visible := False;
  FEditor.OnKeyDown := EditorKeyDown;
  FEditor.OnKeyPress := EditorKeyPress;
  FEditor.OnExit := ConfirmaEdicao;
  FLista := TComboBox.Create(Self);
  FLista.Visible := False;
  FLista.Style := csDropDownList;
  FLista.DropDownCount := 20;
  FLista.OnKeyDown := EditorKeyDown;
  FLista.OnCloseUp := ListaCloseUp;
  FLista.OnExit := ConfirmaEdicao;
  FPainelBancos := TPanel.Create(Self);
  FPainelBancos.Visible := False;
  FPainelBancos.Width := MulDiv(180, CurrentPPI, 96);
  FPainelBancos.Height := MulDiv(330, CurrentPPI, 96);
  FListaBancos := TCheckListBox.Create(Self);
  FListaBancos.Parent := FPainelBancos;
  FListaBancos.Align := alClient;
  B := TButton.Create(Self);
  B.Parent := FPainelBancos;
  B.Align := alBottom;
  B.Caption := 'OK';
  B.OnClick := BancosOkClick;
  for var C: TControl in [FEditor, FLista, FPainelBancos] do
    AplicaTema(C);

  // o controle remoto: o teclado como o do rádio, e o toque longo segurando o botão
  FTela := TBitmap.Create;
  LabelAvisoCabo.Caption := Tr(
    'Pelo cabo vem a tela do rádio; pelo Bluetooth, só o estado (quem recebe, o S-meter e a tensão). Com o cabo ' +
    'plugado, o microfone e o alto-falante podem passar a ser os do plugue: isso não foi testado no BJ.',
    'Over the cable the radio screen comes along; over Bluetooth, only the state (who is receiving, the S-meter and ' +
    'the voltage). With the cable plugged in, the microphone and speaker may become the plug''s: this was not tested ' +
    'on the BJ.');
  LabelControleNota.Caption := Tr(
    'Com o rádio ligado normalmente (firmware v159 ou mais novo), Conectar liga o controle remoto; pelo cabo, a tela ' +
    'do rádio vem junto e daí em diante muda com ela. As teclas daqui ficam apertadas no rádio enquanto o botão do ' +
    'mouse está apertado nelas: segurar 1 s é o toque longo. O PTT transmite enquanto estiver apertado, e o TX TIMEOUT ' +
    'do rádio vale também. Se o PC parar de mandar por 1 s, o rádio solta a tecla e o PTT sozinho. Com a tela do rádio ' +
    'apagada (firmware v164), que poupa a bateria, uma tecla dele a acende.',
    'With the radio on normally (firmware v159 or newer), Connect starts the remote control; over the cable, the radio ' +
    'screen comes along and from then on changes with it. The keys here stay pressed on the radio while the mouse ' +
    'button is pressed on them: holding 1 s is the long press. PTT transmits while pressed, and the radio''s TX TIMEOUT ' +
    'also applies. If the PC stops sending for 1 s, the radio releases the key and the PTT by itself. With the radio ' +
    'screen turned off (firmware v164), which saves battery, a key on the radio turns it on.');
  LabelRemoto.Caption := '';
  LabelTelaRadio.Caption := Tr('Tela do rádio', 'Radio screen');
  FChaveTela := TChave.Create(Self);
  FChaveTela.Parent := LabelTelaRadio.Parent;
  FChaveTela.SetBounds(MulDiv(124, CurrentPPI, 96), MulDiv(455, CurrentPPI, 96), MulDiv(200, CurrentPPI, 96),
                       MulDiv(26, CurrentPPI, 96));
  FChaveTela.Font := LabelTelaRadio.Font;
  FChaveTela.TextoLigada := Tr('Acesa', 'On');
  FChaveTela.TextoDesligada := Tr('Apagada', 'Off');
  FChaveTela.OnClick := ChaveTelaClick;
  // a linha de cima com 4, os números em 4 linhas de 3, as laterais lado a lado e o PTT na largura toda
  for I := 0 to High(TECLAS_RADIO) + 1 do
  begin
    T := TTecla.Create(Self);
    T.Parent := PainelTeclado;
    if I <= High(TECLAS_RADIO) then
    begin
      T.Caption := TECLAS_RADIO[I].Texto;
      T.Letras := TECLAS_RADIO[I].Letras;
      T.Tag := TECLAS_RADIO[I].Codigo;
      T.Tipo := TECLAS_RADIO[I].Tipo;
    end
    else
    begin
      FBotaoPtt := T;
      T.Caption := Tr('PTT  ·  segure para transmitir', 'PTT  ·  hold to transmit');
      T.Tag := -1;
      T.Tipo := ttPtt;
    end;
    if I < 4 then
      T.SetBounds(I * (TECLA_LARGURA + TECLA_VAO), 0, TECLA_LARGURA, TECLA_ALTURA)
    else if I < 16 then
      T.SetBounds((I - 4) mod 3 * (TECLADO_LARGURA - DIGITO_LARGURA) div 2,
                  (1 + (I - 4) div 3) * (TECLA_ALTURA + TECLA_VAO), DIGITO_LARGURA, TECLA_ALTURA)
    else if I < 18 then
      T.SetBounds((I - 16) * (TECLADO_LARGURA + TECLA_VAO) div 2, 5 * (TECLA_ALTURA + TECLA_VAO),
                  (TECLADO_LARGURA - TECLA_VAO) div 2, TECLA_ALTURA)
    else
      T.SetBounds(0, 6 * (TECLA_ALTURA + TECLA_VAO), TECLADO_LARGURA, TECLA_ALTURA);
    T.ScaleForPPI(CurrentPPI);
    // os números grandes, as setas médias, os nomes menores
    if Length(T.Caption) > 1 then
      T.Font.Height := -MulDiv(12, CurrentPPI, 96)
    else if T.Tipo = ttFuncao then
      T.Font.Height := -MulDiv(16, CurrentPPI, 96)
    else
      T.Font.Height := -MulDiv(20, CurrentPPI, 96);
    T.OnMouseDown := TeclaDesce;
    T.OnMouseUp := TeclaSobe;
  end;
  FSegurado := TTimer.Create(Self);
  FSegurado.Enabled := False;
  FSegurado.Interval := 100;
  FSegurado.OnTimer := ConfereSegurado;

  for B in [BotaoGravarRadio, BotaoGravarLogo] do
    Destaca(B, tbPrincipal);
  ListaNav.ItemIndex := CARTAO_CANAIS;
  Mostra(CARTAO_CANAIS);
  ListaPastas.ItemIndex := 0;
  ListaAsPortas;
  // um .tempra na linha de comando (ou com dois cliques) abre o backup
  if (ParamCount > 0) and SameText(ExtractFileExt(ParamStr(1)), EXTENSAO) then
    AbreBackup(ParamStr(1));
  MostraMemoria;
end;

procedure TFormPrincipal.FormDestroy(Sender: TObject);
begin
  FTela.Free;
  FImagem.Free;
  FMemoria.Free;
  FMenu.Free;
end;

procedure TFormPrincipal.CreateWnd;
begin
  inherited;
  DragAcceptFiles(Handle, True);
end;

// Um .tempra abre o backup, um .csv entra nos canais, e o resto vai para o logo.
procedure TFormPrincipal.WMDropFiles(var Msg: TWMDropFiles);
var
  Nome: array[0..MAX_PATH] of Char;
  Ext: string;
begin
  if (FTarefa = nil) and (DragQueryFile(Msg.Drop, 0, Nome, Length(Nome)) > 0) then
  begin
    Ext := ExtractFileExt(Nome);
    if SameText(Ext, EXTENSAO) then
    begin
      if DescartaMudancas then
      begin
        Mostra(CARTAO_CANAIS);
        AbreBackup(Nome);
      end;
    end
    else if SameText(Ext, '.csv') and not FMemoria.Vazia then
    begin
      Mostra(CARTAO_CANAIS);
      DialogoCsv.FileName := Nome;
      BotaoImportarCsvClick(nil);
    end
    else
    begin
      Mostra(CARTAO_LOGO);
      AbreImagem(Nome);
    end;
  end;
  DragFinish(Msg.Drop);
end;

procedure TFormPrincipal.Mostra(Cartao: Integer);
begin
  CancelaEdicao;
  ListaNav.ItemIndex := Cartao;
  Cartoes.ActiveCardIndex := Cartao;
  LabelTitulo.Caption := ListaNav.Items[Cartao];
  ListaNav.Invalidate;
end;

procedure TFormPrincipal.PaintMarcaPaint(Sender: TObject);
var
  G: TGPGraphics;
begin
  G := TGPGraphics.Create(PaintMarca.Canvas.Handle);
  try
    DesenhaMarca(G, 0.5, 0.5, PaintMarca.Width - 1);
  finally
    G.Free;
  end;
end;

procedure TFormPrincipal.ListaNavClick(Sender: TObject);
begin
  if ListaNav.ItemIndex >= 0 then
    Mostra(ListaNav.ItemIndex);
end;

// a navegação nas cores do rádio: o escolhido com a faixa verde à esquerda
procedure TFormPrincipal.ListaNavDrawItem(Control: TWinControl; Index: Integer; Rect: TRect; State: TOwnerDrawState);
begin
  ItemDeLista(ListaNav, Index, Rect, Index = ListaNav.ItemIndex, State, COR_NAV, COR_TEXTO_FRACO, 14);
end;

procedure TFormPrincipal.FormKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
begin
  if (Key = Ord('Z')) and (ssCtrl in Shift) and not FEditor.Visible then
  begin
    BotaoDesfazerClick(nil);
    Key := 0;
  end;
end;

procedure TFormPrincipal.AbreImagem(const Caminho: string);
var
  W: TWICImage;
  L: TBytes;
begin
  W := TWICImage.Create;
  try
    W.LoadFromFile(Caminho);
    L := ImagemParaLogo(W, RadioInteira.Checked);
  except
    on E: Exception do
    begin
      W.Free;
      LabelLogoInfo.Font.Color := COR_VERMELHO;
      LabelLogoInfo.Caption := Format(Tr('Não deu para abrir %s: %s', 'Could not open %s: %s'),
                                      [ExtractFileName(Caminho), E.Message]);
      Exit;
    end;
  end;
  FImagem.Free;
  FImagem := W;
  if (W.Width = LOGO_LARGURA) and (W.Height = LOGO_ALTURA) then
    MostraLogo(L, Format(Tr('%s, 160 x 138: vai como está.', '%s, 160 x 138: goes as it is.'),
                         [ExtractFileName(Caminho)]))
  else
    MostraLogo(L, Format('%s, %d x %d.', [ExtractFileName(Caminho), W.Width, W.Height]));
end;

procedure TFormPrincipal.AbreBackup(const Caminho: string);
begin
  try
    FMemoria.Abre(Caminho);
  except
    on E: Exception do
    begin
      Registra(Format(Tr('Não deu para abrir %s: %s', 'Could not open %s: %s'), [ExtractFileName(Caminho), E.Message]));
      Application.MessageBox(PChar(E.Message), PChar(Caption), MB_OK or MB_ICONWARNING);
      Exit;
    end;
  end;
  FOrigem := ExtractFileName(Caminho);
  FDoArquivo := True;
  FNovo := 0;
  Registra(Format(Tr('Backup aberto: %s.', 'Backup opened: %s.'), [Caminho]));
  MostraMemoria;
end;

// A prévia mostra o logo inteiro, com as cores já em RGB565.
procedure TFormPrincipal.MostraLogo(const L: TBytes; const Info: string);
var
  B: TBitmap;
begin
  FLogo := L;
  B := LogoParaBitmap(L);
  try
    Previa.Picture.Assign(B);
  finally
    B.Free;
  end;
  LabelLogoInfo.Font.Color := COR_TEXTO;
  LabelLogoInfo.Caption := Info;
  Habilita;
end;

procedure TFormPrincipal.MostraTopo;
var
  N: Integer;
begin
  if FMemoria.Vazia then
  begin
    LabelRadio.Caption := Tr('Nenhum rádio lido', 'No radio read');
    LabelRadio.Font.Color := COR_TEXTO_FRACO;
    Exit;
  end;
  N := Length(FMemoria.Mudancas);
  if N > 0 then
  begin
    LabelRadio.Caption := Format(Tr('BJ-8300 · firmware %s · %s · %d %s que ainda não foram para o rádio',
                                    'BJ-8300 · firmware %s · %s · %d %s not yet written to the radio'),
                                 [FMemoria.Versao, FOrigem, N, IfThen(N = 1, Tr('mudança', 'change'),
                                                                      Tr('mudanças', 'changes'))]);
    LabelRadio.Font.Color := COR_AMBAR;
  end
  else
  begin
    LabelRadio.Caption := Format('BJ-8300 · firmware %s · %s, %s',
                                 [FMemoria.Versao, FOrigem, FormatDateTime('dd/mm/yyyy hh:nn', FMemoria.Lida)]);
    LabelRadio.Font.Color := COR_TEXTO_FRACO;
  end;
end;

// O alto da janela e as páginas que dependem da memória: o rádio lido ou o backup aberto.
procedure TFormPrincipal.MostraMemoria;
begin
  CancelaEdicao;
  if FMemoria.Vazia then
    LabelBackup.Caption := Tr('Nada lido ainda.', 'Nothing read yet.')
  else
    LabelBackup.Caption := Format(Tr('Aberto: %s'#13#10'Firmware %s, lido em %s. %d canais salvos.',
                                     'Open: %s'#13#10'Firmware %s, read on %s. %d channels saved.'),
                                  [FOrigem, FMemoria.Versao, FormatDateTime('dd/mm/yyyy hh:nn', FMemoria.Lida),
                                   FMemoria.Salvos]);
  if FMemoria.Vazia then
    FNotas := nil
  else
    FNotas := FMemoria.Notas;
  PreencheGrade;
  PreencheBancos;
  PreencheAjustes;
  PreencheNotas;
  MostraTopo;
  Habilita;
end;

// Depois de cada mudança: o alto, as contagens e os botões.
procedure TFormPrincipal.Mudou;
begin
  MostraTopo;
  if not FMemoria.Vazia then
    LabelContagem.Caption := Format(Tr('%d canais salvos de %d', '%d channels saved out of %d'), [FMemoria.Salvos, CANAIS]);
  Habilita;
end;

procedure TFormPrincipal.PreencheLinhaDoCanal(L, N: Integer);
var
  C: TCanal;
  K: Integer;
  Linhas: TArray<string>;
begin
  C := FMemoria.Canal(N);
  if C.Existe then
    Linhas := [IntToStr(N), C.Nome, FrequenciaTexto(C.RX), IfThen(C.TX = 0, Tr('sem TX', 'no TX'), FrequenciaTexto(C.TX)),
               TomTexto(C.TomRX), TomTexto(C.TomTX), Potencia(C.Potencia), IfThen(C.Estreito, 'NARROW', 'WIDE'),
               IfThen(C.AM, 'AM', 'FM'), SimNao(C.BCL), SimNao(C.Varre), TextoScramble(C.Scramble),
               TextoEncrypt(C.Encrypt), IntToStr(C.Scode + 1), PTT_IDS[C.PttId], TextoBancos(C.Bancos)]
  else
    Linhas := [IntToStr(N)];
  Grade.Rows[L].Clear;
  for K := 0 to High(Linhas) do
    Grade.Cells[K, L] := Linhas[K];
end;

// Os canais na grade: os salvos (e os vazios, se pedido, e o novo) que casam com a busca.
procedure TFormPrincipal.PreencheGrade;
var
  N, L: Integer;
  Busca_, Linha: string;
begin
  LabelVazio.Visible := FMemoria.Vazia;
  Grade.Visible := not FMemoria.Vazia;
  PainelFiltro.Visible := not FMemoria.Vazia;
  if FMemoria.Vazia then
    Exit;
  Busca_ := UpperCase(Trim(Busca.Text));
  FLinhas := nil;
  Grade.RowCount := CANAIS + 1;
  for N := 1 to CANAIS do
  begin
    if not FMemoria.Canal(N).Existe and not CheckVazios.Checked and (N <> FNovo) then
      Continue;
    L := Length(FLinhas) + 1;
    PreencheLinhaDoCanal(L, N);
    if Busca_ <> '' then
    begin
      Linha := UpperCase(string.Join(' ', Grade.Rows[L].ToStringArray));
      if not Linha.Contains(Busca_) then
        Continue;
    end;
    FLinhas := FLinhas + [N];
  end;
  Grade.RowCount := Max(2, Length(FLinhas) + 1);
  if FLinhas = nil then
    Grade.Rows[1].Clear;
  Mudou;
end;

procedure TFormPrincipal.PreencheBancos;
var
  K: Integer;
begin
  GradeBancos.RowCount := BANCOS + 1;
  for K := 0 to BANCOS - 1 do
  begin
    GradeBancos.Cells[0, K + 1] := IntToStr(K + 1);
    if FMemoria.Vazia then
    begin
      GradeBancos.Cells[1, K + 1] := '';
      GradeBancos.Cells[2, K + 1] := '';
      GradeBancos.Cells[3, K + 1] := '';
    end
    else
    begin
      GradeBancos.Cells[1, K + 1] := FMemoria.BancoNome(K);
      GradeBancos.Cells[2, K + 1] := SimNao(FMemoria.BancoAtivo(K));
      GradeBancos.Cells[3, K + 1] := IntToStr(FMemoria.CanaisNoBanco(K));
    end;
  end;
end;

// O valor de uma linha dos ajustes como texto.
function ValorAjuste(M: TMemoriaBJ8300; const A: TLinhaAjuste; Original: Boolean = False): string;
var
  V: Integer;
begin
  Result := '';
  case A.Tipo of
    taItem:
      begin
        if Original then
          V := M.ValorOriginal(A.Item)
        else
          V := M.Valor(A.Item);
        if V >= 0 then
          Result := A.Item.Valores[V].Texto
        else
          Result := '?';
      end;
    taTexto:
      Result := M.Texto(A.Nome);
    taFrequencia:
      Result := FrequenciaTexto(M.Frequencia(A.K));
    taOffset:
      Result := Format('%d.%.3d', [M.Offset(A.K) div 1000, M.Offset(A.K) mod 1000]);
    taGrupoDtmf:
      Result := M.GrupoDtmf(A.K);
  end;
end;

{ As linhas dos ajustes: os itens da pasta escolhida (ou, com a busca, os de todas que casam) e, nas pastas que os têm,
  os textos, as frequências e os offsets dos VFOs e os grupos DTMF. }
procedure TFormPrincipal.PreencheAjustes;
var
  Pasta: TPasta;
  Busca_: string;
  L: Integer;

  procedure Poe(const A: TLinhaAjuste);
  begin
    if (Busca_ = '') or UpperCase(A.Rotulo).Contains(Busca_) then
      FAjustes := FAjustes + [A];
  end;

  procedure Pasta_(P: TPasta);
  var
    It: TItem;
    T: TTextoDesc;
    A: TLinhaAjuste;
    K: Integer;
    Prefixo: string;
  begin
    if Busca_ <> '' then
      Prefixo := P.Nome + ' > '
    else
      Prefixo := '';
    for It in P.Itens do
    begin
      A := Default(TLinhaAjuste);
      A.Tipo := taItem;
      A.Item := It;
      A.Rotulo := Prefixo + It.Rotulo;
      Poe(A);
    end;
    for T in FMenu.Textos do
      if (P.Nome = 'APRS (SMS)') and (T.Nome <> 'MORSE MESSAGE') or (P.Nome = 'MORSE') and (T.Nome = 'MORSE MESSAGE') then
      begin
        A := Default(TLinhaAjuste);
        A.Tipo := taTexto;
        A.Nome := T.Nome;
        A.Tamanho := T.Maximo;
        A.Rotulo := Prefixo + T.Nome;
        Poe(A);
      end;
    if (P.Nome = 'CHANNEL') or (P.Nome = 'REPEATER') then
      for K := 0 to 2 do
      begin
        A := Default(TLinhaAjuste);
        if P.Nome = 'CHANNEL' then
        begin
          A.Tipo := taFrequencia;
          A.Rotulo := Prefixo + 'FREQUENCY (VFO ' + Chr(Ord('A') + K) + ')';
        end
        else
        begin
          A.Tipo := taOffset;
          A.Rotulo := Prefixo + 'OFFSET (VFO ' + Chr(Ord('A') + K) + ')';
        end;
        A.K := K;
        Poe(A);
      end;
    if P.Nome = 'DTMF' then
      for K := 0 to GRUPOS_DTMF - 1 do
      begin
        A := Default(TLinhaAjuste);
        A.Tipo := taGrupoDtmf;
        A.K := K;
        A.Rotulo := Prefixo + Format('GROUP %d (%s)', [K + 1, Tr('código', 'code')]);
        Poe(A);
      end;
  end;

begin
  FAjustes := nil;
  Busca_ := UpperCase(Trim(BuscaAjuste.Text));
  if Busca_ <> '' then
    for Pasta in FMenu.Pastas do
      Pasta_(Pasta)
  else if ListaPastas.ItemIndex >= 0 then
    Pasta_(ListaPastas.Items.Objects[ListaPastas.ItemIndex] as TPasta);
  GradeAjustes.RowCount := Max(2, Length(FAjustes) + 1);
  GradeAjustes.Rows[1].Clear;
  for L := 0 to High(FAjustes) do
  begin
    GradeAjustes.Cells[0, L + 1] := FAjustes[L].Rotulo;
    if FMemoria.Vazia then
      GradeAjustes.Cells[1, L + 1] := ''
    else
      GradeAjustes.Cells[1, L + 1] := ValorAjuste(FMemoria, FAjustes[L]);
  end;
  PainelRoger.Visible := (Busca_ = '') and (ListaPastas.ItemIndex >= 0) and
                         (ListaPastas.Items[ListaPastas.ItemIndex] = 'TX');
end;

procedure TFormPrincipal.PreencheNotas;
var
  Total: Integer;
  Erro: string;
  Valem: Boolean;
begin
  FLinhaRoger.Notas := FNotas;
  Total := FLinhaRoger.Total;
  Valem := NotasValidas(FNotas, Erro);
  if FNotas = nil then
    LabelRogerInfo.Caption := Tr('Nenhuma nota: o CUSTOM fica calado.', 'No notes: CUSTOM stays silent.')
  else if Valem then
    LabelRogerInfo.Caption := Format(Tr('%d de %d notas  ·  %s s de %s s', '%d of %d notes  ·  %s s of %s s'),
                                     [Length(FNotas), ROGER_NOTAS, FormatFloat('0.00', Total / 1000),
                                      FormatFloat('0', ROGER_TOTAL_MS / 1000)])
  else
    LabelRogerInfo.Caption := Erro + ' ' + Tr('Assim não vai para o rádio.', 'It will not go to the radio like this.');
  if (FNotas <> nil) and not Valem then
    LabelRogerInfo.Font.Color := COR_VERMELHO
  else
    LabelRogerInfo.Font.Color := COR_TEXTO_FRACO;
  MostraNotaEscolhida;
end;

// os campos da nota escolhida na linha do tempo, e a tecla dela acesa no piano
procedure TFormPrincipal.MostraNotaEscolhida;
var
  K: Integer;
begin
  K := FLinhaRoger.Escolhida;
  FMostrandoNota := True;
  try
    if (K >= 0) and (K <= High(FNotas)) then
    begin
      EditHz.Text := IntToStr(FNotas[K].Hz);
      EditMs.Text := IntToStr(FNotas[K].Ms);
      if FNotas[K].Hz = 0 then
        LabelNomeNota.Caption := Tr('pausa (0 Hz)', 'pause (0 Hz)')
      else
        LabelNomeNota.Caption := NomeDaNota(NotaDoHz(FNotas[K].Hz));
      FTeclado.Marcada := IfThen(FNotas[K].Hz = 0, -1, NotaDoHz(FNotas[K].Hz));
    end
    else
    begin
      EditHz.Text := '';
      EditMs.Text := '';
      LabelNomeNota.Caption := '';
      FTeclado.Marcada := -1;
    end;
  finally
    FMostrandoNota := False;
  end;
  EditHz.Enabled := (K >= 0) and BotaoPausa.Enabled;
  EditMs.Enabled := EditHz.Enabled;
  BotaoApagarNota.Enabled := EditHz.Enabled;
  BotaoTocarDaqui.Enabled := EditHz.Enabled;
end;

procedure TFormPrincipal.RogerAntes(Sender: TObject);
begin
  FMemoria.Guarda;
end;

// a linha do tempo mudou as notas: elas vão para a memória quando valem (nenhuma também: o CUSTOM fica calado)
procedure TFormPrincipal.RogerMudou(Sender: TObject);
var
  Erro: string;
begin
  FNotas := FLinhaRoger.Notas;
  if (FNotas = nil) or NotasValidas(FNotas, Erro) then
    FMemoria.PoeNotas(FNotas);
  PreencheNotas;
  Mudou;
end;

procedure TFormPrincipal.RogerEscolheu(Sender: TObject);
begin
  MostraNotaEscolhida;
end;

procedure TFormPrincipal.RogerFimDoSom(Sender: TObject);
begin
  BotaoOuvir.Caption := Tr('▶  Tocar', '▶  Play');
end;

// depois da escolhida (ou no fim), tocando
procedure TFormPrincipal.PoeNoRoger(const N: TNota);
var
  K: Integer;
begin
  K := FLinhaRoger.Escolhida;
  if K < 0 then
    K := Length(FNotas)
  else
    Inc(K);
  if not FLinhaRoger.Insere(K, N) then
    Avisa(Format(Tr('Não cabe mais: o ROGER vai até %d notas e %d s.', 'No more room: the ROGER takes up to %d notes ' +
                    'and %d s.'), [ROGER_NOTAS, ROGER_TOTAL_MS div 1000]))
  else if N.Hz > 0 then
  begin
    FLinhaRoger.Para;
    TocaTom(N.Hz, 200);
  end;
end;

procedure TFormPrincipal.BotaoPausaClick(Sender: TObject);
var
  N: TNota;
begin
  N.Hz := 0;
  N.Ms := FLinhaRoger.DuracaoNova;
  PoeNoRoger(N);
end;

procedure TFormPrincipal.BotaoApagarNotaClick(Sender: TObject);
begin
  FLinhaRoger.Apaga(FLinhaRoger.Escolhida);
end;

procedure TFormPrincipal.ComboDuracaoNovaChange(Sender: TObject);
begin
  FLinhaRoger.DuracaoNova := DURACOES_NOVAS[Max(0, ComboDuracaoNova.ItemIndex)];
end;

// o Hz e os ms digitados da nota escolhida: valem quando ficam numa faixa do rádio (0 Hz é pausa)
procedure TFormPrincipal.EditNotaChange(Sender: TObject);
var
  K, Hz, Ms: Integer;
  N: TArray<TNota>;
begin
  K := FLinhaRoger.Escolhida;
  if FMostrandoNota or (K < 0) or (K > High(FNotas)) or not TryStrToInt(Trim(EditHz.Text), Hz) or
     not TryStrToInt(Trim(EditMs.Text), Ms) or (Hz <> 0) and ((Hz < 300) or (Hz > 3000)) or (Ms < 10) or
     (Ms > 1000) or (FLinhaRoger.Total - FNotas[K].Ms + Ms > ROGER_TOTAL_MS) or
     (Hz = FNotas[K].Hz) and (Ms = FNotas[K].Ms) then
    Exit;
  FMemoria.Guarda;
  N := Copy(FNotas);
  N[K].Hz := Hz;
  N[K].Ms := Ms;
  FLinhaRoger.Notas := N;
  RogerMudou(nil);
end;

procedure TFormPrincipal.BuscaChange(Sender: TObject);
begin
  CancelaEdicao;
  PreencheGrade;
end;

procedure TFormPrincipal.ListaPastasClick(Sender: TObject);
begin
  CancelaEdicao;
  BuscaAjuste.Text := '';
  PreencheAjustes;
end;

procedure TFormPrincipal.ListaPastasDrawItem(Control: TWinControl; Index: Integer; Rect: TRect;
  State: TOwnerDrawState);
begin
  ItemDeLista(ListaPastas, Index, Rect, odSelected in State, State, COR_FUNDO, COR_TEXTO, 14);
end;

procedure TFormPrincipal.BuscaAjusteChange(Sender: TObject);
begin
  CancelaEdicao;
  PreencheAjustes;
end;

{ As notas do ROGER CUSTOM }

procedure TFormPrincipal.BotaoOuvirClick(Sender: TObject);
begin
  if FLinhaRoger.Tocando then
    FLinhaRoger.Para
  else
  begin
    FLinhaRoger.Toca(0);
    BotaoOuvir.Caption := Tr('■  Parar', '■  Stop');
  end;
end;

procedure TFormPrincipal.BotaoTocarDaquiClick(Sender: TObject);
begin
  FLinhaRoger.Toca(FLinhaRoger.Escolhida);
  BotaoOuvir.Caption := Tr('■  Parar', '■  Stop');
end;

procedure TFormPrincipal.ComboModelosChange(Sender: TObject);
const
  // Hz e ms, uma nota depois da outra
  MODELOS: array[1..4] of string = ('1200 100', '1000 80 1500 120', '800 70 1000 70 1300 110',
                                    '700 150 0 50 700 50 0 50 700 150');
var
  Numeros: TArray<string>;
  K: Integer;
  Erro: string;
begin
  if ComboModelos.ItemIndex < 1 then
    Exit;
  Numeros := MODELOS[ComboModelos.ItemIndex].Split([' ']);
  SetLength(FNotas, Length(Numeros) div 2);
  for K := 0 to High(FNotas) do
  begin
    FNotas[K].Hz := StrToInt(Numeros[2 * K]);
    FNotas[K].Ms := StrToInt(Numeros[2 * K + 1]);
  end;
  ComboModelos.ItemIndex := 0;
  FMemoria.Guarda;
  if NotasValidas(FNotas, Erro) then
    FMemoria.PoeNotas(FNotas);
  PreencheNotas;
  Mudou;
end;

procedure TFormPrincipal.BotaoImportarSomClick(Sender: TObject);
begin
  if DialogoSom.Execute then
  begin
    FArquivoSom := DialogoSom.FileName;
    ImportaSom;
  end;
end;

procedure TFormPrincipal.EditSomChange(Sender: TObject);
begin
  if FArquivoSom <> '' then
    ImportaSom;
end;

{ As notas do arquivo escolhido, do começo e com a duração dos campos: o MIDI pelas notas, o resto pelo som (uma
  aproximação, que se ouve no PC e se ajusta à mão). }
procedure TFormPrincipal.ImportaSom;
var
  Inicio, Duracao: Double;
  N: TArray<TNota>;
  Erro: string;
begin
  if not TryStrToFloat(Trim(EditInicio.Text).Replace(',', '.'), Inicio, TFormatSettings.Invariant) or (Inicio < 0) or
     not TryStrToFloat(Trim(EditDuracao.Text).Replace(',', '.'), Duracao, TFormatSettings.Invariant) or
     (Duracao < 0.1) or (Duracao > ROGER_TOTAL_MS / 1000) then
  begin
    Avisa(Format(Tr('O começo vai em segundos, e a duração de 0,1 a %.0f s.', 'The start goes in seconds, and the ' +
                    'length from 0.1 to %.0f s.'), [ROGER_TOTAL_MS / 1000]));
    Exit;
  end;
  try
    Screen.Cursor := crHourGlass;
    try
      N := NotasDoArquivo(FArquivoSom, Round(Inicio * 1000), Round(Duracao * 100) * 10);
    finally
      Screen.Cursor := crDefault;
    end;
  except
    on E: Exception do
    begin
      Avisa(Format('%s: %s', [ExtractFileName(FArquivoSom), E.Message]));
      Exit;
    end;
  end;
  FMemoria.Guarda;
  FNotas := N;
  if NotasValidas(FNotas, Erro) then
    FMemoria.PoeNotas(FNotas);
  PreencheNotas;
  Mudou;
  if EhMidi(FArquivoSom) then
    Avisa(Format(Tr('%s: a melodia de %.1f s em diante. Ouça no PC; falta gravar no rádio.',
                    '%s: the melody from %.1f s on. Listen on the PC; still to write to the radio.'),
                 [ExtractFileName(FArquivoSom), Inicio]))
  else
    Avisa(Format(Tr('%s: a melodia aproximada de %.1f s em diante (o rádio toca uma nota por vez). Ouça no PC e ajuste ' +
                    'as notas se precisar; falta gravar no rádio.',
                    '%s: the approximate melody from %.1f s on (the radio plays one note at a time). Listen on the PC ' +
                    'and adjust the notes if needed; still to write to the radio.'),
                 [ExtractFileName(FArquivoSom), Inicio]));
end;

{ O desenho das grades: o cabeçalho cinza, as linhas alternadas, a escolhida em verde claro, e o ajuste que mudou e
  ainda não foi para o rádio em âmbar claro. }
procedure TFormPrincipal.GradeDrawCell(Sender: TObject; ACol, ARow: Integer; Rect: TRect; State: TGridDrawState);
var
  G: TStringGrid;
  C: TCanvas;
  Margem: Integer;
  Escolhida, Mudado, Cinza: Boolean;
begin
  G := Sender as TStringGrid;
  C := G.Canvas;
  Margem := MulDiv(6, CurrentPPI, 96);
  Mudado := False;
  Cinza := False;
  if (ARow > 0) and not FMemoria.Vazia then
    if (G = Grade) and (ARow <= Length(FLinhas)) then
      Cinza := not FMemoria.Canal(FLinhas[ARow - 1]).Existe
    else if (G = GradeAjustes) and (ARow <= Length(FAjustes)) then
      Mudado := ValorAjuste(FMemoria, FAjustes[ARow - 1]) <> ValorAjuste(FMemoria, FAjustes[ARow - 1], True);
  Escolhida := (ARow >= G.Selection.Top) and (ARow <= G.Selection.Bottom);
  if ARow = 0 then
  begin
    C.Brush.Color := COR_CABECALHO;
    C.Font.Style := [fsBold];
    C.Font.Color := COR_TEXTO_FRACO;
  end
  else
  begin
    if Escolhida then
      C.Brush.Color := COR_ESCOLHIDA
    else if Mudado then
      C.Brush.Color := COR_MUDOU
    else if ARow mod 2 = 0 then
      C.Brush.Color := COR_SUPERFICIE
    else
      C.Brush.Color := COR_FUNDO;
    C.Font.Style := [];
    if Cinza then
      C.Font.Color := COR_TEXTO_FRACO
    else
      C.Font.Color := COR_TEXTO;
  end;
  C.FillRect(Rect);
  // a célula de agora, como numa planilha: a que o Enter, o F2 ou a digitação editam
  if (ARow > 0) and (ACol = G.Col) and (ARow = G.Row) and G.Focused then
  begin
    C.Brush.Color := COR_VERDE;
    C.FrameRect(Rect);
    C.FrameRect(System.Types.Rect(Rect.Left + 1, Rect.Top + 1, Rect.Right - 1, Rect.Bottom - 1));
  end;
  C.Brush.Style := bsClear;
  C.TextOut(Rect.Left + Margem, Rect.Top + (Rect.Height - C.TextHeight('Ag')) div 2, G.Cells[ACol, ARow]);
  C.Brush.Style := bsSolid;
  C.Pen.Color := COR_DIVISA;
  C.MoveTo(Rect.Left, Rect.Bottom - 1);
  C.LineTo(Rect.Right, Rect.Bottom - 1);
end;

{ A edição na célula }

function TFormPrincipal.Celula(G: TStringGrid; Col, Lin: Integer; out Opcoes: TArray<string>;
  out Atual: string): TTipoCelula;
var
  K: Integer;
  A: TLinhaAjuste;
begin
  Result := tcNada;
  Opcoes := nil;
  Atual := G.Cells[Col, Lin];
  if FMemoria.Vazia or (Lin < 1) then
    Exit;
  if G = Grade then
  begin
    if (Lin > Length(FLinhas)) or not FMemoria.Canal(FLinhas[Lin - 1]).Existe and (Col <> COL_RX) then
      Exit;
    case Col of
      COL_NOME, COL_RX, COL_TX:
        Result := tcTexto;
      COL_TOM_RX, COL_TOM_TX:
        begin
          Result := tcLista;
          Opcoes := ListaDeTons;
        end;
      COL_POTENCIA:
        begin
          Result := tcLista;
          Opcoes := [Potencia(ptAlta), Potencia(ptMedia), Potencia(ptBaixa)];
        end;
      COL_LARGURA:
        begin
          Result := tcLista;
          Opcoes := ['WIDE', 'NARROW'];
        end;
      COL_MODULACAO:
        begin
          Result := tcLista;
          Opcoes := ['FM', 'AM'];
        end;
      COL_BCL, COL_VARRE:
        begin
          Result := tcLista;
          Opcoes := [SimNao(True), SimNao(False)];
        end;
      COL_SCRAMBLE:
        begin
          Result := tcLista;
          for K := 0 to High(SCRAMBLES) do
            Opcoes := Opcoes + [TextoScramble(SCRAMBLES[K])];
        end;
      COL_ENCRYPT:
        begin
          Result := tcLista;
          for K := 0 to 3 do
            Opcoes := Opcoes + [TextoEncrypt(K)];
        end;
      COL_GRUPO:
        begin
          Result := tcLista;
          for K := 1 to GRUPOS_DTMF do
            Opcoes := Opcoes + [IntToStr(K)];
        end;
      COL_PTT_ID:
        begin
          Result := tcLista;
          for K := 0 to High(PTT_IDS) do
            Opcoes := Opcoes + [PTT_IDS[K]];
        end;
      COL_BANCOS:
        Result := tcBancos;
    end;
  end
  else if G = GradeBancos then
  begin
    if Col = 1 then
      Result := tcTexto
    else if Col = 2 then
    begin
      Result := tcLista;
      Opcoes := [SimNao(True), SimNao(False)];
    end;
  end
  else if (G = GradeAjustes) and (Col = 1) and (Lin <= Length(FAjustes)) then
  begin
    A := FAjustes[Lin - 1];
    if A.Tipo = taItem then
    begin
      Result := tcLista;
      for K := 0 to High(A.Item.Valores) do
        Opcoes := Opcoes + [A.Item.Valores[K].Texto];
    end
    else
      Result := tcTexto;
  end
end;

procedure TFormPrincipal.LinhasEscolhidas(out Primeira, Ultima: Integer);
begin
  Primeira := Grade.Selection.Top;
  Ultima := Grade.Selection.Bottom;
  if (Primeira < 1) or (Ultima > Length(FLinhas)) or (Primeira > Ultima) then
  begin
    Primeira := Grade.Row;
    Ultima := Grade.Row;
  end;
end;

// Um campo de um canal a partir do texto da célula; um canal vazio nasce pela frequência de RX.
procedure TFormPrincipal.AplicaCanal(Col, N: Integer; const Valor: string);
var
  C: TCanal;
  F: Cardinal;
  T: Word;
  K: Integer;
  Novo: Boolean;
begin
  C := FMemoria.Canal(N);
  Novo := not C.Existe;
  if Novo then
  begin
    if Col <> COL_RX then
      Exit;
    // um canal novo, como o SAVE do rádio o grava: alta, larga, na varredura, sem tom e sem banco
    C := Default(TCanal);
    C.Numero := N;
    C.Varre := True;
  end;
  case Col of
    COL_NOME:
      C.Nome := NomeValido(UpperCase(Valor));
    COL_RX:
      begin
        if not TextoFrequencia(Valor, F) or not FaixaRX(F) then
          raise EConvertError.Create(Tr('A frequência de RX vai em MHz, de 18 a 1000.',
                                        'The RX frequency goes in MHz, from 18 to 1000.'));
        if Novo or (C.TX = C.RX) then
          C.TX := F;
        C.RX := F;
      end;
    COL_TX:
      if (Valor.Trim = '') or SameText(Valor.Trim, Tr('sem TX', 'no TX')) or (Valor.Trim = '-') then
        C.TX := 0
      else if TextoFrequencia(Valor, F) and FaixaRX(F) then
        C.TX := F
      else
        raise EConvertError.Create(Tr('A frequência de TX vai em MHz, de 18 a 1000; vazia, o canal fica sem TX.',
                                      'The TX frequency goes in MHz, from 18 to 1000; empty, the channel has no TX.'));
    COL_TOM_RX, COL_TOM_TX:
      begin
        if not TomDeTexto(Valor, T) then
          raise EConvertError.Create(Tr('Tom desconhecido.', 'Unknown tone.'));
        if Col = COL_TOM_RX then
          C.TomRX := T
        else
          C.TomTX := T;
      end;
    COL_POTENCIA:
      for var P := Low(TPotencia) to High(TPotencia) do
        if Valor = Potencia(P) then
          C.Potencia := P;
    COL_LARGURA:
      C.Estreito := Valor = 'NARROW';
    COL_MODULACAO:
      C.AM := Valor = 'AM';
    COL_BCL:
      C.BCL := Valor = SimNao(True);
    COL_VARRE:
      C.Varre := Valor = SimNao(True);
    COL_ENCRYPT:
      for K := 0 to 3 do
        if Valor = TextoEncrypt(K) then
          C.Encrypt := K;
    COL_SCRAMBLE:
      for K := 0 to High(SCRAMBLES) do
        if Valor = TextoScramble(SCRAMBLES[K]) then
          C.Scramble := SCRAMBLES[K];
    COL_GRUPO:
      if TryStrToInt(Valor, K) and (K >= 1) and (K <= GRUPOS_DTMF) then
        C.Scode := K - 1;
    COL_PTT_ID:
      for K := 0 to 3 do
        if Valor = PTT_IDS[K] then
          C.PttId := K;
    COL_BANCOS:
      C.Bancos := BancosDeTexto(Valor);
  end;
  FMemoria.PoeCanal(C);
end;

// os dígitos que o rádio guarda num código DTMF
function SoDtmf(const S: string; Max: Integer): string;
begin
  Result := S.ToUpper;
  for var Ch in Result do
    if (Ch = ' ') or (Pos(Ch, DIGITOS_DTMF) = 0) then
      raise EConvertError.Create(Tr('O DTMF só tem 0-9, A-D, * e #.', 'DTMF only has 0-9, A-D, * and #.'));
  if Length(Result) > Max then
    raise EConvertError.CreateFmt(Tr('Até %d dígitos.', 'Up to %d digits.'), [Max]);
end;

// "0.6", "5", "5,000" -> kHz
function TextoOffset(const S: string; out KHz: Cardinal): Boolean;
var
  F: Cardinal;
begin
  Result := TextoFrequencia(S, F) and (F mod 100 = 0) and (F div 100 < 1000000);
  if Result then
    KHz := F div 100;
end;

{ O valor que saiu do editor, no lugar dele: nos canais, em todas as linhas escolhidas. Um valor que não serve não
  muda nada e aparece embaixo. }
procedure TFormPrincipal.Aplica(G: TStringGrid; Col, Lin: Integer; const Valor: string);
var
  Primeira, Ultima, L, V: Integer;
  A: TLinhaAjuste;
  F: Cardinal;
begin
  FMemoria.Guarda;
  try
    if G = Grade then
    begin
      LinhasEscolhidas(Primeira, Ultima);
      if (Lin < Primeira) or (Lin > Ultima) then
      begin
        Primeira := Lin;
        Ultima := Lin;
      end;
      for L := Primeira to Ultima do
      begin
        AplicaCanal(Col, FLinhas[L - 1], Valor);
        PreencheLinhaDoCanal(L, FLinhas[L - 1]);
      end;
      if FLinhas[Lin - 1] = FNovo then
        FNovo := 0;
      PreencheBancos;
    end
    else if G = GradeBancos then
    begin
      if Col = 1 then
        FMemoria.PoeBancoNome(Lin - 1, NomeValido(UpperCase(Valor)))
      else
        FMemoria.PoeBancoAtivo(Lin - 1, Valor = SimNao(True));
      PreencheBancos;
    end
    else if G = GradeAjustes then
    begin
      A := FAjustes[Lin - 1];
      case A.Tipo of
        taItem:
          for V := 0 to High(A.Item.Valores) do
            if A.Item.Valores[V].Texto = Valor then
              FMemoria.PoeValor_(A.Item, V);
        taTexto:
          FMemoria.PoeTexto(A.Nome, TextoDoCampo(A.Nome, Valor, A.Tamanho));
        taFrequencia:
          if TextoFrequencia(Valor, F) and FaixaRX(F) then
            FMemoria.PoeFrequencia(A.K, F)
          else
            raise EConvertError.Create(Tr('A frequência vai em MHz, de 18 a 1000.',
                                          'The frequency goes in MHz, from 18 to 1000.'));
        taOffset:
          if TextoOffset(Valor, F) then
            FMemoria.PoeOffset(A.K, F)
          else
            raise EConvertError.Create(Tr('O offset vai em MHz, até 999.999, de kHz em kHz.',
                                          'The offset goes in MHz, up to 999.999, in kHz steps.'));
        taGrupoDtmf:
          FMemoria.PoeGrupoDtmf(A.K, SoDtmf(Valor, DIGITOS_GRUPO));
      end;
      PreencheAjustes;
    end
  except
    on E: EConvertError do
    begin
      FMemoria.Desfaz;
      Avisa(E.Message);
    end;
  end;
  Mudou;
  G.Invalidate;
end;

procedure TFormPrincipal.IniciaEdicao(G: TStringGrid; Col, Lin: Integer; const Primeira: string);
var
  Opcoes: TArray<string>;
  Atual: string;
  R: TRect;
  K, Largura: Integer;
  Tipo: TTipoCelula;
begin
  CancelaEdicao;
  Tipo := Celula(G, Col, Lin, Opcoes, Atual);
  if Tipo = tcNada then
    Exit;
  FEdGrade := G;
  FEdCol := Col;
  FEdLin := Lin;
  R := G.CellRect(Col, Lin);
  case Tipo of
    tcTexto:
      begin
        FEditor.Parent := G;
        FEditor.SetBounds(R.Left, R.Top, R.Width, R.Height);
        if Primeira <> '' then
          FEditor.Text := Primeira
        else
          FEditor.Text := Atual;
        FEditor.Visible := True;
        FEditor.SetFocus;
        if Primeira <> '' then
          FEditor.SelStart := Length(FEditor.Text)
        else
          FEditor.SelectAll;
      end;
    tcLista:
      begin
        FLista.Parent := G;
        FLista.Items.Clear;
        for K := 0 to High(Opcoes) do
          FLista.Items.Add(Opcoes[K]);
        FLista.ItemIndex := FLista.Items.IndexOf(Atual);
        // do tamanho da célula; só a parte que abre cresce, até a opção mais comprida caber
        FLista.SetBounds(R.Left, R.Top, R.Width, R.Height);
        Largura := R.Width;
        FLista.Canvas.Font := FLista.Font;
        for K := 0 to High(Opcoes) do
          Largura := Max(Largura, FLista.Canvas.TextWidth(Opcoes[K]) + MulDiv(16, CurrentPPI, 96));
        FLista.Visible := True;
        SendMessage(FLista.Handle, CB_SETDROPPEDWIDTH, Largura, 0);
        FLista.SetFocus;
        FLista.DroppedDown := True;
      end;
    tcBancos:
      begin
        FPainelBancos.Parent := Self;
        FListaBancos.Items.Clear;
        for K := 0 to BANCOS - 1 do
        begin
          FListaBancos.Items.Add(Format('%d  %s', [K + 1, IfThen(FMemoria.BancoNome(K) = '',
                                                                 Format('BANK %.2d', [K + 1]), FMemoria.BancoNome(K))]));
          FListaBancos.Checked[K] := BancosDeTexto(Atual) shr K and 1 <> 0;
        end;
        R.TopLeft := ScreenToClient(G.ClientToScreen(R.TopLeft));
        FPainelBancos.SetBounds(Max(0, Min(R.Left, ClientWidth - FPainelBancos.Width)),
                                Max(0, Min(R.Top + G.RowHeights[Lin], ClientHeight - FPainelBancos.Height)),
                                FPainelBancos.Width, FPainelBancos.Height);
        FPainelBancos.Visible := True;
        FPainelBancos.BringToFront;
        FListaBancos.SetFocus;
      end;
  end;
end;

procedure TFormPrincipal.ConfirmaEdicao(Sender: TObject);
var
  G: TStringGrid;
  Valor: string;
begin
  if (FEdGrade = nil) or not (FEditor.Visible or FLista.Visible) then
    Exit;
  G := FEdGrade;
  FEdGrade := nil;
  if FEditor.Visible then
    Valor := FEditor.Text
  else if FLista.ItemIndex >= 0 then
    Valor := FLista.Items[FLista.ItemIndex]
  else
    Valor := G.Cells[FEdCol, FEdLin];
  FEditor.Visible := False;
  FLista.Visible := False;
  if Valor <> G.Cells[FEdCol, FEdLin] then
    Aplica(G, FEdCol, FEdLin, Valor);
  if G.CanFocus then
    G.SetFocus;
end;

procedure TFormPrincipal.CancelaEdicao;
begin
  FEdGrade := nil;
  if FEditor <> nil then
  begin
    FEditor.Visible := False;
    FLista.Visible := False;
    FPainelBancos.Visible := False;
  end;
end;

procedure TFormPrincipal.EditorKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
var
  G: TStringGrid;
begin
  case Key of
    VK_RETURN:
      begin
        G := FEdGrade;
        ConfirmaEdicao(Sender);
        // o Enter desce para a linha de baixo, como numa planilha
        if (G <> nil) and (G.Row < G.RowCount - 1) then
          G.Row := G.Row + 1;
        Key := 0;
      end;
    VK_ESCAPE:
      begin
        G := FEdGrade;
        CancelaEdicao;
        if (G <> nil) and G.CanFocus then
          G.SetFocus;
        Key := 0;
      end;
  end;
end;

// o Enter e o Esc já fizeram o que tinham de fazer no KeyDown: sem o bipe do campo de uma linha
procedure TFormPrincipal.EditorKeyPress(Sender: TObject; var Key: Char);
begin
  if CharInSet(Key, [#13, #27]) then
    Key := #0;
end;

procedure TFormPrincipal.ListaCloseUp(Sender: TObject);
begin
  ConfirmaEdicao(Sender);
end;

procedure TFormPrincipal.BancosOkClick(Sender: TObject);
var
  K: Integer;
  B: Word;
  G: TStringGrid;
begin
  B := 0;
  for K := 0 to BANCOS - 1 do
    if FListaBancos.Checked[K] then
      B := B or 1 shl K;
  G := FEdGrade;
  FPainelBancos.Visible := False;
  FEdGrade := nil;
  if G <> nil then
  begin
    Aplica(G, FEdCol, FEdLin, TextoBancos(B));
    G.SetFocus;
  end;
end;

// a linha inteira se pinta de novo: a grade só repinta as células que mudaram de escolha
procedure TFormPrincipal.GradeSelectCell(Sender: TObject; ACol, ARow: Integer; var CanSelect: Boolean);
begin
  (Sender as TStringGrid).Invalidate;
end;

procedure TFormPrincipal.GradeDblClick(Sender: TObject);
var
  G: TStringGrid;
  P: TPoint;
  Col, Lin: Integer;
begin
  G := Sender as TStringGrid;
  P := G.ScreenToClient(Mouse.CursorPos);
  G.MouseToCell(P.X, P.Y, Col, Lin);
  if Lin >= 1 then
  begin
    G.Col := Col;
    IniciaEdicao(G, Col, Lin);
  end;
end;

procedure TFormPrincipal.GradeKeyDown(Sender: TObject; var Key: Word; Shift: TShiftState);
var
  G: TStringGrid;
begin
  G := Sender as TStringGrid;
  case Key of
    VK_RETURN, VK_F2, VK_SPACE:
      begin
        IniciaEdicao(G, G.Col, G.Row);
        Key := 0;
      end;
    VK_DELETE:
      if G = Grade then
      begin
        BotaoApagarCanalClick(nil);
        Key := 0;
      end;
    Ord('C'):
      if (ssCtrl in Shift) and (G = Grade) then
      begin
        ItemCopiarClick(nil);
        Key := 0;
      end;
    Ord('V'):
      if (ssCtrl in Shift) and (G = Grade) then
      begin
        ItemColarClick(nil);
        Key := 0;
      end;
  end;
end;

// Começar a digitar numa célula de texto já abre o editor com a letra.
procedure TFormPrincipal.GradeKeyPress(Sender: TObject; var Key: Char);
var
  G: TStringGrid;
  Opcoes: TArray<string>;
  Atual: string;
begin
  G := Sender as TStringGrid;
  if (Key > ' ') and (Celula(G, G.Col, G.Row, Opcoes, Atual) = tcTexto) then
  begin
    IniciaEdicao(G, G.Col, G.Row, Key);
    Key := #0;
  end;
end;

procedure TFormPrincipal.BotaoNovoCanalClick(Sender: TObject);
var
  N, L: Integer;
begin
  for N := 1 to CANAIS do
    if not FMemoria.Canal(N).Existe then
    begin
      FNovo := N;
      Busca.Text := '';
      PreencheGrade;
      for L := 0 to High(FLinhas) do
        if FLinhas[L] = N then
        begin
          Grade.Row := L + 1;
          Grade.Col := COL_RX;
          Grade.SetFocus;
          IniciaEdicao(Grade, COL_RX, L + 1);
          Avisa(Format(Tr('Canal %d: digite a frequência de RX, em MHz.', 'Channel %d: type the RX frequency, in MHz.'),
                       [N]));
        end;
      Exit;
    end;
  Avisa(Tr('Os 960 canais estão ocupados.', 'All 960 channels are taken.'));
end;

procedure TFormPrincipal.BotaoApagarCanalClick(Sender: TObject);
var
  Primeira, Ultima, L, N: Integer;
begin
  LinhasEscolhidas(Primeira, Ultima);
  if (Primeira < 1) or (Ultima > Length(FLinhas)) then
    Exit;
  N := 0;
  for L := Primeira to Ultima do
    if FMemoria.Canal(FLinhas[L - 1]).Existe then
      Inc(N);
  if (N = 0) or (N > 1) and not Confirma(Format(Tr('Apagar %d canais?', 'Delete %d channels?'), [N])) then
    Exit;
  FMemoria.Guarda;
  for L := Primeira to Ultima do
    FMemoria.ApagaCanal(FLinhas[L - 1]);
  PreencheGrade;
  PreencheBancos;
end;

// as linhas escolhidas como texto separado por TAB, uma por canal, com o número na frente
procedure TFormPrincipal.ItemCopiarClick(Sender: TObject);
var
  Primeira, Ultima, L: Integer;
  S: TStringList;
begin
  LinhasEscolhidas(Primeira, Ultima);
  S := TStringList.Create;
  try
    for L := Primeira to Ultima do
      if (L >= 1) and (L <= Length(FLinhas)) and FMemoria.Canal(FLinhas[L - 1]).Existe then
        S.Add(string.Join(#9, Grade.Rows[L].ToStringArray));
    Clipboard.AsText := S.Text;
    Avisa(Format(Tr('%d canais copiados.', '%d channels copied.'), [S.Count]));
  finally
    S.Free;
  end;
end;

{ O que foi copiado (daqui ou de uma planilha, com as mesmas colunas) entra a partir da linha de agora, um canal por
  linha, na ordem: os números de onde veio não importam. }
procedure TFormPrincipal.ItemColarClick(Sender: TObject);
var
  Linhas: TStringList;
  Campos: TArray<string>;
  L, N, Col: Integer;
begin
  if (Grade.Row < 1) or (Grade.Row > Length(FLinhas)) then
    Exit;
  Linhas := TStringList.Create;
  try
    Linhas.Text := Clipboard.AsText;
    FMemoria.Guarda;
    N := FLinhas[Grade.Row - 1];
    try
      for L := 0 to Linhas.Count - 1 do
      begin
        if (N > CANAIS) or (Linhas[L].Trim = '') then
          Continue;
        Campos := Linhas[L].Split([#9]);
        if Length(Campos) <= COL_RX then
          Continue;
        FMemoria.ApagaCanal(N);
        AplicaCanal(COL_RX, N, Campos[COL_RX]);
        for Col := COL_NOME to Min(High(Campos), COL_BANCOS) do
          if Col <> COL_RX then
            AplicaCanal(Col, N, Campos[Col]);
        Inc(N);
      end;
    except
      on E: EConvertError do
      begin
        FMemoria.Desfaz;
        Avisa(E.Message);
      end;
    end;
  finally
    Linhas.Free;
  end;
  PreencheGrade;
  PreencheBancos;
end;

procedure TFormPrincipal.BotaoImportarCsvClick(Sender: TObject);
var
  Canais: TArray<TCanal>;
  Erros: TArray<string>;
  C: TCanal;
  E: string;
begin
  if (Sender <> nil) and not DialogoCsv.Execute then
    Exit;
  Canais := CsvParaCanais(TFile.ReadAllText(DialogoCsv.FileName), FMemoria.Canal, Erros);
  for E in Erros do
    Registra(E);
  if Canais = nil then
  begin
    Avisa(Tr('Nenhum canal no CSV.', 'No channel in the CSV.'));
    Exit;
  end;
  if not Confirma(Format(Tr('Importar %d canais do CSV (%d linhas não servem, veja o Registro)? Os que já existem com o ' +
                            'mesmo número são substituídos.',
                            'Import %d channels from the CSV (%d lines do not fit, see the Log)? The existing ones with ' +
                            'the same number are replaced.'), [Length(Canais), Length(Erros)])) then
    Exit;
  FMemoria.Guarda;
  for C in Canais do
    FMemoria.PoeCanal(C);
  PreencheGrade;
  PreencheBancos;
  Avisa(Format(Tr('%d canais importados; falta gravar no rádio.', '%d channels imported; still to write to the radio.'),
               [Length(Canais)]));
end;

procedure TFormPrincipal.BotaoExportarCsvClick(Sender: TObject);
begin
  DialogoSalvarCsv.FileName := 'BJ8300-canais.csv';
  if not DialogoSalvarCsv.Execute then
    Exit;
  TFile.WriteAllText(DialogoSalvarCsv.FileName, CanaisParaCsv(FMemoria), TEncoding.UTF8);
  Avisa(Format(Tr('CSV salvo em %s.', 'CSV saved to %s.'), [DialogoSalvarCsv.FileName]));
end;

{ As portas: as seriais (o cabo), os rádios que a última procura achou pelo Bluetooth LE, e por último o item que
  procura de novo. }
procedure TFormPrincipal.ListaAsPortas;
var
  Antes: string;
  I, Ch340, NCh340: Integer;
begin
  if (ComboPorta.ItemIndex >= 0) and (ComboPorta.ItemIndex < ComboPorta.Items.Count - 1) then
    Antes := ComboPorta.Items[ComboPorta.ItemIndex]
  else
    Antes := '';
  FPortas := ListaPortas;
  ComboPorta.Items.Clear;
  Ch340 := -1;
  NCh340 := 0;
  for I := 0 to High(FPortas) do
  begin
    ComboPorta.Items.Add(FPortas[I].Descricao);
    if FPortas[I].Ch340 then
    begin
      Inc(NCh340);
      Ch340 := I;
    end;
  end;
  for I := 0 to High(FRadiosBle) do
    ComboPorta.Items.Add(Format('Bluetooth: %s (%s)', [FRadiosBle[I].Nome, FRadiosBle[I].Endereco]));
  ComboPorta.Items.Add(Tr('Bluetooth: procurar o rádio...', 'Bluetooth: search for the radio...'));
  ComboPorta.ItemIndex := ComboPorta.Items.IndexOf(Antes);
  // Sozinha, só a única porta do CH340 ou a única porta: com duas, a outra pode ter outro rádio.
  if ComboPorta.ItemIndex < 0 then
    if NCh340 = 1 then
      ComboPorta.ItemIndex := Ch340
    else if (Length(FPortas) = 1) and (FRadiosBle = nil) then
      ComboPorta.ItemIndex := 0;
  if (Length(FPortas) = 0) and (FRadiosBle = nil) then
    Registra(Tr('Nenhuma porta serial. Ligue o cabo e clique em Atualizar; se ele não aparecer, instale o driver ' +
                'CH341SER da WCH, o fabricante do chip do cabo. Para o Bluetooth, escolha o último item da lista.',
                'No serial port. Plug in the cable and click Refresh; if it does not show up, install the CH341SER ' +
                'driver from WCH, the maker of the cable''s chip. For Bluetooth, choose the last item of the list.'))
  else if ComboPorta.ItemIndex < 0 then
    Registra(Tr('Escolha a porta do cabo.', 'Choose the cable port.'));
  Habilita;
end;

{ O que está escolhido na lista: o índice da porta serial (0..), a do rádio Bluetooth (depois das portas), PROCURA_BLE
  no item que procura, e -2 sem nada. }
function TFormPrincipal.Escolhida: Integer;
begin
  Result := ComboPorta.ItemIndex;
  if Result < 0 then
    Result := -2
  else if Result = ComboPorta.Items.Count - 1 then
    Result := PROCURA_BLE;
end;

procedure TFormPrincipal.Habilita;
var
  Livre, ComPorta, Ajusta, Edita, Remoto: Boolean;
begin
  Livre := FTarefa = nil;
  Remoto := (FTarefa <> nil) and (FAcao = acRemoto);
  ComPorta := Livre and (Escolhida >= 0);
  Edita := Livre and not FMemoria.Vazia;
  ComboPorta.Enabled := Livre;
  BotaoAtualizar.Enabled := Livre;
  BotaoLer.Enabled := ComPorta;
  BotaoGravarRadio.Enabled := ComPorta and FMemoria.Mudou;
  BotaoDesfazer.Enabled := Edita and FMemoria.PodeDesfazer;
  Grade.Enabled := Edita;
  GradeBancos.Enabled := Edita;
  GradeAjustes.Enabled := Edita;
  FLinhaRoger.Enabled := Edita;
  FTeclado.Enabled := Edita;
  BotaoPausa.Enabled := Edita;
  ComboDuracaoNova.Enabled := Edita;
  ComboModelos.Enabled := Edita;
  BotaoImportarSom.Enabled := Edita;
  EditInicio.Enabled := Edita;
  EditDuracao.Enabled := Edita;
  BotaoOuvir.Enabled := FNotas <> nil;
  MostraNotaEscolhida;
  BotaoNovoCanal.Enabled := Edita;
  BotaoApagarCanal.Enabled := Edita;
  BotaoImportarCsv.Enabled := Edita;
  BotaoExportarCsv.Enabled := Edita;
  BotaoImagem.Enabled := Livre;
  // o ajuste só vale para a imagem escolhida que não tem o tamanho do logo
  Ajusta := Livre and (FImagem <> nil) and ((FImagem.Width <> LOGO_LARGURA) or (FImagem.Height <> LOGO_ALTURA));
  RadioPreencher.Enabled := Ajusta;
  RadioInteira.Enabled := Ajusta;
  BotaoLerLogo.Enabled := ComPorta;
  BotaoSalvarLogo.Enabled := Livre and (FLogo <> nil);
  BotaoGravarLogo.Enabled := ComPorta and (FLogo <> nil);
  BotaoConectar.Enabled := ComPorta or Remoto;
  if Remoto then
    BotaoConectar.Caption := Tr('Desconectar', 'Disconnect')
  else
    BotaoConectar.Caption := Tr('Conectar', 'Connect');
  PainelTeclado.Enabled := Remoto;
  BotaoAbrirBackup.Enabled := Livre;
  BotaoSalvarBackup.Enabled := Edita;
  BotaoCancelar.Visible := not Livre and not (FAcao in [acProcurar, acRemoto]);
  Barra.Visible := not Livre and (FAcao <> acRemoto);
end;

procedure TFormPrincipal.Registra(const Msg: string);
begin
  MemoRegistro.Lines.Add(FormatDateTime('hh:nn:ss  ', Now) + Msg);
  LabelEstado.Caption := Msg;
  LabelEstado.Font.Color := COR_TEXTO_FRACO;
end;

// um aviso de uma ação na janela: embaixo, em destaque, e no registro
procedure TFormPrincipal.Avisa(const Msg: string);
begin
  Registra(Msg);
  LabelEstado.Font.Color := COR_AMBAR;
end;

// O que chega da thread vai para a janela pela fila da thread principal. (Com o método anônimo direto no
// TThread.Queue, o compilador não acha a sobrecarga.)
procedure TFormPrincipal.NaJanela(P: TThreadProcedure);
begin
  System.Classes.TThread.Queue(nil, P);
end;

// Os avisos e o andamento chegam da thread.
function TFormPrincipal.Avisos: TProc<string>;
begin
  Result :=
    procedure(Msg: string)
    begin
      NaJanela(procedure begin Registra(Msg) end);
    end;
end;

function TFormPrincipal.Andamento: TProc<Integer, Integer>;
begin
  Result :=
    procedure(Feito, Total: Integer)
    begin
      NaJanela(
        procedure
        begin
          Barra.Max := Max(Total, 1);
          Barra.Position := Feito;
        end);
    end;
end;

function TFormPrincipal.Porta: string;
begin
  Result := ComboPorta.Items[ComboPorta.ItemIndex];
end;

// O caminho do item escolhido, para a thread abrir: a serial, ou a conexão com o rádio Bluetooth.
function TFormPrincipal.AbreTransporte: TFunc<ITransporte>;
var
  K: Integer;
  Nome: string;
  Radio: TRadioBLE;
  Registra_: TProc<string>;
begin
  K := Escolhida;
  if K < Length(FPortas) then
  begin
    Nome := FPortas[K].Nome;
    Result := function: ITransporte begin Result := TPortaSerial.Create(Nome) end;
  end
  else
  begin
    Radio := FRadiosBle[K - Length(FPortas)];
    Registra_ := Avisos();
    Result := function: ITransporte begin Result := TTransporteBLE.Create(Radio.Aparelho, Registra_) end;
  end;
end;

function TFormPrincipal.Confirma(const Pergunta: string): Boolean;
begin
  Result := Application.MessageBox(PChar(Pergunta), PChar(Caption),
                                   MB_YESNO or MB_ICONWARNING or MB_DEFBUTTON2) = IDYES;
end;

// Antes de trocar a memória (ler o rádio, abrir um backup, sair): as mudanças que não foram para o rádio se perdem.
function TFormPrincipal.DescartaMudancas: Boolean;
begin
  Result := not FMemoria.Mudou or Confirma(Tr(
    'As mudanças que ainda não foram para o rádio se perdem. Para guardá-las, responda Não e use Salvar backup.' +
    #13#10#13#10'Continuar assim mesmo?',
    'The changes not yet written to the radio are lost. To keep them, answer No and use Save backup.' + #13#10#13#10 +
    'Continue anyway?'));
end;

function TFormPrincipal.NovoCabo: TCaboBJ;
begin
  FCabo := TCaboBJ.Create;
  FCabo.OnRegistro := Avisos();
  FCabo.OnProgresso := Andamento();
  Result := FCabo;
end;

procedure TFormPrincipal.Inicia(Acao: TAcao; Corpo: TProc<ITransporte>);
begin
  CancelaEdicao;
  FAcao := Acao;
  Barra.Position := 0;
  if Acao in [acProcurar, acRemoto] then   // o remoto abre o transporte ele mesmo, e de novo depois de uma queda
    FTarefa := TTarefa.Create(nil, Corpo)
  else
    FTarefa := TTarefa.Create(AbreTransporte(), Corpo);
  FTarefa.OnTerminate := Terminou;
  FTarefa.Start;
  Habilita;
end;

procedure TFormPrincipal.BotaoAtualizarClick(Sender: TObject);
begin
  ListaAsPortas;
end;

// O último item da lista procura o rádio pelo Bluetooth LE por 6 s; os achados entram na lista.
procedure TFormPrincipal.ComboPortaChange(Sender: TObject);
begin
  if Escolhida = PROCURA_BLE then
  begin
    Registra(Tr('Procurando rádios pelo Bluetooth (6 s). O BLUETOOTH do rádio (menu SYSTEM) tem que estar ON.',
                'Searching for radios over Bluetooth (6 s). The radio''s BLUETOOTH (SYSTEM menu) must be ON.'));
    FAchados := nil;
    Inicia(acProcurar,
      procedure(T: ITransporte)
      begin
        FAchados := ProcuraRadios(6000);
      end);
    Exit;
  end;
  Habilita;
end;

procedure TFormPrincipal.BotaoLerClick(Sender: TObject);
var
  C: TCaboBJ;
  M: TMemoriaBJ8300;
begin
  if not DescartaMudancas then
    Exit;
  C := NovoCabo;
  C.Total := BlocosDaLeitura;
  C.Feito := 0;
  FLida := TMemoriaBJ8300.Create(FMenu);
  M := FLida;
  Registra(Tr('Lendo o rádio...', 'Reading the radio...'));
  Inicia(acLer,
    procedure(T: ITransporte)
    begin
      C.Sessao(T,
        procedure
        begin
          M.LeDoRadio(C);
        end);
    end);
end;

procedure TFormPrincipal.BotaoGravarRadioClick(Sender: TObject);
var
  C: TCaboBJ;
  M: TMemoriaBJ8300;
  Restaurar: Boolean;
begin
  CancelaEdicao;
  FMemoria.Guarda;
  FMemoria.PreparaGravacao;
  FBancosGravados := FMemoria.MudouBancos;
  Restaurar := FDoArquivo;
  if not Confirma(Format(Tr(
       'Gravar no rádio pela %s: %d mudanças (blocos de canais, ajustes, bancos e textos)?%s'#13#10#13#10 +
       'O rádio fica mudo enquanto grava%s.',
       'Write to the radio on %s: %d changes (blocks of channels, settings, banks and texts)?%s'#13#10#13#10 +
       'The radio is muted while writing%s.'),
       [Porta, Length(FMemoria.Mudancas),
        IfThen(Restaurar, Tr(#13#10#13#10'É um backup aberto: o que está nele substitui o que o rádio tem nesses ' +
                             'lugares.',
                             #13#10#13#10'It is an opened backup: what it holds replaces what the radio has in these ' +
                             'places.'), ''),
        IfThen(FBancosGravados, Tr(', e depois é preciso desligar e ligar o rádio para ele montar os bancos',
                                   ', and afterwards the radio must be turned off and on to build the banks'), '')])) then
  begin
    FMemoria.Desfaz;
    Exit;
  end;
  C := NovoCabo;
  M := FMemoria;
  Registra(Tr('Gravando no rádio...', 'Writing to the radio...'));
  Inicia(acGravar,
    procedure(T: ITransporte)
    begin
      C.Sessao(T,
        procedure
        begin
          M.GravaNoRadio(C, Restaurar);
        end);
    end);
end;

procedure TFormPrincipal.BotaoDesfazerClick(Sender: TObject);
begin
  CancelaEdicao;
  if (FTarefa = nil) and FMemoria.Desfaz then
  begin
    PreencheGrade;
    PreencheBancos;
    PreencheAjustes;
    Mudou;
    Grade.Invalidate;
  end;
end;

procedure TFormPrincipal.BotaoImagemClick(Sender: TObject);
begin
  if DialogoImagem.Execute then
    AbreImagem(DialogoImagem.FileName);
end;

procedure TFormPrincipal.RadioAjusteClick(Sender: TObject);
begin
  if FImagem <> nil then
    MostraLogo(ImagemParaLogo(FImagem, RadioInteira.Checked), LabelLogoInfo.Caption);
end;

procedure TFormPrincipal.BotaoLerLogoClick(Sender: TObject);
var
  C: TCaboBJ;
begin
  C := NovoCabo;
  Inicia(acLerLogo,
    procedure(T: ITransporte)
    begin
      C.Sessao(T,
        procedure
        begin
          FLogoLido := LeLogo(C);
        end);
    end);
end;

procedure TFormPrincipal.BotaoSalvarLogoClick(Sender: TObject);
var
  B: TBitmap;
  P: TPngImage;
begin
  if not DialogoSalvar.Execute then
    Exit;
  // as 160 colunas, para o logo voltar igual se for gravado de novo
  B := LogoParaBitmap(FLogo, LOGO_LARGURA);
  P := TPngImage.Create;
  try
    P.Assign(B);
    P.SaveToFile(DialogoSalvar.FileName);
  finally
    P.Free;
    B.Free;
  end;
  Registra(Format(Tr('Logo salvo em %s.', 'Logo saved to %s.'), [DialogoSalvar.FileName]));
end;

procedure TFormPrincipal.BotaoGravarLogoClick(Sender: TObject);
var
  C: TCaboBJ;
  L: TBytes;
begin
  if not Confirma(Format(Tr(
       'Gravar este logo no rádio pela %s?'#13#10#13#10 +
       'O logo que está nele hoje é substituído. Para guardar uma cópia, responda Não e use antes Ler do rádio.',
       'Write this logo to the radio on %s?'#13#10#13#10 +
       'The logo it has today is replaced. To keep a copy, answer No and use Read from radio first.'), [Porta])) then
    Exit;
  C := NovoCabo;
  L := FLogo;
  Inicia(acGravarLogo,
    procedure(T: ITransporte)
    begin
      C.Sessao(T,
        procedure
        begin
          GravaLogo(C, L);
        end);
    end);
end;

procedure TFormPrincipal.BotaoAbrirBackupClick(Sender: TObject);
begin
  if not DescartaMudancas then
    Exit;
  if TDirectory.Exists(PastaDosBackups) then
    DialogoBackup.InitialDir := PastaDosBackups;
  if DialogoBackup.Execute then
    AbreBackup(DialogoBackup.FileName);
end;

procedure TFormPrincipal.BotaoSalvarBackupClick(Sender: TObject);
begin
  DialogoSalvarBackup.FileName := 'BJ8300-' + FormatDateTime('yyyymmdd-hhnn', Now) + EXTENSAO;
  if not DialogoSalvarBackup.Execute then
    Exit;
  FMemoria.Salva(DialogoSalvarBackup.FileName);
  Registra(Format(Tr('Backup salvo em %s.', 'Backup saved to %s.'), [DialogoSalvarBackup.FileName]));
end;

procedure TFormPrincipal.BotaoPastaBackupsClick(Sender: TObject);
begin
  TDirectory.CreateDirectory(PastaDosBackups);
  ShellExecute(Handle, 'open', PChar(PastaDosBackups), nil, nil, SW_SHOWNORMAL);
end;

{ O controle remoto }

procedure TFormPrincipal.BotaoConectarClick(Sender: TObject);
var
  R: TRemoto;
  Tela: Boolean;
  Abre: TFunc<ITransporte>;
begin
  if (FTarefa <> nil) and (FAcao = acRemoto) then
  begin
    FRemoto.Parar := True;
    Exit;
  end;
  // a tela só se pede pelo cabo: pelo Bluetooth do Windows não há prova de que o retrato (uns 500 B a cada quadro)
  // passa sem atrasar as teclas
  Tela := Escolhida < Length(FPortas);
  FRemoto := TRemoto.Create;
  R := FRemoto;
  R.Apaga(not FChaveTela.Ligada);
  R.OnRegistro := Avisos();
  R.OnEstado :=
    procedure(E: TEstadoRadio)
    var
      Apagando: Boolean;
    begin
      Apagando := R.Apagando;   // aqui, na thread do remoto: na janela o R pode já não existir
      NaJanela(
        procedure
        var
          S: string;
        begin
          FConectado := True;
          FEstadoRemoto := E;
          // a chave segue o rádio: uma tecla dele acendeu a tela apagada (v165)
          FChaveTela.Ligada := not Apagando;
          if E.TX = 1 then
            S := Tr('Transmitindo', 'Transmitting')
          else if E.RX = 4 then
            S := Format(Tr('Recebendo na %s · %s', 'Receiving on %s · %s'), [E.Letra, TextoSinal(E.Sinal)])
          else if E.RX = 5 then
            S := Tr('Monitor', 'Monitor')
          else
            S := Tr('Em espera', 'Standby');
          LabelRemoto.Caption := S + Format(' · %d,%d V', [E.Tensao div 10, E.Tensao mod 10]);
          if FTela.Empty then
            PaintVisor.Invalidate;
        end);
    end;
  R.OnTela :=
    procedure(Pixels: TBytes; Codigo: Integer)
    begin
      NaJanela(
        procedure
        var
          B: TBitmap;
        begin
          if Pixels <> nil then
          begin
            B := LogoParaBitmap(Pixels, TELA_LARGURA, TELA_ALTURA);
            try
              FTela.Assign(B);
            finally
              B.Free;
            end;
          end;
          FAvisoTela := AvisoDaTela(Codigo);
          PaintVisor.Invalidate;
        end);
    end;
  FConectado := False;
  FTela.Assign(nil);
  FAvisoTela := '';
  R.OnQueda :=
    procedure
    begin
      NaJanela(
        procedure
        begin
          LabelRemoto.Caption := Tr('Reconectando...', 'Reconnecting...');
          FAvisoTela := Tr('A ligação com o rádio caiu: reconectando...', 'The link to the radio dropped: reconnecting...');
          PaintVisor.Invalidate;
        end);
    end;
  Abre := AbreRemoto();
  Inicia(acRemoto,
    procedure(T: ITransporte)
    begin
      R.ExecutaReligando(Abre, Tela);
    end);
  PaintVisor.Invalidate;
end;

// A tela do rádio apagada pelo controle remoto (firmware v164): vale também com ele já conectado. A chave mostra o
// pedido: uma tecla do rádio que acende a tela a volta para Acesa (o OnEstado).
procedure TFormPrincipal.ChaveTelaClick(Sender: TObject);
begin
  if FRemoto <> nil then
    FRemoto.Apaga(not FChaveTela.Ligada);
end;

{ O transporte do controle remoto, que ele abre de novo depois de uma queda: pela serial, a porta escolhida e, se ela
  sumiu (o USB do cabo caiu com o RF do PTT e voltou com outro nome), a única porta do CH340 que houver; pelo Bluetooth,
  o mesmo rádio. }
function TFormPrincipal.AbreRemoto: TFunc<ITransporte>;
var
  K: Integer;
  Nome: string;
  Registra_: TProc<string>;
begin
  K := Escolhida;
  if K >= Length(FPortas) then
    Exit(AbreTransporte());
  Nome := FPortas[K].Nome;
  Registra_ := Avisos();
  Result :=
    function: ITransporte
    var
      P: TPorta;
      Ch340: TArray<string>;
    begin
      try
        Result := TPortaSerial.Create(Nome);
      except
        on EInOutError do
        begin
          Ch340 := nil;
          for P in ListaPortas do
            if P.Ch340 then
              Ch340 := Ch340 + [P.Nome];
          if (Length(Ch340) <> 1) or (Ch340[0] = Nome) then
            raise;
          Nome := Ch340[0];
          Registra_(Format(Tr('O cabo voltou como %s.', 'The cable came back as %s.'), [Nome]));
          Result := TPortaSerial.Create(Nome);
        end;
      end;
    end;
end;

{ As teclas e o PTT ficam apertados no rádio enquanto o botão esquerdo do mouse está apertado neles, como no teclado: o
  longo de 1 s e a repetição das setas saem do próprio rádio. }
procedure TFormPrincipal.TeclaDesce(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  if (Button <> mbLeft) or (FRemoto = nil) then
    Exit;
  if Sender = FBotaoPtt then
  begin
    FRemoto.Ptt(True);
    FBotaoPtt.Caption := Tr('TRANSMITINDO  ·  solte para parar', 'TRANSMITTING  ·  release to stop');
  end
  else
    FRemoto.Aperta(TControl(Sender).Tag);
  FSegurado.Enabled := True;
end;

procedure TFormPrincipal.TeclaSobe(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  if Button = mbLeft then
    SoltaTudo;
end;

procedure TFormPrincipal.SoltaTudo;
begin
  FSegurado.Enabled := False;
  FBotaoPtt.Caption := Tr('PTT  ·  segure para transmitir', 'PTT  ·  hold to transmit');
  if FRemoto <> nil then
  begin
    FRemoto.Solta;
    FRemoto.Ptt(False);
  end;
end;

// O soltar que o botão não viu (o mouse solto em outra janela, a janela que perdeu o foco): solta do mesmo jeito.
procedure TFormPrincipal.ConfereSegurado(Sender: TObject);
begin
  if GetAsyncKeyState(VK_LBUTTON) and $8000 = 0 then
    SoltaTudo;
end;

{ O visor: a tela do rádio, na proporção dela e no meio, com o aviso do que falta embaixo. Sem ela (o Bluetooth, ou
  antes de chegar), o estado: a letra de quem recebe, o S-meter em 24 segmentos e a tensão; e antes de tudo, o que está
  acontecendo. }
procedure TFormPrincipal.PaintVisorPaint(Sender: TObject);
var
  C: TCanvas;
  W, H, K, X0, Y0, L: Integer;
  Texto: string;
  E: TEstadoRadio;
  Faixa: TRect;
begin
  C := PaintVisor.Canvas;
  C.Brush.Color := COR_NAV;
  C.FillRect(PaintVisor.ClientRect);
  if not FTela.Empty then
  begin
    W := Min(PaintVisor.Width, PaintVisor.Height * TELA_LARGURA div TELA_ALTURA);
    H := W * TELA_ALTURA div TELA_LARGURA;
    C.StretchDraw(System.Types.Rect((PaintVisor.Width - W) div 2, (PaintVisor.Height - H) div 2,
                                    (PaintVisor.Width + W) div 2, (PaintVisor.Height + H) div 2), FTela);
    if FAvisoTela <> '' then
    begin
      C.Font.Height := -MulDiv(13, CurrentPPI, 96);
      C.Font.Style := [];
      Faixa := System.Types.Rect(0, 0, W - MulDiv(24, CurrentPPI, 96), 0);
      DrawText(C.Handle, PChar(FAvisoTela), -1, Faixa, DT_CALCRECT or DT_WORDBREAK or DT_CENTER);
      Faixa.Width := W - MulDiv(24, CurrentPPI, 96);
      OffsetRect(Faixa, (PaintVisor.Width - Faixa.Width) div 2,
                 (PaintVisor.Height + H) div 2 - Faixa.Height - MulDiv(12, CurrentPPI, 96));
      C.Brush.Color := COR_NAV;
      C.FillRect(System.Types.Rect(Faixa.Left - MulDiv(8, CurrentPPI, 96), Faixa.Top - MulDiv(6, CurrentPPI, 96),
                                   Faixa.Right + MulDiv(8, CurrentPPI, 96), Faixa.Bottom + MulDiv(6, CurrentPPI, 96)));
      C.Brush.Style := bsClear;
      C.Font.Color := COR_AMBAR;
      DrawText(C.Handle, PChar(FAvisoTela), -1, Faixa, DT_WORDBREAK or DT_CENTER);
    end;
    Exit;
  end;
  C.Brush.Style := bsClear;
  E := FEstadoRemoto;
  if (FTarefa <> nil) and (FAcao = acRemoto) and E.Valido then
  begin
    // a letra grande, o medidor embaixo dela e o estado
    C.Font.Height := -MulDiv(96, CurrentPPI, 96);
    C.Font.Style := [fsBold];
    C.Font.Color := IfThen(E.TX = 1, COR_AMBAR, IfThen(E.RX = 4, COR_VERDE, COR_TEXTO_FRACO));
    Texto := IfThen(E.TX = 1, 'TX', E.Letra);
    C.TextOut((PaintVisor.Width - C.TextWidth(Texto)) div 2, MulDiv(24, CurrentPPI, 96), Texto);
    C.Font.Style := [];
    L := MulDiv(12, CurrentPPI, 96);
    X0 := (PaintVisor.Width - 24 * L) div 2;
    Y0 := MulDiv(200, CurrentPPI, 96);
    for K := 0 to 23 do
    begin
      if (E.Sinal > K) and (E.RX = 4) or (E.TX = 1) and (E.Sinal > K) then
        C.Brush.Color := IfThen(K < 18, COR_VERDE, COR_AMBAR)
      else
        C.Brush.Color := COR_FUNDO;
      C.FillRect(System.Types.Rect(X0 + K * L, Y0, X0 + K * L + L - 2, Y0 + MulDiv(28, CurrentPPI, 96)));
    end;
    C.Brush.Style := bsClear;
    C.Font.Height := -MulDiv(16, CurrentPPI, 96);
    C.Font.Color := COR_TEXTO;
    Texto := LabelRemoto.Caption;
    C.TextOut((PaintVisor.Width - C.TextWidth(Texto)) div 2, Y0 + MulDiv(44, CurrentPPI, 96), Texto);
    Exit;
  end;
  if (FTarefa = nil) or (FAcao <> acRemoto) then
    Texto := Tr('Clique em Conectar para ligar o controle remoto.', 'Click Connect to start the remote control.')
  else if FConectado then
    Texto := Tr('Esperando a tela do rádio...', 'Waiting for the radio screen...')
  else
    Texto := Tr('Conectando...', 'Connecting...');
  C.Font.Height := -MulDiv(14, CurrentPPI, 96);
  C.Font.Color := COR_TEXTO_FRACO;
  C.TextOut((PaintVisor.Width - C.TextWidth(Texto)) div 2, (PaintVisor.Height - C.TextHeight(Texto)) div 2, Texto);
end;

procedure TFormPrincipal.BotaoCancelarClick(Sender: TObject);
begin
  if FCabo <> nil then
    FCabo.Cancelar := True;
  if FRemoto <> nil then
    FRemoto.Parar := True;
end;

procedure TFormPrincipal.Terminou(Sender: TObject);
var
  T: TTarefa;
  Arq: string;
begin
  T := Sender as TTarefa;
  case FAcao of
    acLer:
      if T.Fim = fOk then
      begin
        FMemoria.Free;
        FMemoria := FLida;
        FLida := nil;
        FOrigem := Tr('lido do rádio', 'read from the radio');
        FDoArquivo := False;
        FNovo := 0;
        // o backup automático: o que estava no rádio, antes de qualquer mudança
        try
          TDirectory.CreateDirectory(PastaDosBackups);
          Arq := TPath.Combine(PastaDosBackups, 'BJ8300-' + FormatDateTime('yyyymmdd-hhnnss', FMemoria.Lida) + EXTENSAO);
          FMemoria.Salva(Arq);
          Registra(Format(Tr('Rádio lido: %d canais salvos. Backup em %s.', 'Radio read: %d channels saved. Backup in %s.'),
                          [FMemoria.Salvos, Arq]));
        except
          on E: Exception do
            Registra(Format(Tr('Rádio lido, mas o backup não foi salvo: %s', 'Radio read, but the backup was not saved: %s'),
                            [E.Message]));
        end;
        MostraMemoria;
        if ListaNav.ItemIndex in [CARTAO_LOGO, CARTAO_CONTROLE, CARTAO_FIRMWARE, CARTAO_BACKUP, CARTAO_REGISTRO] then
          Mostra(CARTAO_CANAIS);
      end
      else
      begin
        FreeAndNil(FLida);
        Registra(T.Mensagem);
      end;
    acGravar:
      if T.Fim = fOk then
      begin
        FMemoria.Gravou;
        FOrigem := Tr('gravado no rádio', 'written to the radio');
        FDoArquivo := False;
        MostraMemoria;
        Registra(IfThen(FBancosGravados, Tr('Gravado e conferido. Desligue e ligue o rádio: ele monta os bancos na partida.',
                                            'Written and checked. Turn the radio off and on: it builds the banks at ' +
                                            'power-up.'),
                        Tr('Gravado e conferido.', 'Written and checked.')));
      end
      else if T.Fim = fConflito then
        Registra(T.Mensagem + ' ' + Tr('Para não perder as mudanças, use antes Salvar backup.',
                                       'To keep the changes, use Save backup first.'))
      else
        Registra(T.Mensagem + ' ' + Tr('O que já foi gravado está conferido, e o resto fica como estava. Grave de novo.',
                                       'What was already written is checked, and the rest stays as it was. Write ' +
                                       'again.'));
    acLerLogo:
      if T.Fim = fOk then
      begin
        FreeAndNil(FImagem);
        MostraLogo(FLogoLido, Tr('O logo que estava no rádio.', 'The logo that was on the radio.'));
        Registra(Tr('Logo lido.', 'Logo read.'));
      end
      else
        Registra(T.Mensagem);
    acGravarLogo:
      if T.Fim = fOk then
        Registra(Tr('Logo gravado e conferido. Ele aparece na próxima partida do rádio, com o POWER-ON MSG em LOGO.',
                    'Logo written and checked. It shows at the radio''s next power-on, with POWER-ON MSG set to LOGO.'))
      else
        Registra(T.Mensagem);
    acProcurar:
      if T.Fim = fOk then
      begin
        FRadiosBle := FAchados;
        ListaAsPortas;
        if FRadiosBle = nil then
          Registra(Tr('Nenhum aparelho Bluetooth LE por perto. Confira o BLUETOOTH do rádio (menu SYSTEM) e procure de ' +
                      'novo.',
                      'No Bluetooth LE device nearby. Check the radio''s BLUETOOTH (SYSTEM menu) and search again.'))
        else
        begin
          ComboPorta.ItemIndex := Length(FPortas);
          Registra(Format(Tr('%d aparelhos Bluetooth LE achados: escolha o rádio na lista.',
                             '%d Bluetooth LE devices found: choose the radio in the list.'), [Length(FRadiosBle)]));
        end;
      end
      else
      begin
        ComboPorta.ItemIndex := -1;
        Registra(T.Mensagem);
      end;
    acRemoto:
      begin
        FConectado := False;
        FTela.Assign(nil);
        FEstadoRemoto := Default(TEstadoRadio);
        LabelRemoto.Caption := '';
        PaintVisor.Invalidate;
        FSegurado.Enabled := False;
        FBotaoPtt.Caption := Tr('PTT  ·  segure para transmitir', 'PTT  ·  hold to transmit');
        FChaveTela.Ligada := True;   // sem o remoto, o rádio volta com a tela acesa
        FAvisoTela := '';
        if T.Fim = fOk then
          Registra(Tr('Controle remoto desconectado.', 'Remote control disconnected.'))
        else
          Registra(T.Mensagem);
      end;
  end;
  if T.Fim in [fFalha, fSemRadio, fConflito, fErro] then
    Application.MessageBox(PChar(LabelEstado.Caption), PChar(Caption), MB_OK or MB_ICONWARNING);
  FreeAndNil(FCabo);
  FreeAndNil(FRemoto);
  FTarefa := nil;
  Habilita;
  if FFechar then
    PostMessage(Handle, WM_CLOSE, 0, 0);
end;

procedure TFormPrincipal.FormCloseQuery(Sender: TObject; var CanClose: Boolean);
begin
  if FTarefa <> nil then
  begin
    CanClose := False;
    if (FAcao = acRemoto) or (Application.MessageBox(PChar(Tr('O rádio está ligado ao Tempra Link. Cancelar e sair?',
                                                              'The radio is busy with Tempra Link. Cancel and quit?')),
                                                     PChar(Caption), MB_YESNO or MB_ICONQUESTION) = IDYES) then
    begin
      FFechar := True;
      BotaoCancelarClick(nil);
    end;
    Exit;
  end;
  CanClose := DescartaMudancas;
end;

end.
