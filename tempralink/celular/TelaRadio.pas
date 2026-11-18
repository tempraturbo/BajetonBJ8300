unit TelaRadio;

{ A tela do rádio no celular: a frente do BJ-8300 para mexer nele à distância (o Remoto.pas, o mesmo do Windows). Em
  cima o visor: a tela do rádio quando ela vem (pelo cabo), ou, pelo Bluetooth, o estado desenhado como o rádio o
  mostraria (quem recebe, o S-meter de 24 segmentos, a TX e a tensão). Embaixo o teclado do rádio, na ordem dele, em
  que cada tecla fica apertada enquanto o dedo está nela (segurar 1 s é o toque longo), e o PTT, que transmite
  enquanto está apertado. Tudo se mede pelo tamanho da tela do celular. }

interface

uses
  System.SysUtils, System.Classes, System.Types, System.UITypes, System.Math, FMX.Types, FMX.Controls, FMX.Objects,
  FMX.Layouts, FMX.Graphics, Transporte, Remoto, LogoFormato, ModeloRadio, TemaCelular;

type
  TSituacao = (stDesligado, stConectando, stLigado);

  TTelaRadio = class
  private
    FRaiz: TLayout;
    FVisor: TRectangle;
    FTela: TPaintBox;
    FTeclas: TArray<TRectangle>;
    FModelos: TArray<TTeclaModelo>;
    FPtt: TRectangle;
    FPttTexto, FPttDica: TText;
    FEstado: TEstadoRadio;
    FImagem: TBitmap;
    FRemoto: TRemoto;
    FSituacao: TSituacao;
    FAviso: string;
    FAvisoTela: string;      // o que da tela não vem do rádio, ou por que ela parou (Remoto.AvisoDaTela)
    procedure Arruma(Sender: TObject);
    procedure PintaTela(Sender: TObject; Canvas: TCanvas);
    procedure PintaEstado(Canvas: TCanvas; const R: TRectF);
    function NovaTecla(const M: TTeclaModelo): TRectangle;
    procedure Aperta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
    procedure Solta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
    procedure Saiu(Sender: TObject);
    procedure MarcaTecla(T: TRectangle; Apertada: Boolean);
    procedure PttAperta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
    procedure PttSolta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
    procedure PttSaiu(Sender: TObject);
    procedure MarcaPtt(Apertado: Boolean);
  public
    OnCaiu: TProc<string>;   // o controle remoto parou sozinho (o rádio sumiu): a mensagem
    constructor Create(Pai: TFmxObject);
    destructor Destroy; override;
    // Liga o controle remoto no transporte (numa thread); com Tela, a tela do rádio vem junto (o retrato, v162).
    procedure Liga(const T: ITransporte; Tela: Boolean);
    procedure Desliga;
    property Raiz: TLayout read FRaiz;
  end;

// Um toque curto do celular, para a tecla parecer de verdade (no Android; nos outros, nada).
procedure Vibra;

implementation

{$IFDEF ANDROID}
uses
  Androidapi.Helpers, Androidapi.JNI.GraphicsContentViewText, Androidapi.JNI.Os, Androidapi.JNIBridge;
{$ENDIF}

procedure Vibra;
{$IFDEF ANDROID}
var
  V: JVibrator;
begin
  V := TJVibrator.Wrap(TAndroidHelper.Context.getSystemService(TJContext.JavaClass.VIBRATOR_SERVICE));
  if (V <> nil) and V.hasVibrator then
    V.vibrate(18);
end;
{$ELSE}
begin
end;
{$ENDIF}

constructor TTelaRadio.Create(Pai: TFmxObject);
var
  K: Integer;
