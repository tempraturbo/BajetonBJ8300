unit SimuladorUpdate;

{ Um BJ-8300 no modo UPDATE, para os testes do GravadorBJ: o lado do rádio do protocolo do BinTalk
  (docs/engenharia-reversa.md, seção 42), com o relógio de mentira. Ele responde o 06 com 01; troca para 10000 com o
  primeiro 01 10000 e responde 01; com o segundo, pede o "carregador" (02) em pedaços, e no fim manda 01 (o pulo
  para o carregador); com o 01 115200 troca para 115200 e pede uma "imagem" em pedaços, com um 01 de andamento no
  meio (o PC manda de novo a última carga, e ele segue); e manda o 04, espera o 03 e termina com o 03. Um pacote do
  PC igual ao anterior, na mesma velocidade, é uma repetição: ele manda de novo a última resposta. Cada byte que ele
  manda só chega certo se a serial do PC estiver na velocidade dele (senão, chega lixo), e cada pacote do PC só vale se
  chegar na velocidade dele. Ele confere que cada pedaço servido é o do arquivo, e guarda o que viu. As falhas se
  ligam pelos campos. }

interface

uses
  System.SysUtils, System.Classes, System.Generics.Collections, GravadorBJ;

type
  TPedaco = record
    Endereco, Tamanho: Cardinal;
  end;

  TRadioUpdate = class(TInterfacedObject, IPortaGravacao)
  private
    FAgora: UInt64;
    FBaudPC, FBaudRadio: Cardinal;
    FSaida: TList<TBytes>;        // o que o rádio mandou e o PC ainda não leu, com a velocidade do rádio
    FSaidaBaud: TList<Cardinal>;
    FEntrada: TBytes;             // o que o PC mandou e ainda não fechou um pacote
    FUltimoPacote: TBytes;        // o último pacote do PC, e a velocidade em que veio
    FUltimoBaud: Cardinal;
    FUltimaResposta: TBytes;
    FAndamento: Boolean;          // a última resposta foi o 01 de andamento no meio da imagem
    FFase, FPedido, FRespostas: Integer;
    FClaro: TBytes;
    procedure Manda(const Carga: TBytes);
    procedure Recebe(const Carga: TBytes);
    procedure PedeProximo;
  public
    // o que o rádio pede: o carregador e depois a imagem, em pedaços
    Carregador, Imagem: TArray<TPedaco>;
    // as falhas, pelo número da resposta (1 = a primeira): perde, estraga o CRC, cala dali em diante, ou manda antes
    // um comando que não existe (07)
    PerdeResposta, EstragaResposta, CalaDepois, DesconhecidoAntes: Integer;
    Mudo: Boolean;
    // o que se viu
    Velocidades: TList<Cardinal>;  // as trocas de velocidade do PC, na ordem
    Erros: TStringList;            // o que não bateu com o protocolo
    Servidos: Integer;             // pedaços conferidos com o arquivo
    Repetidos: Integer;            // pacotes do PC que vieram de novo
    Terminou: Boolean;             // mandou o 03 final
    Fins: Integer;                 // os 03 que o PC mandou
    constructor Create(const Claro: TBytes);
    destructor Destroy; override;
    procedure Escreve(const B: TBytes);
    function Le: TBytes;
    procedure Velocidade(Baud: Cardinal);
    function Agora: UInt64;
    procedure Dorme(Ms: Cardinal);
  end;

implementation

constructor TRadioUpdate.Create(const Claro: TBytes);
begin
  inherited Create;
  FClaro := Claro;
  FBaudPC := VELOCIDADE_INICIO;
  FBaudRadio := VELOCIDADE_INICIO;
  FSaida := TList<TBytes>.Create;
  FSaidaBaud := TList<Cardinal>.Create;
  Velocidades := TList<Cardinal>.Create;
  Erros := TStringList.Create;
  FAgora := 1000;
end;

