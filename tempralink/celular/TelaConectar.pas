unit TelaConectar;

{ A primeira tela do app: procura o rádio pelo Bluetooth e mostra os aparelhos achados; um toque num deles conecta. A
  demonstração abre o controle remoto com o BJ-8300 simulado, sem rádio. Embaixo, o registro da última conexão (o que
  o aparelho anuncia: o Bluetooth do rádio nunca foi provado, e é por ele que se descobre o que faltou).

  No Android, a procura pede antes a permissão de "Dispositivos por perto" (do Android 12 em diante) ou a da
  localização (até o 11, sem a qual o sistema não entrega o que achou). O manifesto declara a procura sem uso da
  localização (AndroidManifest.template.xml), e por isso o Android 12 em diante não pede a localização ligada. }

interface

uses
  System.SysUtils, System.Classes, System.Types, System.UITypes, System.Generics.Defaults, System.Generics.Collections,
  FMX.Types, FMX.Controls, FMX.Objects, FMX.Layouts, Bluetooth, ModeloRadio, TemaCelular;

type
  TTelaConectar = class
  private
    FRaiz: TLayout;
    FProcurar: TRectangle;
    FProcurarTexto: TText;
    FAviso: TText;
    FLista: TVertScrollBox;
    FPe: TLayout;
    FRegistro: TText;
    FLinhas: TStringList;
    FRadios: TArray<TRadioBLE>;
    FProcurando: Boolean;
    procedure Procura(Sender: TObject);
    procedure ComecaProcura;
    procedure MostraRadios;
    procedure Escolhe(Sender: TObject);
    procedure Demonstra(Sender: TObject);
  public
    OnRadio: TProc<TRadioBLE>;   // tocou num aparelho achado
    OnDemonstracao: TProc;
    constructor Create(Pai: TFmxObject);
    destructor Destroy; override;
    procedure Avisa(const S: string; Cor: TAlphaColor = COR_TEXTO_FRACO);
    // De qualquer thread: uma linha no registro da conexão (as últimas ficam à vista).
    procedure Registra(const S: string);
    procedure LimpaRegistro;
    property Raiz: TLayout read FRaiz;
  end;

implementation

{$IFDEF ANDROID}
uses
  System.Permissions, Androidapi.Helpers, Androidapi.JNI.Os;
{$ENDIF}

const
  PROCURA_MS = 6000;
  LINHAS_REGISTRO = 9;

// As permissões que a procura pede: Depois(True) com todas dadas (fora do Android, sempre).
procedure PedePermissoes(const Depois: TProc<Boolean>);
{$IFDEF ANDROID}
var
  P: TArray<string>;
begin
  if TOSVersion.Check(12) then
    P := [JStringToString(TJManifest_permission.JavaClass.BLUETOOTH_SCAN),
          JStringToString(TJManifest_permission.JavaClass.BLUETOOTH_CONNECT)]
  else
    P := [JStringToString(TJManifest_permission.JavaClass.ACCESS_FINE_LOCATION)];
  PermissionsService.RequestPermissions(P,
    procedure(const Permissoes: TClassicStringDynArray; const Respostas: TClassicPermissionStatusDynArray)
    var
      R: TPermissionStatus;
    begin
      for R in Respostas do
        if R <> TPermissionStatus.Granted then
        begin
          Depois(False);
          Exit;
        end;
      Depois(True);
    end);
end;
{$ELSE}
begin
  Depois(True);
end;
{$ENDIF}

constructor TTelaConectar.Create(Pai: TFmxObject);
var
  Cabeca: TLayout;
  T: TText;
  Demo: TRectangle;
  Caixinha: TLayout;
