unit SimuladorBJ;

{ O BJ-8300 simulado do lado do cabo, como o tratador de fábrica e o gancho_cabo funcionam (docs/engenharia-reversa.md,
  seção 4): o magic, o 'F', a cifra, o 'R' que dispara com 4 bytes e o 'W' com 68, o 'E', e o estendido do TEMPRA
  (no firmware de fábrica, o estendido volta com o tamanho que chegou e mais bytes). O relógio é de mentira: cada
  leitura sem nada para ler anda 20 ms. A memória é a do chip de dados (1 MB), e o 0x9000 sai dela como do resto. }

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
  public
    Memoria: TBytes;              // o chip de dados, 1 MB
    Fabrica: Boolean;             // o firmware de fábrica: sem o estendido
    GravaFora: Boolean;           // um TEMPRA que grava pelo estendido em qualquer lugar (o logo)
    Limite: Cardinal;             // até onde o estendido grava (um firmware antigo grava menos)
    Lixo: Boolean;                // chegou byte fora do protocolo
    Sessoes, Lidos: Integer;
    Gravados: TList<Cardinal>;    // os endereços dos blocos gravados, na ordem
    Mudo: Boolean;                // desligado: não responde nada
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
  end;

implementation

uses
  CaboBJ;

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

end.
