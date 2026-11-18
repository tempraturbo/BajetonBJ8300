unit Visual;

{ A cara do Tempra Link: o estilo escuro do Windows (o Windows10 Dark da VCL, embutido no .exe) nos controles comuns, e
  por cima as cores do rádio (firmware/src/ui_tela.c): as superfícies em verde quase preto e o verde de destaque. Os
  botões se desenham arredondados (o TBotaoEstilo no lugar do desenho do estilo, com o mesmo comportamento), com três
  tipos: o comum, o principal da página (verde) e o de perigo (vermelho); as teclas do controle remoto são as do
  rádio (TTecla), e a chave de liga e desliga, uma pílula verde (TChave). }

interface

uses
  Winapi.Windows, Winapi.Messages, System.SysUtils, System.Classes, Vcl.Graphics, Vcl.Controls, Vcl.StdCtrls,
  Vcl.Forms, Vcl.Themes, Winapi.GDIPOBJ;

const
  // em BGR, como o TColor
  COR_NAV = $000D100A;             // #0A100D, a barra de cima do rádio: a navegação e a barra de baixo
  COR_FUNDO = $0011140E;           // #0E1411, as páginas
  COR_SUPERFICIE = $00191D15;      // #151D19, as linhas pares, os campos, as teclas
  COR_CABECALHO = $001D2218;       // #18221D
  COR_DIVISA = $00272D22;          // #222D27
  COR_ESCOLHIDA = $002A3814;       // #14382A, o chip do canal: a linha e o item escolhidos
  COR_MUDOU = $00102E3A;           // #3A2E10, o que mudou e ainda não foi para o rádio
  COR_TEXTO = $00ECF2E8;           // #E8F2EC
  COR_TEXTO_FRACO = $009AA88D;     // #8DA89A
  COR_VERDE = $0099D334;           // #34D399
  COR_AMBAR = $0024BFFB;           // #FBBF24
  COR_VERMELHO = $007171F8;        // #F87171

type
  TTipoBotao = (tbComum, tbPrincipal, tbPerigo);

  // O desenho dos botões, no lugar do do estilo; o clique, o foco e o teclado continuam os do TButton.
  TBotaoEstilo = class(TButtonStyleHook)
  strict protected
    procedure DrawButton(ACanvas: TCanvas; AMouseInControl: Boolean); override;
    procedure PaintBackground(Canvas: TCanvas); override;
  end;

  TTipoTecla = (ttDigito, ttFuncao, ttPtt);

  // Uma tecla do rádio no controle remoto: o OnMouseDown e o OnMouseUp são o apertar e o soltar.
  TTecla = class(TCustomControl)
  private
    FSobre, FApertada: Boolean;
    FLetras: string;
    FTipo: TTipoTecla;
    procedure CMMouseEnter(var Message: TMessage); message CM_MOUSEENTER;
    procedure CMMouseLeave(var Message: TMessage); message CM_MOUSELEAVE;
    procedure CMEnabledChanged(var Message: TMessage); message CM_ENABLEDCHANGED;
    procedure CMTextChanged(var Message: TMessage); message CM_TEXTCHANGED;
  protected
    procedure Paint; override;
    procedure MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure MouseUp(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
  public
    constructor Create(AOwner: TComponent); override;
    property Caption;
    property Font;
    property Letras: string read FLetras write FLetras;
    property Tipo: TTipoTecla read FTipo write FTipo;
    property OnMouseDown;
    property OnMouseUp;
  end;

  // Uma chave de liga e desliga: a pílula verde com a bolinha à direita, ligada, e apagada com ela à esquerda, e o texto
  // do estado ao lado. O clique (ou o espaço, com o foco) a troca e chama o OnClick; o Ligada posto pelo programa não.
  TChave = class(TCustomControl)
  private
    FLigada: Boolean;
    FTextoLigada, FTextoDesligada: string;
    procedure SetLigada(V: Boolean);
    procedure Troca;
    procedure CMFocusChanged(var Message: TMessage); message CM_FOCUSCHANGED;
  protected
    procedure Paint; override;
    procedure MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer); override;
    procedure KeyDown(var Key: Word; Shift: TShiftState); override;
  public
    constructor Create(AOwner: TComponent); override;
    property Ligada: Boolean read FLigada write SetLigada;
    property TextoLigada: string read FTextoLigada write FTextoLigada;
    property TextoDesligada: string read FTextoDesligada write FTextoDesligada;
    property Font;
    property OnClick;
  end;

