unit Melodia;

{ As notas do ROGER CUSTOM a partir de um arquivo de música. O rádio toca uma nota pura por vez (até 32, de 300 a
  3000 Hz, até 3 s: o roger_custom do firmware/src/bancos.c), então do arquivo sai a melodia, não o som. Sem nada de janela, para dar
  para testar.
  - MIDI (.mid): as notas do arquivo, sem a bateria (o canal 10); em cada instante, a mais aguda que está soando. A
    nota que se repete ganha uma pausa de 10 ms antes, para se ouvir o ataque.
  - O resto (MP3, WAV, WMA, M4A), decodificado pelo Media Foundation do Windows: a altura do som medida de 10 em 10 ms
    (o YIN), em semitons; os trechos estáveis viram notas, e o silêncio, pausa. É uma aproximação: assobio, bipe e
    melodia de uma voz só saem bem; música cheia e fala, não.
  Nos dois, o trecho começa na primeira nota a partir do início pedido e vai até a duração pedida; a melodia inteira
  sobe ou desce de oitava para caber na faixa do rádio (a nota que ainda fica fora vai sozinha). Do MIDI ficam as
  primeiras 32 notas; do som, as mais curtas se juntam às vizinhas até caberem. }

interface

uses
  System.SysUtils, BJ8300;

type
  EMelodia = class(Exception);

function EhMidi(const Caminho: string): Boolean;
// O arquivo inteiro: o MIDI pelas notas, o resto pelo som.
function NotasDoArquivo(const Caminho: string; InicioMs, DuracaoMs: Integer): TArray<TNota>;
function NotasDoMidi(const Dados: TBytes; InicioMs, DuracaoMs: Integer): TArray<TNota>;
// O som mono, em -1 a 1, de InicioMs até InicioMs + MsLidos.
function AmostrasDoArquivo(const Caminho: string; InicioMs, MsLidos: Integer; out Taxa: Integer): TArray<Single>;
function NotasDoSom(const Amostras: TArray<Single>; Taxa, DuracaoMs: Integer): TArray<TNota>;

const
  TAXA_DO_SOM = 22050;  // a do WAV do SomDasNotas

// O som das notas como o rádio as toca (um tom puro por vez), num WAV de 22050 Hz, 16 bits e mono, para ouvir no PC:
// SilencioMs de silêncio na frente (a placa de som acorda nele, sem comer o começo), 4 ms de rampa onde o som começa e
// acaba (sem estalo), e a fase que continua de um tom para o seguinte.
function SomDasNotas(const N: TArray<TNota>; SilencioMs: Integer): TBytes;

implementation

uses
  Winapi.Windows, Winapi.ActiveX, System.Classes, System.IOUtils, System.Math, System.Generics.Collections,
  System.Generics.Defaults, Idioma;

type
  // um pedaço da melodia antes de virar nota do rádio: a altura em semitons do MIDI (69 = lá de 440 Hz), -1 pausa
  TTrecho = record
    Nota: Integer;
    Ms: Double;
  end;

function Trecho(Nota: Integer; Ms: Double): TTrecho;
begin
  Result.Nota := Nota;
  Result.Ms := Ms;
end;

function Hz(Nota: Integer): Integer;
begin
  Result := Round(440 * Power(2, (Nota - 69) / 12));
end;

function Cabe(Nota: Integer): Boolean;
begin
  Result := (Hz(Nota) >= 300) and (Hz(Nota) <= 3000);
end;

{ Os trechos como notas do rádio. Junta: passando de 32, as mais curtas se juntam às vizinhas (o som); sem ele, ficam
  as 32 primeiras (o MIDI). }
function ParaORadio(T: TArray<TTrecho>; DuracaoMs: Integer; Junta: Boolean): TArray<TNota>;
const
  DESVIOS: array[0..10] of Integer = (0, 1, -1, 2, -2, 3, -3, 4, -4, 5, -5);  // em oitavas, do mais perto
var
  K, J, Desvio, Melhor, Cabem, MaisCabem: Integer;
  Tempo, Fim: Double;
  Ini, Ate, Ms: Integer;
  N: TNota;

  // as pausas seguidas, e as notas iguais seguidas, viram uma só
  procedure JuntaIguais;
  var
    I: Integer;
  begin
    I := 1;
    while I < Length(T) do
      if T[I].Nota = T[I - 1].Nota then
      begin
        T[I - 1].Ms := T[I - 1].Ms + T[I].Ms;
        Delete(T, I, 1);
      end
      else
        Inc(I);
  end;

