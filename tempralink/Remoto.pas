unit Remoto;

{ O controle remoto do BJ-8300 pelo cabo ou pelo Bluetooth, com o firmware v159 ou mais novo (firmware/src/remoto.c),
  o rádio funcionando normalmente. Sem nada de janela: serve ao Tempra Link do Windows e ao do Android.

  O PC manda, a cada 250 ms e na hora em que algo muda, sempre duas vezes seguidas, o quadro de 7 bytes
    '~' 'R' tecla bits 0 0 soma
  com a tecla segura (o código cru da varredura, abaixo, ou 255) e os bits 1 o PTT, 2 o espelho da tela e 4 a tela
  inteira; os dois do meio vão em 255 (menos bits 0 no fio, que é também o do PTT), menos o primeiro com o pedido da
  tela inteira, que leva a fatia (v158), e a soma é a dos 6 bytes de antes. Com o remoto ligado pelo cabo, o firmware v157
  ignora o PTT do fio: o PTT é o do quadro. O rádio segura a tecla no lugar do teclado (o longo de 1 s e a repetição
  das setas saem dele) e, sem quadro por 1 s, solta a tecla e o PTT e para o espelho sozinho.

  A cada quadro ele responde, em 9 bytes, '~' 'r' rem tx rx sinal lado tensão soma (TEstadoRadio), e, com o espelho
  ligado, manda cada pedaço de tela que vai ao LCD: 01, 'T', x0, x1, y0, y1, as corridas (quantos e a cor, byte alto
  primeiro) e o CRC16 do 'T' em diante, o mesmo formato do UV-17L. O espelho só vai pelo cabo: pelo Bluetooth chegam o
  estado e as teclas. A saída do rádio guarda 2400 B e não anda enquanto a tela se desenha (v158; medido na v158, um
  redesenho leva de 28 a 34 linhas da tela principal): uma mudança grande não cabe, e o rádio marca a perda, que vai
  na resposta seguinte e se limpa nela (v159). A tela inteira vem então por fatias, cada uma um nó de uma árvore das
  128 linhas como num heap (o 1 é a tela toda, o n tem os filhos 2n e 2n + 1), do tamanho que a imagem que já temos
  diz que cabe (sem imagem, as de 16 linhas, 8 a 15), começando pela que tem a linha seguinte à última que chegou:
  o rádio desenha de cima para baixo, e a perda está ali. A fatia que não coube é partida ao meio, até as de 4 linhas
  (32 a 63), que cabem sempre e se pedem de novo. Cada fatia vai num quadro só (duas cópias podiam ser atendidas duas
  vezes) e espera o estado, que sai depois das linhas dela; até ele chegar nada mais vai, e a tecla e o PTT seguem no
  quadro da fatia. A tela inteira se pede no começo e na hora de uma perda (de novo, do começo, se ela vier no meio)
  ou de bytes estragados. }

interface

uses
  System.SysUtils, Transporte;