begin
  FRaiz := TLayout.Create(Pai);
  FRaiz.Parent := Pai;
  FRaiz.Align := TAlignLayout.Client;
  FRaiz.OnResize := Arruma;
  FImagem := nil;

  // o visor: a moldura escura e o vidro
  FVisor := Caixa(FRaiz, COR_NAV, 18, COR_DIVISA);
  FTela := TPaintBox.Create(FVisor);
  FTela.Parent := FVisor;
  FTela.Align := TAlignLayout.Client;
  FTela.Margins.Rect := RectF(8, 8, 8, 8);
  FTela.HitTest := False;
  FTela.OnPaint := PintaTela;

  for K := 0 to High(TECLAS_CIMA) do
    FModelos := FModelos + [TECLAS_CIMA[K]];
  for K := 0 to High(TECLAS_NUMEROS) do
    FModelos := FModelos + [TECLAS_NUMEROS[K]];
  for K := 0 to High(TECLAS_LADO) do
    FModelos := FModelos + [TECLAS_LADO[K]];
  SetLength(FTeclas, Length(FModelos));
  for K := 0 to High(FModelos) do
  begin
    FTeclas[K] := NovaTecla(FModelos[K]);
    FTeclas[K].Tag := K;
  end;

  // o PTT: largo, embaixo, ao alcance do polegar
  FPtt := Caixa(FRaiz, COR_PTT, 20);
  FPtt.OnMouseDown := PttAperta;
  FPtt.OnMouseUp := PttSolta;
  FPtt.OnMouseLeave := PttSaiu;
  FPttTexto := Texto(FPtt, 'PTT', 20, TAlphaColors.White, True);
  FPttTexto.Align := TAlignLayout.Top;
  FPttDica := Texto(FPtt, 'segure para transmitir', 11, $DDFFFFFF);
  FPttDica.Align := TAlignLayout.Bottom;
end;

destructor TTelaRadio.Destroy;
begin
  Desliga;
  FImagem.Free;
  inherited;
end;

function TTelaRadio.NovaTecla(const M: TTeclaModelo): TRectangle;
var
  T, L: TText;
begin
  Result := Caixa(FRaiz, COR_SUPERFICIE, 14, COR_DIVISA);
  Result.OnMouseDown := Aperta;
  Result.OnMouseUp := Solta;
  Result.OnMouseLeave := Saiu;
  if M.Tipo = ttFuncao then
    T := Texto(Result, M.Texto, 15, COR_VERDE, True)
  else
    T := Texto(Result, M.Texto, 22, COR_TEXTO, False);
  T.Align := TAlignLayout.Client;
  if M.Letras <> '' then
  begin
    L := Texto(Result, M.Letras, 9, COR_TEXTO_FRACO);
    L.Align := TAlignLayout.Bottom;
    L.Height := 14;
    L.Margins.Bottom := 4;
  end;
end;

{ A frente: o visor em cima, a linha das teclas de função, os algarismos em 4 linhas de 3, as do lado e o PTT. A
  altura das teclas sai do que sobra depois do visor, entre 40 e 64. }
procedure TTelaRadio.Arruma(Sender: TObject);
const
  MARGEM = 16;
  VAO = 8;
var
  W, H, X, Y, Alto, Visor, Largura, Ptt: Single;
  K, Coluna: Integer;
begin
  W := FRaiz.Width - 2 * MARGEM;
  H := FRaiz.Height - 2 * MARGEM;
  if (W <= 0) or (H <= 0) then
    Exit;
  Ptt := 64;
  Visor := Min(W * RADIO_TELA_ALTURA / RADIO_TELA_LARGURA, H * 0.40);
  Alto := EnsureRange((H - Visor - Ptt - 7 * VAO - 6) / 6, 40, 64);
  // sem espaço para tudo, o visor encolhe primeiro
  Visor := Max(80, Min(Visor, H - Ptt - 6 * Alto - 7 * VAO - 6));
  FVisor.SetBounds(MARGEM, MARGEM, W, Visor);
  Y := MARGEM + Visor + VAO + 6;
  // a linha de cima: 4
  Largura := (W - 3 * VAO) / 4;
  for K := 0 to High(TECLAS_CIMA) do
    FTeclas[K].SetBounds(MARGEM + K * (Largura + VAO), Y, Largura, Alto);
  Y := Y + Alto + VAO;
  // os algarismos: 3 por linha
  Largura := (W - 2 * VAO) / 3;
  for K := 0 to High(TECLAS_NUMEROS) do
  begin
    Coluna := K mod 3;
    X := MARGEM + Coluna * (Largura + VAO);
    FTeclas[Length(TECLAS_CIMA) + K].SetBounds(X, Y + (K div 3) * (Alto + VAO), Largura, Alto);
  end;
  Y := Y + 4 * (Alto + VAO);
  // as do lado: 2
  Largura := (W - VAO) / 2;
  for K := 0 to High(TECLAS_LADO) do
    FTeclas[Length(TECLAS_CIMA) + Length(TECLAS_NUMEROS) + K].SetBounds(MARGEM + K * (Largura + VAO), Y, Largura,
                                                                          Alto * 0.8);
  Y := Y + Alto * 0.8 + VAO;
  FPtt.SetBounds(MARGEM, Y, W, Max(Ptt, MARGEM + H - Y));
  FPttTexto.Height := FPtt.Height * 0.55;
  FPttDica.Height := FPtt.Height * 0.35;