begin
  // sem as pausas das pontas
  while (T <> nil) and (T[0].Nota < 0) do
    Delete(T, 0, 1);
  while (T <> nil) and (T[High(T)].Nota < 0) do
    Delete(T, High(T), 1);
  if T = nil then
    raise EMelodia.Create(Tr('Nenhuma nota nesse trecho.', 'No note in that stretch.'));
  // a duração pedida
  Tempo := 0;
  for K := 0 to High(T) do
  begin
    if Tempo + T[K].Ms >= DuracaoMs then
    begin
      T[K].Ms := DuracaoMs - Tempo;
      SetLength(T, K + 1);
      Break;
    end;
    Tempo := Tempo + T[K].Ms;
  end;
  if Junta then
  begin
    JuntaIguais;
    while Length(T) > ROGER_NOTAS do
    begin
      K := 0;
      for J := 1 to High(T) do
        if T[J].Ms < T[K].Ms then
          K := J;
      // para a vizinha de altura mais perto; uma pausa vai para a nota de antes
      if K = 0 then
        J := 1
      else if (K = High(T)) or (T[K].Nota < 0) or (T[K - 1].Nota >= 0) and
              ((T[K + 1].Nota < 0) or (Abs(T[K - 1].Nota - T[K].Nota) <= Abs(T[K + 1].Nota - T[K].Nota))) then
        J := K - 1
      else
        J := K + 1;
      T[J].Ms := T[J].Ms + T[K].Ms;
      Delete(T, K, 1);
      JuntaIguais;
    end;
  end;
  // a oitava da melodia inteira que deixa mais notas na faixa do rádio, a mais perto da original no empate; a que
  // ainda fica fora vai sozinha
  Melhor := 0;
  MaisCabem := -1;
  for Desvio in DESVIOS do
  begin
    Cabem := 0;
    for K := 0 to High(T) do
      if (T[K].Nota >= 0) and Cabe(T[K].Nota + 12 * Desvio) then
        Inc(Cabem);
    if Cabem > MaisCabem then
    begin
      MaisCabem := Cabem;
      Melhor := Desvio;
    end;
  end;
  for K := 0 to High(T) do
    if T[K].Nota >= 0 then
    begin
      Inc(T[K].Nota, 12 * Melhor);
      while Hz(T[K].Nota) < 300 do
        Inc(T[K].Nota, 12);
      while Hz(T[K].Nota) > 3000 do
        Dec(T[K].Nota, 12);
    end;
  // em 10 ms pelas pontas acumuladas (o arredondamento não se soma), até 1 s por nota (a mais longa se divide), e as
  // 32 primeiras
  Result := nil;
  Tempo := 0;
  for K := 0 to High(T) do
  begin
    Fim := Tempo + T[K].Ms;
    Ini := Round(Tempo / 10) * 10;
    Ate := Round(Fim / 10) * 10;
    Tempo := Fim;
    while (Ate > Ini) and (Length(Result) < ROGER_NOTAS) do
    begin
      Ms := Min(Ate - Ini, 1000);
      if T[K].Nota < 0 then
        N.Hz := 0
      else
        N.Hz := Hz(T[K].Nota);
      N.Ms := Ms;
      Result := Result + [N];
      Inc(Ini, Ms);
    end;
  end;
  while (Result <> nil) and (Result[High(Result)].Hz = 0) do
    SetLength(Result, Length(Result) - 1);
  if Result = nil then
    raise EMelodia.Create(Tr('Nenhuma nota nesse trecho.', 'No note in that stretch.'));
end;

function EhMidi(const Caminho: string): Boolean;
begin
  Result := SameText(ExtractFileExt(Caminho), '.mid') or SameText(ExtractFileExt(Caminho), '.midi');
end;

function NotasDoArquivo(const Caminho: string; InicioMs, DuracaoMs: Integer): TArray<TNota>;
var
  Taxa: Integer;
begin
  if EhMidi(Caminho) then
    Result := NotasDoMidi(TFile.ReadAllBytes(Caminho), InicioMs, DuracaoMs)
  else
    // mais 3 s depois do pedido: o silêncio do começo não conta
    Result := NotasDoSom(AmostrasDoArquivo(Caminho, InicioMs, DuracaoMs + 3000, Taxa), Taxa, DuracaoMs);
end;

function SomDasNotas(const N: TArray<TNota>; SilencioMs: Integer): TBytes;
const
  RAMPA = TAXA_DO_SOM * 4 div 1000;
  ALTURA = 0.3 * 32767;
var
  Amostras: TArray<SmallInt>;
  Tempo, K, I, Ini, Fim: Integer;
  Fase, Ganho: Double;
  S: TBytesStream;

  procedure P32(X: Cardinal); begin S.WriteData(X); end;
  procedure P16(X: Word); begin S.WriteData(X); end;