procedure Destaca(B: TButton; Tipo: TTipoBotao);
// As cores do tema nos painéis, grades, listas, campos e rótulos de dentro de Raiz (os de cor própria ficam). Antes
// de as janelas existirem: mudar o estilo de um controle que já tem janela faz a VCL recriá-la.
procedure AplicaTema(Raiz: TControl);
// Um retângulo de cantos redondos, com as bordas suaves (GDI+); Borda clNone, sem borda.
procedure Arredondado(C: TCanvas; const R: TRect; Raio: Integer; Fundo, Borda: TColor; Grossura: Single = 1);
// O estilo embutido (Recursos.rc); sem ele, o programa fica com a cara do Windows.
procedure LigaEstilo;
// A marca do Tempra Link, "((T))": o T de Tempra como antena e as ondas de rádio dos dois lados, num quadrado escuro de
// cantos redondos, no quadrado de lado S em (X, Y). A mesma da navegação e do ícone (o tempralink.ico).
procedure DesenhaMarca(G: TGPGraphics; X, Y, S: Single);
// Um item das listas desenhadas (a navegação, as pastas): o escolhido numa pílula, com a faixa verde.
procedure ItemDeLista(L: TCustomListBox; Index: Integer; const R: TRect; Escolhido: Boolean;
  State: TOwnerDrawState; Fundo, Texto: TColor; Margem: Integer);

implementation

uses
  System.Generics.Collections, System.Math, Vcl.Styles, Vcl.ExtCtrls, Vcl.Grids, Vcl.WinXPanels, Vcl.ComCtrls, Vcl.CheckLst, Vcl.WinXCtrls,
  Winapi.GDIPAPI;

type
  TControleCor = class(TControl);  // o Color e o Font, que são protegidos

var
  Tipos: TDictionary<TButton, TTipoBotao>;

function CorGdi(C: TColor): ARGB;
begin
  C := ColorToRGB(C);
  Result := MakeColor(255, GetRValue(C), GetGValue(C), GetBValue(C));
end;

procedure Arredondado(C: TCanvas; const R: TRect; Raio: Integer; Fundo, Borda: TColor; Grossura: Single);
var
  G: TGPGraphics;
  P: TGPGraphicsPath;
  B: TGPSolidBrush;
  Caneta: TGPPen;
  X, Y, L, A, D: Single;
begin
  G := TGPGraphics.Create(C.Handle);
  P := TGPGraphicsPath.Create;
  try
    G.SetSmoothingMode(SmoothingModeAntiAlias);
    X := R.Left + Grossura / 2;
    Y := R.Top + Grossura / 2;
    L := R.Width - Grossura;
    A := R.Height - Grossura;
    D := 2 * Raio;
    P.AddArc(X, Y, D, D, 180, 90);
    P.AddArc(X + L - D, Y, D, D, 270, 90);
    P.AddArc(X + L - D, Y + A - D, D, D, 0, 90);
    P.AddArc(X, Y + A - D, D, D, 90, 90);
    P.CloseFigure;
    B := TGPSolidBrush.Create(CorGdi(Fundo));
    try
      G.FillPath(B, P);
    finally
      B.Free;
    end;
    if Borda <> clNone then
    begin
      Caneta := TGPPen.Create(CorGdi(Borda), Grossura);
      try
        G.DrawPath(Caneta, P);
      finally
        Caneta.Free;
      end;
    end;
  finally
    P.Free;
    G.Free;
  end;
end;

// a cor de quem está atrás do controle: o primeiro pai de cor própria, ou o fundo das páginas
function CorDoPai(C: TControl): TColor;
var
  P: TWinControl;
