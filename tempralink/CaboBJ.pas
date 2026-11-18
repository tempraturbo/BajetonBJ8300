unit CaboBJ;

{ A memória do BJ-8300 pelo cabo, ou pelo Bluetooth, com o protocolo de fábrica que o driver do CHIRP usa
  (radtel_rt900.py) e o comando estendido do firmware TEMPRA (gancho_cabo, firmware/src/bancos.c). O protocolo está em
  docs/engenharia-reversa.md, seção 4. Sem nada de janela, para dar para testar.

  A sessão, a 57600 8N1:
    "PROGRAMBT11U"                  ACK (06)
    'F'                             8 bytes (01 ...) e mais 8 (02 ...): a identificação
    "SEND" ' ' 01 01 e 18 zeros     ACK: a cifra
    'R' endereço(2) 64              'R' endereço(2) 64 e os 64 bytes
    'W' endereço(2) 64 e 64 bytes   ACK
    'E'                             ACK: o fim
  Abaixo de 0xF000 os dados vão cifrados com "CO 7". O estendido usa o bit 7 do tamanho: 'R' ou 'W', os 16 bits de
  baixo do endereço e 0x80 + os 8 de cima; o bloco é sempre de 64 bytes, cru. O firmware de fábrica responde o
  estendido com o tamanho que recebeu, e é assim que se sabe que o rádio não está com o TEMPRA. O TEMPRA grava pelo
  estendido só de 0xC1000 a 0xD5FFF (os bancos, o WATCH, o VFO C, o Morse e o APRS), e o primeiro bloco de um setor
  o apaga; um endereço fora disso ele ignora, e responde o ACK do mesmo jeito.

  Três cuidados que vêm do rádio: nada vai antes do magic (ele termina em 'U', que é comando do modo UPDATE, e fora de
  sincronia o rádio entra em UPDATE sozinho); todo pacote tem 4 bytes ou 68, nunca 5 (o quinto fica pendente e engole
  o comando seguinte); e uma sessão por operação (depois do 'E' a sessão só fecha quando nenhum byte chega). }

interface

uses
  System.SysUtils, Transporte;

const
  BLOCO = 64;
  ESTENDIDO_INICIO = $C1000;  // o que o TEMPRA grava pelo estendido
  ESTENDIDO_FIM = $D6000;

type
  ENaoTempra = class(Exception);

  TCaboBJ = class
  private
    FT: ITransporte;
    FDentro: Boolean;  // o rádio aceitou o magic: a sessão fecha com o 'E'
    FResto: TBytes;    // o que chegou além do que se pediu
    procedure Registra(const Msg: string);
    procedure Anda;
    function Recebe(N: Integer; EsperaMs: Cardinal): TBytes;
    procedure Envia(const B: TBytes);
    procedure Entra;
    procedure Sai;
  public
    Cancelar: Boolean;
    OnRegistro: TProc<string>;
    OnProgresso: TProc<Integer, Integer>;
    Identificacao: string;  // os 16 bytes do 'F', em hexadecimal
    Feito, Total: Integer;  // os blocos, para o progresso; o Total quem chama põe
    // Entra (conferindo que o rádio é um BJ-8300 com o firmware TEMPRA), faz o Corpo e sai, mesmo com erro no meio
    // (e mesmo quando o rádio não é TEMPRA: o 'E' fecha a sessão que o magic abriu).
    procedure Sessao(const T: ITransporte; Corpo: TProc);
    function LeBloco(Endereco: Cardinal): TBytes;
    procedure GravaBloco(Endereco: Cardinal; const Dados: TBytes);
    // Tamanho em blocos inteiros.
    function Le(Endereco, Tamanho: Cardinal): TBytes;
  end;

// A cifra "CO 7" do protocolo de fábrica (o _crypt do baofeng_uv17Pro.py, com a tabela 1): a mesma nos dois sentidos.
function Cifra(const B: TBytes): TBytes;
// Os endereços que vão pelo comando estendido: tudo o que o protocolo de fábrica não alcança.
function Estendido(Endereco: Cardinal): Boolean;

implementation

uses
  System.Math, Idioma;

const
  MAGIA: RawByteString = 'PROGRAMBT11U';
  ACK = $06;
  CHAVE: array[0..3] of Byte = (Ord('C'), Ord('O'), Ord(' '), Ord('7'));
  ESPERA_MS = 1500;