begin
  Tempo := SilencioMs;
  for K := 0 to High(N) do
    Inc(Tempo, N[K].Ms);
  SetLength(Amostras, Int64(Tempo) * TAXA_DO_SOM div 1000);
  // as pontas de cada nota pelo tempo acumulado: o arredondamento não se soma
  Tempo := SilencioMs;
  Fase := 0;
  for K := 0 to High(N) do
  begin
    Ini := Int64(Tempo) * TAXA_DO_SOM div 1000;
    Inc(Tempo, N[K].Ms);
    Fim := Min(Int64(Tempo) * TAXA_DO_SOM div 1000, Length(Amostras));
    if N[K].Hz = 0 then
      Continue;
    for I := Ini to Fim - 1 do
    begin
      Ganho := 1;
      if (K = 0) or (N[K - 1].Hz = 0) then
        Ganho := Min(Ganho, (I - Ini) / RAMPA);
      if (K = High(N)) or (N[K + 1].Hz = 0) then
        Ganho := Min(Ganho, (Fim - 1 - I) / RAMPA);
      Amostras[I] := Round(ALTURA * Ganho * Sin(Fase));
      Fase := Fase + 2 * Pi * N[K].Hz / TAXA_DO_SOM;
      if Fase > 2 * Pi then
        Fase := Fase - 2 * Pi;
    end;
  end;
  S := TBytesStream.Create;
  try
    S.WriteBuffer(AnsiString('RIFF')[1], 4);
    P32(36 + 2 * Length(Amostras));
    S.WriteBuffer(AnsiString('WAVEfmt ')[1], 8);
    P32(16);
    P16(1);              // PCM
    P16(1);              // mono
    P32(TAXA_DO_SOM);
    P32(2 * TAXA_DO_SOM);
    P16(2);
    P16(16);
    S.WriteBuffer(AnsiString('data')[1], 4);
    P32(2 * Length(Amostras));
    if Amostras <> nil then
      S.WriteBuffer(Amostras[0], 2 * Length(Amostras));
    Result := Copy(S.Bytes, 0, S.Size);
  finally
    S.Free;
  end;
end;

{ O MIDI }

type
  TSom = record
    Ini, Fim: Int64;   // em ticks, e depois de convertidos em ms (Ms*)
    IniMs, FimMs: Double;
    Nota: Integer;
  end;

function NotasDoMidi(const Dados: TBytes; InicioMs, DuracaoMs: Integer): TArray<TNota>;
var
  P, Fim, Faixa, Faixas, Divisao: Integer;
  Tick, MaiorTick: Int64;
  Status, B, D1, D2, Meta: Byte;
  Id: string;
  Tamanho: Cardinal;
  Abertas: array[0..15, 0..127] of Int64;
  Sons: TArray<TSom>;
  Tempos: TArray<TPair<Int64, Integer>>;  // o tick e os microssegundos por semínima
  Pontos: TArray<Double>;
  T: TArray<TTrecho>;
  S: TSom;
  K, J, Topo: Integer;
  T0, Ate, A, Z: Double;
  Ataque: Boolean;

  procedure Estragado;
  begin
    raise EMelodia.Create(Tr('O arquivo MIDI está estragado ou não é MIDI.', 'The MIDI file is damaged or not MIDI.'));
  end;

  function Le: Byte;
  begin
    if P >= Length(Dados) then
      Estragado;
    Result := Dados[P];
    Inc(P);
  end;

  function Le32: Cardinal;
  begin
    Result := Cardinal(Le) shl 24;
    Result := Result or Cardinal(Le) shl 16;
    Result := Result or Cardinal(Le) shl 8;
    Result := Result or Le;
  end;

  function Vlq: Cardinal;
  var
    I: Integer;
    X: Byte;
  begin
    Result := 0;
    for I := 1 to 4 do
    begin
      X := Le;
      Result := Result shl 7 or X and $7F;
      if X < $80 then
        Exit;
    end;
    Estragado;
  end;

  procedure Fecha(Canal, Nota: Integer; Ate: Int64);
  begin
    if Abertas[Canal, Nota] >= 0 then
    begin
      S.Ini := Abertas[Canal, Nota];
      S.Fim := Ate;
      S.Nota := Nota;
      Sons := Sons + [S];
      Abertas[Canal, Nota] := -1;
    end;
  end;

  // o tick em ms, pelo mapa dos tempos (ou pelos quadros do SMPTE)
  function Ms(X: Int64): Double;
  var
    I: Integer;
    Base: Double;
    Desde: Int64;
    Tempo: Integer;
  begin
    if Divisao and $8000 <> 0 then
      Exit(X * 1000 / (-ShortInt(Divisao shr 8) * (Divisao and $FF)));
    Base := 0;
    Desde := 0;
    Tempo := 500000;
    for I := 0 to High(Tempos) do
    begin
      if Tempos[I].Key > X then
        Break;
      Base := Base + (Tempos[I].Key - Desde) * Tempo / Divisao / 1000;
      Desde := Tempos[I].Key;
      Tempo := Tempos[I].Value;
    end;
    Result := Base + (X - Desde) * Tempo / Divisao / 1000;
  end;

