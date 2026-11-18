unit SimuladorBJ;

{ O BJ-8300 simulado do lado do cabo, como o tratador de fábrica e o gancho_cabo funcionam (docs/engenharia-reversa.md,
  seção 4): o magic, o 'F', a cifra, o 'R' que dispara com 4 bytes e o 'W' com 68, o 'E', e o estendido do TEMPRA
  (no firmware de fábrica, o estendido volta com o tamanho que chegou e mais bytes). O relógio é de mentira: cada
  leitura sem nada para ler anda 20 ms. A memória é a do chip de dados (1 MB), e o 0x9000 sai dela como do resto.
  Fora da sessão, os quadros do controle remoto (v159, firmware/src/remoto.c): cada '~' 'R' com a soma certa vira o
  estado de 9 bytes; com o pedido da tela inteira, fora da TX, o rádio redesenha a tela e, com o espelho, manda as
  linhas do nó pedido (o 5o byte; o 0 é a tela toda) em quadros de uma linha, até a saída não ter mais lugar para o
  pior caso de uma (Capacidade, 2400 B: ela não anda no redesenho); o que não coube, ou a fatia na TX, fica perdido, e o
  estado vem depois das linhas, com a marca. O Redesenha faz o que uma tecla faz: manda as linhas mudadas até a saída
  encher, e a perda vai na resposta seguinte (o bit 5 do lado, que ela limpa). EstragaLinha estraga o CRC de uma
  linha. }

interface

uses
  System.SysUtils, System.Generics.Collections, Transporte;

type
  TRadioBJSimulado = class(TInterfacedObject, ITransporte)
  private
    FRelogio: UInt64;
    FEntrada, FSaida: TBytes;
    FEstado: Integer;  // 0 esperando o magic, 1 o 'F', 2 a cifra, 3 na sessão
    procedure Processa;
    procedure Manda(const B: TBytes);
    procedure Remoto;
    procedure EnviaFatia(No: Integer);
    procedure EnviaLinhas(No, Y0, Y1: Integer);
  public
    Memoria: TBytes;              // o chip de dados, 1 MB
    Fabrica: Boolean;             // o firmware de fábrica: sem o estendido
    GravaFora: Boolean;           // um TEMPRA que grava pelo estendido em qualquer lugar (o logo)
    Limite: Cardinal;             // até onde o estendido grava (um firmware antigo grava menos)
    Lixo: Boolean;                // chegou byte fora do protocolo
    Sessoes, Lidos: Integer;
    Gravados: TList<Cardinal>;    // os endereços dos blocos gravados, na ordem
    Mudo: Boolean;                // desligado: não responde nada
    Serial: Cardinal;             // a velocidade da serial do rádio (Transporte.BAUDS_CABO: 460800 da v160 em diante)
    Porta: Cardinal;              // a da porta do PC; diferentes, o que chega vira ruído e nada volta
    Ruido: Integer;
    // o controle remoto
    RemTecla, RemBits: Integer;   // o último quadro do PC (-1 nenhum)
    ToqueDesde, UltimoToque: UInt64;  // quando a tecla segura mudou, e quanto a última ficou segura (ms)
    RemQuadros, TelasInteiras: Integer;   // TelasInteiras: os redesenhos pedidos e feitos
    Fatias: TList<Integer>;       // os nós desses redesenhos, na ordem
    Perdeu: Boolean;              // a marca da fatia que não coube (o bit 6 do CONTA)
    Sujo: Boolean;                // a perda de fora das fatias, até a resposta (o bit 5 do CONTA)
    Capacidade: Integer;          // a saída do rádio, em bytes
    Status: TBytes;               // os bytes 2..7 do estado (rem é o do quadro), na ordem do firmware
    Tela: TBytes;                 // 160 x 128, o byte baixo primeiro
    EstragaLinha: Integer;        // a linha cujo quadro sai com o CRC errado na próxima tela inteira, -1 nenhuma
    constructor Create;
    destructor Destroy; override;
    function NaSessao: Boolean;
    // O que uma tecla faz: as linhas Y0 a Y1 - 1, com o espelho ligado, até a saída encher.
    procedure Redesenha(Y0, Y1: Integer);
    procedure Poe(Endereco: Cardinal; const B: TBytes);
    function Pega(Endereco, N: Cardinal): TBytes;
    // ITransporte
    procedure Escreve(const B: TBytes);
    function Le: TBytes;
    procedure LimpaEntrada;
    function Agora: UInt64;
    procedure Dorme(Ms: Cardinal);
    function Velocidade(Baud: Cardinal = 0): Cardinal;
  end;

implementation

uses
  System.Math, CaboBJ, Remoto;

const
  MAGIA: RawByteString = 'PROGRAMBT11U';
  IDENT: array[0..15] of Byte = ($01, $36, $01, $80, $04, $00, $05, $20, $02, $00, $02, $60, $01, $03, $30, $04);