begin
  FLinhas := TStringList.Create;
  FRaiz := TLayout.Create(Pai);
  FRaiz.Parent := Pai;
  FRaiz.Align := TAlignLayout.Client;
  FRaiz.Padding.Rect := RectF(16, 16, 16, 16);

  // o pé, antes do resto para ficar embaixo: o registro e a demonstração
  FPe := TLayout.Create(FRaiz);
  FPe.Parent := FRaiz;
  FPe.Align := TAlignLayout.Bottom;
  FPe.Height := 60;
  Demo := Caixa(FPe, COR_SUPERFICIE, 16, COR_DIVISA);
  Demo.Align := TAlignLayout.Bottom;
  Demo.Height := 52;
  Demo.OnClick := Demonstra;
  T := Texto(Demo, 'Demonstração, sem rádio', 15, COR_VERDE, True);
  T.Align := TAlignLayout.Client;
  Caixinha := TLayout.Create(FPe);
  Caixinha.Parent := FPe;
  Caixinha.Align := TAlignLayout.Client;
  Caixinha.ClipChildren := True;
  Caixinha.Margins.Bottom := 8;
  FRegistro := Texto(Caixinha, '', 10, COR_TEXTO_FRACO, False, TTextAlign.Leading);
  FRegistro.Align := TAlignLayout.Client;
  FRegistro.TextSettings.WordWrap := True;
  FRegistro.TextSettings.VertAlign := TTextAlign.Trailing;

  // o cabeçalho: o que fazer no rádio
  Cabeca := TLayout.Create(FRaiz);
  Cabeca.Parent := FRaiz;
  Cabeca.Align := TAlignLayout.Top;
  Cabeca.Height := 84;
  T := Texto(Cabeca, 'Conectar ao ' + RADIO_NOME, 22, COR_TEXTO, True, TTextAlign.Leading);
  T.Align := TAlignLayout.Top;
  T.Height := 32;
  T := Texto(Cabeca, 'No rádio, ligue o Bluetooth: MENU › SYSTEM › BLUETOOTH › ON. Depois procure aqui e toque nele.',
             13, COR_TEXTO_FRACO, False, TTextAlign.Leading);
  T.Align := TAlignLayout.Client;
  T.TextSettings.WordWrap := True;
  T.TextSettings.VertAlign := TTextAlign.Leading;
  T.Margins.Top := 4;

  FProcurar := Caixa(FRaiz, COR_VERDE, 16);
  FProcurar.Align := TAlignLayout.Top;
  FProcurar.Position.Y := 1000;
  FProcurar.Height := 56;
  FProcurar.Margins.Top := 8;
  FProcurar.OnClick := Procura;
  FProcurarTexto := Texto(FProcurar, 'Procurar o rádio', 17, COR_NAV, True);
  FProcurarTexto.Align := TAlignLayout.Client;

  FAviso := Texto(FRaiz, '', 13, COR_TEXTO_FRACO, False, TTextAlign.Leading);
  FAviso.Align := TAlignLayout.Top;
  FAviso.Position.Y := 2000;
  FAviso.Height := 44;
  FAviso.Margins.Top := 8;
  FAviso.TextSettings.WordWrap := True;

  FLista := TVertScrollBox.Create(FRaiz);
  FLista.Parent := FRaiz;
  FLista.Align := TAlignLayout.Client;
  FLista.Margins.Top := 4;
  FLista.Margins.Bottom := 8;
end;

destructor TTelaConectar.Destroy;
begin
  FLinhas.Free;
  inherited;
end;

procedure TTelaConectar.Avisa(const S: string; Cor: TAlphaColor);
begin
  FAviso.Text := S;
  FAviso.TextSettings.FontColor := Cor;
end;

procedure TTelaConectar.Registra(const S: string);
begin
  TThread.Queue(nil,
    procedure
    begin
      FLinhas.Add(S);
      while FLinhas.Count > LINHAS_REGISTRO do
        FLinhas.Delete(0);
      FRegistro.Text := FLinhas.Text.TrimRight;
      FPe.Height := 60 + 130;
    end);
end;

procedure TTelaConectar.LimpaRegistro;
begin
  FLinhas.Clear;
  FRegistro.Text := '';
  FPe.Height := 60;
end;

