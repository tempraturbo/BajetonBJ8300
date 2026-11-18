unit TelaBJ;

{ O desenho da tela do BJ-8300 a partir do retrato que o controle remoto manda (firmware v162): o mesmo código da tela
  do rádio (firmware/src/tela.c e telas.c) com a máquina da prévia (firmware/previa/maquina.c e tela_pc.c), compilado
  pelo firmware/previa/tela_pc.py e ligado dentro do executável (tela\tela_bj_win32.obj e tela_bj_win64.obj) ou, no
  Android, a libtelabj.so que vai no pacote. Sem nada de janela: serve ao Tempra Link do Windows e ao do celular.

  O código da tela guarda a RAM e a tela imitadas em variáveis globais: um desenho por vez (o laço do remoto é um só). }

interface

uses
  System.SysUtils;

const
  TELA_BJ_INTEIRA = 0;   // a tela como o rádio a mostra
  TELA_BJ_PARCIAL = 1;   // o espectro sem as colunas, ou a caixa da MESSAGES sem a tela do APRS ou do Morse embaixo
  TELA_BJ_FORA = 2;      // a tela do APRS ou do Morse: o histórico vem da flash do rádio, fora do retrato
  TELA_BJ_JANELA = 3;    // desenhada, mas o código pediu uma janela fora da tela (um estado que a prévia não conhece)
  TELA_BJ_ERRO_TAMANHO = -1;  // o retrato não tem o tamanho deste formato
  TELA_BJ_ERRO_VERSAO = -2;   // de outra versão do firmware (outras faixas, ou outras tabelas do menu)
  TELA_BJ_ERRO_SOMA = -3;     // bytes estragados

// O retrato inteiro, do '~' às somas, em bytes; e a versão do formato que este desenho conhece.
function TamanhoRetrato: Integer;
function VersaoRetrato: Integer;
// O retrato R desenhado em Imagem (160 x 128 em RGB565, o byte baixo primeiro), com o byte do sinal da resposta que
// veio antes dele (o medidor S: os segmentos, o bit 7 o microfone na TX, 255 nenhum). Devolve um dos TELA_BJ_; nos
// negativos a Imagem fica como estava.
function DesenhaRetrato(const R: TBytes; Sinal: Integer; var Imagem: TBytes): Integer;

implementation

{$IFDEF MSWINDOWS}
uses
  System.Win.Crtl;   // o memcpy e o memset que o código C chama

// no Win32 o nome C leva o "_" (o Delphi procura o nome como esta escrito aqui), e no Win64 nao
{$IFDEF WIN32}
{$L 'tela\tela_bj_win32.obj'}
function _tela_bj_versao: Integer; cdecl; external;
function _tela_bj_tamanho: Integer; cdecl; external;
function _tela_bj_desenha(R: PByte; N, Sinal: Integer; Saida: PWord): Integer; cdecl; external;
{$ELSE}
{$L 'tela\tela_bj_win64.obj'}
function tela_bj_versao: Integer; cdecl; external;
function tela_bj_tamanho: Integer; cdecl; external;
function tela_bj_desenha(R: PByte; N, Sinal: Integer; Saida: PWord): Integer; cdecl; external;
{$ENDIF}
{$ENDIF}

{$IFDEF ANDROID}
const
  BIBLIOTECA = 'libtelabj.so';

function tela_bj_versao: Integer; cdecl; external BIBLIOTECA;
function tela_bj_tamanho: Integer; cdecl; external BIBLIOTECA;
function tela_bj_desenha(R: PByte; N, Sinal: Integer; Saida: PWord): Integer; cdecl; external BIBLIOTECA;
{$ENDIF}

{$IFDEF WIN32}
function tela_bj_versao: Integer;
begin
  Result := _tela_bj_versao;
end;

function tela_bj_tamanho: Integer;
begin
  Result := _tela_bj_tamanho;
end;

function tela_bj_desenha(R: PByte; N, Sinal: Integer; Saida: PWord): Integer;
begin
  Result := _tela_bj_desenha(R, N, Sinal, Saida);
end;
{$ENDIF}

function TamanhoRetrato: Integer;
begin
  Result := tela_bj_tamanho;
end;

function VersaoRetrato: Integer;
begin
  Result := tela_bj_versao;
end;

function DesenhaRetrato(const R: TBytes; Sinal: Integer; var Imagem: TBytes): Integer;
var
  Saida: TBytes;
begin
  if Length(R) <> tela_bj_tamanho then
    Exit(TELA_BJ_ERRO_TAMANHO);
  SetLength(Saida, 160 * 128 * 2);
  Result := tela_bj_desenha(@R[0], Length(R), Sinal, @Saida[0]);
  if Result >= 0 then
    Imagem := Saida;
end;

end.
