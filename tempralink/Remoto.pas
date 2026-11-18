unit Remoto;

{ O controle remoto do BJ-8300 pelo cabo ou pelo Bluetooth, com o firmware v159 ou mais novo (firmware/src/remoto.c),
  o rádio funcionando normalmente; a tela, com o v163 ou mais novo. Sem nada de janela: serve ao Tempra Link do Windows
  e ao do Android.

  O PC manda, a cada ESTADO_MS (com a tela pedida, TELA_MS) e na hora em que algo muda, sempre duas vezes seguidas, o
  quadro de 7 bytes
    '~' 'R' tecla bits 255 255 soma
  com a tecla segura (o código cru da varredura, abaixo, ou 255) e os bits 1 o PTT, 2 a tela e 4 a tela do rádio
  apagada (v164); os dois do meio vão em
  255 (menos bits 0 no fio, que é também o do PTT), e a soma é a dos 6 bytes de antes. Com o remoto ligado pelo cabo,
  o firmware v157 ignora o PTT do fio: o PTT é o do quadro. O rádio segura a tecla no lugar do teclado (o longo de 1 s
  e a repetição das setas saem dele) e, sem quadro por 1 s, solta a tecla e o PTT sozinho.

  A cada quadro ele responde, em 9 bytes, '~' 'r' rem tx rx sinal lado tensão soma (TEstadoRadio) e, com a tela pedida,
  logo depois o retrato dela (v162; na v163 com o lugar do menu): '~' 'S', a versão, o tamanho, o que a tela do rádio lê
  da RAM e da flash, e as somas de Fletcher (o formato no alto do firmware/src/remoto.c). A tela se desenha aqui, com o
  mesmo código do rádio (TelaBJ), e a janela só a recebe quando ela muda. Até a v161 vinha o espelho do LCD, linha a
  linha, que não cabia na saída do rádio e saiu. Enquanto o laço do APRS ou do Morse roda, o rádio responde e não manda
  o retrato (o laço do sinal, que o manda, fica parado).

  O ExecutaReligando se reconecta sozinho: o RF do PTT pode derrubar o USB do cabo, que volta como outro aparelho (às
  vezes com outro nome), e a porta aberta antes morre. Depois de falar com o rádio uma vez, a queda fecha o transporte e
  o abre de novo, até o Parar. }

interface

uses
  System.SysUtils, Transporte;

const
  TELA_LARGURA = 160;
  TELA_ALTURA = 128;
  ESTADO_MS = 250;          // de quanto em quanto vai o quadro (e volta o estado)
  TELA_MS = 150;            // com a tela pedida: um retrato a cada quadro, uns 500 B (11 ms a 460800)
  SEM_RETRATO_MS = 1500;    // a tela pedida, as respostas chegando e o retrato não: o laço do APRS ou do Morse, ou um
                            // firmware antes da v162
  TECLA_MIN_MS = 150;       // o clique mais curto que vai ao rádio: a leitura dele quer a tecla em 3 voltas de 10 ms
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
  BIT_TELA = 2;
  BIT_APAGA = 4;            // v164: a tela do rádio apagada (uma tecla no rádio a acende pelo tempo da luz)
  // o código do OnTela sem imagem: o retrato não veio (TelaBJ tem os outros)
  TELA_SEM_RETRATO = 100;
  QUEDA_MS = 4000;          // conectado, nada do rádio por esse tempo: a ligação caiu (o roger mais longo tem 3 s)