function Cifra(const B: TBytes): TBytes;
var
  I: Integer;
  K: Byte;
begin
  Result := Copy(B);
  for I := 0 to High(Result) do
  begin
    K := CHAVE[I mod 4];
    if (K <> 32) and (Result[I] <> 0) and (Result[I] <> $FF) and (Result[I] <> K) and (Result[I] <> K xor $FF) then
      Result[I] := Result[I] xor K;
  end;
end;

function Estendido(Endereco: Cardinal): Boolean;
begin
  Result := Endereco >= $10000;
end;

function Bytes(const S: RawByteString): TBytes;
begin
  SetLength(Result, Length(S));
  if S <> '' then
    Move(S[1], Result[0], Length(S));
end;

// 'R' ou 'W' e o endereço: 16 bits e o tamanho de fábrica, ou o estendido
function Pacote(Letra: AnsiChar; Endereco: Cardinal): TBytes;
begin
  if Estendido(Endereco) then
    Result := [Ord(Letra), Byte(Endereco shr 8), Byte(Endereco), $80 or Byte(Endereco shr 16)]
  else
    Result := [Ord(Letra), Byte(Endereco shr 8), Byte(Endereco), BLOCO];
end;

{ TCaboBJ }

procedure TCaboBJ.Registra(const Msg: string);
begin
  if Assigned(OnRegistro) then
    OnRegistro(Msg);
end;

procedure TCaboBJ.Anda;
begin
  Inc(Feito);
  if Assigned(OnProgresso) then
    OnProgresso(Feito, Total);
end;

procedure TCaboBJ.Envia(const B: TBytes);
begin
  if Cancelar then
    raise ECancelado.Create(Tr('Cancelado.', 'Cancelled.'));
  FT.Escreve(B);
end;

// N bytes, ou o que chegou até a espera acabar; o que passa de N fica para o próximo
function TCaboBJ.Recebe(N: Integer; EsperaMs: Cardinal): TBytes;
var
  Fim: UInt64;
begin
  Result := FResto;
  Fim := FT.Agora + EsperaMs;
  while (Length(Result) < N) and (FT.Agora < Fim) do
    Result := Result + FT.Le;
  FResto := Copy(Result, N, MaxInt);
  SetLength(Result, Min(N, Length(Result)));
end;

procedure TCaboBJ.Entra;
var
  R: TBytes;
  I, K: Integer;
