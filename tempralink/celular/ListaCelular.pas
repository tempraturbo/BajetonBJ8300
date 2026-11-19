unit ListaCelular;

{ A lista das páginas do celular (os canais, os ajustes, o resto): as linhas são desenhadas numa pintura só, e só as
  que aparecem, para os 960 canais não virarem milhares de controles. Cada linha tem o título, o detalhe embaixo e um
  valor à direita; um toque chama a ação dela. Uma seção é um cabeçalho pequeno, sem toque. }

interface

uses
  System.SysUtils, System.Classes, System.Types, System.UITypes, System.Math, FMX.Types, FMX.Controls, FMX.Objects,
  FMX.Layouts, FMX.Graphics, TemaCelular;

type
  TLinha = record
    Titulo, Detalhe, Direita: string;
    Cor, CorDireita: TAlphaColor;   // 0: a de sempre
    Secao: Boolean;
    Acao: TProc;
  end;

  TListaCelular = class
  private
    FCaixa: TVertScrollBox;
    FPintura: TPaintBox;
    FLinhas: TArray<TLinha>;
    FTopos: TArray<Single>;        // onde cada linha começa; mais um no fim, a altura de tudo
    FApertada: Integer;
    FDesceY, FDesceRolagem: Single;
    procedure Pinta(Sender: TObject; Canvas: TCanvas);
    procedure Desce(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
    procedure Solta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
    procedure Saiu(Sender: TObject);
    procedure Toca(Sender: TObject);
    procedure Rolou(Sender: TObject; const OldViewportPosition, NewViewportPosition: TPointF;
      const ContentSizeChanged: Boolean);
    procedure Ajusta(Sender: TObject);
    function LinhaEm(Y: Single): Integer;
  public
    constructor Create(Pai: TFmxObject);
    // As linhas novas; Topo volta a rolagem para o começo (a página nova), senão ela fica onde estava (a mesma página
    // depois de uma mudança).
    procedure Mostra(const Linhas: TArray<TLinha>; Topo: Boolean);
    property Raiz: TVertScrollBox read FCaixa;
  end;

function Linha(const Titulo: string; const Detalhe: string = ''; const Direita: string = ''; Acao: TProc = nil): TLinha;
function Secao(const Titulo: string): TLinha;

implementation

const
  ALTA = 66;       // com detalhe
  BAIXA = 52;      // sem
  ALTA_SECAO = 38;
  MARGEM = 16;
  TOQUE_MAX = 10;  // o dedo que andou mais que isso rolou a lista, não tocou

function Linha(const Titulo, Detalhe, Direita: string; Acao: TProc): TLinha;
begin
  Result := Default(TLinha);
  Result.Titulo := Titulo;
  Result.Detalhe := Detalhe;
  Result.Direita := Direita;
  Result.Acao := Acao;
end;

function Secao(const Titulo: string): TLinha;
begin
  Result := Default(TLinha);
  Result.Titulo := Titulo;
  Result.Secao := True;
end;

constructor TListaCelular.Create(Pai: TFmxObject);
begin
  FCaixa := TVertScrollBox.Create(Pai);
  FCaixa.Parent := Pai;
  FCaixa.Align := TAlignLayout.Client;
  FCaixa.OnViewportPositionChange := Rolou;
  FCaixa.OnResize := Ajusta;
  FPintura := TPaintBox.Create(FCaixa);
  FPintura.Parent := FCaixa;
  FPintura.Align := TAlignLayout.Top;   // a largura vem da rolagem, mesmo com a lista aparecendo depois
  FPintura.HitTest := True;
  FPintura.OnPaint := Pinta;
  FPintura.OnMouseDown := Desce;
  FPintura.OnMouseUp := Solta;
  FPintura.OnMouseLeave := Saiu;
  FPintura.OnClick := Toca;
  FApertada := -1;
end;

procedure TListaCelular.Ajusta(Sender: TObject);
var
  Altura: Single;
begin
  Altura := 0;
  if FTopos <> nil then
    Altura := FTopos[High(FTopos)] + 24;
  FPintura.Height := Max(Altura, FCaixa.Height);
end;

procedure TListaCelular.Mostra(const Linhas: TArray<TLinha>; Topo: Boolean);
var
  K: Integer;
  Y: Single;
begin
  FLinhas := Linhas;
  SetLength(FTopos, Length(Linhas) + 1);
  Y := 0;
  for K := 0 to High(Linhas) do
  begin
    FTopos[K] := Y;
    if Linhas[K].Secao then
      Y := Y + ALTA_SECAO
    else if Linhas[K].Detalhe <> '' then
      Y := Y + ALTA
    else
      Y := Y + BAIXA;
  end;
  FTopos[High(FTopos)] := Y;
  FApertada := -1;
  Ajusta(nil);
  if Topo then
    FCaixa.ViewportPosition := PointF(0, 0);
  FPintura.Repaint;
end;

function TListaCelular.LinhaEm(Y: Single): Integer;
var
  A, B, M: Integer;
begin
  Result := -1;
  if (Y < 0) or (Length(FLinhas) = 0) or (Y >= FTopos[High(FTopos)]) then
    Exit;
  A := 0;
  B := High(FLinhas);
  while A < B do
  begin
    M := (A + B + 1) div 2;
    if FTopos[M] <= Y then
      A := M
    else
      B := M - 1;
  end;
  Result := A;
end;

procedure TListaCelular.Rolou(Sender: TObject; const OldViewportPosition, NewViewportPosition: TPointF;
  const ContentSizeChanged: Boolean);
begin
  FPintura.Repaint;
end;

procedure TListaCelular.Desce(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
var
  K: Integer;
begin
  FDesceY := Y;
  FDesceRolagem := FCaixa.ViewportPosition.Y;
  K := LinhaEm(Y);
  if (K >= 0) and not FLinhas[K].Secao and Assigned(FLinhas[K].Acao) then
  begin
    FApertada := K;
    FPintura.Repaint;
  end;
end;

procedure TListaCelular.Solta(Sender: TObject; Button: TMouseButton; Shift: TShiftState; X, Y: Single);
begin
  Saiu(Sender);
end;

procedure TListaCelular.Saiu(Sender: TObject);
begin
  if FApertada >= 0 then
  begin
    FApertada := -1;
    FPintura.Repaint;
  end;
end;

procedure TListaCelular.Toca(Sender: TObject);
var
  K: Integer;
  A: TProc;
begin
  if Abs(FCaixa.ViewportPosition.Y - FDesceRolagem) > TOQUE_MAX then
    Exit;
  K := LinhaEm(FDesceY);
  if (K < 0) or FLinhas[K].Secao then
    Exit;
  A := FLinhas[K].Acao;
  if Assigned(A) then
    A();   // a ação pode trocar as linhas: a cópia acima é a desta
end;

procedure TListaCelular.Pinta(Sender: TObject; Canvas: TCanvas);
var
  K, Primeira: Integer;
  Y0, Y1, W, Ld: Single;
  L: TLinha;
  R, T: TRectF;
  Cor: TAlphaColor;
  Estado: TCanvasSaveState;
begin
  W := FPintura.Width;
  Y0 := FCaixa.ViewportPosition.Y;
  Y1 := Y0 + FCaixa.Height;
  Primeira := Max(0, LinhaEm(Y0));
  Canvas.Fill.Kind := TBrushKind.Solid;
  for K := Primeira to High(FLinhas) do
  begin
    if FTopos[K] > Y1 then
      Break;
    L := FLinhas[K];
    R := RectF(0, FTopos[K], W, FTopos[K + 1]);
    if L.Secao then
    begin
      Canvas.Font.Size := 12;
      Canvas.Font.Style := [TFontStyle.fsBold];
      Canvas.Fill.Color := COR_VERDE;
      Canvas.FillText(RectF(MARGEM, R.Top + 14, W - MARGEM, R.Bottom - 2), L.Titulo.ToUpper, False, 1, [],
                      TTextAlign.Leading, TTextAlign.Center);
      Continue;
    end;
    if K = FApertada then
    begin
      Canvas.Fill.Color := COR_ESCOLHIDA;
      Canvas.FillRect(R, 0, 0, [], 1);
    end;
    // o valor à direita primeiro: o título fica com o que sobra
    Ld := 0;
    if L.Direita <> '' then
    begin
      Canvas.Font.Size := 14;
      Canvas.Font.Style := [];
      Ld := Min(Canvas.TextWidth(L.Direita), W * 0.5);
      Cor := L.CorDireita;
      if Cor = 0 then
        Cor := COR_VERDE;
      Canvas.Fill.Color := Cor;
      T := RectF(W - MARGEM - Ld, R.Top, W - MARGEM, R.Bottom);
      Estado := Canvas.SaveState;
      try
        Canvas.IntersectClipRect(T);
        Canvas.FillText(T, L.Direita, False, 1, [], TTextAlign.Trailing, TTextAlign.Center);
      finally
        Canvas.RestoreState(Estado);
      end;
      Ld := Ld + 12;
    end;
    Estado := Canvas.SaveState;
    try
      Canvas.IntersectClipRect(RectF(MARGEM, R.Top, W - MARGEM - Ld, R.Bottom));
      Cor := L.Cor;
      if Cor = 0 then
        Cor := COR_TEXTO;
      Canvas.Fill.Color := Cor;
      Canvas.Font.Size := 16;
      Canvas.Font.Style := [];
      if L.Detalhe = '' then
        Canvas.FillText(RectF(MARGEM, R.Top, W, R.Bottom), L.Titulo, False, 1, [], TTextAlign.Leading,
                        TTextAlign.Center)
      else
      begin
        Canvas.FillText(RectF(MARGEM, R.Top + 10, W, R.Top + 34), L.Titulo, False, 1, [], TTextAlign.Leading,
                        TTextAlign.Center);
        Canvas.Font.Size := 12.5;
        Canvas.Fill.Color := COR_TEXTO_FRACO;
        Canvas.FillText(RectF(MARGEM, R.Top + 34, W, R.Bottom - 8), L.Detalhe, False, 1, [], TTextAlign.Leading,
                        TTextAlign.Center);
      end;
    finally
      Canvas.RestoreState(Estado);
    end;
    Canvas.Fill.Color := COR_DIVISA;
    Canvas.FillRect(RectF(MARGEM, R.Bottom - 1, W, R.Bottom), 0, 0, [], 1);
  end;
end;

end.