type
  TEstadoRadio = record
    Valido: Boolean;
    Tecla: Integer;            // a tecla que o rádio segura pelo remoto, -1 nenhuma
    Ptt, Tela: Boolean;        // o que o rádio tem do último quadro
    TX: Integer;               // 0 parado, 1 transmitindo, 2 recusada
    RX: Integer;               // 0 e 1 sintoniza, 2 espera, 3 vigia, 4 recebendo, 5 MO
    Sinal: Integer;            // os segmentos do medidor, 0 a 24 (2 por S até o S9, depois 1 por 10 dB); -1 nenhum
    Microfone: Boolean;        // transmitindo, o Sinal é o nível do microfone
    SinalCru: Byte;            // o byte do sinal como veio (o desenho da tela o usa)
    Letra: Char;               // quem o medidor mede: 'A', 'B' ou 'C' (LetraDe)
    Vista: Integer;            // o WATCH: 0 DUAL, 1 SINGLE, 2 TRIPLE, 3 TRIPLE com a C no lado B
    EvidenciaB: Boolean;       // o lado B em evidência
    Tensao: Integer;           // décimos de volt
    Acordou: Boolean;          // v165: uma tecla do rádio acendeu a tela que o PC pediu apagada
  end;

  // O que o rádio manda, juntado e separado em estados e retratos da tela.
  TLeitorRemoto = class
  private
    FBuf: TBytes;
  public
    Estado: TEstadoRadio;  // o último que chegou
    Estados: Integer;      // quantos chegaram
    Retrato: TBytes;       // o último que chegou inteiro e com as somas certas
    Retratos: Integer;     // quantos chegaram
    Estragados: Integer;   // retratos com as somas erradas
    procedure Recebe(const B: TBytes);
  end;

  // A tela que mudou (160 x 128 em RGB565, o byte baixo primeiro), e o código do desenho (TelaBJ: TELA_BJ_INTEIRA,
  // PARCIAL, FORA ou JANELA); sem imagem (nil), TELA_SEM_RETRATO ou um erro do desenho (negativo). Só vem quando a
  // imagem ou o código mudam: o retrato chega a cada TELA_MS, quase sempre igual ao de antes.
  TProcTela = reference to procedure(Imagem: TBytes; Codigo: Integer);

  TRemoto = class
  private
    FSegura: Integer;     // a tecla apertada na janela, -1 nenhuma
    FUltima: Integer;     // a do último aperto
    FApertos: Integer;    // quantos apertos: um clique mais curto que uma volta do laço não se perde
    FPtt: Boolean;
    FApaga: Boolean;
    FArmado: Boolean;     // o pedido de apagar já chegou ao rádio: o aviso de que acordou vale
    FFalou: Boolean;      // o ExecutaReligando já falou com o rádio: dali em diante, queda é reconexão
    FCaiu: Boolean;       // caiu e ainda não voltou
  public
    Parar: Boolean;
    PausaReligaMs: Cardinal;   // entre as tentativas de reconectar (os testes o zeram)
    OnRegistro: TProc<string>;
    OnEstado: TProc<TEstadoRadio>;
    OnTela: TProcTela;
    OnQueda: TProc;            // a ligação caiu, e ele tenta de novo
    constructor Create;
    // Até o Parar; o rádio que não responde ou para de responder dá ESemRadio ou EFalha. Com Tela, o retrato vem a cada
    // quadro e se desenha, e o OnTela ouve o que mudou.
    procedure Executa(const T: ITransporte; Tela: Boolean);
    // O Executa sobre o transporte que Abre dá, e de novo a cada queda, até o Parar; só a primeira conexão dá erro.
    procedure ExecutaReligando(const Abre: TFunc<ITransporte>; Tela: Boolean);
    // Da thread da janela: a tecla segura até o Solta, e o PTT apertado até o Ptt(False).
    procedure Aperta(Codigo: Byte);
    procedure Solta;
    procedure Ptt(Liga: Boolean);
    // A tela do rádio apagada enquanto o remoto estiver ligado (firmware v164; o de antes ignora o bit).
    procedure Apaga(Liga: Boolean);
    // O pedido de agora: acaba sozinho quando uma tecla do rádio acende a tela (o Acordou, v165).
    property Apagando: Boolean read FApaga;
  end;

function QuadroRemoto(Tecla, Bits: Byte): TBytes;
function EstadoDe(const B: TBytes; I: Integer; out E: TEstadoRadio): Boolean;
function RetratoDe(const B: TBytes; I: Integer; out Fim: Integer): Integer;
function LetraDe(Vista: Integer; EvidenciaB, DuplaNoOutro, Rodizio: Boolean): Char;
function TextoSinal(Sinal: Integer): string;
// O aviso por cima da tela, pelo código do OnTela: o que dela não vem do rádio, ou por que ela parou ('' nada).
function AvisoDaTela(Codigo: Integer): string;

const
  // o RetratoDe
  R_OK = 0;
  R_FALTA = 1;    // ainda não chegou inteiro
  R_ERRADO = 2;