begin
  P := 0;
  if (Length(Dados) < 14) or (TEncoding.ANSI.GetString(Dados, 0, 4) <> 'MThd') then
    Estragado;
  P := 4;
  Tamanho := Le32;
  if Tamanho < 6 then
    Estragado;
  Le;
  Le;
  Faixas := Le shl 8;
  Faixas := Faixas or Le;
  Divisao := Le shl 8;
  Divisao := Divisao or Le;
  if (Divisao = 0) or (Divisao and $8000 <> 0) and ((-ShortInt(Divisao shr 8) <= 0) or (Divisao and $FF = 0)) then
    Estragado;
  P := 8 + Integer(Tamanho);
  for K := 0 to 15 do
    for J := 0 to 127 do
      Abertas[K, J] := -1;
  MaiorTick := 0;
  Faixa := 0;
  while (Faixa < Faixas) and (P + 8 <= Length(Dados)) do
  begin
    Id := TEncoding.ANSI.GetString(Dados, P, 4);
    Inc(P, 4);
    Tamanho := Le32;
    if Tamanho > Cardinal(Length(Dados) - P) then
      Estragado;
    Fim := P + Integer(Tamanho);
    if Id <> 'MTrk' then
    begin
      P := Fim;
      Continue;
    end;
    Inc(Faixa);
    Tick := 0;
    Status := 0;
    D1 := 0;
    D2 := 0;
    while P < Fim do
    begin
      Inc(Tick, Vlq);
      B := Le;
      if B = $FF then
      begin
        Meta := Le;
        Tamanho := Vlq;
        if P + Integer(Tamanho) > Fim then
          Estragado;
        if (Meta = $51) and (Tamanho = 3) then
          Tempos := Tempos + [TPair<Int64, Integer>.Create(Tick, Dados[P] shl 16 or Dados[P + 1] shl 8 or Dados[P + 2])];
        Inc(P, Tamanho);
        Status := 0;
        Continue;
      end;
      if (B = $F0) or (B = $F7) then
      begin
        Tamanho := Vlq;
        Inc(P, Tamanho);
        Status := 0;
        Continue;
      end;
      if B >= $80 then
      begin
        Status := B;
        D1 := Le;
      end
      else if Status = 0 then
        Estragado
      else
        D1 := B;
      case Status shr 4 of
        $8, $9, $A, $B, $E:
          D2 := Le;
        $C, $D:
          D2 := 0;
      else
        Estragado;
      end;
      if (Status and $F = 9) or (D1 > 127) then  // o canal 10 é a bateria
        Continue;
      if (Status shr 4 = $9) and (D2 > 0) then
      begin
        Fecha(Status and $F, D1, Tick);  // a mesma nota de novo, sem ter sido solta
        Abertas[Status and $F, D1] := Tick;
      end
      else if Status shr 4 in [$8, $9] then
        Fecha(Status and $F, D1, Tick);
    end;
    if P > Fim then
      Estragado;
    MaiorTick := Max(MaiorTick, Tick);
  end;
  for K := 0 to 15 do
    for J := 0 to 127 do
      Fecha(K, J, MaiorTick);
  TArray.Sort<TPair<Int64, Integer>>(Tempos, TComparer<TPair<Int64, Integer>>.Construct(
    function(const X, Y: TPair<Int64, Integer>): Integer
    begin
      Result := CompareValue(X.Key, Y.Key);
    end));
  // o começo: o primeiro ataque a partir do início pedido; só valem as notas que começam dali até o fim do trecho
  T0 := -1;
  for K := 0 to High(Sons) do
  begin
    Sons[K].IniMs := Ms(Sons[K].Ini);
    Sons[K].FimMs := Ms(Sons[K].Fim);
    if (Sons[K].IniMs >= InicioMs) and ((T0 < 0) or (Sons[K].IniMs < T0)) then
      T0 := Sons[K].IniMs;
  end;
  if T0 < 0 then
    raise EMelodia.CreateFmt(Tr('O MIDI não tem nota a partir de %.1f s.', 'The MIDI has no note from %.1f s on.'),
                             [InicioMs / 1000]);
  Ate := T0 + DuracaoMs;
  Pontos := nil;
  for K := High(Sons) downto 0 do
    if (Sons[K].IniMs < T0) or (Sons[K].IniMs >= Ate) or (Sons[K].FimMs <= Sons[K].IniMs) then
      Delete(Sons, K, 1)
    else
    begin
      Sons[K].FimMs := Min(Sons[K].FimMs, Ate);
      Pontos := Pontos + [Sons[K].IniMs, Sons[K].FimMs];
    end;
  TArray.Sort<Double>(Pontos);
  // a mais aguda em cada pedaço entre dois pontos
  T := nil;
  for K := 0 to High(Pontos) - 1 do
  begin
    A := Pontos[K];
    Z := Pontos[K + 1];
    if Z <= A then
      Continue;
    Topo := -1;
    Ataque := False;
    for S in Sons do
      if (S.IniMs <= A) and (S.FimMs > A) and (S.Nota >= Topo) then
      begin
        Ataque := (S.Nota > Topo) and (S.IniMs = A) or (S.Nota = Topo) and (Ataque or (S.IniMs = A));
        Topo := S.Nota;
      end;
    if (T <> nil) and (T[High(T)].Nota = Topo) and (not Ataque or (Topo < 0)) then
      T[High(T)].Ms := T[High(T)].Ms + Z - A
    else
    begin
      // a mesma nota atacada de novo: 10 ms de pausa antes, tirados da de antes
      if (T <> nil) and (T[High(T)].Nota = Topo) and (T[High(T)].Ms > 20) then
      begin
        T[High(T)].Ms := T[High(T)].Ms - 10;
        T := T + [Trecho(-1, 10)];
      end;
      T := T + [Trecho(Topo, Z - A)];
    end;
  end;
  Result := ParaORadio(T, DuracaoMs, False);