end;

procedure TTelaRadio.MarcaTecla(T: TRectangle; Apertada: Boolean);
begin
  if Apertada then
  begin
    T.Fill.Color := COR_ESCOLHIDA;
    T.Stroke.Color := COR_VERDE;
    T.Scale.Point := PointF(0.96, 0.96);
    T.Position.Point := T.Position.Point + PointF(T.Width * 0.02, T.Height * 0.02);
  end
  else if T.Scale.X < 1 then
  begin
    T.Fill.Color := COR_SUPERFICIE;
    T.Stroke.Color := COR_DIVISA;
    T.Position.Point := T.Position.Point - PointF(T.Width * 0.02, T.Height * 0.02);
    T.Scale.Point := PointF(1, 1);
  end;
end;

procedure TTelaRadio.Aperta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
var
  T: TRectangle;
begin
  T := Sender as TRectangle;
  MarcaTecla(T, True);
  Vibra;
  if FRemoto <> nil then
    FRemoto.Aperta(FModelos[T.Tag].Codigo);
end;

procedure TTelaRadio.Solta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
begin
  Saiu(Sender);
end;

procedure TTelaRadio.Saiu(Sender: TObject);
var
  T: TRectangle;
begin
  T := Sender as TRectangle;
  if T.Scale.X >= 1 then
    Exit;
  MarcaTecla(T, False);
  if FRemoto <> nil then
    FRemoto.Solta;
end;

procedure TTelaRadio.MarcaPtt(Apertado: Boolean);
begin
  if Apertado then
  begin
    FPtt.Fill.Color := COR_PTT_APERTADO;
    FPttTexto.Text := 'TX';
    FPttTexto.TextSettings.FontColor := COR_NAV;
    FPttDica.Text := 'transmitindo · solte para parar';
    FPttDica.TextSettings.FontColor := COR_NAV;
  end
  else
  begin
    FPtt.Fill.Color := COR_PTT;
    FPttTexto.Text := 'PTT';
    FPttTexto.TextSettings.FontColor := TAlphaColors.White;
    FPttDica.Text := 'segure para transmitir';
    FPttDica.TextSettings.FontColor := $DDFFFFFF;
  end;
end;

procedure TTelaRadio.PttAperta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
begin
  if FRemoto = nil then
    Exit;
  MarcaPtt(True);
  Vibra;
  FRemoto.Ptt(True);
end;

procedure TTelaRadio.PttSolta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
begin
  PttSaiu(Sender);
end;

procedure TTelaRadio.PttSaiu(Sender: TObject);
begin
  MarcaPtt(False);
  if FRemoto <> nil then
    FRemoto.Ptt(False);
end;

procedure TTelaRadio.Liga(const T: ITransporte; Tela: Boolean);
var
  R: TRemoto;