destructor TRadioUpdate.Destroy;
begin
  FSaida.Free;
  FSaidaBaud.Free;
  Velocidades.Free;
  Erros.Free;
  inherited;
end;

function TRadioUpdate.Agora: UInt64;
begin
  Result := FAgora;
end;

procedure TRadioUpdate.Dorme(Ms: Cardinal);
begin
  Inc(FAgora, Ms);
end;

procedure TRadioUpdate.Velocidade(Baud: Cardinal);
begin
  FBaudPC := Baud;
  Velocidades.Add(Baud);
end;

procedure TRadioUpdate.Manda(const Carga: TBytes);
var
  P: TBytes;
begin
  FUltimaResposta := Carga;
  Inc(FRespostas);
  if Mudo or ((CalaDepois > 0) and (FRespostas > CalaDepois)) then
    Exit;
  if FRespostas = DesconhecidoAntes then
  begin
    FSaida.Add(Empacota(TBytes.Create(7)));
    FSaidaBaud.Add(FBaudRadio);
    Exit;  // o PC repete o último pacote, e aí a resposta de verdade vai
  end;
  if FRespostas = PerdeResposta then
    Exit;
  P := Empacota(Carga);
  if FRespostas = EstragaResposta then
    P[Length(P) - 1] := P[Length(P) - 1] xor $5A;
  FSaida.Add(P);
  FSaidaBaud.Add(FBaudRadio);
end;

procedure TRadioUpdate.PedeProximo;
var
  Lista: TArray<TPedaco>;
  C: TBytes;
begin
  FAndamento := False;
  if FFase = 2 then
    Lista := Carregador
  else
    Lista := Imagem;
  if FPedido < Length(Lista) then
  begin
    SetLength(C, 9);
    C[0] := 2;
    PCardinal(@C[1])^ := Lista[FPedido].Endereco;
    PCardinal(@C[5])^ := Lista[FPedido].Tamanho;
    Manda(C);
  end
  else if FFase = 2 then
    Manda(TBytes.Create(1))  // o carregador chegou: pula para ele
  else
    Manda(TBytes.Create(4)); // a imagem chegou: pede o 03
end;

{ Um pacote novo do PC, na velocidade certa. }
procedure TRadioUpdate.Recebe(const Carga: TBytes);
var
  Baud, Endereco, N, Esperado: Cardinal;
  Lista: TArray<TPedaco>;
begin
  case Carga[0] of
    6:
      if FFase = 0 then
      begin
        FFase := 1;
        Manda(TBytes.Create(1));
      end
      else
        Erros.Add('06 fora de hora');
    1:
      begin
        Baud := PCardinal(@Carga[1])^;
        if (FFase = 1) and (Baud = VELOCIDADE_TROCA) then
        begin
          FFase := 2;  // o primeiro 01 10000: troca e responde na velocidade nova
          FBaudRadio := VELOCIDADE_TROCA;
          Manda(TBytes.Create(1));
        end
        else if (FFase = 2) and (FPedido = 0) and (Baud = VELOCIDADE_TROCA) then
          PedeProximo  // o segundo: pede o carregador
        else if (FFase = 2) and (FPedido = Length(Carregador)) and (Baud = VELOCIDADE_TRABALHO) then
        begin
          FFase := 3;
          FPedido := 0;
          FBaudRadio := VELOCIDADE_TRABALHO;
          PedeProximo;
        end
        else
          Erros.Add(Format('01 %d fora de hora', [Baud]));
      end;
    2:
      begin
        if FFase = 2 then
          Lista := Carregador
        else
          Lista := Imagem;
        Endereco := PCardinal(@Carga[1])^;
        N := PCardinal(@Carga[5])^;
        if (FPedido < Length(Lista)) and (Endereco = Lista[FPedido].Endereco) then
        begin
          // perto do fim do arquivo vem o que há, com a carga do tamanho pedido (o resto em zero), como no BinTalk
          Esperado := Lista[FPedido].Tamanho;
          if Endereco + Esperado > Cardinal(Length(FClaro)) then
            Esperado := Cardinal(Length(FClaro)) - Endereco;
          if (N <> Esperado) or (Cardinal(Length(Carga)) <> 9 + Lista[FPedido].Tamanho) or
             not CompareMem(@Carga[9], @FClaro[Endereco], N) then
            Erros.Add(Format('o pedaço em %x não é o do arquivo', [Endereco]))
          else
            Inc(Servidos);
          Inc(FPedido);
          // no meio da imagem, um 01 de andamento
          if (FFase = 3) and (FPedido = 2) then
          begin
            Manda(TBytes.Create(1));
            FAndamento := True;
          end
          else
            PedeProximo;
        end
        else
          Erros.Add(Format('02 em %x fora de hora', [Endereco]));
      end;
    3:
      begin
        Inc(Fins);
        if (FFase = 3) and (FPedido = Length(Imagem)) then
        begin
          Terminou := True;
          Manda(TBytes.Create(3));
        end
        else
          Erros.Add('03 fora de hora');
      end;
  else
    Erros.Add(Format('comando %d desconhecido', [Carga[0]]));
  end;