implementation

uses
  System.Classes, System.Math, System.SyncObjs, Idioma, TelaBJ;

// Os dois do meio em 0xFF, que o firmware não lê: o PTT do BJ é o fio do RX da serial, e cada bit 0 o aperta.
function QuadroRemoto(Tecla, Bits: Byte): TBytes;
var
  K: Integer;
begin
  Result := TBytes.Create(Ord('~'), Ord('R'), Tecla, Bits, $FF, $FF, 0);
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
  // os que só podem ser poucos valores: um embaralhado que passou pela soma ainda cai aqui (o bit 7 do rem e o 5 do
  // lado vinham da perda do espelho, até a v161; desde a v165 o bit 7 do rem é o Acordou)
  if (S <> B[I + 8]) or (B[I + 2] and 31 > 18) or (B[I + 3] > 2) or (B[I + 4] > 5) or (B[I + 6] > 63) or
     (B[I + 6] and 24 = 16) or ((B[I + 5] and $7F > 24) and (B[I + 5] <> 255)) then
    Exit(False);
  E.Tecla := B[I + 2] and 31 - 1;
  E.Ptt := B[I + 2] and 32 <> 0;
  E.Tela := B[I + 2] and 64 <> 0;
  E.Acordou := B[I + 2] and 128 <> 0;
  E.TX := B[I + 3];
  E.RX := B[I + 4];
  E.SinalCru := B[I + 5];
  if B[I + 5] = 255 then
    E.Sinal := -1
  else
  begin
    E.Sinal := B[I + 5] and $7F;
    E.Microfone := B[I + 5] and $80 <> 0;
  end;
  // o lado em bits crus (v159): 1:0 a vista, 2 a evidência no B, 3 a dupla escuta parada no outro, 4 o rodízio com a
  // terceira no lugar dele (só com a dupla escuta no outro)
  E.Vista := B[I + 6] and 3;
  E.EvidenciaB := B[I + 6] and 4 <> 0;
  E.Letra := LetraDe(E.Vista, E.EvidenciaB, B[I + 6] and 8 <> 0, B[I + 6] and 16 <> 0);
  E.Tensao := B[I + 7] * 444 div 1000;
  E.Valido := True;
  Result := True;
end;

// O retrato que começa em I ('~' 'S', a versão e o tamanho do que vem, o byte baixo primeiro, e as duas somas de
// Fletcher do '~' ao fim): R_OK com o Fim depois dele, R_FALTA ou R_ERRADO.
function RetratoDe(const B: TBytes; I: Integer; out Fim: Integer): Integer;
var
  N, K: Integer;
  S1, S2: Byte;
begin
  if I + 5 > Length(B) then
    Exit(R_FALTA);
  if (B[I] <> Ord('~')) or (B[I + 1] <> Ord('S')) then
    Exit(R_ERRADO);
  N := B[I + 3] or B[I + 4] shl 8;
  if (N < 32) or (N > 2048) then
    Exit(R_ERRADO);
  Fim := I + 5 + N + 2;
  if Fim > Length(B) then
    Exit(R_FALTA);
  S1 := 0;
  S2 := 0;
  for K := I to Fim - 3 do
  begin
    S1 := Byte(S1 + B[K]);
    S2 := Byte(S2 + S1);
  end;
  if (S1 <> B[Fim - 2]) or (S2 <> B[Fim - 1]) then
    Exit(R_ERRADO);
  Result := R_OK;
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

function AvisoDaTela(Codigo: Integer): string;
begin
  case Codigo of
    TELA_BJ_PARCIAL:
      Result := Tr('Parte desta tela só aparece no rádio (o espectro, ou o APRS e o Morse por baixo da caixa).',
                   'Part of this screen only shows on the radio (the spectrum, or APRS and Morse under the box).');
    TELA_BJ_FORA:
      Result := Tr('A tela do APRS e do Morse só aparece no rádio.', 'The APRS and Morse screen only shows on the radio.');
    TELA_SEM_RETRATO:
      Result := Tr('A tela parou de vir: o rádio está no APRS ou no Morse, ou o firmware é anterior à v162.',
                   'The screen stopped coming: the radio is in APRS or Morse, or the firmware is older than v162.');
    TELA_BJ_ERRO_VERSAO, TELA_BJ_ERRO_TAMANHO:
      Result := Tr('Este Tempra Link não conhece a tela deste firmware: use os dois da mesma versão.',
                   'This Tempra Link does not know this firmware''s screen: use both from the same version.');
  else
    Result := '';
  end;
