unit BJ8300;

{ A memória do BJ-8300 que o Tempra Link lê, mostra, edita, guarda e grava. Sem nada de janela, para dar para testar.

  Duas cópias das áreas: a original (o que estava no rádio, ou no backup aberto) e a atual, já com as mudanças. As
  áreas são as que o driver do CHIRP deste repositório lê (chirp/bj8300_v3.7TEMPRA.py): as do protocolo de fábrica
  (os canais, os VFOs A e B, os ajustes, o DTMF, o rádio FM/AM/HF e o modo), a calibração (só para o backup) e as do
  firmware TEMPRA, cada uma num setor: o cabeçalho dos bancos, os 16 bancos, o WATCH, o VFO C, o Morse e o APRS.

  Essas do TEMPRA são registros com assinatura e soma (le_registro e grava_registro, firmware/src/bancos.c): um
  registro que não confere o rádio lê como zerado, com a assinatura, e o VFO C como cópia do VFO B. Aqui é igual: os
  valores se leem da "vista", que põe o registro assim quando ele não confere, e mudar um campo grava o registro
  inteiro na atual e refaz a soma.

  O canal é o registro de 32 bytes do protocolo de fábrica (MEM_FORMAT do radtel_rt900.py):
    0-3 RX e 4-7 TX em BCD, em 10 Hz, o byte baixo primeiro (TX todo FF: sem TX); 8-9 e 10-11 os tons; 12 o grupo
    DTMF (bits 3-0); 13 o PTT-ID (1-0); 14 o scramble (7-4) e a potência (1-0: 0 alta, 1 média, 2 baixa); 15 o
    estreito (6), o ENCRYPT (5-4), o BCL (3), a varredura (2) e o AM (1); 16-19 o FHSS, que fica como está; 20-31 o
    nome, completado com FF. Vazio: o primeiro byte FF, ou a RX zero.
  Os bancos (bancos.c): o cabeçalho "BNK3" (os ligados em 4-5, o escolhido em 6, a soma em 8-9) e um setor por banco,
  "BCO1" com o nome (12 bytes, completado com 0), o mapa (120 bytes, um bit por canal) e a soma.
  As notas do ROGER CUSTOM (firmware v164, o roger_custom do bancos.c), num setor próprio, 0xDA000: "BCO1", quantas (1
  a 32), um byte 0, as 32 notas (Hz e ms, u16, byte baixo primeiro; as que sobram em 0) e a soma, 136 bytes. A soma é
  0x1234 mais as 67 palavras de 16 bits de antes dela (não a dos outros registros, que só vê os últimos bytes). }

interface

uses
  System.SysUtils, System.Classes, System.Generics.Collections, CaboBJ, Ajustes;

const
  CANAIS = 960;
  BANCOS = 16;
  BANCO_NOME = 12;
  GRUPOS_DTMF = 15;
  DIGITOS_GRUPO = 6;
  // a posição é o código que o rádio guarda (o DTMF_CHARS do radtel_rt900.py; o 10 é um espaço, que não se digita)
  DIGITOS_DTMF = '0123456789 *#ABCD';
  EXTENSAO = '.tempra';
  // o ROGER CUSTOM (v164), os limites do firmware e do UV-17L
  ROGER_NOTAS = 32;
  ROGER_TOTAL_MS = 3000;
  ROGER_REGISTRO = 136;

type
  TPotencia = (ptAlta, ptMedia, ptBaixa);

  TCanal = record
    Numero: Integer;
    Existe: Boolean;
    Nome: string;
    RX, TX: Cardinal;  // em 10 Hz; TX 0 = sem TX
    TomRX, TomTX: Word;
    Potencia: TPotencia;
    Estreito, Varre, BCL, AM: Boolean;
    Encrypt, Scramble, Scode, PttId: Byte;
    Bancos: Word;      // o bit k: o canal está no banco k + 1
  end;

  TArea = record
    Endereco, Tamanho: Cardinal;
  end;

  // uma nota do ROGER CUSTOM
  TNota = record
    Hz, Ms: Word;      // Hz 0 = pausa
  end;

const
  AREAS: array[0..29] of TArea = (
    (Endereco: $00000; Tamanho: $7800), (Endereco: $08000; Tamanho: $40), (Endereco: $09000; Tamanho: $40),
    (Endereco: $0A000; Tamanho: $140), (Endereco: $0B000; Tamanho: $400), (Endereco: $0C000; Tamanho: $400),
    (Endereco: $0D000; Tamanho: $40), (Endereco: $0F000; Tamanho: $280),
    (Endereco: $C1000; Tamanho: $C0),
    (Endereco: $C2000; Tamanho: $C0), (Endereco: $C3000; Tamanho: $C0), (Endereco: $C4000; Tamanho: $C0),
    (Endereco: $C5000; Tamanho: $C0), (Endereco: $C6000; Tamanho: $C0), (Endereco: $C7000; Tamanho: $C0),
    (Endereco: $C8000; Tamanho: $C0), (Endereco: $C9000; Tamanho: $C0), (Endereco: $CA000; Tamanho: $C0),
    (Endereco: $CB000; Tamanho: $C0), (Endereco: $CC000; Tamanho: $C0), (Endereco: $CD000; Tamanho: $C0),
    (Endereco: $CE000; Tamanho: $C0), (Endereco: $CF000; Tamanho: $C0), (Endereco: $D0000; Tamanho: $C0),
    (Endereco: $D1000; Tamanho: $C0),
    (Endereco: $D2000; Tamanho: $40), (Endereco: $D3000; Tamanho: $80), (Endereco: $D4000; Tamanho: $40),
    (Endereco: $D5000; Tamanho: $40),
    (Endereco: $DA000; Tamanho: $C0));
  AREA_CANAIS = 0;
  AREA_VFOS = 1;
  AREA_AJUSTES = 2;
  AREA_DTMF = 3;
  AREA_CALIBRACAO = 7;  // só leitura: vai no backup e nunca é gravada
  AREA_CABECALHO = 8;
  AREA_BANCO1 = 9;
  AREA_WATCH = 25;
  AREA_VFC = 26;
  AREA_ROGER = 29;  // v164: falta nos backups de antes, e vale apagada