begin
  P := C.Parent;
  while (P <> nil) and (seClient in P.StyleElements) do
    P := P.Parent;
  if P = nil then
    Result := COR_FUNDO
  else
    Result := TControleCor(P).Color;
end;

function Escala(C: TControl; X: Integer): Integer;
begin
  Result := MulDiv(X, C.CurrentPPI, 96);
end;

{ TChave }

constructor TChave.Create(AOwner: TComponent);
begin
  inherited;
  ControlStyle := ControlStyle + [csOpaque];
  TabStop := True;
  DoubleBuffered := True;
  FLigada := True;
end;

procedure TChave.SetLigada(V: Boolean);
begin
  if V <> FLigada then
  begin
    FLigada := V;
    Invalidate;
  end;
end;

procedure TChave.Troca;
begin
  Ligada := not FLigada;
  Click;
end;

procedure TChave.CMFocusChanged(var Message: TMessage);
begin
  inherited;
  Invalidate;
end;

procedure TChave.MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  inherited;
  if Button = mbLeft then
  begin
    if CanFocus then
      SetFocus;
    Troca;
  end;
end;

procedure TChave.KeyDown(var Key: Word; Shift: TShiftState);
begin
  inherited;
  if Key = VK_SPACE then
    Troca;
end;

procedure TChave.Paint;
var
  A, L, M, Y: Integer;
  R: TRect;
  T: string;
begin
  Canvas.Brush.Color := COR_FUNDO;
  Canvas.FillRect(ClientRect);
  A := Escala(Self, 20);
  L := Escala(Self, 40);
  M := Escala(Self, 4);
  Y := (Height - A) div 2;
  R := Rect(0, Y, L, Y + A);
  if FLigada then
  begin
    Arredondado(Canvas, R, A div 2, COR_VERDE, IfThen(Focused, COR_TEXTO, COR_VERDE));
    R := Rect(L - A + M, Y + M, L - M, Y + A - M);
    Arredondado(Canvas, R, R.Height div 2, COR_FUNDO, COR_FUNDO);
  end
  else
  begin
    Arredondado(Canvas, R, A div 2, COR_SUPERFICIE, IfThen(Focused, COR_TEXTO, COR_TEXTO_FRACO));
    R := Rect(M, Y + M, A - M, Y + A - M);
    Arredondado(Canvas, R, R.Height div 2, COR_TEXTO_FRACO, COR_TEXTO_FRACO);
  end;
  if FLigada then
    T := FTextoLigada
  else
    T := FTextoDesligada;
  Canvas.Font := Font;
  Canvas.Font.Color := COR_TEXTO;
  Canvas.Brush.Style := bsClear;
  Canvas.TextOut(L + Escala(Self, 10), (Height - Canvas.TextHeight(T)) div 2, T);
end;

{ TBotaoEstilo }

procedure TBotaoEstilo.PaintBackground(Canvas: TCanvas);
begin
  // o DrawButton pinta tudo
end;

procedure TBotaoEstilo.DrawButton(ACanvas: TCanvas; AMouseInControl: Boolean);
type
  TCores = record
    Fundo, Sobre, Apertado, Borda, Texto: TColor;
  end;
const
  CORES: array[TTipoBotao] of TCores = (
    (Fundo: $0022261F; Sobre: $002A3026; Apertado: $00191C16; Borda: $0033392E; Texto: COR_TEXTO),
    (Fundo: COR_VERDE; Sobre: $00B3E35B; Apertado: $007DB020; Borda: COR_VERDE; Texto: $00162205),
    (Fundo: $002828C6; Sobre: $003131E0; Apertado: $001B1B8E; Borda: $002828C6; Texto: $00FFFFFF));
var
  B: TButton;
  Tipo: TTipoBotao;
  Cor: TCores;
  Fundo, Borda, Texto: TColor;
  R: TRect;
  Foco: Boolean;