constructor TRadioBJSimulado.Create;
begin
  inherited;
  SetLength(Memoria, $100000);
  FillChar(Memoria[0], Length(Memoria), $FF);
  Gravados := TList<Cardinal>.Create;
  Fatias := TList<Integer>.Create;
  Capacidade := 2400;
  Limite := ESTENDIDO_FIM;
  Serial := BAUD_CABO;
  Porta := BAUD_CABO;
  RemTecla := -1;
  RemBits := -1;
  Status := TBytes.Create(0, 4, 13, 4, 167);    // parado, recebendo, S7, a B (em evidência, DUAL), 7,4 V
  SetLength(Tela, 160 * 128 * 2);
  EstragaLinha := -1;
end;

destructor TRadioBJSimulado.Destroy;
begin
  Gravados.Free;
  Fatias.Free;
  inherited;
end;

procedure TRadioBJSimulado.Poe(Endereco: Cardinal; const B: TBytes);
begin
  if B <> nil then
    Move(B[0], Memoria[Endereco], Length(B));
end;

function TRadioBJSimulado.Pega(Endereco, N: Cardinal): TBytes;
begin
  Result := Copy(Memoria, Endereco, N);
end;

function TRadioBJSimulado.NaSessao: Boolean;
begin
  Result := FEstado <> 0;
end;

procedure TRadioBJSimulado.Manda(const B: TBytes);
begin
  FSaida := FSaida + B;
end;

// Um quadro do remoto no começo da entrada: pedida, a fatia da tela inteira, e o estado.
procedure TRadioBJSimulado.Remoto;
var
  K, No: Integer;
  S: Byte;
  Q: TBytes;
begin
  S := 0;
  for K := 0 to 5 do
    S := Byte(S + FEntrada[K]);
  if (FEntrada[1] <> Ord('R')) or (S <> FEntrada[6]) then
  begin
    Lixo := True;
    Delete(FEntrada, 0, 1);
    Exit;
  end;
  Inc(RemQuadros);
  K := IfThen(FEntrada[2] < 18, FEntrada[2], -1);
  if K <> RemTecla then
  begin
    if RemTecla >= 0 then
      UltimoToque := FRelogio - ToqueDesde;
    ToqueDesde := FRelogio;
  end;
  RemTecla := K;
  RemBits := FEntrada[3];
  No := FEntrada[4] and 63;
  Delete(FEntrada, 0, 7);
  if RemBits and 4 <> 0 then
    if Status[0] <> 0 then
      Perdeu := True
    else
    begin
      Inc(TelasInteiras);
      Fatias.Add(No);
      Perdeu := False;
      if RemBits and 2 <> 0 then
        EnviaFatia(No);
    end;
  Q := TBytes.Create(Ord('~'), Ord('r'), IfThen(RemTecla < 0, 0, RemTecla + 1) or (RemBits and 3) shl 5 or
                     IfThen(Perdeu, $80, 0), Status[0], Status[1], Status[2], Status[3] or IfThen(Sujo, 32, 0),
                     Status[4], 0);
  Sujo := False;
  for K := 0 to 7 do
    Q[8] := Byte(Q[8] + Q[K]);
  Manda(Q);
end;

procedure TRadioBJSimulado.EnviaFatia(No: Integer);
begin
  EnviaLinhas(No, 0, 128);
end;

procedure TRadioBJSimulado.Redesenha(Y0, Y1: Integer);
begin
  if RemBits and 2 <> 0 then
    EnviaLinhas(0, Y0, Y1);
end;

// As linhas Y0 a Y1 - 1 do nó No (o 0, todas), cada uma com o lugar do pior caso (160 corridas) na saída, como o
// remoto_envia: a que não cabe se perde, com as duas marcas (a de fora, Sujo, só fora da fatia).
procedure TRadioBJSimulado.EnviaLinhas(No, Y0, Y1: Integer);
var
  K, Y, X, N, U, Usado: Integer;
  Q: TBytes;
  Cor: Word;

  function Pixel(X, Y: Integer): Word;
  begin
    Result := Tela[(Y * 160 + X) * 2] or Tela[(Y * 160 + X) * 2 + 1] shl 8;
  end;

begin
  Usado := 0;
  for Y := Y0 to Y1 - 1 do
  begin
    U := Y or 128;
    while U > No do
      U := U shr 1;
    if U <> No then
      Continue;
    if Usado + 160 * 3 + 8 > Capacidade then
    begin
      Perdeu := True;
      Sujo := Sujo or (No = 0);
      Break;
    end;
    Q := TBytes.Create(1, Ord('T'), 0, 160, Y, Y + 1);
    X := 0;
    while X < 160 do
    begin
      Cor := Pixel(X, Y);
      N := 1;
      while (X + N < 160) and (Pixel(X + N, Y) = Cor) do
        Inc(N);
      Q := Q + TBytes.Create(N, Cor shr 8, Cor and $FF);
      Inc(X, N);
    end;
    K := Crc16(Q, 1, Length(Q) - 1) xor IfThen(Y = EstragaLinha, 1, 0);
    Manda(Q + TBytes.Create(K shr 8, K and $FF));
    Inc(Usado, Length(Q) + 2);
  end;
  EstragaLinha := -1;