begin
  Desliga;
  R := TRemoto.Create;
  FRemoto := R;
  FSituacao := stConectando;
  FAviso := '';
  FAvisoTela := '';
  FEstado := Default(TEstadoRadio);
  FTela.Repaint;
  R.OnEstado :=
    procedure(E: TEstadoRadio)
    begin
      TThread.Queue(nil,
        procedure
        begin
          if FRemoto <> R then
            Exit;
          FEstado := E;
          FSituacao := stLigado;
          FTela.Repaint;
        end);
    end;
  R.OnTela :=
    procedure(Pixels: TBytes; Codigo: Integer)
    begin
      TThread.Queue(nil,
        procedure
        var
          M: TBitmapData;
          X, Y: Integer;
        begin
          if FRemoto <> R then
            Exit;
          FAvisoTela := AvisoDaTela(Codigo);
          if Pixels <> nil then
          begin
            if FImagem = nil then
              FImagem := TBitmap.Create(RADIO_TELA_LARGURA, RADIO_TELA_ALTURA);
            if FImagem.Map(TMapAccess.Write, M) then
              try
                for Y := 0 to RADIO_TELA_ALTURA - 1 do
                  for X := 0 to RADIO_TELA_LARGURA - 1 do
                    M.SetPixel(X, Y, $FF000000 or CorDoPixel(Pixels, X, Y));
              finally
                FImagem.Unmap(M);
              end;
          end;
          FTela.Repaint;
        end);
    end;
  TThread.CreateAnonymousThread(
    procedure
    var
      Msg: string;
    begin
      Msg := '';
      try
        R.Executa(T, Tela);
      except
        on E: Exception do
          Msg := E.Message;
      end;
      TThread.Queue(nil,
        procedure
        begin
          if FRemoto = R then
          begin
            FRemoto := nil;
            FSituacao := stDesligado;
            FAviso := Msg;
            FTela.Repaint;
            if (Msg <> '') and Assigned(OnCaiu) then
              OnCaiu(Msg);
          end;
          R.Free;
        end);
    end).Start;
end;

procedure TTelaRadio.Desliga;
begin
  if FRemoto <> nil then
  begin
    FRemoto.Parar := True;
    FRemoto := nil;
  end;
  FSituacao := stDesligado;
  MarcaPtt(False);
end;

procedure TTelaRadio.PintaTela(Sender: TObject; Canvas: TCanvas);
var
  R, D: TRectF;
  W, H: Single;
begin
  R := FTela.LocalRect;
  Canvas.Fill.Kind := TBrushKind.Solid;
  Canvas.Fill.Color := COR_TELA;
  Canvas.FillRect(R, 10, 10, AllCorners, 1);
  if (FImagem <> nil) and (FSituacao = stLigado) then
  begin
    // a tela do rádio, na proporção dela e no meio
    W := Min(R.Width, R.Height * RADIO_TELA_LARGURA / RADIO_TELA_ALTURA);
    H := W * RADIO_TELA_ALTURA / RADIO_TELA_LARGURA;
    D := RectF(R.CenterPoint.X - W / 2, R.CenterPoint.Y - H / 2, R.CenterPoint.X + W / 2, R.CenterPoint.Y + H / 2);
    Canvas.DrawBitmap(FImagem, RectF(0, 0, FImagem.Width, FImagem.Height), D, 1, True);
    if FAvisoTela <> '' then
    begin
      // o aviso numa faixa no pé da tela, sobre ela
      Canvas.Fill.Color := (COR_NAV and $00FFFFFF) or $E0000000;
      Canvas.FillRect(RectF(D.Left, D.Bottom - 44, D.Right, D.Bottom), 0, 0, [], 1);
      Canvas.Fill.Color := COR_AMBAR;
      Canvas.Font.Size := 12;
      Canvas.FillText(RectF(D.Left + 8, D.Bottom - 44, D.Right - 8, D.Bottom), FAvisoTela, True, 1, [],
                      TTextAlign.Center, TTextAlign.Center);
    end;
  end
  else
    PintaEstado(Canvas, R);
end;

{ O estado como o rádio o mostraria: em cima o Bluetooth e a tensão; no meio a letra de quem recebe (ou TX), e ao lado
  o que está acontecendo e o S; embaixo o S-meter de 24 segmentos (2 por S até o S9, depois 1 por 10 dB). }
procedure TTelaRadio.PintaEstado(Canvas: TCanvas; const R: TRectF);
const
  MARCAS: array[0..7] of Integer = (2, 6, 10, 14, 18, 20, 22, 24);
  ROTULOS: array[0..7] of string = ('1', '3', '5', '7', '9', '+20', '+40', '+60');