begin
  B := Control as TButton;
  if not Tipos.TryGetValue(B, Tipo) then
    Tipo := tbComum;
  Cor := CORES[Tipo];
  Borda := Cor.Borda;
  Texto := Cor.Texto;
  if not B.Enabled then
  begin
    Fundo := $00181B14;
    Borda := $0023271E;
    Texto := $00565F4E;
  end
  else if FPressed then
    Fundo := Cor.Apertado
  else if AMouseInControl then
    Fundo := Cor.Sobre
  else
    Fundo := Cor.Fundo;
  R := Control.ClientRect;
  ACanvas.Brush.Color := CorDoPai(Control);
  ACanvas.FillRect(R);
  // o anel do foco só com o teclado em uso (o Windows esconde enquanto se usa o mouse)
  Foco := B.Focused and (SendMessage(Handle, WM_QUERYUISTATE, 0, 0) and UISF_HIDEFOCUS = 0);
  if Foco then
    Arredondado(ACanvas, R, Escala(Control, 8), Fundo, COR_VERDE, 2)
  else
    Arredondado(ACanvas, R, Escala(Control, 8), Fundo, Borda);
  ACanvas.Font := B.Font;
  ACanvas.Font.Color := Texto;
  if Tipo = tbPrincipal then
    ACanvas.Font.Style := [fsBold];
  ACanvas.Brush.Style := bsClear;
  R.Inflate(-Escala(Control, 6), 0);
  DrawText(ACanvas.Handle, PChar(B.Caption), -1, R, DT_CENTER or DT_VCENTER or DT_SINGLELINE or DT_END_ELLIPSIS);
end;

procedure Destaca(B: TButton; Tipo: TTipoBotao);
begin
  Tipos.AddOrSetValue(B, Tipo);
  B.Invalidate;
end;

{ TTecla }

constructor TTecla.Create(AOwner: TComponent);
begin
  inherited;
  ControlStyle := ControlStyle + [csOpaque] - [csSetCaption];
  DoubleBuffered := True;
  TabStop := False;
  Font.Name := 'Segoe UI Semibold';
end;

procedure TTecla.CMMouseEnter(var Message: TMessage);
begin
  inherited;
  FSobre := True;
  Invalidate;
end;

procedure TTecla.CMMouseLeave(var Message: TMessage);
begin
  inherited;
  FSobre := False;
  Invalidate;
end;

procedure TTecla.CMEnabledChanged(var Message: TMessage);
begin
  inherited;
  FApertada := False;
  Invalidate;
end;

procedure TTecla.CMTextChanged(var Message: TMessage);
begin
  inherited;
  Invalidate;
end;