const
  TELA_LARGURA = 160;
  TELA_ALTURA = 128;
  ESTADO_MS = 250;          // de quanto em quanto vai o quadro (e volta o estado)
  NO_MENOR = 32;            // os nós de 4 linhas (32 a 63): não se partem (v158)
  FATIA_BYTES = 2400 - 488 - 100;   // a fatia cabe com as linhas, menos a última, até aqui: o rádio só põe a linha
                            // com lugar para o pior caso (160 corridas, 488 B), e uns 100 B de folga para o que ele
                            // desenhar sozinho
  FATIA_DE_NOVO_MS = 250;   // o nó de 4 linhas perdido, ou a fatia na TX, se pede de novo depois disso
  FATIA_ESPERA_MS = 500;    // sem estado depois da fatia, ela se pede de novo
  TECLA_MIN_MS = 150;       // o clique mais curto que vai ao rádio: a leitura dele quer a tecla em 3 voltas de 10 ms,
                            // e cada fatia o deixa uns 65 ms sem ler (o redesenho da tela inteira)
  TECLA_PAUSA_MS = 200;     // depois de soltar a tecla ou o PTT, as fatias esperam isto (o rádio trata a solta)
  // os códigos crus da varredura do teclado (a tabela 0x01E60357 do firmware)
  TECLA_PF2 = 0;
  TECLA_PF3 = 1;
  TECLA_OK = 2;
  TECLA_CIMA = 3;
  TECLA_BAIXO = 4;
  TECLA_EXIT = 5;
  TECLA_ESTRELA = 9;
  TECLA_ZERO = 13;
  TECLA_CERQUILHA = 17;
  TECLA_NENHUMA = 255;
  // o código de cada algarismo, de 0 a 9
  TECLA_DIGITO: array[0..9] of Byte = (13, 6, 7, 8, 10, 11, 12, 14, 15, 16);
  // os bits do quadro do PC
  BIT_PTT = 1;
  BIT_ESPELHO = 2;
  BIT_TELA_INTEIRA = 4;

type
  TEstadoRadio = record
    Valido: Boolean;
    Tecla: Integer;            // a tecla que o rádio segura pelo remoto, -1 nenhuma
    Ptt, Espelho, Perdeu: Boolean;
    TX: Integer;               // 0 parado, 1 transmitindo, 2 recusada
    RX: Integer;               // 0 e 1 sintoniza, 2 espera, 3 vigia, 4 recebendo, 5 MO
    Sinal: Integer;            // os segmentos do medidor, 0 a 24 (2 por S até o S9, depois 1 por 10 dB); -1 nenhum
    Microfone: Boolean;        // transmitindo, o Sinal é o nível do microfone
    Letra: Char;               // quem o medidor mede: 'A', 'B' ou 'C' (LetraDe)
    Vista: Integer;            // o WATCH: 0 DUAL, 1 SINGLE, 2 TRIPLE, 3 TRIPLE com a C no lado B
    EvidenciaB: Boolean;       // o lado B em evidência
    Tensao: Integer;           // décimos de volt
    Sujo: Boolean;             // o espelho perdeu linha desde a resposta anterior (v159)
  end;

  // O que o rádio manda, juntado e separado em estados e quadros da tela.
  TLeitorRemoto = class
  private
    FBuf: TBytes;
    function Quadro(I: Integer; out Fim: Integer): Integer;
  public
    Tela: TBytes;          // 160 x 128 em RGB565, o byte baixo primeiro (como o logo)
    TelaMudou: Boolean;
    Estragados: Integer;   // quadros da tela que não conferiram
    Estado: TEstadoRadio;  // o último que chegou
    Estados: Integer;      // quantos chegaram
    Sujos: Integer;        // quantos chegaram com a perda (cada perda vem numa resposta só)
    Linhas: Integer;       // quantas linhas da tela chegaram
    UltimaLinha: Integer;  // a última linha da tela que chegou, -1 nenhuma
    LinhaPerdida: Integer; // a seguinte à última que chegou antes da última resposta com a perda: o rádio desenha de
                           // cima para baixo, e numa tecla a perda começa ali
    constructor Create;
    procedure Recebe(const B: TBytes);
  end;

  TRemoto = class
  private
    FSegura: Integer;     // a tecla apertada na janela, -1 nenhuma
    FUltima: Integer;     // a do último aperto
    FApertos: Integer;    // quantos apertos: um clique mais curto que uma volta do laço não se perde
    FPtt: Boolean;
  public
    Parar: Boolean;
    OnRegistro: TProc<string>;
    OnEstado: TProc<TEstadoRadio>;
    OnTela: TProc<TBytes>;
    constructor Create;
    // Até o Parar; o rádio que não responde ou para de responder dá ESemRadio ou EFalha. Com Espelho (só pelo cabo),
    // a tela do rádio vem junto.
    procedure Executa(const T: ITransporte; Espelho: Boolean);
    // Da thread da janela: a tecla segura até o Solta, e o PTT apertado até o Ptt(False).
    procedure Aperta(Codigo: Byte);
    procedure Solta;
    procedure Ptt(Liga: Boolean);
  end;