type
  EArquivo = class(Exception);
  EConflito = class(Exception);

  TEstado = array[0..High(AREAS)] of TBytes;

  // O que vai ser gravado: um bloco de 64 bytes de uma área de fábrica, ou uma área estendida inteira (o setor).
  TGravar = record
    Area: Integer;
    Desloc, Tamanho: Cardinal;
  end;

  TMemoriaBJ8300 = class
  private
    FMenu: TMenu;
    FOriginal, FAtual: TEstado;
    FDesfazer: TStack<TEstado>;
    FRegistros: TArray<TRegistroDesc>;  // por área: Tamanho 0 = a área não é registro
    function Onde(Endereco: Cardinal; out Area, Desloc: Integer): Boolean;
    function Vista(const E: TEstado; Area: Integer): TBytes;
    procedure Garante(Area: Integer);
    procedure Sela(Area: Integer);
    function LeByte(const E: TEstado; Endereco: Cardinal): Byte;
    procedure PoeBits(Endereco: Cardinal; Mascara, Valor: Byte);
    function Algarismos(Endereco: Cardinal; N: Integer): Cardinal;
    procedure PoeAlgarismos(Endereco: Cardinal; N: Integer; V: Cardinal);
    function Copia(const E: TEstado): TEstado;
    function Mapa(K: Integer): TBytes;
    procedure OrdenaMicGain;
  public
    Versao: string;    // o firmware que estava no rádio
    Lida: TDateTime;   // quando, no horário local
    constructor Create(Menu: TMenu);
    destructor Destroy; override;
    function Vazia: Boolean;
    procedure LeDoRadio(C: TCaboBJ);  // dentro da sessão do cabo
    procedure Salva(const Arquivo: string);
    procedure Abre(const Arquivo: string);
    // o histórico do desfazer: Guarda antes de cada mudança
    procedure Guarda;
    function Desfaz: Boolean;
    function PodeDesfazer: Boolean;
    function Mudou: Boolean;
    // os canais
    function Canal(N: Integer): TCanal;
    procedure PoeCanal(const C: TCanal);
    procedure ApagaCanal(N: Integer);
    function Salvos: Integer;
    // os bancos
    function BancoNome(K: Integer): string;   // '' = o de fábrica, BANK nn
    procedure PoeBancoNome(K: Integer; const Nome: string);
    function BancoAtivo(K: Integer): Boolean;
    procedure PoeBancoAtivo(K: Integer; Ativo: Boolean);
    function CanaisNoBanco(K: Integer): Integer;
    // os ajustes do menu, os VFOs e os textos
    property Menu: TMenu read FMenu;
    function Valor(It: TItem): Integer;
    function ValorOriginal(It: TItem): Integer;
    procedure PoeValor_(It: TItem; V: Integer);
    function Frequencia(Lado: Integer): Cardinal;          // em 10 Hz
    procedure PoeFrequencia(Lado: Integer; F: Cardinal);
    function Offset(Lado: Integer): Cardinal;              // em kHz
    procedure PoeOffset(Lado: Integer; KHz: Cardinal);
    function Texto(const Nome: string): string;
    procedure PoeTexto(const Nome, Valor: string);
    // os códigos dos 15 grupos DTMF
    function GrupoDtmf(K: Integer): string;
    procedure PoeGrupoDtmf(K: Integer; const Digitos: string);
    // as notas do ROGER CUSTOM: nil sem registro que confira (o CUSTOM fica calado); PoeNotas com nil apaga o registro
    function Notas: TArray<TNota>;
    procedure PoeNotas(const N: TArray<TNota>);
    // a gravação: as regras do rádio, o que mudou, e a gravação disso pelo cabo, conferida contra o que o rádio tem
    procedure PreparaGravacao;
    function Mudancas: TArray<TGravar>;
    function MudouBancos: Boolean;
    procedure GravaNoRadio(C: TCaboBJ; Restaurar: Boolean);
    procedure Gravou;  // o que está no rádio agora é a atual
  end;

function BlocosDaLeitura: Integer;
// O logo de abertura (Logo.pas): a leitura e a gravação, conferida lendo de volta (o firmware que não grava o logo pelo
// cabo responde o ACK do mesmo jeito).
function LeLogo(C: TCaboBJ): TBytes;
procedure GravaLogo(C: TCaboBJ; const Dados: TBytes);
function Soma(const B: TBytes; N: Integer): Word;
function TomTexto(T: Word): string;
function TomDeTexto(const S: string; out T: Word): Boolean;
function ListaDeTons: TArray<string>;
function FrequenciaTexto(F: Cardinal): string;
function FaixaRX(F: Cardinal): Boolean;
function NomeValido(const S: string): string;
function TextoFrequencia(const S: string; out F: Cardinal): Boolean;
function TextoScramble(V: Byte): string;
function TextoEncrypt(V: Byte): string;
// O texto como o rádio o aceita no campo (o _clean_text do driver), ou EConvertError.
function TextoDoCampo(const Nome, S: string; Maximo: Integer): string;
// As notas como o rádio as toca: de 1 a 32, cada uma de 0 (pausa) ou 300 a 3000 Hz e de 10 a 1000 ms, até 3 s.
function NotasValidas(const N: TArray<TNota>; out Erro: string): Boolean;
// O registro de 136 bytes (o formato no alto) e as notas dele (nil se não confere).
function RegistroDeNotas(const N: TArray<TNota>): TBytes;
function NotasDoRegistro(const B: TBytes): TArray<TNota>;

const
  // o código do tom: 0 sem tom; de 1 a 105 o DCS normal e de 106 a 210 o invertido (nesta ordem de códigos); 600 ou
  // mais o CTCSS em décimos de Hz (o _decode_tone do radtel_rt900.py)
  TONS_DCS: array[0..104] of Word = (
    23, 25, 26, 31, 32, 36, 43, 47, 51, 53, 54, 65, 71, 72, 73, 74, 114, 115, 116, 122, 125, 131, 132, 134, 143, 145,
    152, 155, 156, 162, 165, 172, 174, 205, 212, 223, 225, 226, 243, 244, 245, 246, 251, 252, 255, 261, 263, 265,
    266, 271, 274, 306, 311, 315, 325, 331, 332, 343, 346, 351, 356, 364, 365, 371, 411, 412, 413, 423, 431, 432,
    445, 446, 452, 454, 455, 462, 464, 465, 466, 503, 506, 516, 523, 526, 532, 546, 565, 606, 612, 624, 627, 631,
    632, 645, 654, 662, 664, 703, 712, 723, 731, 732, 734, 743, 754);
  TONS_CTCSS: array[0..49] of Word = (
    670, 693, 719, 744, 770, 797, 825, 854, 885, 915, 948, 974, 1000, 1035, 1072, 1109, 1148, 1188, 1230, 1273, 1318,
    1365, 1413, 1462, 1514, 1567, 1598, 1622, 1655, 1679, 1713, 1738, 1773, 1799, 1835, 1862, 1899, 1928, 1966, 1995,
    2035, 2065, 2107, 2181, 2257, 2291, 2336, 2418, 2503, 2541);
  // o SCRAMBLE em ordem de frequência, como o menu do rádio: o valor que o rádio guarda de cada um (o SCRAMBLE_VALUES)
  SCRAMBLES: array[0..8] of Byte = (0, 3, 4, 2, 5, 6, 7, 8, 1);

implementation

uses
  System.IOUtils, System.ZLib, System.DateUtils, System.Math, Idioma, LogoFormato, Transporte;

const
  MAGIA: array[0..7] of Byte = (Ord('T'), Ord('E'), Ord('M'), Ord('P'), Ord('R'), Ord('A'), 0, 1);
  MODELO_RADIO = 'BJ-8300';
  CABECALHO_TAM = 10;
  BANCO_TAM = 4 + BANCO_NOME + 120 + 2;
  MAPA_INICIO = 4 + BANCO_NOME;
  WATCH_SINGLE = 1;

const
  ROGER_MARCA: array[0..3] of Byte = (Ord('B'), Ord('C'), Ord('O'), Ord('1'));