end;

{ O som }

function NotasDoSom(const Amostras: TArray<Single>; Taxa, DuracaoMs: Integer): TArray<TNota>;
const
  LIMIAR = 0.15;   // o d' do YIN abaixo do qual o som tem altura
  ACEITA = 0.35;   // sem nenhum abaixo do LIMIAR, o menor ainda vale até aqui
  MENOR_MS = 40;   // o trecho mais curto que fica sozinho
var
  X: TArray<Single>;
  R, Passo, TauMin, TauMax, Quadros, Q, I, J, Tau, Inicio, Silencios: Integer;
  Norm, Rms: TArray<Double>;
  Soma, Diff, Pico, F, Ant, Prox, Den, MsQuadro: Double;
  Brutas, Notas, Janela: TArray<Integer>;
  T: TArray<TTrecho>;
begin
  // até uns 22 kHz, pela média de pares: a altura que interessa fica abaixo de 3 kHz
  X := Copy(Amostras);
  R := Taxa;
  while R > 30000 do
  begin
    for I := 0 to Length(X) div 2 - 1 do
      X[I] := (X[2 * I] + X[2 * I + 1]) / 2;
    SetLength(X, Length(X) div 2);
    R := R div 2;
  end;
  // de 70 a 3000 Hz; cada quadro olha 2 x TauMax amostras, de 10 em 10 ms
  Passo := R div 100;
  TauMin := Max(2, R div 3000);
  TauMax := R div 70;
  MsQuadro := Passo * 1000 / R;
  Quadros := 0;
  if Length(X) >= 2 * TauMax then
    Quadros := (Length(X) - 2 * TauMax) div Passo + 1;
  if Quadros < 1 then
    raise EMelodia.Create(Tr('O trecho de som é curto demais.', 'The stretch of sound is too short.'));
  SetLength(Norm, TauMax + 1);
  SetLength(Brutas, Quadros);
  SetLength(Rms, Quadros);
  Pico := 0;
  for Q := 0 to Quadros - 1 do
  begin
    Inicio := Q * Passo;
    Soma := 0;
    for J := 0 to TauMax - 1 do
      Soma := Soma + Sqr(X[Inicio + J]);
    Rms[Q] := Sqrt(Soma / TauMax);
    Pico := Max(Pico, Rms[Q]);
    // o YIN: a diferença do quadro com ele mesmo deslocado, normalizada pela média acumulada
    Soma := 0;
    for Tau := 1 to TauMax do
    begin
      Diff := 0;
      for J := 0 to TauMax - 1 do
        Diff := Diff + Sqr(X[Inicio + J] - X[Inicio + J + Tau]);
      Soma := Soma + Diff;
      if Soma > 0 then
        Norm[Tau] := Diff * Tau / Soma
      else
        Norm[Tau] := 1;
    end;
    // o primeiro vale abaixo do LIMIAR, até o fundo dele; sem nenhum, o menor, se não for alto demais
    Tau := -1;
    for J := TauMin to TauMax - 1 do
      if Norm[J] < LIMIAR then
      begin
        Tau := J;
        while (Tau < TauMax - 1) and (Norm[Tau + 1] < Norm[Tau]) do
          Inc(Tau);
        Break;
      end;
    if Tau < 0 then
    begin
      Tau := TauMin;
      for J := TauMin + 1 to TauMax - 1 do
        if Norm[J] < Norm[Tau] then
          Tau := J;
      if Norm[Tau] > ACEITA then
        Tau := -1;
    end;
    if Tau < 0 then
      Brutas[Q] := -1
    else
    begin
      // o fundo do vale entre as amostras, pela parábola dos três pontos
      Ant := Norm[Tau - 1];
      Prox := Norm[Tau + 1];
      Den := Ant - 2 * Norm[Tau] + Prox;
      F := Tau;
      if Den > 0 then
        F := Tau + (Ant - Prox) / (2 * Den);
      Brutas[Q] := Round(69 + 12 * Log2(R / F / 440));
    end;
  end;
  // o silêncio: abaixo de um décimo do pico (-20 dB)
  for Q := 0 to Quadros - 1 do
    if (Rms[Q] < Pico / 10) or (Rms[Q] < 0.001) then
      Brutas[Q] := -1;
  // a mediana de 5 quadros tira os erros soltos (de oitava, quase sempre)
  SetLength(Notas, Quadros);
  for Q := 0 to Quadros - 1 do
  begin
    Janela := nil;
    Silencios := 0;
    for J := Max(0, Q - 2) to Min(Quadros - 1, Q + 2) do
      if Brutas[J] < 0 then
        Inc(Silencios)
      else
        Janela := Janela + [Brutas[J]];
    if Silencios > Length(Janela) then
      Notas[Q] := -1
    else
    begin
      TArray.Sort<Integer>(Janela);
      Notas[Q] := Janela[Length(Janela) div 2];
    end;
  end;
  // os quadros iguais seguidos viram um trecho; o curto demais vai para o vizinho de antes (o primeiro, para o de
  // depois), e as iguais que ficam lado a lado se juntam; o silêncio das pontas fica, porque sai inteiro depois
  T := nil;
  for Q := 0 to Quadros - 1 do
    if (T <> nil) and (T[High(T)].Nota = Notas[Q]) then
      T[High(T)].Ms := T[High(T)].Ms + MsQuadro
    else
      T := T + [Trecho(Notas[Q], MsQuadro)];
  I := 0;
  while (I < Length(T)) and (Length(T) > 1) do
    if (T[I].Ms < MENOR_MS) and ((T[I].Nota >= 0) or (I > 0) and (I < High(T))) then
    begin
      J := IfThen(I > 0, I - 1, 1);
      T[J].Ms := T[J].Ms + T[I].Ms;
      Delete(T, I, 1);
      if (I > 0) and (I < Length(T)) and (T[I].Nota = T[I - 1].Nota) then
      begin
        T[I - 1].Ms := T[I - 1].Ms + T[I].Ms;
        Delete(T, I, 1);
      end;
      I := Max(0, I - 1);
    end
    else
      Inc(I);
  Result := ParaORadio(T, DuracaoMs, True);
