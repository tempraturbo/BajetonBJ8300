unit TemaCelular;

{ O tema do Tempra Link no celular: as cores do Tempra Link do Windows (Visual.pas), o escuro com o verde do rádio, e
  as peças com que as telas se montam em código (sem .fmx), para cada uma caber em qualquer tamanho de tela. }

interface

uses
  System.SysUtils, System.Types, System.UITypes, FMX.Types, FMX.Controls, FMX.Objects, FMX.Graphics, FMX.Layouts;

const
  COR_NAV = TAlphaColor($FF0A100D);         // a barra de cima e a de baixo
  COR_FUNDO = TAlphaColor($FF0E1411);       // as páginas
  COR_SUPERFICIE = TAlphaColor($FF151D19);  // os cartões e as teclas
  COR_SUPERFICIE_ALTA = TAlphaColor($FF1B2620);
  COR_DIVISA = TAlphaColor($FF222D27);
  COR_ESCOLHIDA = TAlphaColor($FF14382A);   // o item escolhido, a tecla apertada
  COR_TEXTO = TAlphaColor($FFE8F2EC);
  COR_TEXTO_FRACO = TAlphaColor($FF8DA89A);
  COR_VERDE = TAlphaColor($FF34D399);
  COR_VERDE_ESCURO = TAlphaColor($FF0F5132);
  COR_AMBAR = TAlphaColor($FFFBBF24);
  COR_VERMELHO = TAlphaColor($FFF87171);
  COR_PTT = TAlphaColor($FFC62828);
  COR_PTT_APERTADO = TAlphaColor($FFFBBF24);
  COR_TELA = TAlphaColor($FF050806);        // o vidro da tela do rádio

// Um retângulo arredondado, preso ao pai.
function Caixa(Pai: TFmxObject; Cor: TAlphaColor; Raio: Single = 14; Borda: TAlphaColor = TAlphaColors.Null): TRectangle;
// Um texto que não pega toque (quem pega é a caixa embaixo).
function Texto(Pai: TFmxObject; const S: string; Tamanho: Single; Cor: TAlphaColor; Negrito: Boolean = False;
  Alinha: TTextAlign = TTextAlign.Center): TText;
// Um desenho de ícone de 24 x 24 (os caminhos em ICONES), que não pega toque.
function Icone(Pai: TFmxObject; const Caminho: string; Cor: TAlphaColor): TPath;

const
  // os ícones, desenhados aqui, em 24 x 24
  ICONE_RADIO = 'M7 2 L9 2 L9 6 L17 6 Q19 6 19 8 L19 20 Q19 22 17 22 L7 22 Q5 22 5 20 L5 8 Q5 6 7 6 Z ' +
    'M8 9 L16 9 L16 13 L8 13 Z M8 15.5 A1 1 0 1 0 8.01 15.5 Z M12 15.5 A1 1 0 1 0 12.01 15.5 Z ' +
    'M16 15.5 A1 1 0 1 0 16.01 15.5 Z M8 19 A1 1 0 1 0 8.01 19 Z M12 19 A1 1 0 1 0 12.01 19 Z M16 19 A1 1 0 1 0 16.01 19 Z';
  ICONE_CANAIS = 'M4 5 L20 5 L20 7 L4 7 Z M4 11 L20 11 L20 13 L4 13 Z M4 17 L14 17 L14 19 L4 19 Z';
  ICONE_AJUSTES = 'M4 6 L20 6 L20 8 L4 8 Z M4 16 L20 16 L20 18 L4 18 Z M8 4 L11 4 L11 10 L8 10 Z M14 14 L17 14 L17 20 L14 20 Z';
  // quatro quadrados que ocupam a caixa: o Fit do TPath escala pelos limites do desenho, e três quadradinhos numa
  // faixa estreita viravam três blocos grandes
  ICONE_MAIS = 'M4 4 L10 4 L10 10 L4 10 Z M14 4 L20 4 L20 10 L14 10 Z M4 14 L10 14 L10 20 L4 20 Z ' +
    'M14 14 L20 14 L20 20 L14 20 Z';
  ICONE_BLUETOOTH = 'M11 2 L18 8 L13 12 L18 16 L11 22 L11 14 L7 18 L5.6 16.6 L10.2 12 L5.6 7.4 L7 6 L11 10 Z ' +
    'M13 6.8 L13 10 L15 8.4 Z M13 14 L13 17.2 L15 15.6 Z';

implementation

function Caixa(Pai: TFmxObject; Cor: TAlphaColor; Raio: Single; Borda: TAlphaColor): TRectangle;
begin
  Result := TRectangle.Create(Pai);
  Result.Parent := Pai;
  Result.Fill.Color := Cor;
  Result.XRadius := Raio;
  Result.YRadius := Raio;
  if Borda = TAlphaColors.Null then
    Result.Stroke.Kind := TBrushKind.None
  else
  begin
    Result.Stroke.Color := Borda;
    Result.Stroke.Thickness := 1;
  end;
end;

function Texto(Pai: TFmxObject; const S: string; Tamanho: Single; Cor: TAlphaColor; Negrito: Boolean;
  Alinha: TTextAlign): TText;
begin
  Result := TText.Create(Pai);
  Result.Parent := Pai;
  Result.HitTest := False;
  Result.Text := S;
  Result.TextSettings.Font.Size := Tamanho;
  if Negrito then
    Result.TextSettings.Font.Style := [TFontStyle.fsBold];
  Result.TextSettings.FontColor := Cor;
  Result.TextSettings.HorzAlign := Alinha;
  Result.TextSettings.WordWrap := False;
end;

function Icone(Pai: TFmxObject; const Caminho: string; Cor: TAlphaColor): TPath;
begin
  Result := TPath.Create(Pai);
  Result.Parent := Pai;
  Result.HitTest := False;
  Result.Data.Data := Caminho;
  Result.WrapMode := TPathWrapMode.Fit;
  Result.Fill.Color := Cor;
  Result.Stroke.Kind := TBrushKind.None;
  Result.Width := 24;
  Result.Height := 24;
end;

end.