// 0x1234 mais as palavras de 16 bits dos primeiros N bytes (N par), a soma do roger do firmware
function SomaRoger(const B: TBytes; N: Integer): Word;
var
  K: Integer;
  S: Cardinal;
begin
  S := $1234;
  K := 0;
  while K < N do
  begin
    Inc(S, B[K] or B[K + 1] shl 8);
    Inc(K, 2);
  end;
  Result := Word(S);
end;

function NotasValidas(const N: TArray<TNota>; out Erro: string): Boolean;
var
  Nota: TNota;
  Total: Integer;
begin
  Erro := '';
  Total := 0;
  if (Length(N) < 1) or (Length(N) > ROGER_NOTAS) then
    Erro := Format(Tr('De 1 a %d notas.', 'From 1 to %d notes.'), [ROGER_NOTAS]);
  for Nota in N do
  begin
    if (Nota.Hz <> 0) and ((Nota.Hz < 300) or (Nota.Hz > 3000)) then
      Erro := Tr('Os tons vão de 300 a 3000 Hz (0 é pausa).', 'Tones go from 300 to 3000 Hz (0 is a pause).');
    if (Nota.Ms < 10) or (Nota.Ms > 1000) then
      Erro := Tr('Cada nota vai de 10 a 1000 ms.', 'Each note goes from 10 to 1000 ms.');
    Inc(Total, Nota.Ms);
  end;
  if Total > ROGER_TOTAL_MS then
    Erro := Format(Tr('O ROGER passa de %d ms (tem %d).', 'The ROGER exceeds %d ms (it has %d).'), [ROGER_TOTAL_MS, Total]);
  Result := Erro = '';
end;

function RegistroDeNotas(const N: TArray<TNota>): TBytes;
var
  K: Integer;
begin
  Result := nil;
  SetLength(Result, ROGER_REGISTRO);   // zerado: o byte livre e as notas que sobram
  Move(ROGER_MARCA[0], Result[0], 4);
  Result[4] := Length(N);
  for K := 0 to High(N) do
  begin
    PWord(@Result[6 + K * 4])^ := N[K].Hz;
    PWord(@Result[8 + K * 4])^ := N[K].Ms;
  end;
  PWord(@Result[ROGER_REGISTRO - 2])^ := SomaRoger(Result, ROGER_REGISTRO - 2);
end;

function NotasDoRegistro(const B: TBytes): TArray<TNota>;
var
  K, N: Integer;
  Erro: string;
begin
  Result := nil;
  if (Length(B) < ROGER_REGISTRO) or not CompareMem(@B[0], @ROGER_MARCA[0], 4) or
     (PWord(@B[ROGER_REGISTRO - 2])^ <> SomaRoger(B, ROGER_REGISTRO - 2)) then
    Exit;
  N := B[4];
  if (N < 1) or (N > ROGER_NOTAS) then
    Exit;
  SetLength(Result, N);
  for K := 0 to N - 1 do
  begin
    Result[K].Hz := PWord(@B[6 + K * 4])^;
    Result[K].Ms := PWord(@B[8 + K * 4])^;
  end;
  if not NotasValidas(Result, Erro) then
    Result := nil;
end;

function BlocosDaLeitura: Integer;
var
  A: TArea;
begin
  Result := 0;
  for A in AREAS do
    Inc(Result, A.Tamanho div BLOCO);
end;

function LeLogo(C: TCaboBJ): TBytes;
begin
  C.Total := LOGO_TAMANHO div BLOCO;
  C.Feito := 0;
  Result := C.Le(LOGO_ENDERECO, LOGO_TAMANHO);
end;

procedure GravaLogo(C: TCaboBJ; const Dados: TBytes);
var
  D: Cardinal;
  Lido: TBytes;
begin
  C.Total := 2 * LOGO_TAMANHO div BLOCO;
  C.Feito := 0;
  D := 0;
  while D < LOGO_TAMANHO do
  begin
    C.GravaBloco(LOGO_ENDERECO + D, Copy(Dados, D, BLOCO));
    Inc(D, BLOCO);
  end;
  Lido := C.Le(LOGO_ENDERECO, LOGO_TAMANHO);
  if not CompareMem(@Lido[0], @Dados[0], LOGO_TAMANHO) then
    raise EFalha.Create(Tr('O rádio não guardou o logo: este firmware ainda não grava o logo pelo cabo.',
                           'The radio did not keep the logo: this firmware does not write the logo over the cable ' +
                           'yet.'));
end;

// a soma dos registros (soma_de, bancos.c): começa em 0x1234 e anda um bit a cada byte
function Soma(const B: TBytes; N: Integer): Word;
var
  I: Integer;
begin
  Result := $1234;
  for I := 0 to N - 1 do
    Result := Word((Result shl 1) + B[I]);
end;

// "88.5", "D023N", "D023I" ou "OFF"
function TomTexto(T: Word): string;
begin
  if T >= 600 then
    Exit(Format('%d.%d', [T div 10, T mod 10]));
  if (T < 1) or (T > 2 * Length(TONS_DCS)) then
    Exit('OFF');
  Result := Format('D%.3d', [TONS_DCS[(T - 1) mod Length(TONS_DCS)]]);
  if T <= Length(TONS_DCS) then
    Result := Result + 'N'
  else
    Result := Result + 'I';
end;

// o contrário do TomTexto: "OFF", "88.5", "D023N" ou "D023I" (também "D23N" e "88,5")
function TomDeTexto(const S: string; out T: Word): Boolean;
var
  U: string;
  K, Codigo: Integer;
  Decimos: Double;
begin
  U := S.Trim.ToUpper.Replace(',', '.');
  T := 0;
  if (U = '') or (U = 'OFF') then
    Exit(True);
  if U.StartsWith('D') and (Length(U) >= 3) and CharInSet(U[Length(U)], ['N', 'I', 'R']) and
     TryStrToInt(Copy(U, 2, Length(U) - 2), Codigo) then
  begin
    for K := 0 to High(TONS_DCS) do
      if TONS_DCS[K] = Codigo then
      begin
        T := K + 1;
        if U[Length(U)] <> 'N' then
          Inc(T, Length(TONS_DCS));
        Exit(True);
      end;
    Exit(False);
  end;
  if TryStrToFloat(U, Decimos, TFormatSettings.Invariant) then
    for K := 0 to High(TONS_CTCSS) do
      if TONS_CTCSS[K] = Round(Decimos * 10) then
      begin
        T := TONS_CTCSS[K];
        Exit(True);
      end;
  Result := False;
end;

function ListaDeTons: TArray<string>;
var
  K: Integer;
begin
  Result := ['OFF'];
  for K := 0 to High(TONS_CTCSS) do
    Result := Result + [TomTexto(TONS_CTCSS[K])];
  for K := 1 to 2 * Length(TONS_DCS) do
    Result := Result + [TomTexto(K)];
end;

// o que o rádio sintoniza (o VALID_BANDS do driver): de 18 a 1000 MHz
function FaixaRX(F: Cardinal): Boolean;
begin
  Result := (F >= 1800000) and (F < 100000000);