end;

{ TLeitorRemoto }

procedure TLeitorRemoto.Recebe(const B: TBytes);
var
  I, Fim: Integer;
  E: TEstadoRadio;
begin
  FBuf := FBuf + B;
  I := 0;
  while I < Length(FBuf) do
    if FBuf[I] <> Ord('~') then
      Inc(I)
    else if I + 2 > Length(FBuf) then
      Break
    else if FBuf[I + 1] = Ord('S') then
      case RetratoDe(FBuf, I, Fim) of
        R_OK:
          begin
            Retrato := Copy(FBuf, I, Fim - I);
            Inc(Retratos);
            I := Fim;
          end;
        R_FALTA:
          Break;
      else
        // um '~' 'S' do ruído, ou um retrato estragado: o que vem depois dele se lê de novo
        Inc(Estragados);
        Inc(I);
      end
    else if I + 9 > Length(FBuf) then
      Break
    else if EstadoDe(FBuf, I, E) then
    begin
      Estado := E;
      Inc(Estados);
      Inc(I, 9);
    end
    else
      Inc(I);
  // o que pode ser o começo de algo fica; um buffer que só cresce (nada conferiu) se corta
  Delete(FBuf, 0, I);
  if Length(FBuf) > 8192 then
    FBuf := nil;
end;

{ TRemoto }

constructor TRemoto.Create;
begin
  FSegura := -1;
  PausaReligaMs := 1000;
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

procedure TRemoto.Apaga(Liga: Boolean);
begin
  FArmado := False;
  FApaga := Liga;
end;

procedure TRemoto.ExecutaReligando(const Abre: TFunc<ITransporte>; Tela: Boolean);
var
  T: ITransporte;
  Ate: UInt64;
begin
  FFalou := False;
  FCaiu := False;
  while not Parar do
  begin
    try
      T := Abre();
      Executa(T, Tela);
    except
      on E: Exception do
      begin
        T := nil;                  // fecha a porta: a do USB que caiu não serve mais
        if Parar then
          Exit;
        if not FFalou then
          raise;
        if not FCaiu then
        begin
          FCaiu := True;
          if Assigned(OnRegistro) then
            OnRegistro(Format(Tr('A ligação com o rádio caiu (%s). Reconectando...',
                                 'The link to the radio dropped (%s). Reconnecting...'), [E.Message]));
          if Assigned(OnQueda) then
            OnQueda;
        end;
        Ate := TThread.GetTickCount64 + PausaReligaMs;
        while not Parar and (TThread.GetTickCount64 < Ate) do
          TThread.Sleep(20);
      end;
    end;
    T := nil;
  end;
end;

procedure TRemoto.Executa(const T: ITransporte; Tela: Boolean);
var
  Leitor: TLeitorRemoto;
  Agora, Inicio, Chegou, Proximo, UltimoRetrato, TeclaDesde: UInt64;
  Estados, Retratos, Tecla, Enviado, Apertos, Ativa, Codigo, Mostrado: Integer;
  SemRetrato: Boolean;
  Bits: Byte;
  B, Imagem, Mostrada: TBytes;