begin
  FT.LimpaEntrada;
  FResto := nil;
  FT.Escreve(Bytes(MAGIA));
  R := Recebe(1, ESPERA_MS);
  // Sem resposta, as velocidades mais baixas, uma a uma (Transporte.BAUDS_CABO): 115200 da v157 à v159, 57600 no de
  // fábrica e no TEMPRA até a v156. O que foi na velocidade errada chega ao rádio como ruído, e o conferidor dele
  // (0x01E3FD12) procura o primeiro 'G' e os 7 bytes seguintes: um 'G' no ruído estraga só a primeira tentativa, por
  // isso vão duas. O Bluetooth não tem velocidade (0), e não volta.
  for K := 1 to High(BAUDS_CABO) do
    if (R = nil) and (FT.Velocidade > BAUDS_CABO[K]) then
    begin
      FT.Velocidade(BAUDS_CABO[K]);
      for I := 1 to 2 do
        if R = nil then
        begin
          FT.LimpaEntrada;
          FT.Escreve(Bytes(MAGIA));
          R := Recebe(1, ESPERA_MS);
        end;
    end;
  if R = nil then
    raise ESemRadio.Create(Tr('O rádio não respondeu. Confira se ele está ligado, com o cabo bem encaixado, e se a ' +
                              'porta é a do cabo.',
                              'The radio did not answer. Check that it is on, with the cable firmly plugged in, and ' +
                              'that the port is the cable''s.'));
  if R[0] <> ACK then
    raise EFalha.CreateFmt(Tr('O rádio respondeu %.2X em vez de entrar no modo de programação. Desligue e ligue o ' +
                              'rádio e tente de novo.',
                              'The radio answered %.2X instead of entering programming mode. Turn the radio off ' +
                              'and on and try again.'), [R[0]]);
  FDentro := True;
  FT.Escreve([Ord('F')]);
  R := Recebe(16, ESPERA_MS);
  Identificacao := '';
  for I := 0 to High(R) do
    Identificacao := Identificacao + Format('%.2X', [R[I]]);
  Registra(Tr('Identificação: ', 'Identification: ') + Identificacao);
  if (Length(R) <> 16) or (R[0] <> 1) or (R[8] <> 2) then
    raise EFalha.Create(Tr('O rádio não se identificou como um BJ-8300.', 'The radio did not identify as a BJ-8300.'));
  FT.Escreve(Bytes('SEND ') + [1, 1] + [0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]);
  R := Recebe(1, ESPERA_MS);
  if (R = nil) or (R[0] <> ACK) then
    raise EFalha.Create(Tr('O rádio não aceitou a cifra do protocolo.', 'The radio did not accept the protocol''s cipher.'));
  // o firmware TEMPRA responde o estendido com 64 bytes; o de fábrica, com o tamanho que recebeu (mais de 128)
  FT.Escreve(Pacote('R', ESTENDIDO_INICIO));
  R := Recebe(4, ESPERA_MS);
  if (Length(R) <> 4) or (R[0] <> Ord('R')) then
    raise EFalha.Create(Tr('O rádio não respondeu à leitura de teste.', 'The radio did not answer the test read.'));
  if R[3] <> BLOCO then
  begin
    Recebe(R[3], ESPERA_MS);  // o resto da resposta, para o cabo não sair de sincronia
    raise ENaoTempra.Create(Tr('Este BJ-8300 não está com o firmware TEMPRA (V3.7T ou mais novo): o Tempra Link só ' +
                               'trabalha com ele.',
                               'This BJ-8300 is not running the TEMPRA firmware (V3.7T or newer): Tempra Link only ' +
                               'works with it.'));
  end;
  if Length(Recebe(BLOCO, ESPERA_MS)) <> BLOCO then
    raise EFalha.Create(Tr('O rádio não respondeu à leitura de teste.', 'The radio did not answer the test read.'));
end;

procedure TCaboBJ.Sai;
begin
  FT.Escreve([Ord('E')]);
  Recebe(1, 300);
end;

procedure TCaboBJ.Sessao(const T: ITransporte; Corpo: TProc);
begin
  FT := T;
  FDentro := False;
  try
    Entra;
    Corpo;
  finally
    if FDentro then
      Sai;
    FT := nil;
  end;
end;

function TCaboBJ.LeBloco(Endereco: Cardinal): TBytes;
var
  P, R: TBytes;
begin
  P := Pacote('R', Endereco);
  Envia(P);
  R := Recebe(4 + BLOCO, ESPERA_MS);
  if (Length(R) <> 4 + BLOCO) or (R[0] <> P[0]) or (R[1] <> P[1]) or (R[2] <> P[2]) or (R[3] <> BLOCO) then
    raise EFalha.CreateFmt(Tr('O rádio não respondeu direito à leitura de %.6X.', 'The radio did not answer the read ' +
                              'of %.6X properly.'), [Endereco]);
  Result := Copy(R, 4, BLOCO);
  if Endereco < $F000 then
    Result := Cifra(Result);
  Anda;
end;

procedure TCaboBJ.GravaBloco(Endereco: Cardinal; const Dados: TBytes);
var
  D, R: TBytes;
begin
  if Length(Dados) <> BLOCO then
    raise EArgumentException.CreateFmt('bloco de %d bytes (tem de ser %d)', [Length(Dados), BLOCO]);
  D := Dados;
  if Endereco < $F000 then
    D := Cifra(D);
  Envia(Pacote('W', Endereco) + D);
  R := Recebe(1, ESPERA_MS);
  if (R = nil) or (R[0] <> ACK) then
    raise EFalha.CreateFmt(Tr('O rádio não confirmou a gravação de %.6X.', 'The radio did not acknowledge the write of ' +
                              '%.6X.'), [Endereco]);
  Anda;
end;

function TCaboBJ.Le(Endereco, Tamanho: Cardinal): TBytes;
var
  A: Cardinal;
begin
  Result := nil;
  A := Endereco;
  while A < Endereco + Tamanho do
  begin
    Result := Result + LeBloco(A);
    Inc(A, BLOCO);
  end;
end;

end.