end;

{ O Media Foundation: só o que a leitura usa, na ordem dos cabeçalhos do Windows (mfobjects.h e mfreadwrite.h); os
  métodos que não se chamam ficam como marcadores do lugar. }

type
  IMFAttributes = interface(IUnknown)
    ['{2CD2D921-C447-44A7-A13C-4ADABFC247E3}']
    procedure _GetItem; stdcall;
    procedure _GetItemType; stdcall;
    procedure _CompareItem; stdcall;
    procedure _Compare; stdcall;
    function GetUINT32(const [Ref] Chave: TGUID; out Valor: UINT32): HResult; stdcall;
    procedure _GetUINT64; stdcall;
    procedure _GetDouble; stdcall;
    function GetGUID(const [Ref] Chave: TGUID; out Valor: TGUID): HResult; stdcall;
    procedure _GetStringLength; stdcall;
    procedure _GetString; stdcall;
    procedure _GetAllocatedString; stdcall;
    procedure _GetBlobSize; stdcall;
    procedure _GetBlob; stdcall;
    procedure _GetAllocatedBlob; stdcall;
    procedure _GetUnknown; stdcall;
    procedure _SetItem; stdcall;
    procedure _DeleteItem; stdcall;
    procedure _DeleteAllItems; stdcall;
    procedure _SetUINT32; stdcall;
    procedure _SetUINT64; stdcall;
    procedure _SetDouble; stdcall;
    function SetGUID(const [Ref] Chave, Valor: TGUID): HResult; stdcall;
    procedure _SetString; stdcall;
    procedure _SetBlob; stdcall;
    procedure _SetUnknown; stdcall;
    procedure _LockStore; stdcall;
    procedure _UnlockStore; stdcall;
    procedure _GetCount; stdcall;
    procedure _GetItemByIndex; stdcall;
    procedure _CopyAllItems; stdcall;
  end;

  IMFMediaType = interface(IMFAttributes)
    ['{44AE0FA8-EA31-4109-8D2E-4CAE4997C555}']
  end;

  IMFMediaBuffer = interface(IUnknown)
    ['{045FA593-8799-42B8-BC8D-8968C6453507}']
    function Lock(out Bytes: PByte; Maximo, Atual: PDWORD): HResult; stdcall;
    function Unlock: HResult; stdcall;
  end;

  IMFSample = interface(IMFAttributes)
    ['{C40A00F2-B93A-4D80-AE8C-5A1C634F58E4}']
    procedure _GetSampleFlags; stdcall;
    procedure _SetSampleFlags; stdcall;
    procedure _GetSampleTime; stdcall;
    procedure _SetSampleTime; stdcall;
    procedure _GetSampleDuration; stdcall;
    procedure _SetSampleDuration; stdcall;
    procedure _GetBufferCount; stdcall;
    procedure _GetBufferByIndex; stdcall;
    function ConvertToContiguousBuffer(out Buffer: IMFMediaBuffer): HResult; stdcall;
  end;

  IMFSourceReader = interface(IUnknown)
    ['{70AE66F2-C809-4E4F-8915-BDCB406B7993}']
    procedure _GetStreamSelection; stdcall;
    function SetStreamSelection(Fluxo: DWORD; Escolhido: BOOL): HResult; stdcall;
    procedure _GetNativeMediaType; stdcall;
    function GetCurrentMediaType(Fluxo: DWORD; out Tipo: IMFMediaType): HResult; stdcall;
    function SetCurrentMediaType(Fluxo: DWORD; Reservado: PDWORD; Tipo: IMFMediaType): HResult; stdcall;
    function SetCurrentPosition(const [Ref] Formato: TGUID; const [Ref] Posicao: TPropVariant): HResult; stdcall;
    function ReadSample(Fluxo, Controle: DWORD; FluxoLido, Flags: PDWORD; Tempo: PInt64;
      out Amostra: IMFSample): HResult; stdcall;
  end;