procedure TTecla.MouseDown(Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  if Button = mbLeft then
  begin
    FApertada := True;
    Invalidate;
  end;
  inherited;
end;

procedure TTecla.MouseUp(Button: TMouseButton; Shift: TShiftState; X, Y: Integer);
begin
  if Button = mbLeft then
  begin
    FApertada := False;
    Invalidate;
  end;
  inherited;
end;

// como as teclas de borracha do rádio: escuras, com o número grande e as letras embaixo; a apertada acende
procedure TTecla.Paint;
var
  R, RLetras: TRect;
  Fundo, Borda, Texto: TColor;
begin
  R := ClientRect;
  Canvas.Brush.Color := CorDoPai(Self);
  Canvas.FillRect(R);
  Texto := IfThen(FTipo = ttFuncao, COR_VERDE, COR_TEXTO);
  if FTipo = ttPtt then
  begin
    Fundo := IfThen(FApertada, $001B1B8E, IfThen(FSobre, $003131E0, $002828C6));
    Borda := IfThen(FApertada, $004444EF, $002828C6);
    Texto := $00FFFFFF;
  end
  else if FApertada then
  begin
    Fundo := COR_VERDE;
    Borda := COR_VERDE;
    Texto := $00162205;
  end
  else
  begin
    Fundo := IfThen(FSobre, $00262C21, COR_SUPERFICIE);
    Borda := $00343B2E;
  end;
  if not Enabled then
  begin
    Fundo := $00161A13;
    Borda := $001F241B;
    Texto := $00505A4A;
  end;
  Arredondado(Canvas, R, Escala(Self, 10), Fundo, Borda);
  Canvas.Brush.Style := bsClear;
  Canvas.Font := Font;
  Canvas.Font.Color := Texto;
  if FLetras = '' then
    DrawText(Canvas.Handle, PChar(Caption), -1, R, DT_CENTER or DT_VCENTER or DT_SINGLELINE)
  else
  begin
    // o número um pouco acima do meio, e as letras pequenas embaixo
    R.Bottom := R.Bottom - Escala(Self, 9);
    DrawText(Canvas.Handle, PChar(Caption), -1, R, DT_CENTER or DT_VCENTER or DT_SINGLELINE);
    RLetras := ClientRect;
    RLetras.Top := RLetras.Bottom - Escala(Self, 17);
    Canvas.Font.Name := 'Segoe UI';
    Canvas.Font.Height := -Escala(Self, 9);
    if Enabled and not FApertada then
      Canvas.Font.Color := COR_TEXTO_FRACO;
    DrawText(Canvas.Handle, PChar(FLetras), -1, RLetras, DT_CENTER or DT_TOP or DT_SINGLELINE);
  end;
end;

{ O tema }

procedure AplicaTema(Raiz: TControl);

  procedure Pinta(C: TControl);
  var
    K: Integer;
  begin
    if C is TLabel then
    begin
      // com o estilo, o rótulo ficaria com a cor de texto dele, e não com a sua
      C.StyleElements := C.StyleElements - [seFont];
      if TLabel(C).Font.Color = clGrayText then
        TLabel(C).Font.Color := COR_TEXTO_FRACO
      else if TLabel(C).Font.Color = clWindowText then
        TLabel(C).Font.Color := COR_TEXTO;
    end
    else if (C is TCustomPanel) or (C is TCustomGrid) or (C is TCustomListBox) and
            (TListBox(C).Style <> lbStandard) then
    begin
      // as superfícies que se pintam com a própria cor (o Color, que o estilo deixaria de lado)
      // a navegação, a barra de baixo e as prévias pretas guardam a cor delas
      C.StyleElements := C.StyleElements - [seClient];
      if (TControleCor(C).Color <> COR_NAV) and (TControleCor(C).Color <> clBlack) then
        TControleCor(C).Color := COR_FUNDO;
      if C is TCustomPanel then
        TPanel(C).ParentBackground := False;
    end
    else if (C is TCustomEdit) or (C is TCustomComboBox) or (C is TCustomListBox) then
    begin
      C.StyleElements := C.StyleElements - [seClient, seFont];
      TControleCor(C).Color := COR_SUPERFICIE;
      TControleCor(C).Font.Color := COR_TEXTO;
    end;
    if C is TWinControl then
      for K := 0 to TWinControl(C).ControlCount - 1 do
        Pinta(TWinControl(C).Controls[K]);
  end;

begin
  Pinta(Raiz);
end;

procedure DesenhaMarca(G: TGPGraphics; X, Y, S: Single);
var
  Fundo: TGPLinearGradientBrush;
  Borda, Onda: TGPPen;
  Branco: TGPSolidBrush;
  P: TGPGraphicsPath;
  Cx, Cy, R, D: Single;
  K: Integer;
begin
  G.SetSmoothingMode(SmoothingModeAntiAlias);
  // o quadrado: do verde escuro do chip do canal ao quase preto da barra do rádio
  R := S * 0.22;
  D := 2 * R;
  P := TGPGraphicsPath.Create;
  Fundo := TGPLinearGradientBrush.Create(MakePoint(X, Y), MakePoint(X, Y + S), CorGdi($00203A28), CorGdi(COR_NAV));
  Borda := TGPPen.Create(CorGdi($00365B40), Max(1, S / 64));
  try
    P.AddArc(X, Y, D, D, 180, 90);
    P.AddArc(X + S - D, Y, D, D, 270, 90);
    P.AddArc(X + S - D, Y + S - D, D, D, 0, 90);
    P.AddArc(X, Y + S - D, D, D, 90, 90);
    P.CloseFigure;
    G.FillPath(Fundo, P);
    G.DrawPath(Borda, P);
  finally
    Borda.Free;
    Fundo.Free;
    P.Free;
  end;
  // as ondas: dois arcos de cada lado, em volta do meio do T
  Cx := X + S / 2;
  Cy := Y + S * 0.47;
  for K := 1 to 2 do
  begin
    R := S * (0.17 + 0.13 * K);
    Onda := TGPPen.Create(CorGdi(IfThen(K = 1, $00B3E35B, COR_VERDE)), S * 0.065);
    try
      Onda.SetStartCap(LineCapRound);
      Onda.SetEndCap(LineCapRound);
      G.DrawArc(Onda, Cx - R, Cy - R, 2 * R, 2 * R, 145, 70);
      G.DrawArc(Onda, Cx - R, Cy - R, 2 * R, 2 * R, -35, 70);
    finally
      Onda.Free;
    end;
  end;
  // o T: a barra e a haste, de pontas redondas
  Branco := TGPSolidBrush.Create(CorGdi(COR_TEXTO));
  P := TGPGraphicsPath.Create;
  try
    D := S * 0.075;
    P.AddArc(Cx - S * 0.14, Cy - S * 0.13, D, D, 90, 180);
    P.AddArc(Cx + S * 0.14 - D, Cy - S * 0.13, D, D, 270, 180);
    P.CloseFigure;
    G.FillPath(Branco, P);
    P.Reset;
    P.AddArc(Cx - D / 2, Cy - S * 0.13, D, D, 180, 180);
    P.AddArc(Cx - D / 2, Cy + S * 0.20 - D, D, D, 0, 180);
    P.CloseFigure;
    G.FillPath(Branco, P);
  finally
    P.Free;
    Branco.Free;
  end;
end;

// A VCL registra sozinha os estilos embutidos (os recursos VCLSTYLE), com o Vcl.Styles nos uses; carregar de novo
// daria estilo repetido.
procedure ItemDeLista(L: TCustomListBox; Index: Integer; const R: TRect; Escolhido: Boolean;
  State: TOwnerDrawState; Fundo, Texto: TColor; Margem: Integer);
var
  C: TCanvas;
  P: TRect;
  Item: string;
begin
  C := L.Canvas;
  C.Brush.Color := Fundo;
  C.FillRect(R);
  P := R;
  P.Inflate(-Escala(L, 8), -Escala(L, 3));
  C.Font.Style := [];
  if Escolhido then
  begin
    Arredondado(C, P, Escala(L, 8), COR_ESCOLHIDA, clNone);
    Arredondado(C, Rect(P.Left + Escala(L, 4), P.Top + Escala(L, 8), P.Left + Escala(L, 7), P.Bottom - Escala(L, 8)),
                Escala(L, 1), COR_VERDE, clNone);
    C.Font.Style := [fsBold];
    Texto := COR_TEXTO;
  end;
  Item := TListBox(L).Items[Index];
  C.Font.Color := Texto;
  C.Brush.Style := bsClear;
  C.TextOut(P.Left + Escala(L, Margem), R.Top + (R.Height - C.TextHeight('Ag')) div 2, Item);
  C.Brush.Style := bsSolid;
  // sem o estilo, a VCL desenha o retângulo do foco depois do item; desenhar antes o apaga (é XOR)
  if (odFocused in State) and not TStyleManager.IsCustomStyleActive then
    C.DrawFocusRect(R);
end;

procedure LigaEstilo;
begin
  if not TStyleManager.TrySetStyle('Windows10 Dark', False) then
    try
      TStyleManager.SetStyle(TStyleManager.LoadFromResource(HInstance, 'ESTILO'));
    except
      // sem o recurso, fica a cara do Windows
    end;
end;

initialization
  Tipos := TDictionary<TButton, TTipoBotao>.Create;
  TCustomStyleEngine.UnRegisterStyleHook(TButton, TButtonStyleHook);
  TCustomStyleEngine.RegisterStyleHook(TButton, TBotaoEstilo);

finalization
  Tipos.Free;

end.