end;

// o nome com o que a tela do rádio escreve, até 12 letras; o CHIRP grava minúsculas, e elas ficam como vieram
function NomeValido(const S: string): string;
begin
  Result := '';
  for var Ch in S do
    if (Length(Result) < 12) and (CharInSet(Ch, [' '..'_']) or CharInSet(Ch, ['a'..'z'])) then
      Result := Result + Ch;
end;

// 10 Hz -> "462.12500"
function FrequenciaTexto(F: Cardinal): string;
begin
  Result := Format('%d.%.5d', [F div 100000, F mod 100000]);
end;

// "462.125", "462,125" ou "462.12500" -> 10 Hz
function TextoFrequencia(const S: string; out F: Cardinal): Boolean;
var
  T, Inteira, Fracao: string;
  P: Integer;
begin
  T := S.Trim.Replace(',', '.');
  P := T.IndexOf('.');
  if P < 0 then
  begin
    Inteira := T;
    Fracao := '';
  end
  else
  begin
    Inteira := T.Substring(0, P);
    Fracao := T.Substring(P + 1);
  end;
  Result := (Inteira <> '') and (Length(Inteira) <= 4) and (Length(Fracao) <= 5);
  for var Ch in Inteira + Fracao do
    Result := Result and CharInSet(Ch, ['0'..'9']);
  if Result then
    F := Cardinal(StrToInt(Inteira)) * 100000 + Cardinal(StrToInt((Fracao + '00000').Substring(0, 5)));
end;

function TextoScramble(V: Byte): string;
var
  K: Integer;
begin
  for K := 0 to High(SCRAMBLES) do
    if SCRAMBLES[K] = V then
    begin
      if K = 0 then
        Exit('OFF');
      Exit(Format('%d Hz', [2600 + 100 * K]));
    end;
  Result := '3400 Hz';  // de 9 a 15 o rádio inverte em 3400 Hz
end;

function TextoEncrypt(V: Byte): string;
begin
  if V = 0 then
    Result := 'OFF'
  else
    Result := 'ENCRY' + IntToStr(V);
end;

function TextoDoCampo(const Nome, S: string; Maximo: Integer): string;
const
  INDICATIVO = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ';
  MORSE = '0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz !"$&''()+,-./:;=?@';
var
  Pode: string;
begin
  Result := S;
  if (Nome = 'CALLSIGN') or (Nome = 'TO') then
  begin
    // o indicativo e o destino em maiúsculas, só com letras e algarismos (e o '-' do destino), como o aprs_guarda_texto
    Result := S.ToUpper;
    Pode := INDICATIVO;
    if Nome = 'TO' then
      Pode := Pode + '-';
  end
  else if Nome = 'MORSE MESSAGE' then
    Pode := MORSE
  else
    Pode := '';
  for var Ch in Result do
    if (Pode <> '') and (Pos(Ch, Pode) = 0) or (Pode = '') and ((Ch < ' ') or (Ch > '~') or CharInSet(Ch, ['{', '|', '~'])) then
      raise EConvertError.CreateFmt(Tr('"%s" não cabe neste campo.', '"%s" does not fit this field.'), [Ch]);
  if Length(Result) > Maximo then
    raise EConvertError.CreateFmt(Tr('Até %d caracteres.', 'Up to %d characters.'), [Maximo]);
end;

// o BCD dos canais, o byte baixo primeiro: 0 se algum algarismo passa de 9
function Bcd(const B: TBytes; I: Integer): Cardinal;
var
  K: Integer;
begin
  Result := 0;
  for K := 3 downto 0 do
  begin
    if (B[I + K] shr 4 > 9) or (B[I + K] and 15 > 9) then
      Exit(0);
    Result := Result * 100 + Cardinal(B[I + K] shr 4) * 10 + Cardinal(B[I + K] and 15);
  end;
end;

procedure PoeBcd(var B: TBytes; I: Integer; V: Cardinal);
var
  K: Integer;
begin
  for K := 0 to 3 do
  begin
    B[I + K] := Byte(V mod 10 or (V div 10 mod 10) shl 4);
    V := V div 100;
  end;
end;

function Valido(const A: TBytes; const R: TRegistroDesc): Boolean;
begin
  Result := (Length(A) >= R.Tamanho) and CompareMem(@A[0], @R.Magia[0], Length(R.Magia)) and
            (Soma(A, R.Tamanho - 2) = A[R.Tamanho - 2] or A[R.Tamanho - 1] shl 8);
end;

{ TMemoriaBJ8300 }

constructor TMemoriaBJ8300.Create(Menu: TMenu);
var
  R: TRegistroDesc;
  A, D, K: Integer;
begin
  FMenu := Menu;
  FDesfazer := TStack<TEstado>.Create;
  SetLength(FRegistros, Length(AREAS));
  for R in Menu.Registros do
    if Onde(R.Endereco, A, D) then
      FRegistros[A] := R;
  FRegistros[AREA_CABECALHO].Endereco := AREAS[AREA_CABECALHO].Endereco;
  FRegistros[AREA_CABECALHO].Magia := TEncoding.ASCII.GetBytes('BNK3');
  FRegistros[AREA_CABECALHO].Tamanho := CABECALHO_TAM;
  for K := 0 to BANCOS - 1 do
  begin
    FRegistros[AREA_BANCO1 + K].Endereco := AREAS[AREA_BANCO1 + K].Endereco;
    FRegistros[AREA_BANCO1 + K].Magia := TEncoding.ASCII.GetBytes('BCO1');
    FRegistros[AREA_BANCO1 + K].Tamanho := BANCO_TAM;
  end;
end;

destructor TMemoriaBJ8300.Destroy;
begin
  FDesfazer.Free;
  inherited;
end;

function TMemoriaBJ8300.Vazia: Boolean;
begin
  Result := FAtual[0] = nil;
end;

function TMemoriaBJ8300.Onde(Endereco: Cardinal; out Area, Desloc: Integer): Boolean;
var
  I: Integer;
begin
  for I := 0 to High(AREAS) do
    if (Endereco >= AREAS[I].Endereco) and (Endereco < AREAS[I].Endereco + AREAS[I].Tamanho) then
    begin
      Area := I;
      Desloc := Endereco - AREAS[I].Endereco;
      Exit(True);
    end;
  Result := False;
end;

// A área como o rádio a lê: um registro que não confere vale zerado, com a assinatura (o VFO C, o B duas vezes) e a
// soma, e o resto do setor apagado.
function TMemoriaBJ8300.Vista(const E: TEstado; Area: Integer): TBytes;
var
  R: TRegistroDesc;
  S: Word;
begin
  R := FRegistros[Area];
  if (R.Tamanho = 0) or Valido(E[Area], R) then
    Exit(E[Area]);
  SetLength(Result, AREAS[Area].Tamanho);
  FillChar(Result[0], Length(Result), $FF);
  FillChar(Result[0], R.Tamanho, 0);
  Move(R.Magia[0], Result[0], Length(R.Magia));
  if Area = AREA_VFC then
  begin
    Move(E[AREA_VFOS][$20], Result[4], $20);
    Move(E[AREA_VFOS][$20], Result[$24], $20);
  end;
  S := Soma(Result, R.Tamanho - 2);
  Result[R.Tamanho - 2] := Byte(S);
  Result[R.Tamanho - 1] := Byte(S shr 8);