function QuadroRemoto(Tecla, Bits: Byte; Fatia: Byte = $FF): TBytes;
function EstadoDe(const B: TBytes; I: Integer; out E: TEstadoRadio): Boolean;
function LetraDe(Vista: Integer; EvidenciaB, DuplaNoOutro, Rodizio: Boolean): Char;
function TextoSinal(Sinal: Integer): string;
function Crc16(const B: TBytes; Inicio, N: Integer): Word;

implementation

uses
  System.Math, System.SyncObjs, Idioma;

// O quinto byte é a fatia da tela inteira (com o BIT_TELA_INTEIRA, o nó da árvore das linhas); fora dela, e no sexto,
// 0xFF, que o firmware não lê: o PTT do BJ é o fio do RX da serial, e cada bit 0 o aperta.
function QuadroRemoto(Tecla, Bits: Byte; Fatia: Byte): TBytes;
var
  K: Integer;
begin
  Result := TBytes.Create(Ord('~'), Ord('R'), Tecla, Bits, Fatia, $FF, 0);
  for K := 0 to 5 do
    Result[6] := Byte(Result[6] + Result[K]);
end;

// O estado de 9 bytes que começa em I, se a soma confere e os valores cabem no que o firmware manda.
function EstadoDe(const B: TBytes; I: Integer; out E: TEstadoRadio): Boolean;
var
  K: Integer;
  S: Byte;
begin
  E := Default(TEstadoRadio);
  if (I + 9 > Length(B)) or (B[I] <> Ord('~')) or (B[I + 1] <> Ord('r')) then
    Exit(False);
  S := 0;
  for K := 0 to 7 do
    S := Byte(S + B[I + K]);
  // os que só podem ser poucos valores: um embaralhado que passou pela soma ainda cai aqui
  if (S <> B[I + 8]) or (B[I + 2] and 31 > 18) or (B[I + 3] > 2) or (B[I + 4] > 5) or (B[I + 6] > 63) or
     (B[I + 6] and 24 = 16) or ((B[I + 5] and $7F > 24) and (B[I + 5] <> 255)) then
    Exit(False);
  E.Tecla := B[I + 2] and 31 - 1;
  E.Ptt := B[I + 2] and 32 <> 0;
  E.Espelho := B[I + 2] and 64 <> 0;
  E.Perdeu := B[I + 2] and 128 <> 0;
  E.TX := B[I + 3];
  E.RX := B[I + 4];
  if B[I + 5] = 255 then
    E.Sinal := -1
  else
  begin
    E.Sinal := B[I + 5] and $7F;
    E.Microfone := B[I + 5] and $80 <> 0;
  end;
  // o lado em bits crus (v159): 1:0 a vista, 2 a evidência no B, 3 a dupla escuta parada no outro, 4 o rodízio com a
  // terceira no lugar dele (só com a dupla escuta no outro), 5 a perda
  E.Vista := B[I + 6] and 3;
  E.EvidenciaB := B[I + 6] and 4 <> 0;
  E.Letra := LetraDe(E.Vista, E.EvidenciaB, B[I + 6] and 8 <> 0, B[I + 6] and 16 <> 0);
  E.Sujo := B[I + 6] and 32 <> 0;
  E.Tensao := B[I + 7] * 444 div 1000;
  E.Valido := True;
  Result := True;
end;

