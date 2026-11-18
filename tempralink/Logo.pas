unit Logo;

{ O logo de abertura do BJ-8300 no formato do rádio: 160 x 138 em RGB565, em 0x10000 do chip de dados (o
  firmware/scripts/logo_bmp.py), que o Tempra Link lê pelo comando estendido do cabo (CaboBJ.pas). Sem nada de
  janela, para dar para testar. }

interface

uses
  Winapi.Windows, System.SysUtils, Vcl.Graphics;

const
  LOGO_ENDERECO = $010000;
  LOGO_LARGURA = 160;  // o que a memória guarda
  LOGO_VISIVEL = 160;  // o que aparece: tudo
  LOGO_ALTURA = 138;
  LOGO_TAMANHO = LOGO_LARGURA * LOGO_ALTURA * 2;

function ImagemParaLogo(Imagem: TGraphic; Inteira: Boolean): TBytes;
function LogoParaBitmap(const Dados: TBytes; Largura: Integer = LOGO_VISIVEL): TBitmap;

implementation

uses
  System.Types, System.Math, Idioma;

{ A imagem }

{ A imagem como o rádio a guarda: 160 x 138 em RGB565, byte baixo primeiro, a linha de cima primeiro. Uma de
  160 x 138 vai como está; as outras vão inteiras com faixas pretas, ou preenchendo e cortando as sobras. }
function ImagemParaLogo(Imagem: TGraphic; Inteira: Boolean): TBytes;
var
  B: TBitmap;
  Escala: Double;
  W, H, X, Y, I: Integer;
  P: PRGBTriple;
  Cor: Word;
begin
  if (Imagem.Width = 0) or (Imagem.Height = 0) then
    raise EInvalidGraphic.Create(Tr('A imagem está vazia.', 'The image is empty.'));
  if Imagem is TWICImage then
    TWICImage(Imagem).InterpolationMode := wipmFant;  // sem isso ela se estica pelo GDI, ponto a ponto
  B := TBitmap.Create;
  try
    B.PixelFormat := pf24bit;
    if (Imagem.Width = LOGO_LARGURA) and (Imagem.Height = LOGO_ALTURA) then
      B.SetSize(LOGO_LARGURA, LOGO_ALTURA)
    else
      B.SetSize(LOGO_VISIVEL, LOGO_ALTURA);
    B.Canvas.Brush.Color := clBlack;
    B.Canvas.FillRect(Rect(0, 0, B.Width, B.Height));
    if B.Width = LOGO_LARGURA then
      B.Canvas.Draw(0, 0, Imagem)
    else
    begin
      if Inteira then
        Escala := Min(LOGO_VISIVEL / Imagem.Width, LOGO_ALTURA / Imagem.Height)
      else
        Escala := Max(LOGO_VISIVEL / Imagem.Width, LOGO_ALTURA / Imagem.Height);
      W := Max(1, Round(Imagem.Width * Escala));
      H := Max(1, Round(Imagem.Height * Escala));
      B.Canvas.StretchDraw(Bounds((LOGO_VISIVEL - W) div 2, (LOGO_ALTURA - H) div 2, W, H), Imagem);
    end;

    SetLength(Result, LOGO_TAMANHO);
    FillChar(Result[0], LOGO_TAMANHO, 0);
    for Y := 0 to LOGO_ALTURA - 1 do
    begin
      P := B.ScanLine[Y];
      for X := 0 to B.Width - 1 do
      begin
        Cor := (P.rgbtRed * 31 + 127) div 255 shl 11 or (P.rgbtGreen * 63 + 127) div 255 shl 5 or
               (P.rgbtBlue * 31 + 127) div 255;
        I := (Y * LOGO_LARGURA + X) * 2;
        Result[I] := Byte(Cor);
        Result[I + 1] := Byte(Cor shr 8);
        Inc(P);
      end;
    end;
  finally
    B.Free;
  end;
end;

// As Largura colunas da esquerda do logo, para ver ou salvar.
function LogoParaBitmap(const Dados: TBytes; Largura: Integer): TBitmap;
var
  X, Y: Integer;
  P: PRGBTriple;
  Cor: Word;
begin
  Result := TBitmap.Create;
  Result.PixelFormat := pf24bit;
  Result.SetSize(Largura, LOGO_ALTURA);
  for Y := 0 to LOGO_ALTURA - 1 do
  begin
    P := Result.ScanLine[Y];
    for X := 0 to Largura - 1 do
    begin
      Cor := Dados[(Y * LOGO_LARGURA + X) * 2] or Dados[(Y * LOGO_LARGURA + X) * 2 + 1] shl 8;
      P.rgbtRed := (Cor shr 11) * 255 div 31;
      P.rgbtGreen := (Cor shr 5 and 63) * 255 div 63;
      P.rgbtBlue := (Cor and 31) * 255 div 31;
      Inc(P);
    end;
  end;
end;

end.