end;

// antes de mudar um campo de um registro: a atual passa a ter o registro como o rádio o lê
procedure TMemoriaBJ8300.Garante(Area: Integer);
begin
  if FRegistros[Area].Tamanho <> 0 then
    FAtual[Area] := Copy(Vista(FAtual, Area));
end;

procedure TMemoriaBJ8300.Sela(Area: Integer);
var
  N: Integer;
  S: Word;
begin
  N := FRegistros[Area].Tamanho;
  if N = 0 then
    Exit;
  S := Soma(FAtual[Area], N - 2);
  FAtual[Area][N - 2] := Byte(S);
  FAtual[Area][N - 1] := Byte(S shr 8);
end;

function TMemoriaBJ8300.LeByte(const E: TEstado; Endereco: Cardinal): Byte;
var
  Area, Desloc: Integer;
begin
  if not Onde(Endereco, Area, Desloc) then
    raise EArgumentException.CreateFmt('endereço %.6X fora das áreas', [Endereco]);
  Result := Vista(E, Area)[Desloc];
end;

procedure TMemoriaBJ8300.PoeBits(Endereco: Cardinal; Mascara, Valor: Byte);
var
  Area, Desloc: Integer;
begin
  if not Onde(Endereco, Area, Desloc) then
    raise EArgumentException.CreateFmt('endereço %.6X fora das áreas', [Endereco]);
  Garante(Area);
  FAtual[Area][Desloc] := FAtual[Area][Desloc] and not Mascara or Valor and Mascara;
  Sela(Area);
end;

procedure TMemoriaBJ8300.LeDoRadio(C: TCaboBJ);
var
  I: Integer;
begin
  for I := 0 to High(AREAS) do
    FOriginal[I] := C.Le(AREAS[I].Endereco, AREAS[I].Tamanho);
  Versao := 'TEMPRA';
  Lida := Now;
  FAtual := Copia(FOriginal);
  FDesfazer.Clear;
  OrdenaMicGain;
end;

{ O MIC GAIN do rádio põe o DEFAULT (a calibração de fábrica de cada rádio, em 0xF210) na ordem do valor, entre o
  HIGH (31), o MID (25) e o LOW (19), e o empate vai antes (o _mic_gain_order do driver): a lista daqui também. }
procedure TMemoriaBJ8300.OrdenaMicGain;
const
  NIVEIS: array[0..2] of string = ('HIGH', 'MID', 'LOW');
  GANHOS: array[0..2] of Integer = (31, 25, 19);
var
  It: TItem;
  Ordem: TArray<string>;
  Novos: TArray<TValor>;
  K, D: Integer;
  T: string;
begin
  It := FMenu.Item('MIC', 'MIC GAIN');
  if It = nil then
    Exit;
  D := 0;
  for K := 0 to 2 do
    if GANHOS[K] > FOriginal[AREA_CALIBRACAO][$210] and $1F then
      Inc(D);
  Ordem := nil;
  for K := 0 to 2 do
  begin
    if K = D then
      Ordem := Ordem + ['DEFAULT'];
    Ordem := Ordem + [NIVEIS[K]];
  end;
  if D = 3 then
    Ordem := Ordem + ['DEFAULT'];
  Novos := nil;
  for T in Ordem do
    for K := 0 to High(It.Valores) do
      if It.Valores[K].Texto = T then
        Novos := Novos + [It.Valores[K]];
  if Length(Novos) = Length(It.Valores) then
    It.Valores := Novos;
end;

function TMemoriaBJ8300.Copia(const E: TEstado): TEstado;
var
  K: Integer;
begin
  for K := 0 to High(AREAS) do
    Result[K] := Copy(E[K]);
end;

procedure TMemoriaBJ8300.Guarda;
begin
  FDesfazer.Push(Copia(FAtual));
end;

function TMemoriaBJ8300.Desfaz: Boolean;
begin
  Result := FDesfazer.Count > 0;
  if Result then
    FAtual := FDesfazer.Pop;
end;

function TMemoriaBJ8300.PodeDesfazer: Boolean;
begin
  Result := FDesfazer.Count > 0;
end;

function TMemoriaBJ8300.Mudou: Boolean;
begin
  Result := not Vazia and (Mudancas <> nil);
end;

{ Os canais }

function TMemoriaBJ8300.Mapa(K: Integer): TBytes;
begin
  Result := Vista(FAtual, AREA_BANCO1 + K);
end;

function TMemoriaBJ8300.Canal(N: Integer): TCanal;
const
  NIVEL: array[0..3] of TPotencia = (ptAlta, ptMedia, ptBaixa, ptAlta);
var
  B, M: TBytes;
  I, K: Integer;
begin
  Result := Default(TCanal);
  Result.Numero := N;
  B := Copy(FAtual[AREA_CANAIS], (N - 1) * 32, 32);
  if B[0] = $FF then
    Exit;
  Result.RX := Bcd(B, 0);
  Result.Existe := Result.RX <> 0;
  if not Result.Existe then
    Exit;
  if PCardinal(@B[4])^ <> $FFFFFFFF then
    Result.TX := Bcd(B, 4);
  Result.TomRX := B[8] or B[9] shl 8;
  Result.TomTX := B[10] or B[11] shl 8;
  if Result.TomRX = $FFFF then
    Result.TomRX := 0;
  if Result.TomTX = $FFFF then
    Result.TomTX := 0;
  Result.Scode := B[12] and 15;
  Result.PttId := B[13] and 3;
  Result.Potencia := NIVEL[B[14] and 3];
  Result.Scramble := B[14] shr 4;
  Result.AM := B[15] and 2 <> 0;
  Result.Varre := B[15] and 4 <> 0;
  Result.BCL := B[15] and 8 <> 0;
  Result.Encrypt := B[15] shr 4 and 3;
  Result.Estreito := B[15] and $40 <> 0;
  for K := 0 to BANCOS - 1 do
  begin
    M := Mapa(K);
    if M[MAPA_INICIO + (N - 1) shr 3] shr ((N - 1) and 7) and 1 <> 0 then
      Result.Bancos := Result.Bancos or 1 shl K;
  end;
  I := 20;
  while (I < 32) and (B[I] <> $FF) and (B[I] <> 0) do
  begin
    Result.Nome := Result.Nome + Char(B[I]);
    Inc(I);
  end;
end;

{ O registro do canal: um que existia guarda os bits que o rádio não usa aqui (o resto dos bytes 12, 13 e 15, e o FHSS);
  um novo começa como o set_memory do driver (16 bytes 0 e 16 FF). Os bancos mudam só no mapa dos que mudaram. }
procedure TMemoriaBJ8300.PoeCanal(const C: TCanal);
const
  BITS: array[TPotencia] of Byte = (0, 1, 2);
var
  B: TBytes;
  I, K, Area, Pos: Integer;
  Bit: Byte;