// Quem o medidor mede, como a tela B do rádio (o lado do medidor de fábrica, 0x01E452D0): com o rodízio, a terceira
// (a C, ou o B na vista 3, que põe a C no lado B); senão o lado em evidência, ou o outro com a dupla escuta parada nele:
// o A, ou o B (a C na vista 3). Até a v158 o firmware mandava a letra pronta.
function LetraDe(Vista: Integer; EvidenciaB, DuplaNoOutro, Rodizio: Boolean): Char;
begin
  if Rodizio then
  begin
    if Vista = 3 then
      Result := 'B'
    else
      Result := 'C';
  end
  else if EvidenciaB = DuplaNoOutro then
    Result := 'A'
  else if Vista = 3 then
    Result := 'C'
  else
    Result := 'B';
end;

// Os segmentos do medidor em S: 2 por S do S1 ao S9, e 1 por 10 dB acima.
function TextoSinal(Sinal: Integer): string;
begin
  if Sinal <= 0 then
    Result := 'S0'
  else if Sinal <= 18 then
    Result := 'S' + IntToStr((Sinal + 1) div 2)
  else
    Result := Format('S9+%d', [(Sinal - 18) * 10]);
end;

function Crc16(const B: TBytes; Inicio, N: Integer): Word;
var
  I, K: Integer;
begin
  Result := 0;
  for I := Inicio to Inicio + N - 1 do
  begin
    Result := Result xor (Word(B[I]) shl 8);
    for K := 1 to 8 do
      if Result and $8000 <> 0 then
        Result := Word(Result shl 1) xor $1021
      else
        Result := Word(Result shl 1);
  end;
end;

{ TLeitorRemoto }

const
  Q_OK = 0;
  Q_FALTA = 1;    // ainda não chegou inteiro
  Q_ERRADO = 2;

constructor TLeitorRemoto.Create;
begin
  SetLength(Tela, TELA_LARGURA * TELA_ALTURA * 2);
  UltimaLinha := -1;
end;

// O quadro da tela que começa no 01 em I: conferido, posto na Tela.
function TLeitorRemoto.Quadro(I: Integer; out Fim: Integer): Integer;
var
  X0, X1, Y0, Y1, N, P, Q, X, Y, K: Integer;
begin
  if Length(FBuf) - I < 6 then
    Exit(Q_FALTA);
  X0 := FBuf[I + 2];
  X1 := FBuf[I + 3];
  Y0 := FBuf[I + 4];
  Y1 := FBuf[I + 5];
  if (FBuf[I + 1] <> Ord('T')) or (X0 >= X1) or (X1 > TELA_LARGURA) or (Y0 >= Y1) or (Y1 > TELA_ALTURA) then
    Exit(Q_ERRADO);
  // as corridas têm de somar os pixels do retângulo, sem passar
  N := (X1 - X0) * (Y1 - Y0);
  P := I + 6;
  while N > 0 do
  begin
    if P + 3 > Length(FBuf) then
      Exit(Q_FALTA);
    if (FBuf[P] = 0) or (FBuf[P] > N) then
      Exit(Q_ERRADO);
    Dec(N, FBuf[P]);
    Inc(P, 3);
  end;
  if P + 2 > Length(FBuf) then
    Exit(Q_FALTA);
  if Crc16(FBuf, I + 1, P - I - 1) <> FBuf[P] shl 8 or FBuf[P + 1] then
    Exit(Q_ERRADO);
  X := X0;
  Y := Y0;
  Q := I + 6;
  while Q < P do
  begin
    for K := 1 to FBuf[Q] do
    begin
      Tela[(Y * TELA_LARGURA + X) * 2] := FBuf[Q + 2];
      Tela[(Y * TELA_LARGURA + X) * 2 + 1] := FBuf[Q + 1];
      Inc(X);
      if X = X1 then
      begin
        X := X0;
        Inc(Y);
      end;
    end;
    Inc(Q, 3);
  end;
  TelaMudou := True;
  UltimaLinha := Y1 - 1;
  Inc(Linhas);
  Fim := P + 2;
  Result := Q_OK;
end;

procedure TLeitorRemoto.Recebe(const B: TBytes);
var
  I, Fim: Integer;
  E: TEstadoRadio;