begin
  Leitor := TLeitorRemoto.Create;
  try
    T.LimpaEntrada;
    Inicio := T.Agora;
    Chegou := Inicio;
    Proximo := Inicio;
    UltimoRetrato := Inicio;
    SemRetrato := False;
    Estados := 0;
    Retratos := 0;
    Enviado := -1;
    Apertos := FApertos;
    Ativa := -1;
    TeclaDesde := 0;
    Mostrado := TELA_SEM_RETRATO;   // nada mostrado ainda: o primeiro retrato vai
    Mostrada := nil;
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
        Ativa := -1;
      Tecla := IfThen(Ativa < 0, TECLA_NENHUMA, Ativa);
      Bits := IfThen(FPtt, BIT_PTT, 0) or IfThen(Tela, BIT_TELA, 0) or IfThen(FApaga, BIT_APAGA, 0);
      if (Tecla or Bits shl 8 <> Enviado) or (Agora >= Proximo) then
      begin
        // duas vezes: um resto de quadro perdido no buffer do rádio não estraga o seguinte (o rádio usa o mais novo)
        T.Escreve(QuadroRemoto(Tecla, Bits) + QuadroRemoto(Tecla, Bits));
        Enviado := Tecla or Bits shl 8;
        Proximo := Agora + UInt64(IfThen(Tela, TELA_MS, ESTADO_MS));
      end;
      B := T.Le;
      if B <> nil then
      begin
        Chegou := Agora;
        Leitor.Recebe(B);
      end;
      if Leitor.Estados > Estados then
      begin
        if (Estados = 0) and Assigned(OnRegistro) then
          if FCaiu then
            OnRegistro(Tr('Controle remoto reconectado.', 'Remote control reconnected.'))
          else
            OnRegistro(Tr('Controle remoto conectado.', 'Remote control connected.'));
        FFalou := True;
        FCaiu := False;
        Estados := Leitor.Estados;
        // v165: uma tecla do rádio acendeu a tela que se pediu apagada: o pedido acaba, e a janela a mostra acesa. Só
        // depois de uma resposta sem o aviso: uma que já vinha no caminho, de antes do pedido, não conta
        if FApaga then
          if not Leitor.Estado.Acordou then
            FArmado := True
          else if FArmado then
            FApaga := False;
        if Assigned(OnEstado) then
          OnEstado(Leitor.Estado);
      end;
      // o retrato, desenhado com o sinal da resposta que veio antes dele (a do mesmo quadro)
      if Leitor.Retratos > Retratos then
      begin
        Retratos := Leitor.Retratos;
        UltimoRetrato := Agora;
        SemRetrato := False;
        Imagem := nil;
        Codigo := DesenhaRetrato(Leitor.Retrato, Leitor.Estado.SinalCru, Imagem);
        // só o que mudou vai à janela, que assim só repinta quando a tela do rádio muda
        if (Codigo <> Mostrado) or (Length(Imagem) <> Length(Mostrada)) or
           not CompareMem(Pointer(Imagem), Pointer(Mostrada), Length(Imagem)) then
        begin
          Mostrado := Codigo;
          Mostrada := Imagem;
          if Assigned(OnTela) then
            OnTela(Imagem, Codigo);
        end;
      end
      else if Tela and (Estados > 0) and not SemRetrato and (Agora >= UltimoRetrato + SEM_RETRATO_MS) then
      begin
        SemRetrato := True;
        Mostrado := TELA_SEM_RETRATO;
        if Assigned(OnTela) then
          OnTela(nil, TELA_SEM_RETRATO);
      end;
      // sem resposta a 460800, o firmware v157 a v159 (115200)
      if (Estados = 0) and (Agora > Inicio + 700) and (T.Velocidade = BAUD_CABO) then
      begin
        T.Velocidade(BAUD_CABO_V157);
        T.LimpaEntrada;
        Inicio := Agora;
        UltimoRetrato := Agora;
      end;
      if (Estados = 0) and (Agora > Inicio + 3000) then
        raise ESemRadio.Create(Tr(
          'O rádio não respondeu. O controle remoto precisa do firmware v159 ou mais novo (a tela, do v163), com o ' +
          'rádio ligado normalmente (fora do modo de gravação).',
          'The radio did not answer. Remote control needs firmware v159 or newer (the screen, v163), with the radio ' +
          'on normally (not in programming mode).'));
      if (Estados > 0) and (Agora > Chegou + QUEDA_MS) then
        raise EFalha.CreateFmt(Tr('nada chegou do rádio em %d s', 'nothing came from the radio in %d s'),
                               [QUEDA_MS div 1000]);
    end;
    // a tecla e o PTT soltos e a tela desligada, com tempo para os bytes saírem antes de a porta fechar
    T.Escreve(QuadroRemoto(TECLA_NENHUMA, 0) + QuadroRemoto(TECLA_NENHUMA, 0));
    T.Dorme(50);
  finally
    Leitor.Free;
  end;
end;

end.