begin
  B := Copy(FAtual[AREA_CANAIS], (C.Numero - 1) * 32, 32);
  if not Canal(C.Numero).Existe then
  begin
    FillChar(B[0], 16, 0);
    FillChar(B[16], 16, $FF);
  end;
  PoeBcd(B, 0, C.RX);
  if C.TX = 0 then
    FillChar(B[4], 4, $FF)
  else
    PoeBcd(B, 4, C.TX);
  PWord(@B[8])^ := C.TomRX;
  PWord(@B[10])^ := C.TomTX;
  B[12] := B[12] and $F0 or C.Scode and 15;
  B[13] := B[13] and $FC or C.PttId and 3;
  B[14] := B[14] and $0C or BITS[C.Potencia] or (C.Scramble and 15) shl 4;
  B[15] := B[15] and $81 or Ord(C.Estreito) shl 6 or (C.Encrypt and 3) shl 4 or Ord(C.BCL) shl 3 or
           Ord(C.Varre) shl 2 or Ord(C.AM) shl 1;
  for I := 0 to 11 do
    if I < Length(C.Nome) then
      B[20 + I] := Byte(C.Nome[I + 1])
    else
      B[20 + I] := $FF;
  Move(B[0], FAtual[AREA_CANAIS][(C.Numero - 1) * 32], 32);
  Pos := MAPA_INICIO + (C.Numero - 1) shr 3;
  Bit := 1 shl ((C.Numero - 1) and 7);
  for K := 0 to BANCOS - 1 do
  begin
    Area := AREA_BANCO1 + K;
    if (Mapa(K)[Pos] and Bit <> 0) <> (C.Bancos shr K and 1 <> 0) then
    begin
      Garante(Area);
      FAtual[Area][Pos] := FAtual[Area][Pos] xor Bit;
      Sela(Area);
    end;
  end;
end;

procedure TMemoriaBJ8300.ApagaCanal(N: Integer);
var
  C: TCanal;
begin
  C := Canal(N);
  if C.Existe and (C.Bancos <> 0) then
  begin
    C.Bancos := 0;   // sai dos bancos junto: um canal novo nesse lugar não herda os bancos do velho
    PoeCanal(C);
  end;
  FillChar(FAtual[AREA_CANAIS][(N - 1) * 32], 32, $FF);
end;

function TMemoriaBJ8300.Salvos: Integer;
var
  N: Integer;
begin
  Result := 0;
  if not Vazia then
    for N := 1 to CANAIS do
      if Canal(N).Existe then
        Inc(Result);
end;

{ Os bancos }

function TMemoriaBJ8300.BancoNome(K: Integer): string;
var
  M: TBytes;
  I: Integer;
begin
  Result := '';
  M := Mapa(K);
  I := 4;
  while (I < 4 + BANCO_NOME) and (M[I] <> 0) and (M[I] <> $FF) do
  begin
    Result := Result + Char(M[I]);
    Inc(I);
  end;
  Result := Result.TrimRight;
end;

procedure TMemoriaBJ8300.PoeBancoNome(K: Integer; const Nome: string);
var
  I, Area: Integer;
begin
  Area := AREA_BANCO1 + K;
  Garante(Area);
  for I := 0 to BANCO_NOME - 1 do
    if I < Length(Nome) then
      FAtual[Area][4 + I] := Byte(Nome[I + 1])
    else
      FAtual[Area][4 + I] := 0;
  Sela(Area);
end;

function TMemoriaBJ8300.BancoAtivo(K: Integer): Boolean;
var
  H: TBytes;
begin
  H := Vista(FAtual, AREA_CABECALHO);
  Result := (H[4] or H[5] shl 8) shr K and 1 <> 0;
end;

procedure TMemoriaBJ8300.PoeBancoAtivo(K: Integer; Ativo: Boolean);
var
  L: Word;
begin
  Garante(AREA_CABECALHO);
  L := FAtual[AREA_CABECALHO][4] or FAtual[AREA_CABECALHO][5] shl 8;
  if Ativo then
    L := L or 1 shl K
  else
    L := L and not (1 shl K);
  FAtual[AREA_CABECALHO][4] := Byte(L);
  FAtual[AREA_CABECALHO][5] := Byte(L shr 8);
  Sela(AREA_CABECALHO);
end;

function TMemoriaBJ8300.CanaisNoBanco(K: Integer): Integer;
var
  N: Integer;
  M: TBytes;
begin
  Result := 0;
  M := Mapa(K);
  for N := 1 to CANAIS do
    if (M[MAPA_INICIO + (N - 1) shr 3] shr ((N - 1) and 7) and 1 <> 0) and Canal(N).Existe then
      Inc(Result);
end;

{ Os ajustes }

function TMemoriaBJ8300.Valor(It: TItem): Integer;
begin
  Result := ValorDoItem(It, function(E: Cardinal): Byte begin Result := LeByte(FAtual, E) end);
end;

function TMemoriaBJ8300.ValorOriginal(It: TItem): Integer;
begin
  Result := ValorDoItem(It, function(E: Cardinal): Byte begin Result := LeByte(FOriginal, E) end);
end;

procedure TMemoriaBJ8300.PoeValor_(It: TItem; V: Integer);
var
  B: TByteDoValor;
begin
  for B in It.Valores[V].Bytes do
    PoeBits(B.Endereco, B.Mascara, B.Valor);
end;

// um algarismo por byte, o mais alto primeiro; o que não é algarismo conta 0 (o _digits do driver)
function TMemoriaBJ8300.Algarismos(Endereco: Cardinal; N: Integer): Cardinal;
var
  I: Integer;
  D: Byte;
begin
  Result := 0;
  for I := 0 to N - 1 do
  begin
    D := LeByte(FAtual, Endereco + Cardinal(I));
    if D > 9 then
      D := 0;
    Result := Result * 10 + D;
  end;
end;

procedure TMemoriaBJ8300.PoeAlgarismos(Endereco: Cardinal; N: Integer; V: Cardinal);
var
  I: Integer;
begin
  for I := N - 1 downto 0 do
  begin
    PoeBits(Endereco + Cardinal(I), $FF, V mod 10);
    V := V div 10;
  end;
end;

function TMemoriaBJ8300.Frequencia(Lado: Integer): Cardinal;
begin
  Result := Algarismos(FMenu.Vfos[Lado], 8);
end;

procedure TMemoriaBJ8300.PoeFrequencia(Lado: Integer; F: Cardinal);
begin
  PoeAlgarismos(FMenu.Vfos[Lado], 8, F);
end;

function TMemoriaBJ8300.Offset(Lado: Integer): Cardinal;
begin
  Result := Algarismos(FMenu.Vfos[Lado] + 20, 6);
end;

procedure TMemoriaBJ8300.PoeOffset(Lado: Integer; KHz: Cardinal);
begin
  PoeAlgarismos(FMenu.Vfos[Lado] + 20, 6, KHz);
end;

function TMemoriaBJ8300.Texto(const Nome: string): string;
var
  T: TTextoDesc;
  I, N: Integer;
