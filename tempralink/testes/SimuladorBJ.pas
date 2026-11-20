unit SimuladorBJ;

{ O BJ-8300 simulado do lado do cabo, como o tratador de fábrica e o gancho_cabo funcionam (docs/engenharia-reversa.md,
  seção 4): o magic, o 'F', a cifra, o 'R' que dispara com 4 bytes e o 'W' com 68, o 'E', e o estendido do TEMPRA
  (no firmware de fábrica, o estendido volta com o tamanho que chegou e mais bytes). O relógio é de mentira: cada
  leitura sem nada para ler anda 20 ms. A memória é a do chip de dados (1 MB), e o 0x9000 sai dela como do resto.
  A gravação de fábrica é a do tratador (0x01E43A68): o bloco que começa um setor de 4 KB o apaga, e o resto vai por
  cima, como a flash grava (só zera bits). Os blocos que o rádio guarda na RAM (o 0x8000, o 0x9000 e os do rádio de
  AM, FM e SSB, 0xB000, 0xB040 e 0xB080) se gravam inteiros; o 0xB080 só usa 25 bytes, e a leitura dele traz no resto o
  que sobrou no buffer do cabo (0x01E43A84).
  Fora da sessão, os quadros do controle remoto (v162, firmware/src/remoto.c): cada '~' 'R' com a soma certa vira o
  estado de 9 bytes e, com a tela pedida, o Retrato logo depois (o que se puser nele: os testes usam os da prévia,
  testes\retrato-*.bin). Com o Laco (o APRS ou o Morse no rádio), o estado vem e o retrato não. EstragaRetrato
  estraga um byte do próximo. }

interface

uses
  System.SysUtils, System.Generics.Collections, Transporte;

type
  TRadioBJSimulado = class(TInterfacedObject, ITransporte)
  private
    FRelogio: UInt64;
    FEntrada, FSaida: TBytes;
    FBuffer: TBytes;   // o buffer de 64 B do cabo no rádio (0x14E02): o último bloco lido ou gravado
    FEstado: Integer;  // 0 esperando o magic, 1 o 'F', 2 a cifra, 3 na sessão
    procedure GravaFabrica(Endereco: Cardinal; const Dados: TBytes);
    procedure Processa;
    procedure Manda(const B: TBytes);
    procedure Remoto;
  public
    Memoria: TBytes;              // o chip de dados, 1 MB
    Fabrica: Boolean;             // o firmware de fábrica: sem o estendido
    Versao: string;               // a que o TEMPRA responde no VERSAO_CABO (v170; vazia nos de antes, que leem o chip)
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
    Acorda: Boolean;              // v165: uma tecla do rádio acendeu a tela (o ACORDADO); o pedido novo de apagar o zera
    ToqueDesde, UltimoToque: UInt64;  // quando a tecla segura mudou, e quanto a última ficou segura (ms)
    RemQuadros, Retratos: Integer;   // Retratos: os mandados
    Status: TBytes;               // os bytes 3..7 do estado (rem é o do quadro), na ordem do firmware
    Retrato: TBytes;              // o que vai depois do estado com a tela pedida (nil, nada)
    Laco: Boolean;                // o laço do APRS ou do Morse: o estado vem, e o retrato não
    EstragaRetrato: Boolean;      // o próximo retrato sai com um byte trocado
    constructor Create;
    destructor Destroy; override;
    function NaSessao: Boolean;
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
  Limite := ESTENDIDO_FIM;
  Versao := FAIXA.Maxima;
  Serial := BAUD_CABO;
  Porta := BAUD_CABO;
  RemTecla := -1;
  RemBits := -1;
  Status := TBytes.Create(0, 4, 13, 4, 167);    // parado, recebendo, S7, a B (em evidência, DUAL), 7,4 V
end;

destructor TRadioBJSimulado.Destroy;
begin
  Gravados.Free;
  inherited;
end;

procedure TRadioBJSimulado.Poe(Endereco: Cardinal; const B: TBytes);
begin
  if B <> nil then
    Move(B[0], Memoria[Endereco], Length(B));
end;

procedure TRadioBJSimulado.GravaFabrica(Endereco: Cardinal; const Dados: TBytes);
var
  I: Integer;
begin
  case Endereco of
    $8000, $9000, $B000, $B040:
      Poe(Endereco, Dados);
    $B080:
      Poe(Endereco, Copy(Dados, 0, 25));
  else
    if Endereco and $FFF = 0 then
      FillChar(Memoria[Endereco], $1000, $FF);
    for I := 0 to High(Dados) do
      Memoria[Endereco + Cardinal(I)] := Memoria[Endereco + Cardinal(I)] and Dados[I];
  end;
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

// Um quadro do remoto no começo da entrada: o estado e, com a tela pedida, o retrato.
procedure TRadioBJSimulado.Remoto;
var
  K: Integer;
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
  if (FEntrada[3] and BIT_APAGA <> 0) and ((RemBits < 0) or (RemBits and BIT_APAGA = 0)) then
    Acorda := False;              // o pedido novo apaga na hora (firmware v165)
  RemBits := FEntrada[3];
  Delete(FEntrada, 0, 7);
  Q := TBytes.Create(Ord('~'), Ord('r'), IfThen(RemTecla < 0, 0, RemTecla + 1) or (RemBits and 3) shl 5 or
                     IfThen(Acorda and (RemBits and BIT_APAGA <> 0), 128, 0), Status[0],
                     Status[1], Status[2], Status[3], Status[4], 0);
  for K := 0 to 7 do
    Q[8] := Byte(Q[8] + Q[K]);
  Manda(Q);
  if (RemBits and BIT_TELA <> 0) and (Retrato <> nil) and not Laco then
  begin
    Q := Copy(Retrato);
    if EstragaRetrato then
      Q[Length(Q) div 2] := Q[Length(Q) div 2] xor $10;
    EstragaRetrato := False;
    Manda(Q);
    Inc(Retratos);
  end;
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
              if (Endereco = $B080) and (Length(FBuffer) = Tam) then
                Dados := Copy(Memoria, Endereco, 25) + Copy(FBuffer, 25, Tam - 25);
              FBuffer := Copy(Dados);
              if Endereco < $F000 then
                Dados := Cifra(Dados);
              Manda([Ord('R'), Byte(Endereco shr 8), Byte(Endereco), Tam] + Dados);
            end
            else if Fabrica then
              // o de fábrica ecoa o tamanho com o bit 7 e manda esse tanto
              Manda([Ord('R'), Byte(Endereco shr 8), Byte(Endereco), Tam] + Copy(Memoria, Endereco and $FFFF, Tam))
            else if (Versao <> '') and (Tam = $80 or VERSAO_CABO shr 16) then
            begin
              // v170: a versão, e o resto do bloco com o que o pacote tinha (aqui, zeros)
              Dados := TEncoding.ASCII.GetBytes('TEMPRA ' + Versao);
              SetLength(Dados, BLOCO);
              Manda([Ord('R'), Byte(Endereco shr 8), Byte(Endereco), BLOCO] + Dados);
            end
            else
              // o chip de 1 MB: o endereço dá a volta
              Manda([Ord('R'), Byte(Endereco shr 8), Byte(Endereco), BLOCO] + Copy(Memoria, Endereco and $FFFFF, BLOCO));
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
              FBuffer := Copy(Dados);
              GravaFabrica(Endereco, Dados);
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