const
  MF_VERSION = $00020070;
  MFSTARTUP_LITE = 1;
  MF_SOURCE_READER_FIRST_AUDIO_STREAM = $FFFFFFFD;
  MF_SOURCE_READER_ALL_STREAMS = $FFFFFFFE;
  MF_SOURCE_READERF_ENDOFSTREAM = 2;
  MF_MT_MAJOR_TYPE: TGUID = '{48EBA18E-F8C9-4687-BF11-0A74C9F96A8F}';
  MF_MT_SUBTYPE: TGUID = '{F7E34C9A-42E8-4714-B74B-CB29D72C35E5}';
  MF_MT_AUDIO_NUM_CHANNELS: TGUID = '{37E48BF5-645E-4C5B-89DE-ADA9E29B696A}';
  MF_MT_AUDIO_SAMPLES_PER_SECOND: TGUID = '{5FAEEAE7-0290-4C31-9E8A-C534F68D9DBA}';
  MF_MT_AUDIO_BITS_PER_SAMPLE: TGUID = '{F2DEB57F-40FA-4764-AA33-ED4F2D1FF669}';
  MFMediaType_Audio: TGUID = '{73647561-0000-0010-8000-00AA00389B71}';
  MFAudioFormat_PCM: TGUID = '{00000001-0000-0010-8000-00AA00389B71}';
  MFAudioFormat_Float: TGUID = '{00000003-0000-0010-8000-00AA00389B71}';
  TEMPO_EM_100NS: TGUID = '{00000000-0000-0000-0000-000000000000}';  // o GUID_NULL do SetCurrentPosition

// delayed: o Windows sem o Media Foundation (as edições N) abre o programa, e só a leitura do som falha (o aviso é de
// que o delayed é só do Windows, como o programa)
{$WARN SYMBOL_PLATFORM OFF}
function MFStartup(Versao: ULONG; Flags: DWORD): HResult; stdcall; external 'mfplat.dll' delayed;
function MFShutdown: HResult; stdcall; external 'mfplat.dll' delayed;
function MFCreateMediaType(out Tipo: IMFMediaType): HResult; stdcall; external 'mfplat.dll' delayed;
function MFCreateSourceReaderFromURL(Url: PWideChar; Atributos: IMFAttributes;
  out Leitor: IMFSourceReader): HResult; stdcall; external 'mfreadwrite.dll' delayed;
{$WARN SYMBOL_PLATFORM ON}

function AmostrasDoArquivo(const Caminho: string; InicioMs, MsLidos: Integer; out Taxa: Integer): TArray<Single>;
var
  Com: HResult;
  Leitor: IMFSourceReader;
  Tipo: IMFMediaType;
  Amostra: IMFSample;
  Buffer: IMFMediaBuffer;
  Canais_, Bits_, Hz_: UINT32;
  Canais, Bits: Integer;
  Sub: TGUID;
  Flutuante: Boolean;
  Posicao: TPropVariant;
  Flags, N: DWORD;
  Tempo, Desde, Primeiro: Int64;
  Bytes: PByte;
  Quadro, Quantos, Cheio, K, C: Integer;
  V: Single;

  procedure Confere(R: HResult);
  begin
    if Failed(R) then
      raise EMelodia.CreateFmt(Tr('Não deu para ler o som de %s (erro %.8X do Windows).',
                                  'Could not read the sound of %s (Windows error %.8X).'),
                               [ExtractFileName(Caminho), Cardinal(R)]);
  end;

  // uma amostra de um canal, de -1 a 1
  function Uma(P: PByte): Single;
  begin
    if Flutuante then
      Result := PSingle(P)^
    else
      case Bits of
        8: Result := (P^ - 128) / 128;
        16: Result := PSmallInt(P)^ / 32768;
        24: Result := (Integer(P[0] shl 8 or P[1] shl 16 or P[2] shl 24) div 256) / 8388608;
      else
        Result := PInteger(P)^ / 2147483648.0;
      end;
  end;