begin
  T := FMenu.Texto(Nome);
  N := Min(LeByte(FAtual, T.Registro + Cardinal(T.PosTamanho)), T.Maximo);
  Result := '';
  for I := 0 to N - 1 do
    Result := Result + Char(LeByte(FAtual, T.Registro + Cardinal(T.Inicio + I)));
end;

// o tamanho no byte dele e as letras, com o resto do campo em 0
procedure TMemoriaBJ8300.PoeTexto(const Nome, Valor: string);
var
  T: TTextoDesc;
  I: Integer;
begin
  T := FMenu.Texto(Nome);
  PoeBits(T.Registro + Cardinal(T.PosTamanho), $FF, Min(Length(Valor), T.Maximo));
  for I := 0 to T.Maximo - 1 do
    if I < Length(Valor) then
      PoeBits(T.Registro + Cardinal(T.Inicio + I), $FF, Byte(Valor[I + 1]))
    else
      PoeBits(T.Registro + Cardinal(T.Inicio + I), $FF, 0);
end;

{ Os grupos DTMF (o pttid do MEM_FORMAT): 15 de 16 bytes em 0xA020, os 6 primeiros com o código de cada dígito
  (DIGITOS_DTMF) e FF depois do último. }

function TMemoriaBJ8300.GrupoDtmf(K: Integer): string;
var
  I: Integer;
  D: Byte;
begin
  Result := '';
  for I := 0 to DIGITOS_GRUPO - 1 do
  begin
    D := FAtual[AREA_DTMF][$20 + K * 16 + I];
    if D >= Length(DIGITOS_DTMF) then
      Break;
    Result := Result + DIGITOS_DTMF[D + 1];
  end;
  Result := Result.Replace(' ', '');
end;

procedure TMemoriaBJ8300.PoeGrupoDtmf(K: Integer; const Digitos: string);
var
  I: Integer;
begin
  for I := 0 to DIGITOS_GRUPO - 1 do
    if I < Length(Digitos) then
      FAtual[AREA_DTMF][$20 + K * 16 + I] := Pos(Digitos[I + 1], DIGITOS_DTMF) - 1
    else
      FAtual[AREA_DTMF][$20 + K * 16 + I] := $FF;
end;

{ A gravação }

{ As regras que o driver do CHIRP aplica antes de toda gravação (sync_out): o rádio sem zonas (Full Channel, 0x9028 =
  1), sem o TRIPLE WATCH quando o WATCH é SINGLE, e sem banco ligado que não tenha canal (o rádio recusa esses). }
procedure TMemoriaBJ8300.PreparaGravacao;
var
  K: Integer;
begin
  if FAtual[AREA_AJUSTES][$28] <> 1 then
    FAtual[AREA_AJUSTES][$28] := 1;
  if (Vista(FAtual, AREA_WATCH)[4] = WATCH_SINGLE) and (FAtual[AREA_AJUSTES][4] and 1 <> 0) then
    FAtual[AREA_AJUSTES][4] := FAtual[AREA_AJUSTES][4] and $FE;
  for K := 0 to BANCOS - 1 do
    if BancoAtivo(K) and (CanaisNoBanco(K) = 0) then
      PoeBancoAtivo(K, False);
end;

// Os blocos de fábrica que mudaram, de 64 em 64 bytes, e as áreas estendidas que mudaram, inteiras; na ordem da
// gravação, a mesma do driver: as de fábrica e depois as do TEMPRA.
function TMemoriaBJ8300.Mudancas: TArray<TGravar>;
var
  A: Integer;
  D: Cardinal;
  G: TGravar;
begin
  Result := nil;
  for A := 0 to High(AREAS) do
  begin
    if A = AREA_CALIBRACAO then
      Continue;
    G.Area := A;
    if Estendido(AREAS[A].Endereco) then
    begin
      G.Desloc := 0;
      G.Tamanho := AREAS[A].Tamanho;
      if not CompareMem(@FOriginal[A][0], @FAtual[A][0], AREAS[A].Tamanho) then
        Result := Result + [G];
    end
    else
    begin
      G.Tamanho := BLOCO;
      D := 0;
      while D < AREAS[A].Tamanho do
      begin
        G.Desloc := D;
        if not CompareMem(@FOriginal[A][D], @FAtual[A][D], BLOCO) then
          Result := Result + [G];
        Inc(D, BLOCO);
      end;
    end;
  end;
end;

// gravou banco: o rádio só monta o filtro dos bancos na partida
function TMemoriaBJ8300.MudouBancos: Boolean;
var
  G: TGravar;
begin
  for G in Mudancas do
    if (G.Area >= AREA_CABECALHO) and (G.Area < AREA_BANCO1 + BANCOS) then
      Exit(True);
  Result := False;
end;

{ Cada pedaço que mudou, conferido antes contra o que o rádio tem: igual ao novo, pula; igual ao lido, grava; nem um
  nem outro, o rádio mudou desde a leitura, e nada se grava (com Restaurar, um backup aberto, grava assim mesmo). As
  áreas estendidas vão inteiras, a partir do primeiro bloco, que apaga o setor, e se conferem lendo de volta: um
  firmware que não grava a área responde o ACK do mesmo jeito. O VFO C não se grava enquanto o C está no lado B do
  rádio (TRIPLE): ali ele mora nos lugares do B, e o registro é do rádio. }
procedure TMemoriaBJ8300.GravaNoRadio(C: TCaboBJ; Restaurar: Boolean);
var
  G: TGravar;
  Gravar: TArray<TGravar>;
  Conflito: string;
  Novo, Velho, NoRadio: TBytes;
  D: Cardinal;
begin
  Gravar := nil;
  Conflito := '';
  C.Total := 0;
  for G in Mudancas do
    Inc(C.Total, G.Tamanho div BLOCO * 3);
  C.Feito := 0;
  for G in Mudancas do
  begin
    if (G.Area = AREA_VFC) and (Vista(FOriginal, AREA_WATCH)[8] <> 0) then
      raise EConflito.Create(Tr('O C está no lado B do rádio agora (TRIPLE): ali o VFO C é do rádio. Mude o C no ' +
                                'rádio, ou aperte A/B nele para ir ao A, e leia de novo.',
                                'C is on side B of the radio right now (TRIPLE): there, VFO C belongs to the radio. ' +
                                'Change C on the radio, or press A/B on it to move to A, and read again.'));
    Novo := Copy(FAtual[G.Area], G.Desloc, G.Tamanho);
    Velho := Copy(FOriginal[G.Area], G.Desloc, G.Tamanho);
    NoRadio := C.Le(AREAS[G.Area].Endereco + G.Desloc, G.Tamanho);
    if CompareMem(@NoRadio[0], @Novo[0], G.Tamanho) then
      Continue;
    if not CompareMem(@NoRadio[0], @Velho[0], G.Tamanho) and not Restaurar then
      Conflito := Conflito + Format(' %.6X', [AREAS[G.Area].Endereco + G.Desloc])
    else
      Gravar := Gravar + [G];
  end;
  if Conflito <> '' then
    raise EConflito.CreateFmt(Tr('O rádio mudou desde a leitura (em%s). Nada foi gravado: leia o rádio de novo.',
                                 'The radio changed since it was read (at%s). Nothing was written: read the radio ' +
                                 'again.'), [Conflito]);
  for G in Gravar do
  begin
    D := 0;
    while D < G.Tamanho do
    begin
      C.GravaBloco(AREAS[G.Area].Endereco + G.Desloc + D, Copy(FAtual[G.Area], G.Desloc + D, BLOCO));
      Inc(D, BLOCO);
    end;
    if Estendido(AREAS[G.Area].Endereco) then
    begin
      NoRadio := C.Le(AREAS[G.Area].Endereco, G.Tamanho);
      if not CompareMem(@NoRadio[0], @FAtual[G.Area][0], G.Tamanho) then
        if G.Area = AREA_ROGER then
          raise EFalha.Create(Tr('O rádio não guardou as notas do ROGER CUSTOM: elas precisam do firmware v164 ou mais ' +
                                 'novo.',
                                 'The radio did not keep the ROGER CUSTOM notes: they need firmware v164 or newer.'))
        else
          raise EFalha.CreateFmt(Tr('O rádio não guardou o que foi gravado em %.6X: ele precisa do firmware V3.7T ou ' +
                                    'mais novo.',
                                    'The radio did not keep what was written at %.6X: it needs firmware V3.7T or ' +
                                    'newer.'), [AREAS[G.Area].Endereco]);
    end;
  end;