procedure TTelaConectar.Procura(Sender: TObject);
begin
  if FProcurando then
    Exit;
  PedePermissoes(
    procedure(Dadas: Boolean)
    begin
      if Dadas then
        ComecaProcura
      else
        Avisa('Sem a permissão de dispositivos por perto o app não acha o rádio. Ela se dá em Configurações › Apps › ' +
              'Tempra Link › Permissões.', COR_AMBAR);
    end);
end;

procedure TTelaConectar.ComecaProcura;
begin
  FProcurando := True;
  FProcurar.Opacity := 0.6;
  FProcurarTexto.Text := 'Procurando...';
  Avisa(Format('Procurando por %d s...', [PROCURA_MS div 1000]));
  TThread.CreateAnonymousThread(
    procedure
    var
      Achados: TArray<TRadioBLE>;
      Msg: string;
    begin
      Msg := '';
      try
        Achados := ProcuraRadios(PROCURA_MS);
      except
        on E: Exception do
          Msg := E.Message;
      end;
      TThread.Queue(nil,
        procedure
        begin
          FProcurando := False;
          FProcurar.Opacity := 1;
          FProcurarTexto.Text := 'Procurar de novo';
          if Msg <> '' then
            Avisa(Msg, COR_VERMELHO)
          else
          begin
            FRadios := Achados;
            MostraRadios;
          end;
        end);
    end).Start;
end;

// Os achados, os com nome antes (o rádio tem nome; relógios e fones sem nome vão para o fim), em ordem de nome.
procedure TTelaConectar.MostraRadios;
var
  K: Integer;
  C: TRectangle;
  I: TControl;
  T: TText;
begin
  TArray.Sort<TRadioBLE>(FRadios, TComparer<TRadioBLE>.Construct(
    function(const A, B: TRadioBLE): Integer
    begin
      Result := Ord(A.Aparelho.DeviceName = '') - Ord(B.Aparelho.DeviceName = '');
      if Result = 0 then
        Result := CompareText(A.Nome, B.Nome);
    end));
  FLista.Content.DeleteChildren;
  for K := 0 to High(FRadios) do
  begin
    C := Caixa(FLista, COR_SUPERFICIE, 14, COR_DIVISA);
    C.Position.Y := K * 100;
    C.Align := TAlignLayout.Top;
    C.Height := 62;
    C.Margins.Bottom := 8;
    C.Tag := K;
    C.OnClick := Escolhe;
    I := Icone(C, ICONE_BLUETOOTH, COR_VERDE);
    I.Position.Point := PointF(14, 19);
    T := Texto(C, FRadios[K].Nome, 16, COR_TEXTO, True, TTextAlign.Leading);
    T.Align := TAlignLayout.Top;
    T.Height := 24;
    T.Margins.Rect := RectF(50, 9, 12, 0);
    T := Texto(C, FRadios[K].Endereco, 12, COR_TEXTO_FRACO, False, TTextAlign.Leading);
    T.Align := TAlignLayout.Top;
    T.Position.Y := 100;
    T.Height := 18;
    T.Margins.Rect := RectF(50, 0, 12, 0);
  end;
  if FRadios = nil then
    Avisa('Nenhum aparelho achado. Confira o BLUETOOTH do rádio em ON e o Bluetooth do celular ligado (até o ' +
          'Android 11, a localização também).', COR_AMBAR)
  else if Length(FRadios) = 1 then
    Avisa('1 aparelho achado: toque nele para conectar.')
  else
    Avisa(Format('%d aparelhos achados: toque no rádio para conectar.', [Length(FRadios)]));
end;

procedure TTelaConectar.Escolhe(Sender: TObject);
begin
  if Assigned(OnRadio) then
    OnRadio(FRadios[(Sender as TFmxObject).Tag]);
end;

procedure TTelaConectar.Demonstra(Sender: TObject);
begin
  if Assigned(OnDemonstracao) then
    OnDemonstracao;
end;

end.