end;

procedure TRadioUpdate.Escreve(const B: TBytes);
var
  N: Integer;
  Pacote, Carga: TBytes;
begin
  // o que chega fora da velocidade do rádio é lixo para ele
  if FBaudPC <> FBaudRadio then
  begin
    Erros.Add(Format('pacote do PC a %d com o rádio a %d', [FBaudPC, FBaudRadio]));
    Exit;
  end;
  FEntrada := FEntrada + B;
  while Length(FEntrada) >= 6 do
  begin
    if (FEntrada[0] <> $AA) or (FEntrada[1] <> $55) then
    begin
      Erros.Add('pacote do PC sem o AA 55');
      FEntrada := nil;
      Exit;
    end;
    N := FEntrada[2] or FEntrada[3] shl 8;
    if Length(FEntrada) < N + 6 then
      Exit;
    Pacote := Copy(FEntrada, 0, N + 6);
    Delete(FEntrada, 0, N + 6);
    if Crc16(Pacote, 0, N + 4) <> (Pacote[N + 4] or Pacote[N + 5] shl 8) then
    begin
      Erros.Add('CRC errado num pacote do PC');
      Continue;
    end;
    if (N = 0) then
    begin
      Erros.Add('pacote do PC sem carga');
      Continue;
    end;
    if (Length(Pacote) = Length(FUltimoPacote)) and CompareMem(@Pacote[0], @FUltimoPacote[0], Length(Pacote)) and
       (FBaudPC = FUltimoBaud) then
    begin
      Inc(Repetidos);
      if FAndamento then
        PedeProximo           // o PC mandou de novo a última carga depois do andamento: segue
      else
        Manda(FUltimaResposta);  // uma repetição: a mesma resposta
    end
    else
    begin
      FUltimoPacote := Pacote;
      FUltimoBaud := FBaudPC;
      Carga := Copy(Pacote, 4, N);
      Recebe(Carga);
    end;
  end;
end;

function TRadioUpdate.Le: TBytes;
var
  I: Integer;
begin
  if FSaida.Count = 0 then
  begin
    Inc(FAgora, 20);  // a leitura espera uns 20 ms pelo primeiro byte
    Exit(nil);
  end;
  // aos pedaços, como a serial entrega: no máximo 7 bytes por leitura
  Result := Copy(FSaida[0], 0, 7);
  if FSaidaBaud[0] <> FBaudPC then
    for I := 0 to High(Result) do
      Result[I] := $00;
  if Length(FSaida[0]) <= 7 then
  begin
    FSaida.Delete(0);
    FSaidaBaud.Delete(0);
  end
  else
    FSaida[0] := Copy(FSaida[0], 7, MaxInt);
  Inc(FAgora, 1);
end;

end.