begin
  FBuf := FBuf + B;
  I := 0;
  while I < Length(FBuf) do
    if FBuf[I] = 1 then
      case Quadro(I, Fim) of
        Q_OK:
          I := Fim;
        Q_FALTA:
          Break;
      else
        // um 01 do ruído, ou um quadro estragado: o que vem depois dele se lê de novo
        Inc(Estragados);
        Inc(I);
      end
    else if FBuf[I] = Ord('~') then
    begin
      if I + 9 > Length(FBuf) then
        Break;
      if EstadoDe(FBuf, I, E) then
      begin
        Estado := E;
        Inc(Estados);
        if E.Sujo then
        begin
          Inc(Sujos);
          LinhaPerdida := (UltimaLinha + 1) mod TELA_ALTURA;
        end;
        Inc(I, 9);
      end
      else
        Inc(I);
    end
    else
      Inc(I);
  // o que pode ser o começo de algo fica; um buffer que só cresce (nada conferiu) se corta
  Delete(FBuf, 0, I);
  if Length(FBuf) > 4096 then
    FBuf := nil;
end;

{ TRemoto }

constructor TRemoto.Create;
begin
  FSegura := -1;
end;

procedure TRemoto.Aperta(Codigo: Byte);
begin
  FUltima := Codigo;
  FSegura := Codigo;
  TInterlocked.Increment(FApertos);
end;

procedure TRemoto.Solta;
begin
  FSegura := -1;
end;

procedure TRemoto.Ptt(Liga: Boolean);
begin
  FPtt := Liga;
end;

procedure TRemoto.Executa(const T: ITransporte; Espelho: Boolean);
var
  Leitor: TLeitorRemoto;
  Agora, Inicio, Chegou, Proximo, UltimaTela, FatiaEm: UInt64;
  Vistos, Sujos, Estados, Tecla, Enviado, EstadosDaFatia, LinhasDaFatia, No, Apertos, Ativa: Integer;
  TeclaDesde, SoltouEm: UInt64;
  Bits: Byte;
  Fila: TBytes;   // os nós que faltam, o próximo no fim
  Esperando, Ignora, Pediu, TemTela, Livre: Boolean;
  B: TBytes;

  // As linhas de um nó: Linhas a partir de Primeira.
  procedure Faixa(No: Integer; out Primeira, Linhas: Integer);
  var
    D: Integer;
  begin
    Linhas := TELA_ALTURA;
    D := No;
    while D > 1 do
    begin
      D := D shr 1;
      Linhas := Linhas shr 1;
    end;
    Primeira := (No - TELA_ALTURA div Linhas) * Linhas;
  end;

  // O nó cabe na saída do rádio, pela imagem que já temos: os bytes das linhas dele, menos a última.
  function Cabe(No: Integer): Boolean;
  var
    Primeira, Linhas, Y, I, Soma: Integer;
  begin
    Faixa(No, Primeira, Linhas);
    Soma := 0;
    for Y := Primeira to Primeira + Linhas - 2 do
    begin
      Inc(Soma, 8 + 3);   // o cabeçalho, o CRC e a primeira corrida
      for I := Y * TELA_LARGURA + 1 to (Y + 1) * TELA_LARGURA - 1 do
        if PWord(@Leitor.Tela[I * 2])^ <> PWord(@Leitor.Tela[I * 2 - 2])^ then
          Inc(Soma, 3);
    end;
    Result := Soma <= FATIA_BYTES;
  end;

  // Os nós da tela toda, os maiores que cabem (sem imagem, os de 16 linhas), de cima para baixo.
  procedure Cobre(No: Integer; var Lista: TBytes);
  begin
    if (No >= NO_MENOR) or (TemTela and Cabe(No)) or (not TemTela and (No >= 8)) then
      Lista := Lista + [No]
    else
    begin
      Cobre(2 * No, Lista);
      Cobre(2 * No + 1, Lista);
    end;
  end;

  // A fila da tela inteira, começando pelo nó da linha Linha e dando a volta (o próximo no fim).
  function MontaFila(Linha: Integer): TBytes;
  var
    Lista: TBytes;
    K, N, I, Primeira, Linhas: Integer;
  begin
    Lista := nil;
    Cobre(1, Lista);
    N := Length(Lista);
    K := 0;
    repeat
      Faixa(Lista[K], Primeira, Linhas);
      if Linha < Primeira + Linhas then
        Break;
      Inc(K);
    until K = N - 1;
    SetLength(Result, N);
    for I := 0 to N - 1 do
      Result[N - 1 - I] := Lista[(K + I) mod N];
  end;