end;

{ As notas do ROGER CUSTOM }

function TMemoriaBJ8300.Notas: TArray<TNota>;
begin
  Result := NotasDoRegistro(FAtual[AREA_ROGER]);
end;

// a área inteira: o registro e o resto do que se grava (os 56 bytes até o fim do terceiro bloco) em FF, como o setor
// apagado; sem notas, a área toda em FF (o firmware não toca nada)
procedure TMemoriaBJ8300.PoeNotas(const N: TArray<TNota>);
var
  B, R: TBytes;
begin
  SetLength(B, AREAS[AREA_ROGER].Tamanho);
  FillChar(B[0], Length(B), $FF);
  if N <> nil then
  begin
    R := RegistroDeNotas(N);
    Move(R[0], B[0], ROGER_REGISTRO);
  end;
  FAtual[AREA_ROGER] := B;
end;

procedure TMemoriaBJ8300.Gravou;
begin
  FOriginal := Copia(FAtual);
end;

{ O arquivo: a marca "TEMPRA" 0 1, o modelo, a versão do firmware e a data (texto com 1 byte de tamanho), as áreas
  (endereço e tamanho em 4 bytes, byte baixo primeiro, e os dados) e o CRC32 de tudo antes dele. Guarda a atual, com
  as mudanças. O mesmo formato do Tempra Link do UV-17L, que recusa este pelo modelo (e este, o dele). }

procedure EscreveTexto(S: TStream; const T: string);
var
  B: TBytes;
begin
  B := TEncoding.UTF8.GetBytes(T);
  S.WriteData(Byte(Length(B)));
  S.WriteBuffer(B, Length(B));
end;

procedure TMemoriaBJ8300.Salva(const Arquivo: string);
var
  S: TBytesStream;
  I: Integer;
  C: Cardinal;
begin
  S := TBytesStream.Create;
  try
    S.WriteBuffer(MAGIA, SizeOf(MAGIA));
    EscreveTexto(S, MODELO_RADIO);
    EscreveTexto(S, Versao);
    EscreveTexto(S, DateToISO8601(Lida, False));
    S.WriteData(Byte(Length(AREAS)));
    for I := 0 to High(AREAS) do
    begin
      S.WriteData(AREAS[I].Endereco);
      S.WriteData(AREAS[I].Tamanho);
      S.WriteBuffer(FAtual[I], Length(FAtual[I]));
    end;
    C := crc32(0, PByte(S.Memory), S.Size);
    S.WriteData(C);
    S.SaveToFile(Arquivo);
  finally
    S.Free;
  end;
end;

procedure TMemoriaBJ8300.Abre(const Arquivo: string);
var
  B: TBytes;
  P, I, K: Integer;
  Novos: TEstado;
  End_, Tam: Cardinal;

  procedure Falta(N: Integer);
  begin
    if P + N > Length(B) - 4 then
      raise EArquivo.Create(Tr('O arquivo está cortado.', 'The file is truncated.'));
  end;

  function Texto_: string;
  var
    N: Integer;
  begin
    Falta(1);
    N := B[P];
    Falta(1 + N);
    Result := TEncoding.UTF8.GetString(B, P + 1, N);
    Inc(P, 1 + N);
  end;

  function Numero: Cardinal;
  begin
    Falta(4);
    Result := PCardinal(@B[P])^;
    Inc(P, 4);
  end;

var
  Modelo, NovaVersao, Data: string;
begin
  B := TFile.ReadAllBytes(Arquivo);
  if (Length(B) < SizeOf(MAGIA) + 4) or not CompareMem(@B[0], @MAGIA, SizeOf(MAGIA)) then
    raise EArquivo.Create(Tr('Não é um arquivo do Tempra Link.', 'Not a Tempra Link file.'));
  if crc32(0, @B[0], Length(B) - 4) <> PCardinal(@B[Length(B) - 4])^ then
    raise EArquivo.Create(Tr('O arquivo está estragado (o CRC não confere).', 'The file is damaged (the CRC does not match).'));
  P := SizeOf(MAGIA);
  Modelo := Texto_;
  if Modelo <> MODELO_RADIO then
    raise EArquivo.CreateFmt(Tr('O arquivo é de outro rádio (%s): abra-o no Tempra Link dele.',
                                'The file is from another radio (%s): open it in that radio''s Tempra Link.'), [Modelo]);
  NovaVersao := Texto_;
  Data := Texto_;
  Falta(1);
  K := B[P];
  Inc(P);
  for I := 1 to K do
  begin
    End_ := Numero;
    Tam := Numero;
    Falta(Tam);
    for var A := 0 to High(AREAS) do
      if (AREAS[A].Endereco = End_) and (AREAS[A].Tamanho = Tam) then
        Novos[A] := Copy(B, P, Tam);
    Inc(P, Tam);
  end;
  // os backups de antes da v164 não têm o roger: o setor apagado, como num rádio que nunca o gravou
  if Novos[AREA_ROGER] = nil then
  begin
    SetLength(Novos[AREA_ROGER], AREAS[AREA_ROGER].Tamanho);
    FillChar(Novos[AREA_ROGER][0], AREAS[AREA_ROGER].Tamanho, $FF);
  end;
  for I := 0 to High(AREAS) do
    if Length(Novos[I]) <> Integer(AREAS[I].Tamanho) then
      raise EArquivo.CreateFmt(Tr('Falta no arquivo a área %.6X.', 'The file lacks the area at %.6X.'), [AREAS[I].Endereco]);
  FOriginal := Novos;
  FAtual := Copia(FOriginal);
  FDesfazer.Clear;
  Versao := NovaVersao;
  Lida := ISO8601ToDate(Data, False);
  OrdenaMicGain;
end;

end.