end;

procedure TRadioBJSimulado.Processa;
var
  Letra, Tam: Byte;
  Endereco: Cardinal;
  Dados: TBytes;
  M: TBytes;
begin
  while FEntrada <> nil do
    case FEstado of
      0:
        begin
          if FEntrada[0] = Ord('~') then
          begin
            if Length(FEntrada) < 7 then
              Exit;
            Remoto;
            Continue;
          end;
          SetLength(M, Length(MAGIA));
          Move(MAGIA[1], M[0], Length(MAGIA));
          if Length(FEntrada) < Length(M) then
          begin
            if not CompareMem(@FEntrada[0], @M[0], Length(FEntrada)) then
            begin
              Lixo := True;
              FEntrada := nil;
            end;
            Exit;
          end;
          if not CompareMem(@FEntrada[0], @M[0], Length(M)) then
          begin
            Lixo := True;
            FEntrada := nil;
            Exit;
          end;
          Delete(FEntrada, 0, Length(M));
          Manda([$06]);
          FEstado := 1;
          Inc(Sessoes);
        end;
      1:
        begin
          if FEntrada[0] <> Ord('F') then
            Lixo := True
          else
          begin
            SetLength(M, Length(IDENT));
            Move(IDENT[0], M[0], Length(IDENT));
            Manda(M);
          end;
          Delete(FEntrada, 0, 1);
          FEstado := 2;
        end;
      2:
        begin
          if Length(FEntrada) < 25 then
            Exit;
          Delete(FEntrada, 0, 25);
          Manda([$06]);
          FEstado := 3;
        end;
      3:
        begin
          Letra := FEntrada[0];
          if Letra = Ord('E') then
          begin
            Delete(FEntrada, 0, 1);
            Manda([$06]);
            FEstado := 0;
            Continue;
          end;
          if Length(FEntrada) < 4 then
            Exit;
          Tam := FEntrada[3];
          Endereco := FEntrada[1] shl 8 or FEntrada[2];
          if Tam and $80 <> 0 then
            Endereco := Endereco or Cardinal(Tam and $7F) shl 16;
          if Letra = Ord('R') then
          begin
            Delete(FEntrada, 0, 4);
            Inc(Lidos);
            if Tam and $80 = 0 then
            begin
              Dados := Copy(Memoria, Endereco, Tam);
              if Endereco < $F000 then
                Dados := Cifra(Dados);
              Manda([Ord('R'), Byte(Endereco shr 8), Byte(Endereco), Tam] + Dados);
            end
            else if Fabrica then
              // o de fábrica ecoa o tamanho com o bit 7 e manda esse tanto
              Manda([Ord('R'), Byte(Endereco shr 8), Byte(Endereco), Tam] + Copy(Memoria, Endereco and $FFFF, Tam))
            else
              Manda([Ord('R'), Byte(Endereco shr 8), Byte(Endereco), BLOCO] + Copy(Memoria, Endereco, BLOCO));
          end
          else if Letra = Ord('W') then
          begin
            if Length(FEntrada) < 4 + BLOCO then
              Exit;
            Dados := Copy(FEntrada, 4, BLOCO);
            Delete(FEntrada, 0, 4 + BLOCO);
            if Tam and $80 = 0 then
            begin
              if Endereco < $F000 then
                Dados := Cifra(Dados);
              Poe(Endereco, Dados);
              Gravados.Add(Endereco);
            end
            else if not Fabrica and (GravaFora or (Endereco >= ESTENDIDO_INICIO) and (Endereco < Limite)) then
            begin
              if Endereco and $FFF = 0 then
                FillChar(Memoria[Endereco], $1000, $FF);
              Poe(Endereco, Dados);
              Gravados.Add(Endereco);
            end;
            Manda([$06]);
          end
          else
          begin
            Lixo := True;
            FEntrada := nil;
          end;
        end;
    end;
end;

procedure TRadioBJSimulado.Escreve(const B: TBytes);
begin
  if Mudo then
    Exit;
  if Porta <> Serial then
  begin
    Inc(Ruido, Length(B));
    Exit;
  end;
  FEntrada := FEntrada + B;
  Processa;
end;

function TRadioBJSimulado.Le: TBytes;
begin
  Result := FSaida;
  FSaida := nil;
  if Result = nil then
    Inc(FRelogio, 20);
end;

procedure TRadioBJSimulado.LimpaEntrada;
begin
  FSaida := nil;
end;

function TRadioBJSimulado.Agora: UInt64;
begin
  Result := FRelogio;
end;

procedure TRadioBJSimulado.Dorme(Ms: Cardinal);
begin
  Inc(FRelogio, Ms);
end;

function TRadioBJSimulado.Velocidade(Baud: Cardinal): Cardinal;
begin
  if Baud <> 0 then
  begin
    Porta := Baud;
    FSaida := nil;
  end;
  Result := Porta;
end;

end.