var
  E: TEstadoRadio;
  P, Linha: TRectF;
  K: Integer;
  Seg, Vao, X, Y, Letra: Single;
  Cor: TAlphaColor;
  S: string;

  procedure Escreve(const Txt: string; const Onde: TRectF; Tamanho: Single; C: TAlphaColor; Negrito: Boolean;
    Alinha: TTextAlign = TTextAlign.Leading);
  begin
    Canvas.Font.Size := Tamanho;
    if Negrito then
      Canvas.Font.Style := [TFontStyle.fsBold]
    else
      Canvas.Font.Style := [];
    Canvas.Fill.Color := C;
    Canvas.FillText(Onde, Txt, False, 1, [], Alinha, TTextAlign.Center);
  end;

begin
  E := FEstado;
  P := R;
  P.Inflate(-14, -10);
  // a linha de cima
  Linha := RectF(P.Left, P.Top, P.Right, P.Top + 20);
  Escreve(RADIO_NOME, Linha, 12, COR_TEXTO_FRACO, True);
  if FSituacao = stLigado then
    Escreve(Format('%d,%d V', [E.Tensao div 10, E.Tensao mod 10]), Linha, 12, COR_TEXTO, True, TTextAlign.Trailing);
  if FSituacao <> stLigado then
  begin
    if FSituacao = stConectando then
      S := 'Conectando ao rádio...'
    else if FAviso <> '' then
      S := FAviso
    else
      S := 'Desconectado';
    Canvas.Font.Size := 14;
    Canvas.Fill.Color := COR_TEXTO_FRACO;
    Canvas.FillText(RectF(P.Left, P.Top + 24, P.Right, P.Bottom), S, True, 1, [], TTextAlign.Center, TTextAlign.Center);
    Exit;
  end;
  // a letra grande e o que está acontecendo
  Letra := Min(P.Height * 0.46, 64);
  Y := P.Top + 26;
  if E.TX = 1 then
  begin
    Cor := COR_AMBAR;
    S := 'TX';
  end
  else
  begin
    if E.RX = 4 then
      Cor := COR_VERDE
    else
      Cor := COR_TEXTO;
    S := E.Letra;
  end;
  Escreve(S, RectF(P.Left, Y, P.Left + Letra * 1.6, Y + Letra), Letra, Cor, True);
  X := P.Left + Letra * 1.7;
  if E.TX = 1 then
    S := 'Transmitindo'
  else if E.RX = 4 then
    S := 'Recebendo'
  else if E.RX = 5 then
    S := 'Monitor'
  else
    S := 'Em espera';
  Escreve(S, RectF(X, Y + 2, P.Right, Y + Letra * 0.5), 15, Cor, True);
  if E.Sinal >= 0 then
    Escreve(TextoSinal(E.Sinal), RectF(X, Y + Letra * 0.5, P.Right, Y + Letra), 22, COR_TEXTO, True)
  else
    Escreve('—', RectF(X, Y + Letra * 0.5, P.Right, Y + Letra), 22, COR_TEXTO_FRACO, True);
  // o S-meter
  Y := P.Bottom - 34;
  Vao := 3;
  Seg := (P.Width - 23 * Vao) / 24;
  for K := 0 to 23 do
  begin
    if (E.Sinal > K) and ((E.RX in [4, 5]) or (E.TX = 1)) then
    begin
      if K < 18 then
        Canvas.Fill.Color := COR_VERDE
      else
        Canvas.Fill.Color := COR_AMBAR;
    end
    else
      Canvas.Fill.Color := COR_SUPERFICIE_ALTA;
    Canvas.FillRect(RectF(P.Left + K * (Seg + Vao), Y, P.Left + K * (Seg + Vao) + Seg, Y + 14), 2, 2, AllCorners, 1);
  end;
  for K := 0 to High(MARCAS) do
    Escreve(ROTULOS[K], RectF(P.Left + MARCAS[K] * (Seg + Vao) - Seg - 20, Y + 16, P.Left + MARCAS[K] * (Seg + Vao) + 20,
      Y + 32), 10, COR_TEXTO_FRACO, False, TTextAlign.Center);
end;

end.
