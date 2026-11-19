unit TelaConectar;

{ A primeira tela do app: os rádios usados antes (RadiosLembrados), cada um com o apelido que se der a ele, que um
  toque conecta sem a procura inteira (ela para assim que o rádio aparece); e a procura pelo Bluetooth, que mostra os
  aparelhos achados, que um toque conecta. A demonstração abre o controle remoto com o BJ-8300 simulado, sem rádio. Embaixo, o registro da última conexão (o que
  o aparelho anuncia: o Bluetooth do rádio nunca foi provado, e é por ele que se descobre o que faltou).

  No Android, a procura pede antes a permissão de "Dispositivos por perto" (do Android 12 em diante) ou a da
  localização (até o 11, sem a qual o sistema não entrega o que achou). O manifesto declara a procura sem uso da
  localização (AndroidManifest.template.xml), e por isso o Android 12 em diante não pede a localização ligada. }

interface

uses
  System.SysUtils, System.Classes, System.Types, System.UITypes, System.Generics.Defaults, System.Generics.Collections,
  System.StrUtils, FMX.Types, FMX.Controls, FMX.Objects, FMX.Layouts, Bluetooth, ModeloRadio, TemaCelular,
  RadiosLembrados;

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
    FLembrados: TArray<TLembrado>;
    FProcurando: Boolean;
    procedure Procura(Sender: TObject);
    procedure ComecaProcura;
    procedure Escolhe(Sender: TObject);
    procedure EscolheLembrado(Sender: TObject);
    procedure Apelido(Sender: TObject);
    procedure EsqueceLembrado(Sender: TObject);
    procedure Demonstra(Sender: TObject);
  public
    OnRadio: TProc<TRadioBLE>;     // tocou num aparelho achado
    OnLembrado: TProc<TLembrado>;  // tocou num rádio usado antes
    OnApelido: TProc<TLembrado>;   // pediu para dar o apelido (a janela pergunta, e chama o MostraRadios)
    OnDemonstracao: TProc;
    constructor Create(Pai: TFmxObject);
    destructor Destroy; override;
    procedure Avisa(const S: string; Cor: TAlphaColor = COR_TEXTO_FRACO);
    // De qualquer thread: uma linha no registro da conexão (as últimas ficam à vista).
    procedure Registra(const S: string);
    procedure LimpaRegistro;
    // Os lembrados e, embaixo, os achados na última procura (menos os que já são lembrados).
    procedure MostraRadios;
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
  MostraRadios;
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
            if Length(FRadios) = 1 then
              Avisa('1 aparelho achado: toque nele para conectar.')
            else if FRadios <> nil then
              Avisa(Format('%d aparelhos achados: toque no rádio para conectar.', [Length(FRadios)]))
            else if Achados <> nil then
              Avisa('Por perto, só os rádios usados antes: toque num deles para conectar.')
            else
              Avisa('Nenhum aparelho achado. Confira o BLUETOOTH do rádio em ON e o Bluetooth do celular ligado (até ' +
                    'o Android 11, a localização também).', COR_AMBAR);
          end;
        end);
    end).Start;
end;

