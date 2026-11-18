unit LogoFormato;

{ O logo de abertura do BJ-8300 no formato do rádio: 160 x 138 em RGB565, byte baixo primeiro, a linha de cima
  primeiro, em 0x10000 do chip de dados (o firmware/scripts/logo_bmp.py). A tela do controle remoto vem no mesmo
  formato, com 128 linhas. Sem nada de janela: serve ao Tempra Link do Windows (Logo.pas) e ao do celular. }

interface

uses
  System.SysUtils;

const
  LOGO_ENDERECO = $010000;
  LOGO_LARGURA = 160;  // o que a memória guarda
  LOGO_VISIVEL = 160;  // o que aparece: tudo
  LOGO_ALTURA = 138;
  LOGO_TAMANHO = LOGO_LARGURA * LOGO_ALTURA * 2;

// A cor RGB565 do pixel (X, Y) de uma imagem LOGO_LARGURA de largura, em $00RRGGBB.
function CorDoPixel(const Dados: TBytes; X, Y: Integer): Cardinal; inline;

implementation

function CorDoPixel(const Dados: TBytes; X, Y: Integer): Cardinal;
var
  Cor: Word;
begin
  Cor := Dados[(Y * LOGO_LARGURA + X) * 2] or Dados[(Y * LOGO_LARGURA + X) * 2 + 1] shl 8;
  Result := (Cor shr 11) * 255 div 31 shl 16 or (Cor shr 5 and 63) * 255 div 63 shl 8 or (Cor and 31) * 255 div 31;
end;

end.