begin
  Leitor := TLeitorRemoto.Create;
  try
    T.LimpaEntrada;
    Inicio := T.Agora;
    Chegou := Inicio;
    Proximo := Inicio;
    UltimaTela := 0;
    FatiaEm := Inicio;
    Pediu := False;
    TemTela := False;
    Esperando := False;
    Ignora := False;
    Fila := nil;
    EstadosDaFatia := 0;
    Vistos := 0;
    Sujos := 0;
    Estados := 0;
    Enviado := -1;
    LinhasDaFatia := 0;
    Apertos := FApertos;
    Ativa := -1;
    TeclaDesde := 0;
    SoltouEm := 0;
    while not Parar do
    begin
      Agora := T.Agora;
      // a tecla: cada aperto vai ao rádio por pelo menos TECLA_MIN_MS, mesmo num clique mais curto
      if FApertos <> Apertos then
      begin
        Apertos := FApertos;
        Ativa := FUltima;
        TeclaDesde := Agora;
      end
      else if (Ativa >= 0) and (FSegura <> Ativa) and (Agora >= TeclaDesde + TECLA_MIN_MS) then
      begin
        Ativa := -1;
        SoltouEm := Agora;
      end;
      Tecla := IfThen(Ativa < 0, TECLA_NENHUMA, Ativa);
      Bits := IfThen(FPtt, BIT_PTT, 0) or IfThen(Espelho, BIT_ESPELHO, 0);
      // a tela inteira: no começo e na hora de uma perda (no rádio ou aqui); no meio de uma passada, ela recomeça
      if Espelho and (not Pediu or (Leitor.Sujos > Sujos) or (Leitor.Estragados > Vistos)) then
      begin
        if Leitor.Sujos > Sujos then
          Fila := MontaFila(Leitor.LinhaPerdida)
        else
          Fila := MontaFila((Leitor.UltimaLinha + 1) mod TELA_ALTURA);
        Ignora := Esperando;   // o estado da fatia que está no ar não vale para a fila nova
        Pediu := True;
        Sujos := Leitor.Sujos;
        Vistos := Leitor.Estragados;
        FatiaEm := Agora;
      end;
      // com a tecla ou o PTT apertados, e até TECLA_PAUSA_MS depois de soltar, nenhuma fatia: o redesenho da tela
      // inteira deixa o rádio uns 65 ms sem ler o teclado, e o clique se perdia
      if (Ativa >= 0) or FPtt then
        SoltouEm := Agora;
      Livre := Agora >= SoltouEm + TECLA_PAUSA_MS;
      if (Fila <> nil) and Livre then
      begin
        // a fatia: um quadro só, e nada mais até o estado dela (ou o tempo dele)
        if not Esperando and (Agora >= FatiaEm) then
        begin
          T.Escreve(QuadroRemoto(Tecla, Bits or BIT_TELA_INTEIRA, Fila[High(Fila)]));
          Esperando := True;
          EstadosDaFatia := Leitor.Estados;
          LinhasDaFatia := Leitor.Linhas;
          FatiaEm := Agora + FATIA_ESPERA_MS;
          Enviado := Tecla or Bits shl 8;
          Proximo := Agora + ESTADO_MS;
        end;
      end
      else if not Esperando and ((Tecla or Bits shl 8 <> Enviado) or (Agora >= Proximo)) then
      begin
        // duas vezes: um resto de quadro perdido no buffer do rádio não estraga o seguinte (o rádio usa o mais novo)
        T.Escreve(QuadroRemoto(Tecla, Bits) + QuadroRemoto(Tecla, Bits));
        Enviado := Tecla or Bits shl 8;
        Proximo := Agora + ESTADO_MS;
      end;
      B := T.Le;
      if B <> nil then
      begin
        Chegou := Agora;
        Leitor.Recebe(B);
      end;
      if Esperando and (Leitor.Estados > EstadosDaFatia) then
      begin
        // o estado da fatia: sem perda, a seguinte; perdida, partida ao meio, ou de novo daqui a pouco se é das
        // menores ou se nem se redesenhou (nenhuma linha: o rádio na TX, ou no meio de uma digitação, v161)
        Esperando := False;
        FatiaEm := Agora;
        if Ignora then
          Ignora := False
        else
        begin
          No := Fila[High(Fila)];
          if not Leitor.Estado.Perdeu then
            SetLength(Fila, High(Fila))
          else if (No >= NO_MENOR) or (Leitor.Estado.TX <> 0) or (Leitor.Linhas = LinhasDaFatia) then
            FatiaEm := Agora + FATIA_DE_NOVO_MS
          else
          begin
            Fila[High(Fila)] := 2 * No + 1;
            Fila := Fila + [2 * No];
          end;
        end;
      end
      else if Esperando and (Agora >= FatiaEm) then
        Esperando := False;   // o estado não veio: a mesma fatia de novo
      if Leitor.Estados > Estados then
      begin
        if (Estados = 0) and Assigned(OnRegistro) then
          OnRegistro(Tr('Controle remoto conectado.', 'Remote control connected.'));
        Estados := Leitor.Estados;
        if Assigned(OnEstado) then
          OnEstado(Leitor.Estado);
      end;
      if Leitor.TelaMudou and (not TemTela or (Agora >= UltimaTela + 100)) then
      begin
        if Assigned(OnTela) then
          OnTela(Copy(Leitor.Tela));
        Leitor.TelaMudou := False;
        TemTela := True;
        UltimaTela := Agora;
      end;
      // sem resposta a 460800, o firmware v157 a v159 (115200): a fatia que foi vira ruído, e a passada recomeça
      if (Estados = 0) and (Agora > Inicio + 700) and (T.Velocidade = BAUD_CABO) then
      begin
        T.Velocidade(BAUD_CABO_V157);
        T.LimpaEntrada;
        Inicio := Agora;
        Pediu := False;
        Esperando := False;
        Ignora := False;
        Fila := nil;
      end;
      if (Estados = 0) and (Agora > Inicio + 3000) then
        raise ESemRadio.Create(Tr(
          'O rádio não respondeu. O controle remoto precisa do firmware v159 ou mais novo, com o rádio ligado ' +
          'normalmente (fora do modo de gravação).',
          'The radio did not answer. Remote control needs firmware v159 or newer, with the radio on normally (not in ' +
          'programming mode).'));
      if (Estados > 0) and (Agora > Chegou + 10000) then
        raise EFalha.Create(Tr('Nada chegou do rádio em 10 s. Confira o cabo (ou o Bluetooth) e se o rádio está ligado.',
                               'Nothing came from the radio in 10 s. Check the cable (or Bluetooth) and whether the ' +
                               'radio is on.'));
    end;
    // a tecla e o PTT soltos e o espelho desligado, com tempo para os bytes saírem antes de a porta fechar
    T.Escreve(QuadroRemoto(TECLA_NENHUMA, 0) + QuadroRemoto(TECLA_NENHUMA, 0));
    T.Dorme(50);
  finally
    Leitor.Free;
  end;
end;

end.