// Os lembrados, o usado por último primeiro; depois os achados, os com nome antes (o rádio tem nome; relógios e fones
// sem nome vão para o fim), em ordem de nome.
procedure TTelaConectar.MostraRadios;
var
  Y: Single;

  // um cartão da lista, com o ícone, o nome e a linha de baixo; Toque é o do cartão inteiro
  function Cartao(const Nome, Baixo: string; Tag: Integer; Toque: TNotifyEvent; Direita: Single): TRectangle;
  var
    I: TControl;
    T: TText;
  begin
    Result := Caixa(FLista, COR_SUPERFICIE, 14, COR_DIVISA);
    Result.Position.Y := Y;
    Y := Y + 100;
    Result.Align := TAlignLayout.Top;
    Result.Height := 62;
    Result.Margins.Bottom := 8;
    Result.Tag := Tag;
    Result.OnClick := Toque;
    I := Icone(Result, ICONE_BLUETOOTH, COR_VERDE);
    I.Position.Point := PointF(14, 19);
    T := Texto(Result, Nome, 16, COR_TEXTO, True, TTextAlign.Leading);
    T.Align := TAlignLayout.Top;
    T.Height := 24;
    T.Margins.Rect := RectF(50, 9, 12 + Direita, 0);
    T := Texto(Result, Baixo, 12, COR_TEXTO_FRACO, False, TTextAlign.Leading);
    T.Align := TAlignLayout.Top;
    T.Position.Y := 100;
    T.Height := 18;
    T.Margins.Rect := RectF(50, 0, 12 + Direita, 0);
  end;

  // um botão de texto no lado direito do cartão
  procedure Botao(C: TRectangle; const S: string; Topo: Single; Toque: TNotifyEvent);
  var
    T: TText;
  begin
    T := Texto(C, S, 13, COR_VERDE, True, TTextAlign.Trailing);
    T.HitTest := True;
    T.Tag := C.Tag;
    T.OnClick := Toque;
    T.SetBounds(C.Width - 100, Topo, 88, 26);
    T.Anchors := [TAnchorKind.akTop, TAnchorKind.akRight];
  end;

  procedure Secao(const S: string);
  var
    T: TText;
  begin
    T := Texto(FLista, S, 13, COR_TEXTO_FRACO, True, TTextAlign.Leading);
    T.Position.Y := Y;
    Y := Y + 100;
    T.Align := TAlignLayout.Top;
    T.Height := 26;
  end;

var
  K: Integer;
  C: TRectangle;
  Novos: TArray<TRadioBLE>;
  R: TRadioBLE;
  L: TLembrado;
  Ja: Boolean;
begin
  FLembrados := Lembrados;
  Novos := nil;
  for R in FRadios do
  begin
    Ja := False;
    for L in FLembrados do
      Ja := Ja or SameText(L.Endereco, R.Endereco);
    if not Ja then
      Novos := Novos + [R];
  end;
  FRadios := Novos;
  TArray.Sort<TRadioBLE>(FRadios, TComparer<TRadioBLE>.Construct(
    function(const A, B: TRadioBLE): Integer
    begin
      Result := Ord(A.Aparelho.DeviceName = '') - Ord(B.Aparelho.DeviceName = '');
      if Result = 0 then
        Result := CompareText(A.Nome, B.Nome);
    end));
  FLista.Content.DeleteChildren;
  Y := 0;
  if FLembrados <> nil then
    Secao('Usados antes: toque para conectar');
  for K := 0 to High(FLembrados) do
  begin
    C := Cartao(FLembrados[K].Titulo, IfThen(FLembrados[K].Apelido = '', '', FLembrados[K].Nome + ' · ') +
                FLembrados[K].Endereco, K, EscolheLembrado, 96);
    Botao(C, 'Apelido', 4, Apelido);
    Botao(C, 'Esquecer', 32, EsqueceLembrado);
  end;
  if (FLembrados <> nil) and (FRadios <> nil) then
    Secao('Achados agora');
  for K := 0 to High(FRadios) do
    Cartao(FRadios[K].Nome, FRadios[K].Endereco, K, Escolhe, 0);
end;

procedure TTelaConectar.Escolhe(Sender: TObject);
begin
  if Assigned(OnRadio) then
    OnRadio(FRadios[(Sender as TFmxObject).Tag]);
end;

procedure TTelaConectar.EscolheLembrado(Sender: TObject);
begin
  if not FProcurando and Assigned(OnLembrado) then
    OnLembrado(FLembrados[(Sender as TFmxObject).Tag]);
end;

procedure TTelaConectar.Apelido(Sender: TObject);
begin
  if Assigned(OnApelido) then
    OnApelido(FLembrados[(Sender as TFmxObject).Tag]);
end;

procedure TTelaConectar.EsqueceLembrado(Sender: TObject);
begin
  Esquece(FLembrados[(Sender as TFmxObject).Tag].Endereco);
  MostraRadios;
end;

procedure TTelaConectar.Demonstra(Sender: TObject);
begin
  if Assigned(OnDemonstracao) then
    OnDemonstracao;
end;

end.