begin
  Result := nil;
  Com := CoInitializeEx(nil, COINIT_APARTMENTTHREADED);
  try
    try
      Confere(MFStartup(MF_VERSION, MFSTARTUP_LITE));
    except
      on E: EExternalException do
        raise EMelodia.Create(Tr('Este Windows não tem o Media Foundation, que lê MP3, WAV e os outros sons (nas ' +
                                 'edições N ele vem no Media Feature Pack). Um MIDI funciona sem ele.',
                                 'This Windows lacks Media Foundation, which reads MP3, WAV and the other sounds (on ' +
                                 'N editions it comes in the Media Feature Pack). A MIDI works without it.'));
    end;
    try
      Confere(MFCreateSourceReaderFromURL(PChar(Caminho), nil, Leitor));
      Confere(Leitor.SetStreamSelection(MF_SOURCE_READER_ALL_STREAMS, False));
      Confere(Leitor.SetStreamSelection(MF_SOURCE_READER_FIRST_AUDIO_STREAM, True));
      // o som decodificado em float; o PCM se o decodificador não der float
      Confere(MFCreateMediaType(Tipo));
      Confere(Tipo.SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio));
      Confere(Tipo.SetGUID(MF_MT_SUBTYPE, MFAudioFormat_Float));
      if Failed(Leitor.SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nil, Tipo)) then
      begin
        Confere(Tipo.SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM));
        Confere(Leitor.SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nil, Tipo));
      end;
      Tipo := nil;
      Confere(Leitor.GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, Tipo));
      Confere(Tipo.GetUINT32(MF_MT_AUDIO_NUM_CHANNELS, Canais_));
      Confere(Tipo.GetUINT32(MF_MT_AUDIO_SAMPLES_PER_SECOND, Hz_));
      Confere(Tipo.GetUINT32(MF_MT_AUDIO_BITS_PER_SAMPLE, Bits_));
      Confere(Tipo.GetGUID(MF_MT_SUBTYPE, Sub));
      Flutuante := Sub = MFAudioFormat_Float;
      if (Canais_ < 1) or (Canais_ > 8) or (Hz_ < 4000) or (Hz_ > 384000) or (Bits_ mod 8 <> 0) or (Bits_ < 8) or
         (Bits_ > 32) or Flutuante and (Bits_ <> 32) then
        Confere(E_FAIL);
      Canais := Canais_;
      Bits := Bits_;
      Taxa := Hz_;
      Quadro := Canais * Bits div 8;
      // pula para o início; o que vier antes dele (o leitor para no quadro de antes) se descarta pelo tempo
      if InicioMs > 0 then
      begin
        FillChar(Posicao, SizeOf(Posicao), 0);
        Posicao.vt := VT_I8;
        Posicao.hVal.QuadPart := Int64(InicioMs) * 10000;
        Leitor.SetCurrentPosition(TEMPO_EM_100NS, Posicao);
      end;
      Desde := Int64(InicioMs) * Taxa div 1000;
      Quantos := Int64(MsLidos) * Taxa div 1000;
      SetLength(Result, Quantos);
      Cheio := 0;
      while Cheio < Quantos do
      begin
        Amostra := nil;
        Confere(Leitor.ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, nil, @Flags, @Tempo, Amostra));
        if Flags and MF_SOURCE_READERF_ENDOFSTREAM <> 0 then
          Break;
        if Amostra = nil then
          Continue;
        Confere(Amostra.ConvertToContiguousBuffer(Buffer));
        Confere(Buffer.Lock(Bytes, nil, @N));
        try
          Primeiro := Tempo * Taxa div 10000000;
          for K := 0 to Integer(N) div Quadro - 1 do
            if (Primeiro + K >= Desde) and (Cheio < Quantos) then
            begin
              V := 0;
              for C := 0 to Integer(Canais) - 1 do
                V := V + Uma(@Bytes[K * Quadro + C * (Bits div 8)]);
              Result[Cheio] := V / Canais;
              Inc(Cheio);
            end;
        finally
          Buffer.Unlock;
        end;
        Buffer := nil;
      end;
      SetLength(Result, Cheio);
    finally
      // as interfaces soltas antes de o Media Foundation fechar
      Buffer := nil;
      Amostra := nil;
      Tipo := nil;
      Leitor := nil;
      MFShutdown;
    end;
  finally
    if Succeeded(Com) then
      CoUninitialize;
  end;
end;

end.
