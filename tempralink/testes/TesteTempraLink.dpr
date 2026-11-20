program TesteTempraLink;

{ Os testes do Tempra Link do BJ-8300, sem rádio: o menu, a cifra, a leitura pelo cabo contra o rádio simulado e
  contra o que o driver do CHIRP lê da mesma memória (testes/fixtures.py gera), a gravação (que o fixtures.py confere
  depois pelo driver), as recusas (firmware de fábrica, rádio mudo, conflito, firmware que não guarda), o backup, o CSV,
  o logo e o controle remoto (o protocolo, o leitor e o laço, contra o simulador).
  Uso: TesteTempraLink.exe <pasta tempralink> }

{$APPTYPE CONSOLE}

uses
  System.SysUtils, System.Classes, System.IOUtils, System.ZLib, System.Math, System.StrUtils, System.RegularExpressions,
  Idioma, Transporte, CaboBJ, Ajustes, BJ8300, Csv, Logo, LogoFormato, Remoto, TelaBJ, SimuladorBJ, GravadorBJ,
  SimuladorUpdate, Atualizacao, Compatibilidade;

var
  Pasta: string;
  Erros: Integer;
  Menu: TMenu;

function Iguais(const A, B: TBytes): Boolean;
begin
  Result := (Length(A) = Length(B)) and ((Length(A) = 0) or CompareMem(@A[0], @B[0], Length(A)));
end;

procedure Confere(Ok: Boolean; const Oque: string);
begin
  if Ok then
    Writeln('ok     ', Oque)
  else
  begin
    Writeln('FALHOU ', Oque);
    Inc(Erros);
  end;
end;

// As áreas no formato do fixtures.py: endereço e tamanho (4 bytes cada) e os dados, uma atrás da outra.
procedure CarregaAreas(R: TRadioBJSimulado; const Arquivo: string);
var
  B: TBytes;
  P: Integer;
  E, T: Cardinal;
begin
  B := TFile.ReadAllBytes(Arquivo);
  P := 0;
  while P < Length(B) do
  begin
    E := PCardinal(@B[P])^;
    T := PCardinal(@B[P + 4])^;
    R.Poe(E, Copy(B, P + 8, T));
    Inc(P, 8 + Integer(T));
  end;
end;

procedure SalvaAreas(R: TRadioBJSimulado; const Arquivo: string);
var
  S: TBytesStream;
  A: TArea;
begin
  S := TBytesStream.Create;
  try
    for A in AREAS do
    begin
      S.WriteData(A.Endereco);
      S.WriteData(A.Tamanho);
      S.WriteBuffer(R.Pega(A.Endereco, A.Tamanho), A.Tamanho);
    end;
    S.SaveToFile(Arquivo);
  finally
    S.Free;
  end;
end;

function NovoRadio: TRadioBJSimulado;
begin
  Result := TRadioBJSimulado.Create;
  CarregaAreas(Result, TPath.Combine(Pasta, 'testes\radio.bin'));
end;

// lê o rádio para uma memória nova
function Le(R: ITransporte): TMemoriaBJ8300;
var
  C: TCaboBJ;
  M: TMemoriaBJ8300;
begin
  M := TMemoriaBJ8300.Create(Menu);
  C := TCaboBJ.Create;
  try
    C.Sessao(R, procedure begin M.LeDoRadio(C) end);
  finally
    C.Free;
  end;
  Result := M;
end;

procedure TesteMenu;
var
  P: TPasta;
  It: TItem;
  Itens: Integer;
  Ok: Boolean;
  V, W: Integer;
begin
  Itens := 0;
  Ok := True;
  for P in Menu.Pastas do
    for It in P.Itens do
    begin
      Inc(Itens);
      Ok := Ok and (Length(It.Valores) >= 2);
      for V := 0 to High(It.Valores) do
        for W := V + 1 to High(It.Valores) do
          Ok := Ok and (It.Valores[V].Texto <> It.Valores[W].Texto);
    end;
  Confere((Menu.Pastas.Count = 15) and (Itens = 89), Format('menu: %d pastas e %d itens', [Menu.Pastas.Count, Itens]));
  Confere(Ok, 'menu: todo item com dois valores ou mais, de textos diferentes');
  It := Menu.Item('SCAN', 'THRESHOLD', '-');
  Confere((Length(It.Valores) = 9) and (It.Valores[0].Texto = 'S1') and (It.Valores[8].Texto = 'S9'),
          'menu: o THRESHOLD na pasta SCAN, de S1 a S9 (v167)');
  Confere((Menu.Vfos[0] = $8000) and (Menu.Vfos[2] = $D3004) and (Length(Menu.Registros) = 4) and
          (Length(Menu.Textos) = 4), 'menu: os VFOs, os registros e os textos');
end;

procedure TesteCifra;
var
  B: TBytes;
  I: Integer;
begin
  SetLength(B, 256);
  for I := 0 to 255 do
    B[I] := I;
  Confere(Iguais(Cifra(Cifra(B)), B), 'cifra: duas vezes volta ao que era');
  // "CO 7": o primeiro byte com C (0x43), o terceiro nunca (a chave é espaço), e 0, FF e a própria chave passam iguais
  Confere((Cifra([$41, $41, $41, $41])[0] = $02) and (Cifra([$41, $41, $41, $41])[2] = $41) and
          (Cifra([0, $FF, $43, $BC])[0] = 0) and (Cifra([$43, 0, 0, 0])[0] = $43) and (Cifra([$BC, 0, 0, 0])[0] = $BC),
          'cifra: a regra do _crypt do CHIRP');
end;

// o que o driver do CHIRP leu da mesma memória (esperado.txt), linha a linha
procedure TesteLeitura;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  M: TMemoriaBJ8300;
  Linha: string;
  C: TArray<string>;
  Ca: TCanal;
  It: TItem;
  Certos, Falhas: Integer;

  procedure Bate(Ok: Boolean; const L: string);
  begin
    if Ok then
      Inc(Certos)
    else
    begin
      Inc(Falhas);
      Writeln('  difere: ', L);
    end;
  end;

begin
  R := NovoRadio;
  T := R;
  M := Le(T);
  try
    Confere(not R.Lixo and (R.Sessoes = 1) and not R.NaSessao, 'leitura: uma sessão, fechada, sem byte fora do protocolo');
    Confere(R.Lidos = BlocosDaLeitura + 2, Format('leitura: %d blocos (e os de teste e da versão do firmware)', [R.Lidos]));
    Certos := 0;
    Falhas := 0;
    for Linha in TFile.ReadAllLines(TPath.Combine(Pasta, 'testes\esperado.txt'), TEncoding.UTF8) do
    begin
      C := Linha.Split([#9]);
      case C[0][1] of
        'C':
          begin
            Ca := M.Canal(StrToInt(C[1]));
            if C[2] = '-' then
              Bate(not Ca.Existe, Linha)
            else
              Bate(Ca.Existe and (Ca.Nome = C[2]) and (Ca.RX = Cardinal(StrToInt(C[3]))) and
                   (Ca.TX = Cardinal(StrToInt(C[4]))) and (TomTexto(Ca.TomRX) = C[5]) and (TomTexto(Ca.TomTX) = C[6]) and
                   (Ord(Ca.Potencia) = StrToInt(C[7])) and (Ord(Ca.Estreito) = StrToInt(C[8])) and
                   (Ord(Ca.Varre) = StrToInt(C[9])) and (Ord(Ca.BCL) = StrToInt(C[10])) and
                   (Ord(Ca.AM) = StrToInt(C[11])) and (Ca.Encrypt = StrToInt(C[12])) and
                   (Ca.Scramble = StrToInt(C[13])) and (Ca.Scode = StrToInt(C[14])) and
                   (Ca.PttId = StrToInt(C[15])) and (Ca.Bancos = StrToInt(C[16])), Linha);
          end;
        'A':
          begin
            It := Menu.Item(C[1], C[2], C[3][1]);
            Bate((It <> nil) and (M.Valor(It) = StrToInt(C[4])), Linha);
          end;
        'B':
          Bate((M.BancoNome(StrToInt(C[1])) = C[2]) and (Ord(M.BancoAtivo(StrToInt(C[1]))) = StrToInt(C[3])) and
               (M.CanaisNoBanco(StrToInt(C[1])) = StrToInt(C[4])), Linha);
        'T':
          Bate(M.Texto(C[1]) = C[2], Linha);
        'D':
          Bate(M.GrupoDtmf(StrToInt(C[1])) = C[2], Linha);
        'F':
          Bate((M.Frequencia(StrToInt(C[1])) = Cardinal(StrToInt(C[2]))) and
               (M.Offset(StrToInt(C[1])) = Cardinal(StrToInt(C[3]))), Linha);
      end;
    end;
    Confere((Falhas = 0) and (Certos > 100), Format('leitura igual à do driver do CHIRP: %d linhas certas, %d erradas',
                                                    [Certos, Falhas]));
    Confere(not M.Mudou and not M.PodeDesfazer, 'leitura: nada mudado');
  finally
    M.Free;
  end;
end;

procedure Escolhe(M: TMemoriaBJ8300; const Pasta, Nome: string; Lado: Char; const Texto: string);
var
  It: TItem;
  V: Integer;
begin
  It := Menu.Item(Pasta, Nome, Lado);
  for V := 0 to High(It.Valores) do
    if It.Valores[V].Texto = Texto then
    begin
      M.PoeValor_(It, V);
      Exit;
    end;
  raise Exception.CreateFmt('%s > %s não tem %s', [Pasta, Nome, Texto]);
end;

{ As mudanças que o fixtures.py confere pelo driver depois (confere(), a mesma lista lá). }
procedure Muda(M: TMemoriaBJ8300);
var
  C: TCanal;
  T: Word;
begin
  M.Guarda;
  C := Default(TCanal);
  C.Numero := 10;
  C.Existe := True;
  C.Nome := 'TESTE 10';
  C.RX := 43950000;
  C.TX := 43450000;
  TomDeTexto('D023N', T);
  C.TomRX := T;
  C.TomTX := 885;
  C.Potencia := ptMedia;
  C.Estreito := True;
  C.Varre := True;
  C.BCL := True;
  C.Encrypt := 2;
  C.Scramble := SCRAMBLES[4];  // 3000 Hz
  C.Scode := 3;
  C.PttId := 1;
  C.Bancos := 1 or 1 shl 4 or 1 shl 15;
  M.PoeCanal(C);
  M.ApagaCanal(2);
  M.PoeBancoNome(4, 'NOVO');
  M.PoeBancoAtivo(4, True);
  Escolhe(M, 'AUDIO', 'SQUELCH', '-', '7');
  Escolhe(M, 'TX', 'TX TIMEOUT', '-', '30S');
  Escolhe(M, 'AUDIO', 'EXPANDER', '-', 'HIGH');
  Escolhe(M, 'AUDIO', 'EQ BASS', '-', '+3 DB');
  Escolhe(M, 'WATCH', 'WATCH', '-', 'SINGLE');
  Escolhe(M, 'CHANNEL', 'STEP', 'C', '12.5K');
  Escolhe(M, 'CHANNEL', 'TX POWER', 'B', 'LOW');
  Escolhe(M, 'MORSE', 'SPEED', '-', '25 WPM');
  Escolhe(M, 'APRS (SMS)', 'PATH', '-', 'NONE');
  Escolhe(M, 'SCAN', 'THRESHOLD', '-', 'S6');
  M.PoeTexto('CALLSIGN', 'NOCALL');
  M.PoeTexto('MORSE MESSAGE', 'TESTE 73');
  M.PoeFrequencia(0, 14555000);
  M.PoeFrequencia(2, 43300000);
  M.PoeOffset(2, 600);
  M.PoeGrupoDtmf(1, '9876');
end;

procedure TesteGravacao;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  M: TMemoriaBJ8300;
  C: TCaboBJ;
  E: Cardinal;
  SoOsCanais: Boolean;
begin
  R := NovoRadio;
  T := R;
  M := Le(T);
  C := TCaboBJ.Create;
  try
    Muda(M);
    Confere(M.Mudou and M.PodeDesfazer, 'gravação: as mudanças aparecem');
    Confere(M.Canal(10).Existe and (M.Canal(10).Bancos = 1 or 1 shl 4 or 1 shl 15) and not M.Canal(2).Existe and
            (M.CanaisNoBanco(0) = 2) and (M.BancoNome(4) = 'NOVO'), 'gravação: o canal novo nos bancos, o apagado fora');
    M.PreparaGravacao;
    Confere(Menu.Item('WATCH', 'TRIPLE WATCH').Valores[M.Valor(Menu.Item('WATCH', 'TRIPLE WATCH'))].Texto = 'OFF',
            'gravação: com o WATCH em SINGLE, o TRIPLE WATCH desliga (a regra do driver)');
    Confere(M.MudouBancos, 'gravação: mexeu nos bancos (o rádio pede para ser desligado)');
    R.Gravados.Clear;
    C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    M.Gravou;
    Confere(not M.Mudou and not R.Lixo and not R.NaSessao, 'gravação: terminou, e nada mais a gravar');
    SoOsCanais := True;
    for E in R.Gravados do
      if E < $8000 then
        SoOsCanais := SoOsCanais and ((E = $0000) or (E = $0100));
    Confere(SoOsCanais, 'gravação: dos canais, só os blocos do 2 e do 10');
    SalvaAreas(R, TPath.Combine(Pasta, 'testes\gravado.bin'));
    // uma segunda leitura vê o mesmo que ficou na memória
    M.Free;
    M := Le(T);
    Confere((M.Canal(10).Nome = 'TESTE 10') and (M.Texto('CALLSIGN') = 'NOCALL') and (M.Frequencia(2) = 43300000) and
            (M.GrupoDtmf(1) = '9876') and (M.Offset(2) = 600), 'gravação: a releitura vê o que foi gravado');
  finally
    C.Free;
    M.Free;
  end;
end;

procedure TesteRecusas;
var
  Motivo, Lido: TIncompativel;
  Achou: Boolean;
  Versao: string;
  R: TRadioBJSimulado;
  T: ITransporte;
  M: TMemoriaBJ8300;
  C: TCaboBJ;
  Msg: string;
begin
  // o firmware de fábrica e o TEMPRA de fora da FAIXA (o que não diz a versão, o antigo e o mais novo): recusados na
  // entrada, antes de ler ou gravar qualquer bloco, com a sessão fechada e o cabo em ordem
  for Motivo := Low(TIncompativel) to High(TIncompativel) do
  begin
    R := NovoRadio;
    case Motivo of
      inFabrica: R.Fabrica := True;
      inSemVersao: R.Versao := '';
      inAntiga: R.Versao := 'V3.7T';
      inNova: R.Versao := 'V9.9T';
    end;
    T := R;
    Msg := '';
    Achou := False;
    try
      Le(T).Free;
    except
      on E: EIncompativel do
      begin
        Achou := True;
        Msg := E.Message;
        Lido := E.Motivo;
        Versao := E.Versao;
      end;
    end;
    Confere(Achou and (Lido = Motivo) and (Msg <> '') and not R.NaSessao and not R.Lixo and (R.Lidos = 1 + Ord(Motivo <> inFabrica)) and
            (Versao = IfThen(Motivo in [inAntiga, inNova], R.Versao, '')),
            Format('recusa: o firmware %s, sem ler nada, com a sessão fechada (%s)',
                   [IfThen(Motivo = inFabrica, 'de fábrica', 'TEMPRA "' + R.Versao + '"'), Msg]));
  end;
  // o rádio mudo
  R := NovoRadio;
  R.Mudo := True;
  T := R;
  Msg := '';
  try
    Le(T).Free;
  except
    on E: ESemRadio do
      Msg := E.Message;
  end;
  Confere(Msg <> '', 'recusa: o rádio que não responde');
  // o rádio mudou depois da leitura: nada se grava
  R := NovoRadio;
  T := R;
  M := Le(T);
  C := TCaboBJ.Create;
  try
    M.Guarda;
    Escolhe(M, 'AUDIO', 'SQUELCH', '-', '2');
    R.Memoria[$9000] := R.Memoria[$9000] xor 1;
    R.Gravados.Clear;
    Msg := '';
    try
      C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    except
      on E: EConflito do
        Msg := E.Message;
    end;
    Confere((Msg <> '') and (R.Gravados.Count = 0) and not R.NaSessao, 'recusa: o rádio mudou desde a leitura');
    // o backup aberto grava assim mesmo
    C.Sessao(T, procedure begin M.GravaNoRadio(C, True) end);
    Confere(R.Gravados.Count = 1, 'recusa: com Restaurar, grava por cima');
  finally
    C.Free;
    M.Free;
  end;
  // um firmware que não guarda o Morse pelo cabo
  R := NovoRadio;
  R.Limite := $D4000;
  T := R;
  M := Le(T);
  C := TCaboBJ.Create;
  try
    M.Guarda;
    Escolhe(M, 'MORSE', 'SPEED', '-', '30 WPM');
    Msg := '';
    try
      C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    except
      on E: EFalha do
        Msg := E.Message;
    end;
    Confere((Msg <> '') and not R.NaSessao, 'recusa: o firmware que responde mas não guarda');
  finally
    C.Free;
    M.Free;
  end;
  // o C no lado B do rádio: o VFO C é do rádio
  R := NovoRadio;
  R.Memoria[$D2008] := 1;
  R.Poe($D2000 + 12, [Byte(Soma(R.Pega($D2000, 12), 12)), Byte(Soma(R.Pega($D2000, 12), 12) shr 8)]);
  T := R;
  M := Le(T);
  C := TCaboBJ.Create;
  try
    M.Guarda;
    M.PoeFrequencia(2, 44000000);
    Msg := '';
    try
      C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    except
      on E: EConflito do
        Msg := E.Message;
    end;
    Confere(Msg.Contains('TRIPLE'), 'recusa: o VFO C com o C no lado B');
  finally
    C.Free;
    M.Free;
  end;
end;

procedure TesteRegistros;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  M: TMemoriaBJ8300;
  C: TCaboBJ;
begin
  // um rádio que nunca gravou o Morse, o APRS e o VFO C: eles valem o de fábrica, e mudar um campo cria o registro
  R := NovoRadio;
  FillChar(R.Memoria[$D3000], $3000, $FF);
  T := R;
  M := Le(T);
  C := TCaboBJ.Create;
  try
    Confere((Menu.Item('MORSE', 'SPEED').Valores[M.Valor(Menu.Item('MORSE', 'SPEED'))].Texto = '15 WPM') and
            (M.Texto('CALLSIGN') = '') and (M.Frequencia(2) = M.Frequencia(1)),
            'registros apagados: o Morse em 15 WPM, o APRS vazio e o VFO C igual ao B');
    M.Guarda;
    M.PoeTexto('MORSE MESSAGE', 'OK');
    C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    Confere(Iguais(R.Pega($D4000, 4), TEncoding.ASCII.GetBytes('MRS1')) and
            (Soma(R.Pega($D4000, 46), 46) = R.Memoria[$D4000 + 46] or R.Memoria[$D4000 + 47] shl 8) and
            (R.Memoria[$D4000 + 4] = 2) and (R.Memoria[$D4000 + 5] = 0) and (R.Memoria[$D3000] = $FF),
            'registros apagados: o Morse nasce com a assinatura e a soma, e o resto fica como estava');
  finally
    C.Free;
    M.Free;
  end;
end;

procedure TesteBackup;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  M, N: TMemoriaBJ8300;
  Arq: string;
  B: TBytesStream;
  Crc: Cardinal;
  Msg: string;
begin
  R := NovoRadio;
  T := R;
  M := Le(T);
  N := TMemoriaBJ8300.Create(Menu);
  try
    Arq := TPath.Combine(TPath.GetTempPath, 'tl-bj-teste' + EXTENSAO);
    M.Guarda;
    M.PoeBancoNome(1, 'GUARDADO');
    M.Salva(Arq);
    N.Abre(Arq);
    Confere((N.BancoNome(1) = 'GUARDADO') and (N.Canal(3).Nome = 'gmrs 3') and (N.Texto('TO') = 'APRS'),
            'backup: abre o que salvou, com as mudanças');
    // o backup do Tempra Link do UV-17L: recusado pelo modelo
    B := TBytesStream.Create;
    try
      B.WriteBuffer(TBytes.Create(Ord('T'), Ord('E'), Ord('M'), Ord('P'), Ord('R'), Ord('A'), 0, 1), 8);
      B.WriteData(Byte(6));
      B.WriteBuffer(TEncoding.ASCII.GetBytes('UV-17L'), 6);
      B.WriteData(Byte(0));
      B.WriteData(Byte(0));
      B.WriteData(Byte(0));
      Crc := crc32(0, PByte(B.Memory), B.Size);
      B.WriteData(Crc);
      B.SaveToFile(Arq);
    finally
      B.Free;
    end;
    Msg := '';
    try
      N.Abre(Arq);
    except
      on E: EArquivo do
        Msg := E.Message;
    end;
    Confere(Msg.Contains('UV-17L'), 'backup: recusa o arquivo do UV-17L (' + Msg + ')');
    TFile.Delete(Arq);
  finally
    N.Free;
    M.Free;
  end;
end;

procedure TesteCsv;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  M: TMemoriaBJ8300;
  Lista: TArray<TCanal>;
  Erros_: TArray<string>;
  Ok: Boolean;
  C: TCanal;
begin
  R := NovoRadio;
  T := R;
  M := Le(T);
  try
    Lista := CsvParaCanais(CanaisParaCsv(M), function(N: Integer): TCanal begin Result := M.Canal(N) end, Erros_);
    Ok := (Length(Lista) = 6) and (Erros_ = nil);
    for C in Lista do
      Ok := Ok and (C.RX = M.Canal(C.Numero).RX) and (C.TX = M.Canal(C.Numero).TX) and
            (C.TomRX = M.Canal(C.Numero).TomRX) and (C.TomTX = M.Canal(C.Numero).TomTX) and
            (C.AM = M.Canal(C.Numero).AM) and (C.Estreito = M.Canal(C.Numero).Estreito) and
            (C.Nome = M.Canal(C.Numero).Nome);
    Confere(Ok, Format('CSV: os %d canais vão e voltam iguais', [Length(Lista)]));
  finally
    M.Free;
  end;
end;

procedure TesteLogo;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  C: TCaboBJ;
  L, Lido: TBytes;
  I: Integer;
  Msg: string;
begin
  R := NovoRadio;
  SetLength(L, LOGO_TAMANHO);
  for I := 0 to High(L) do
    L[I] := Byte(I * 7);
  R.Poe(LOGO_ENDERECO, L);
  T := R;
  C := TCaboBJ.Create;
  try
    C.Sessao(T, procedure begin Lido := LeLogo(C) end);
    Confere(Iguais(Lido, L), 'logo: lê os 44160 bytes de 0x10000');
    for I := 0 to High(L) do
      L[I] := Byte(I * 3);
    Msg := '';
    try
      C.Sessao(T, procedure begin GravaLogo(C, L) end);
    except
      on E: EFalha do
        Msg := E.Message;
    end;
    Confere(Msg <> '', 'logo: o firmware de hoje não grava o logo, e o Tempra Link percebe');
    R.GravaFora := True;
    C.Sessao(T, procedure begin GravaLogo(C, L) end);
    Confere(Iguais(R.Pega(LOGO_ENDERECO, LOGO_TAMANHO), L), 'logo: um firmware que grava, grava');
  finally
    C.Free;
  end;
end;

// O retrato e a tela de uma cena da prévia (tests/teste_tela_pc.py --amostras): o que o firmware v163 mandaria e o que
// a prévia desenha.
function Amostra(const Cena: string; out Tela: TBytes): TBytes;
begin
  Result := TFile.ReadAllBytes(TPath.Combine(Pasta, 'testes\retrato-' + Cena + '.bin'));
  Tela := TFile.ReadAllBytes(TPath.Combine(Pasta, 'testes\tela-' + Cena + '.bin'));
end;

procedure TesteRemoto;
const
  PRINCIPAL = '1-a-em-evidencia-recebendo';   // com o medidor em 12 segmentos (S6)
  MENU = '9-menu-roger';                      // sem sinal (255)
var
  R: TRadioBJSimulado;
  T: ITransporte;
  Rem: TRemoto;
  L: TLeitorRemoto;
  E: TEstadoRadio;
  B, Fluxo, Ret, RetMenu, Tela, TelaMenu, Ultima, Imagem: TBytes;
  I, P, N, Telas, Estados, Mudancas, Codigo, SemRetrato: Integer;
  Msg, Letras: string;
begin
  // o quadro do PC e o estado do rádio, byte a byte (firmware/src/remoto.c)
  Confere(Iguais(QuadroRemoto(17, BIT_PTT or BIT_TELA), TBytes.Create(126, 82, 17, 3, 255, 255, 226)),
          'remoto: o quadro do PC, com a soma');
  // o lado em bits crus (v159): a vista 3 e a evidência no B
  B := TBytes.Create(126, 114, 10 or 32 or 64, 1, 4, 13 or $80, 3 or 4, 167, 0);
  for I := 0 to 7 do
    B[8] := Byte(B[8] + B[I]);
  Confere(EstadoDe(B, 0, E) and (E.Tecla = 9) and E.Ptt and E.Tela and (E.TX = 1) and (E.RX = 4) and
          (E.Sinal = 13) and E.Microfone and (E.SinalCru = 13 or $80) and (E.Letra = 'C') and (E.Vista = 3) and
          E.EvidenciaB and (E.Tensao = 74), 'remoto: o estado de 9 bytes, campo a campo');
  // a letra, os mesmos 10 casos que o firmware conferia até a v158 (tests/teste_remoto.py): (vista, evidência no B,
  // dupla escuta no outro, rodízio)
  Letras := '';
  for I := 0 to 9 do
    Letras := Letras + LetraDe(Ord('0000223331'[I + 1]) - Ord('0'), '0011001000'[I + 1] = '1',
                               '0101110110'[I + 1] = '1', '0000010010'[I + 1] = '1');
  Confere(Letras = 'ABBABCCCBA', 'remoto: a letra de quem o medidor mede, pelos bits do lado (' + Letras + ')');
  B[6] := 16;
  B[8] := 0;
  for I := 0 to 7 do
    B[8] := Byte(B[8] + B[I]);
  Confere(not EstadoDe(B, 0, E), 'remoto: o rodízio sem a dupla escuta no outro não vale, mesmo com a soma');
  B[6] := 3 or 4;
  B[8] := 0;
  for I := 0 to 7 do
    B[8] := Byte(B[8] + B[I]);
  B[8] := Byte(B[8] + 1);
  Confere(not EstadoDe(B, 0, E), 'remoto: a soma errada não vale');
  B[3] := 3;
  B[8] := 0;
  for I := 0 to 7 do
    B[8] := Byte(B[8] + B[I]);
  Confere(not EstadoDe(B, 0, E), 'remoto: um tx que o firmware não manda não vale, mesmo com a soma');
  B[3] := 1;
  B[8] := Byte(B[8] - 2);
  Confere(EstadoDe(B, 0, E), 'remoto: e o mesmo estado com o tx certo e a soma refeita vale');
  Confere((TextoSinal(0) = 'S0') and (TextoSinal(1) = 'S1') and (TextoSinal(2) = 'S1') and (TextoSinal(3) = 'S2') and
          (TextoSinal(18) = 'S9') and (TextoSinal(19) = 'S9+10') and (TextoSinal(24) = 'S9+60'),
          'remoto: o medidor em S (2 segmentos por S até o S9, depois 10 dB)');

  // o desenho da tela (TelaBJ, o código da tela do rádio ligado dentro do exe): as amostras da prévia
  Ret := Amostra(PRINCIPAL, Tela);
  RetMenu := Amostra(MENU, TelaMenu);
  Confere((TamanhoRetrato = Length(Ret)) and (Ret[2] = VersaoRetrato),
          'tela: o tamanho e a versão do retrato são os das amostras (' + IntToStr(TamanhoRetrato) + ' B)');
  Imagem := nil;
  Codigo := DesenhaRetrato(Ret, 12, Imagem);
  Confere((Codigo = TELA_BJ_INTEIRA) and Iguais(Imagem, Tela),
          'tela: a principal, com o medidor em S6, sai igual à prévia, pixel por pixel');
  Codigo := DesenhaRetrato(RetMenu, 255, Imagem);
  Confere((Codigo = TELA_BJ_INTEIRA) and Iguais(Imagem, TelaMenu),
          'tela: o menu, sem sinal (o 255 não é a TX), sai igual à prévia');
  B := Copy(Ret);
  B[100] := B[100] xor $10;
  Ultima := Imagem;
  Codigo := DesenhaRetrato(B, 12, Imagem);
  N := DesenhaRetrato(Copy(Ret, 0, Length(Ret) - 1), 12, Imagem);
  B := Copy(Ret);
  B[2] := B[2] xor 1;
  P := DesenhaRetrato(B, 12, Imagem);
  Confere((Codigo = TELA_BJ_ERRO_SOMA) and (N = TELA_BJ_ERRO_TAMANHO) and (P = TELA_BJ_ERRO_VERSAO) and Iguais(Imagem, Ultima),
          'tela: os erros (somas, tamanho, versão), e a imagem fica como estava');

  // o leitor: o estado e o retrato no meio do ruído, picados em pedaços, e um retrato estragado que se pula
  B := Copy(Ret);
  B[200] := B[200] xor 1;
  R := NovoRadio;
  T := R;
  R.Retrato := Ret;
  T.Escreve(QuadroRemoto(TECLA_NENHUMA, BIT_TELA));
  Fluxo := TBytes.Create(1, 2, 126, 9, 126, 83) + B + TBytes.Create(126) + T.Le + TBytes.Create(126, 83, 4);
  L := TLeitorRemoto.Create;
  try
    P := 0;
    N := 1;
    while P < Length(Fluxo) do
    begin
      L.Recebe(Copy(Fluxo, P, N));
      Inc(P, N);
      N := N mod 97 + 1;
    end;
    Confere((L.Estados = 1) and L.Estado.Valido and (L.Estado.Letra = 'B') and (TextoSinal(L.Estado.Sinal) = 'S7') and
            (L.Retratos = 1) and Iguais(L.Retrato, Ret) and (L.Estragados > 0),
            'remoto: o estado e o retrato saem lidos do meio do ruído, e o estragado se pula');
  finally
    L.Free;
  end;

  // o laço: a tecla e o PTT vão ao rádio, a tela vem desenhada, e no fim tudo solto
  R := NovoRadio;
  T := R;
  R.Retrato := Ret;
  R.Status[2] := 12;
  Rem := TRemoto.Create;
  try
    Telas := 0;
    Estados := 0;
    Mudancas := 0;
    Codigo := -99;
    Ultima := nil;
    Rem.OnTela :=
      procedure(Px: TBytes; K: Integer)
      begin
        if Px <> nil then
          Inc(Telas);
        Ultima := Px;
        Codigo := K;
      end;
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        case Estados of
          3:
            Rem.Aperta(TECLA_DIGITO[7]);
          6:
            begin
              Confere(R.RemTecla = 14, 'remoto: a tecla 7 chega ao rádio como o código 14');
              Rem.Solta;
              Rem.Ptt(True);
            end;
          9:
            begin
              Confere((R.RemTecla = -1) and (R.RemBits and BIT_PTT <> 0) and Es.Ptt,
                      'remoto: o PTT chega, e o estado o mostra');
              Rem.Ptt(False);
              Inc(Mudancas);
            end;
          20:
            R.Status[2] := 255;   // o sinal some: a tela muda (o medidor apaga)
          30:
            R.Status[2] := 12;
          40:
            Rem.Parar := True;
        end;
      end;
    Rem.Executa(T, True);
    // o retrato vem a cada quadro, e a janela só ouve a tela que mudou: a primeira, sem o sinal e de novo com ele
    Confere((Telas = 3) and (R.Retratos >= 30) and (Codigo = TELA_BJ_INTEIRA) and Iguais(Ultima, Tela),
            'remoto: a tela vem só quando muda, desenhada igual à prévia (' + IntToStr(Telas) + ' telas de ' +
            IntToStr(R.Retratos) + ' retratos)');
    Confere((R.RemTecla = -1) and (R.RemBits = 0), 'remoto: no fim, a tecla, o PTT e a tela desligados no rádio');
    Confere(Mudancas = 1, 'remoto: o laço chega ao fim pelo Parar');
  finally
    Rem.Free;
  end;

  // o retrato estragado não vira tela; o laço do APRS ou do Morse (o estado vem, o retrato não) dá o aviso uma vez, e
  // a tela volta quando ele acaba; a tela muda quando o rádio muda (o menu)
  R := NovoRadio;
  T := R;
  R.Retrato := Ret;
  R.Status[2] := 12;   // o sinal da amostra
  Rem := TRemoto.Create;
  try
    Telas := 0;
    Estados := 0;
    SemRetrato := 0;
    N := -1;
    Rem.OnTela :=
      procedure(Px: TBytes; K: Integer)
      begin
        if K = TELA_SEM_RETRATO then
        begin
          Inc(SemRetrato);
          N := Estados;
        end
        else if (K <> TELA_BJ_INTEIRA) or not (Iguais(Px, Tela) or Iguais(Px, TelaMenu)) then
          Inc(Telas);   // aqui, as telas erradas
        Ultima := Px;
      end;
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        case Estados of
          5:
            R.EstragaRetrato := True;
          10:
            R.Laco := True;
          30:
            begin
              R.Laco := False;
              R.Retrato := RetMenu;
              R.Status[2] := 255;
            end;
          40:
            Rem.Parar := True;
        end;
      end;
    Rem.Executa(T, True);
    Confere(Telas = 0, 'remoto: o retrato estragado no meio não vira tela errada');
    Confere((SemRetrato = 1) and (N >= 10 + SEM_RETRATO_MS div TELA_MS - 1) and (N < 30),
            'remoto: o laço do APRS ou do Morse dá o aviso uma vez, depois de ' + IntToStr(SEM_RETRATO_MS) +
            ' ms sem retrato (no estado ' + IntToStr(N) + ')');
    Confere(Iguais(Ultima, TelaMenu), 'remoto: depois do laço a tela volta, e é a de agora (o menu)');
  finally
    Rem.Free;
  end;

  // v161: o clique curto (o Aperta e o Solta na mesma volta do laço) chega ao rádio, segurado o mínimo
  R := NovoRadio;
  T := R;
  R.Retrato := Ret;
  Rem := TRemoto.Create;
  try
    Estados := 0;
    Mudancas := 0;
    Telas := 0;
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        if Es.Tecla = TECLA_DIGITO[2] then
          Inc(Mudancas);
        if Estados = 25 then
          Telas := R.UltimoToque;
        case Estados of
          10:
            begin
              Rem.Aperta(TECLA_DIGITO[2]);
              Rem.Solta;
            end;
          30:
            Rem.Parar := True;
        end;
      end;
    Rem.Executa(T, True);
    Confere((Mudancas > 0) and (Telas >= TECLA_MIN_MS),
            'remoto: o clique curto chega ao rádio, segurado o mínimo (' + IntToStr(Telas) + ' ms)');
  finally
    Rem.Free;
  end;

  // sem tela (o Bluetooth do Windows), nada de retrato; e o rádio mudo dá a mensagem do firmware
  R := NovoRadio;
  T := R;
  R.Retrato := Ret;
  Rem := TRemoto.Create;
  try
    Estados := 0;
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        if Estados = 20 then
          Rem.Parar := True;
      end;
    Rem.Executa(T, False);
    Confere(R.Retratos = 0, 'remoto: sem a tela pedida o rádio não manda retrato');
  finally
    Rem.Free;
  end;
  R := NovoRadio;
  R.Mudo := True;
  T := R;
  Rem := TRemoto.Create;
  try
    Msg := '';
    try
      Rem.Executa(T, True);
    except
      on Ex: ESemRadio do
        Msg := Ex.Message;
    end;
    Confere(Pos('controle remoto', Msg) > 0, 'remoto: o rádio mudo dá a mensagem do controle remoto');
  finally
    Rem.Free;
  end;
end;

// A velocidade do cabo (v157, v160): 460800, e 115200 e 57600 com os firmwares de antes, na leitura; o remoto a 460800
// e, sem resposta, a 115200 (v157 a v159).
function Nota(Hz, Ms: Word): TNota;
begin
  Result.Hz := Hz;
  Result.Ms := Ms;
end;

function MesmasNotas(const A, B: TArray<TNota>): Boolean;
var
  K: Integer;
begin
  Result := Length(A) = Length(B);
  for K := 0 to High(A) do
    Result := Result and (A[K].Hz = B[K].Hz) and (A[K].Ms = B[K].Ms);
end;

// O ROGER CUSTOM (firmware v164): o registro, a leitura, a gravação e os backups de antes.
procedure TesteRoger;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  M, N: TMemoriaBJ8300;
  C: TCaboBJ;
  Notas: TArray<TNota>;
  Reg, Exemplo, B: TBytes;
  Erro, Msg, Arq: string;
  G: TArray<TGravar>;
  Crc: Cardinal;
  P, K: Integer;
begin
  // as notas do tests/teste_roger.py: o registro que o firmware toca, byte a byte
  Notas := [Nota(988, 80), Nota(0, 20), Nota(1319, 81), Nota(2637, 150), Nota(300, 10), Nota(3000, 1000)];
  Reg := RegistroDeNotas(Notas);
  Exemplo := TFile.ReadAllBytes(TPath.Combine(Pasta, 'testes\roger-exemplo.bin'));
  Confere(Iguais(Reg, Exemplo), 'roger: o registro é o que o firmware toca (o do tests/teste_roger.py)');
  Confere(MesmasNotas(NotasDoRegistro(Reg), Notas), 'roger: e as notas voltam dele');
  Reg[10] := Reg[10] xor 4;
  Confere(NotasDoRegistro(Reg) = nil, 'roger: um byte estragado no começo, e ele não vale');
  Confere(NotasValidas(Notas, Erro) and not NotasValidas(nil, Erro) and not NotasValidas([Nota(299, 100)], Erro) and
          not NotasValidas([Nota(1000, 9)], Erro) and not NotasValidas([Nota(1000, 1000), Nota(1000, 1000),
          Nota(1000, 1000), Nota(1000, 10)], Erro) and NotasValidas([Nota(1000, 1000), Nota(1000, 1000),
          Nota(0, 1000)], Erro), 'roger: os limites do rádio (1 a 32 notas, 300 a 3000 Hz ou pausa, 10 a 1000 ms, 3 s)');

  // o rádio sem o registro: nenhuma nota; gravar: só a área do roger, e a releitura vê as notas
  R := NovoRadio;
  T := R;
  M := Le(T);
  N := nil;
  C := TCaboBJ.Create;
  try
    Confere(M.Notas = nil, 'roger: o rádio sem o registro, nenhuma nota');
    M.Guarda;
    M.PoeNotas(Notas);
    G := M.Mudancas;
    Confere((Length(G) = 1) and (G[0].Area = AREA_ROGER) and (G[0].Tamanho = $C0), 'roger: só a área do roger muda');
    C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    M.Gravou;
    B := R.Pega($DA000, $C0);
    Confere(Iguais(Copy(B, 0, ROGER_REGISTRO), Exemplo) and (B[ROGER_REGISTRO] = $FF) and (B[$BF] = $FF),
            'roger: no rádio, o registro e o resto em FF');
    N := Le(T);
    Confere(MesmasNotas(N.Notas, Notas), 'roger: a releitura vê as notas');
    // sem notas: a área apagada (o CUSTOM fica calado)
    M.Guarda;
    M.PoeNotas(nil);
    C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    M.Gravou;
    B := R.Pega($DA000, $C0);
    Confere((B[0] = $FF) and (B[$BF] = $FF) and (M.Notas = nil), 'roger: sem notas, a área apagada');
  finally
    C.Free;
    M.Free;
    N.Free;
  end;

  // o firmware até a v163 não grava ali (responde o ACK e ignora): a mensagem pede o v164
  R := NovoRadio;
  R.Limite := $D6000;
  T := R;
  M := Le(T);
  C := TCaboBJ.Create;
  try
    M.Guarda;
    M.PoeNotas(Notas);
    Msg := '';
    try
      C.Sessao(T, procedure begin M.GravaNoRadio(C, False) end);
    except
      on E: EFalha do
        Msg := E.Message;
    end;
    Confere(Pos('v164', Msg) > 0, 'roger: o firmware de antes não guarda as notas, e a mensagem pede o v164 (' + Msg + ')');
  finally
    C.Free;
    M.Free;
  end;

  // o backup de antes da v164, sem a área do roger: abre, sem notas
  R := NovoRadio;
  T := R;
  M := Le(T);
  N := TMemoriaBJ8300.Create(Menu);
  try
    Arq := TPath.Combine(TPath.GetTempPath, 'tl-bj-roger' + EXTENSAO);
    M.Salva(Arq);
    B := TFile.ReadAllBytes(Arq);
    // a última área é a do roger (8 bytes do endereço e do tamanho e os $C0), antes do CRC: sai, e uma área a menos
    Delete(B, Length(B) - 4 - 8 - $C0, 8 + $C0 + 4);
    P := 8;                                  // a marca; o modelo, a versão e a data, cada um com o tamanho na frente
    for K := 1 to 3 do
      Inc(P, 1 + B[P]);
    Dec(B[P]);                               // quantas áreas
    Crc := crc32(0, @B[0], Length(B));
    B := B + TBytes.Create(Byte(Crc), Byte(Crc shr 8), Byte(Crc shr 16), Byte(Crc shr 24));
    TFile.WriteAllBytes(Arq, B);
    Msg := '';
    try
      N.Abre(Arq);
    except
      on E: Exception do
        Msg := E.Message;
    end;
    Confere((Msg = '') and (N.Notas = nil) and (N.Canal(3).Nome = 'gmrs 3'),
            'roger: o backup de antes, sem a área, abre sem notas (' + Msg + ')');
    TFile.Delete(Arq);
  finally
    N.Free;
    M.Free;
  end;
end;

// A tela do rádio apagada pelo controle remoto (firmware v164): o bit 4 vai no quadro enquanto pedido, e sai no fim; e
// (v165) a tecla do rádio que acende a tela acaba o pedido, e o pedido novo apaga de novo.
procedure TesteTelaApagada;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  Rem: TRemoto;
  Estados, ComBit, SemBit, Acordou, DeNovo: Integer;
begin
  R := NovoRadio;
  T := R;
  Rem := TRemoto.Create;
  try
    Estados := 0;
    ComBit := 0;
    SemBit := 0;
    Acordou := 0;
    DeNovo := 0;
    Rem.Apaga(True);
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        case Estados of
          5:
            begin
              if R.RemBits and BIT_APAGA <> 0 then
                Inc(ComBit);
              Rem.Apaga(False);
            end;
          10:
            begin
              if R.RemBits and BIT_APAGA = 0 then
                Inc(SemBit);
              Rem.Apaga(True);
            end;
          15:
            R.Acorda := True;        // uma tecla do rádio
          20:
            begin
              if not Rem.Apagando and (R.RemBits and BIT_APAGA = 0) then
                Inc(Acordou);
              Rem.Apaga(True);       // a chave de novo em Apagada
            end;
          25:
            if Rem.Apagando and (R.RemBits and BIT_APAGA <> 0) and not R.Acorda then
              Inc(DeNovo);
          26:
            Rem.Parar := True;
        end;
      end;
    Rem.Executa(T, False);
    Confere((ComBit = 1) and (SemBit = 1) and (R.RemBits = 0),
            'remoto: a tela apagada vai no bit 4 enquanto pedida, sai quando não, e no fim nada fica');
    Confere(Acordou = 1, 'remoto: v165, a tecla do rádio que acende a tela acaba o pedido (a chave volta a Acesa)');
    Confere(DeNovo = 1, 'remoto: v165, e o pedido novo apaga de novo, sem o aviso velho');
  finally
    Rem.Free;
  end;
end;

// O controle remoto que se reconecta (v165): a ligação cai (o rádio emudece: o USB do cabo caiu), a porta não abre na
// primeira tentativa, e na seguinte o rádio volta; a primeira conexão que falha ainda dá o erro.
procedure TesteReligando;
var
  R1, R2, R3: TRadioBJSimulado;
  T1, T2, T3: ITransporte;
  Rem: TRemoto;
  Abertas, Quedas, Estados: Integer;
  Msgs, Msg: string;
begin
  R1 := NovoRadio;
  R2 := NovoRadio;
  T1 := R1;
  T2 := R2;
  Rem := TRemoto.Create;
  try
    Rem.PausaReligaMs := 0;
    Abertas := 0;
    Quedas := 0;
    Estados := 0;
    Msgs := '';
    Rem.OnRegistro := procedure(S: string) begin Msgs := Msgs + S + '|'; end;
    Rem.OnQueda := procedure begin Inc(Quedas); end;
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        if Estados = 10 then
          R1.Mudo := True;
        if Estados = 20 then
          Rem.Parar := True;
      end;
    Rem.ExecutaReligando(
      function: ITransporte
      begin
        Inc(Abertas);
        case Abertas of
          1: Result := T1;
          2: raise EInOutError.Create('a porta sumiu');
        else
          Result := T2;
        end;
      end, False);
    Confere((Abertas = 3) and (Quedas = 1) and (Estados = 20) and Msgs.Contains('caiu') and Msgs.Contains('reconectado'),
            'remoto: v165, a ligação que cai se refaz sozinha, e a porta que não abre se tenta de novo (' + Msgs + ')');
  finally
    Rem.Free;
  end;
  R3 := NovoRadio;
  R3.Mudo := True;
  T3 := R3;
  Rem := TRemoto.Create;
  try
    Rem.PausaReligaMs := 0;
    Msg := '';
    try
      Rem.ExecutaReligando(function: ITransporte begin Result := T3; end, False);
    except
      on E: ESemRadio do
        Msg := E.Message;
    end;
    Confere(Msg <> '', 'remoto: v165, o rádio que nunca respondeu ainda dá o erro, sem ficar tentando');
  finally
    Rem.Free;
  end;
  // o firmware que este Tempra Link não conhece: o remoto nem começa, e o erro diz por quê
  R3 := NovoRadio;
  R3.Versao := 'V3.7T';
  T3 := R3;
  Rem := TRemoto.Create;
  try
    Rem.PausaReligaMs := 0;
    Msg := '';
    try
      Rem.ExecutaReligando(function: ITransporte begin Result := T3; end, False);
    except
      on E: EIncompativel do
        Msg := E.Message;
    end;
    Confere((Msg <> '') and (R3.RemQuadros = 0) and not R3.NaSessao,
            'remoto: o firmware de fora da faixa não recebe nenhum quadro (' + Msg + ')');
  finally
    Rem.Free;
  end;
end;

procedure TesteVelocidade;
var
  R: TRadioBJSimulado;
  M: TMemoriaBJ8300;
  Rem: TRemoto;
  Msg: string;
  Estados: Integer;
begin
  R := NovoRadio;
  M := Le(R);
  try
    Confere((R.Sessoes = 1) and (R.Ruido = 0) and (R.Porta = BAUD_CABO) and not M.Vazia,
            'velocidade: o firmware v160 (460800) le de primeira');
  finally
    M.Free;
  end;
  R := NovoRadio;
  R.Serial := BAUD_CABO_V157;
  M := Le(R);
  try
    Confere((R.Sessoes = 1) and (R.Ruido > 0) and (R.Porta = BAUD_CABO_V157) and not M.Vazia,
            'velocidade: o firmware v157 a v159 (115200) sem resposta a 460800, e le a 115200 numa sessao so');
  finally
    M.Free;
  end;
  R := NovoRadio;
  R.Serial := BAUD_CABO_ANTIGA;
  M := Le(R);
  try
    Confere((R.Sessoes = 1) and (R.Ruido > 0) and (R.Porta = BAUD_CABO_ANTIGA) and not M.Vazia,
            'velocidade: o firmware ate a v156 (57600) sem resposta a 460800 nem a 115200, e le a 57600 numa sessao so');
  finally
    M.Free;
  end;
  // o remoto: a 460800 de primeira, e a tela vem
  R := NovoRadio;
  Rem := TRemoto.Create;
  try
    Estados := 0;
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        if Estados = 20 then
          Rem.Parar := True;
      end;
    R.Retrato := TFile.ReadAllBytes(TPath.Combine(Pasta, 'testes\retrato-9-menu-roger.bin'));
    Rem.Executa(R, True);
    Confere((R.Porta = BAUD_CABO) and (R.Ruido = 0) and (R.Retratos > 0),
            'velocidade: o remoto fala a 460800 com o firmware v160, e a tela vem');
  finally
    Rem.Free;
  end;
  R := NovoRadio;
  R.Serial := BAUD_CABO_ANTIGA;
  Rem := TRemoto.Create;
  try
    Msg := '';
    try
      Rem.Executa(R, True);
    except
      on E: ESemRadio do
        Msg := E.Message;
    end;
    Confere((Msg <> '') and (R.RemQuadros = 0), 'velocidade: o remoto nao fala com o firmware de antes (57600)');
  finally
    Rem.Free;
  end;
end;

// Um transporte que avisa a cada leitura, com o relógio dele: o teste age no meio do Executa sem esperar resposta.
type
  TComRelogio = class(TInterfacedObject, ITransporte)
  private
    FT: ITransporte;
    FCada: TProc<UInt64>;
  public
    constructor Create(const T: ITransporte; Cada: TProc<UInt64>);
    procedure Escreve(const B: TBytes);
    function Le: TBytes;
    procedure LimpaEntrada;
    function Agora: UInt64;
    procedure Dorme(Ms: Cardinal);
    function Velocidade(Baud: Cardinal = 0): Cardinal;
  end;

constructor TComRelogio.Create(const T: ITransporte; Cada: TProc<UInt64>);
begin
  inherited Create;
  FT := T;
  FCada := Cada;
end;

procedure TComRelogio.Escreve(const B: TBytes);
begin
  FT.Escreve(B);
end;

function TComRelogio.Le: TBytes;
begin
  Result := FT.Le;
  FCada(FT.Agora);
end;

procedure TComRelogio.LimpaEntrada;
begin
  FT.LimpaEntrada;
end;

function TComRelogio.Agora: UInt64;
begin
  Result := FT.Agora;
end;

procedure TComRelogio.Dorme(Ms: Cardinal);
begin
  FT.Dorme(Ms);
end;

function TComRelogio.Velocidade(Baud: Cardinal): Cardinal;
begin
  Result := FT.Velocidade(Baud);
end;

// O rádio mudo com o PTT apertado (o TX pelo Bluetooth do celular, 18/11): a ligação não cai enquanto o PTT está
// apertado, e cai QUEDA_MS depois de ele soltar; e, com o PausaTelaNoPtt, o retrato não se pede com o PTT.
procedure TestePttMudo;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  Rem: TRemoto;
  Estados, RetratosNoPtt: Integer;
  Apertou, Soltou, Falhou: UInt64;
  Msg: string;
  Tela: TBytes;
begin
  R := NovoRadio;
  Rem := TRemoto.Create;
  try
    Estados := 0;
    Apertou := 0;
    Soltou := 0;
    Falhou := 0;
    T := TComRelogio.Create(R,
      procedure(Agora: UInt64)
      begin
        if (Apertou > 0) and (Soltou = 0) and (Agora > Apertou + 6000) then
        begin
          Rem.Ptt(False);
          Soltou := Agora;
        end;
      end);
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        if Estados = 5 then
        begin
          Rem.Ptt(True);
          R.Mudo := True;
          Apertou := R.Agora;
        end;
      end;
    Msg := '';
    try
      Rem.Executa(T, False);
    except
      on E: EFalha do
      begin
        Msg := E.Message;
        Falhou := R.Agora;
      end;
    end;
    Confere((Msg <> '') and (Soltou > 0) and (Falhou >= Soltou + QUEDA_MS) and (Falhou < Soltou + QUEDA_MS + 500),
            Format('remoto: o rádio mudo com o PTT apertado não derruba a ligação; solto, ela cai em %d s ' +
                   '(apertou %d, soltou %d, caiu %d: %s)', [QUEDA_MS div 1000, Apertou, Soltou, Falhou, Msg]));
  finally
    Rem.Free;
  end;

  // a pausa do retrato: com o PTT apertado, o quadro vai sem o pedido da tela, e o rádio não manda retrato
  R := NovoRadio;
  R.Retrato := Amostra('1-a-em-evidencia-recebendo', Tela);
  T := R;
  Rem := TRemoto.Create;
  try
    Rem.PausaTelaNoPtt := True;
    Estados := 0;
    RetratosNoPtt := -1;
    Rem.OnEstado :=
      procedure(Es: TEstadoRadio)
      begin
        Inc(Estados);
        if Estados = 5 then
          Rem.Ptt(True);
        if Estados = 8 then
          RetratosNoPtt := R.Retratos;
        if Estados = 20 then
        begin
          Confere((R.RemBits and BIT_PTT <> 0) and (R.RemBits and BIT_TELA = 0) and (R.Retratos = RetratosNoPtt),
                  'remoto: com o PausaTelaNoPtt, o PTT apertado vai sem o pedido da tela, e nenhum retrato vem');
          Rem.Ptt(False);
        end;
        if Estados = 30 then
        begin
          Confere((R.RemBits and BIT_TELA <> 0) and (R.Retratos > RetratosNoPtt),
                  'remoto: solto o PTT, o pedido da tela e o retrato voltam');
          Rem.Parar := True;
        end;
      end;
    Rem.Executa(T, True);
  finally
    Rem.Free;
  end;
end;

{ O firmware que a conversa pelo cabo mostra (o Verificar da página Firmware): o TEMPRA com a velocidade em que
  respondeu, que dá a faixa da versão, o de fábrica e o rádio mudo. }
procedure TesteFirmwareNoRadio;
var
  R: TRadioBJSimulado;
  T: ITransporte;
  C: TCaboBJ;
  Baud: Cardinal;

  function Ve(Radio: TRadioBJSimulado): TCaboBJ;
  begin
    T := Radio;
    Result := TCaboBJ.Create;
    try
      Result.Sessao(T, procedure begin end);
    except
      on EIncompativel do ;
      on ESemRadio do ;
    end;
  end;

begin
  for Baud in BAUDS_CABO do
  begin
    R := NovoRadio;
    R.Serial := Baud;
    C := Ve(R);
    Confere((C.Firmware = fnTempra) and (C.Versao = FAIXA.Maxima) and not R.NaSessao,
            Format('firmware no rádio: o TEMPRA %s, com o cabo a %d', [C.Versao, Baud]));
    C.Free;
  end;
  // o de antes da v170: a leitura da versão traz o chip (o endereço dá a volta), sem o "TEMPRA "
  R := NovoRadio;
  R.Versao := '';
  R.Poe(VERSAO_CABO and $FFFFF, TEncoding.ASCII.GetBytes('TEMPRX V3.8T'));
  C := Ve(R);
  Confere((C.Firmware = fnTempra) and (C.Versao = '') and not R.NaSessao,
          'firmware no rádio: o TEMPRA de antes da v170 não diz a versão, nem com um texto parecido no chip');
  C.Free;
  R := NovoRadio;
  R.Fabrica := True;
  R.Serial := BAUD_CABO_ANTIGA;
  C := Ve(R);
  Confere((C.Firmware = fnFabrica) and not R.NaSessao and not R.Lixo, 'firmware no rádio: o de fábrica');
  C.Free;
  R := NovoRadio;
  R.Mudo := True;
  C := Ve(R);
  Confere(C.Firmware = fnNenhum, 'firmware no rádio: o mudo não mostra nenhum');
  C.Free;
end;

{ A gravação do firmware (GravadorBJ): os pacotes iguais aos do próprio BinTalk, a conferência do .anc (o de fábrica e
  o da release passam, um byte trocado e o tamanho errado não) e a conversa inteira contra o rádio simulado no modo
  UPDATE, com as falhas: resposta perdida, CRC errado, comando desconhecido, rádio mudo, rádio que cala no meio e o
  cancelamento. }
procedure TesteGravadorBJ;
const
  // o PackageFmt.Packing do BinTe ANC Bootloader.exe, chamado por reflexão no .NET do Windows
  VETORES: array[0..6, 0..1] of string = (
    ('06', 'AA5501000642F2'), ('0110270000', 'AA55050001102700000BBF'), ('0100C20100', 'AA5505000100C201005CDC'),
    ('03', 'AA55010003E7A2'), ('02001000000400000000DEADBEEF', 'AA550E0002001000000400000000DEADBEEF976E'),
    ('02', 'AA55010002C6B2'), ('0400', 'AA5502000400E370'));
var
  Fab, Rel, B: TBytes;
  A, A2, X: TAnc;
  I: Integer;

  function DeHex(const S: string): TBytes;
  var
    K: Integer;
  begin
    SetLength(Result, Length(S) div 2);
    for K := 0 to High(Result) do
      Result[K] := StrToInt('$' + Copy(S, 2 * K + 1, 2));
  end;

  function ParaHex(const D: TBytes): string;
  var
    V: Byte;
  begin
    Result := '';
    for V in D do
      Result := Result + IntToHex(V, 2);
  end;

  function Pedacos(Inicio, Tamanho, Quantos: Cardinal): TArray<TPedaco>;
  var
    K: Integer;
  begin
    SetLength(Result, Quantos);
    for K := 0 to Quantos - 1 do
    begin
      Result[K].Endereco := Inicio + Cardinal(K) * Tamanho;
      Result[K].Tamanho := Tamanho;
    end;
  end;

  // uma gravação contra o rádio simulado; devolve a classe da exceção ('' se terminou)
  function Grava(Falha: TProc<TRadioUpdate>; Cancela: Boolean; out R: TRadioUpdate; out Porta: IPortaGravacao): string;
  var
    G: TGravadorBJ;
  begin
    R := TRadioUpdate.Create(A2.Claro);
    Porta := R;
    R.Carregador := Pedacos($100000, 512, 3);
    // a imagem, com o último pedaço passando do fim do arquivo
    R.Imagem := Pedacos($200000, 1024, 4) + Pedacos(Length(A2.Claro) - 300, 512, 1);
    if Assigned(Falha) then
      Falha(R);
    G := TGravadorBJ.Create;
    try
      G.Cancelar := Cancela;
      try
        G.Grava(Porta, A2);
        Result := '';
      except
        on E: Exception do
          Result := E.ClassName;
      end;
    finally
      G.Free;
    end;
  end;

var
  R: TRadioUpdate;
  Porta: IPortaGravacao;
  Antes: UInt64;
begin
  for I := 0 to High(VETORES) do
    Confere(ParaHex(Empacota(DeHex(VETORES[I, 0]))) = VETORES[I, 1],
            'gravador: o pacote de ' + VETORES[I, 0] + ' é o do BinTalk');

  Fab := TFile.ReadAllBytes(TPath.Combine(Pasta, '..\firmware\original\BJ8300_V0.12.anc'));
  Rel := TFile.ReadAllBytes(TPath.Combine(Pasta, '..\firmware\releases\BJ8300_BANK_v4.0T.anc'));
  Confere((ConfereAnc(Fab, A) = '') and (A.Versao = 'V0.12') and A.Fabrica,
          'gravador: o .anc de fábrica (V0.12) confere');
  Confere((ConfereAnc(Rel, A2) = '') and (A2.Versao = 'V4.0T') and not A2.Fabrica,
          'gravador: o .anc da release (V4.0T) confere');
  B := Copy(Fab);
  Kdh(B);
  Kdh(B);
  Confere(Iguais(B, Fab), 'gravador: a camada KDH, posta duas vezes, volta ao arquivo');
  Confere(not Iguais(A.Claro, Fab), 'gravador: e tirada, muda o arquivo');
  for I in TArray<Integer>.Create(3, $40 + 7, Length(Rel) div 2, Length(Rel) - 30) do
  begin
    B := Copy(Rel);
    B[I] := B[I] xor $10;
    Confere(ConfereAnc(B, X) <> '', Format('gravador: um byte trocado em %x é recusado', [I]));
  end;
  Confere(ConfereAnc(Copy(Rel, 0, Length(Rel) - 1), X) <> '', 'gravador: um byte a menos é recusado');
  Confere(ConfereAnc(nil, X) <> '', 'gravador: o arquivo vazio é recusado');

  Confere(Grava(nil, False, R, Porta) = '', 'gravador: a gravação inteira termina');
  Confere(R.Erros.Count = 0, 'gravador: o rádio não viu nada fora do protocolo ' + R.Erros.CommaText);
  Confere(R.Servidos = 8, 'gravador: os 8 pedaços pedidos vieram iguais ao arquivo sem a camada KDH');
  Confere(R.Terminou and (R.Fins = 1), 'gravador: o 04 leva o 03, e o rádio termina');
  Confere((R.Velocidades.Count = 2) and (R.Velocidades[0] = VELOCIDADE_TROCA) and
          (R.Velocidades[1] = VELOCIDADE_TRABALHO), 'gravador: a serial vai de 9600 a 10000 e a 115200, na hora certa');
  Confere(R.Repetidos = 1, 'gravador: o 01 de andamento faz o PC mandar de novo a última carga (uma vez)');
  Confere(R.Vazias = 0, Format('gravador: com o pacote do rádio já na fila, nenhuma leitura espera os 20 ms à toa (%d)',
                               [R.Vazias]));

  Confere(Grava(procedure(X: TRadioUpdate) begin X.PerdeResposta := 6 end, False, R, Porta) = '',
          'gravador: uma resposta perdida no meio do carregador: repete e termina');
  Confere((R.Erros.Count = 0) and (R.Servidos = 8) and (R.Repetidos >= 2), 'gravador: e serve tudo certo');
  Confere(Grava(procedure(X: TRadioUpdate) begin X.PerdeResposta := 1 end, False, R, Porta) = '',
          'gravador: a primeira resposta perdida: repete o 06 e termina');
  Confere(Grava(procedure(X: TRadioUpdate) begin X.EstragaResposta := 9 end, False, R, Porta) = '',
          'gravador: uma resposta com o CRC errado: repete e termina');
  Confere((R.Erros.Count = 0) and (R.Servidos = 8), 'gravador: e serve tudo certo');
  Confere(Grava(procedure(X: TRadioUpdate) begin X.DesconhecidoAntes := 4 end, False, R, Porta) = '',
          'gravador: um comando desconhecido não trava: repete e termina');
  Confere((R.Erros.Count = 0) and (R.Servidos = 8), 'gravador: e serve tudo certo');

  Confere(Grava(procedure(X: TRadioUpdate) begin X.Mudo := True end, False, R, Porta) = 'ESemRadio',
          'gravador: o rádio mudo dá "não respondeu"');
  Antes := R.Agora;
  Confere((Antes > 6 * ESPERA_PACOTE) and (Antes < 8 * ESPERA_PACOTE),
          'gravador: depois de 5 repetições, de 2 s em 2 s (como o BinTalk)');
  Confere(Grava(procedure(X: TRadioUpdate) begin X.CalaDepois := 7 end, False, R, Porta) = 'EFalha',
          'gravador: o rádio que cala no meio dá falha no meio da gravação');
  Confere(Grava(procedure(X: TRadioUpdate) begin X.Mudo := True end, True, R, Porta) = 'ECancelado',
          'gravador: antes de o rádio responder, dá para cancelar');
end;

{ Os releases pelo GitHub (Atualizacao), sem internet: a resposta de verdade da V3.7T (sem as notas,
  testes/github-release.json), com o SHA-256 que o GitHub dá igual ao do .anc da release no repositório, numa lista
  com releases inventados (um firmware da FAIXA, um mais novo, uma pré-release, um rascunho e um só do Tempra Link); a
  troca do programa; e a FAIXA, que acompanha o firmware/src/versao.h. }
procedure TesteAtualizacao;

  function Rel(const Tag, Extra, Arquivos: string): string;
  begin
    Result := '{"tag_name": "' + Tag + '", "html_url": "https://github.com/x/releases/tag/' + Tag + '"' + Extra +
              ', "assets": [' + Arquivos + ']}';
  end;

  function Arq(const Nome: string; Tamanho: Integer): string;
  begin
    Result := '{"name": "' + Nome + '", "size": ' + IntToStr(Tamanho) + ', "digest": "sha256:AB12", ' +
              '"browser_download_url": "https://x/' + Nome + '"}';
  end;

var
  P: TProcura;
  Real, Json, Exe, VersaoH: string;
  Velho, Novo: TBytes;
  Inventada: TFaixa;  // não "Faixa": no Delphi, ela esconderia a constante FAIXA
  Erro: string;
begin
  Real := TFile.ReadAllText(TPath.Combine(Pasta, 'testes\github-release.json'), TEncoding.UTF8);
  Confere(LeReleases('[' + Real + ']', FAIXA, 'BJ8300_BANK_v', '.anc', 'TempraLink-BJ8300_v', P) = '',
          'atualização: o release de verdade se lê');
  Confere((P.MaisNovo.Tag = 'v3.7T') and (P.MaisNovo.Arquivo = 'BJ8300_BANK_v3.7T.anc') and (P.MaisNovo.Versao = '3.7T') and
          (P.MaisNovo.Tamanho = TAMANHO_ANC) and P.MaisNovo.Url.EndsWith('/v3.7T/BJ8300_BANK_v3.7T.anc'),
          'atualização: o arquivo de firmware dele, com a versão e o tamanho');
  // o .anc da V3.7T saiu de firmware/releases na V4.0T: o SHA-256 dele, o do histórico
  Confere(P.MaisNovo.Sha256 = 'fc0890e41baac86ab2af503612a0452277ba7613271d71f5cdc2e67297538342',
          'atualização: o SHA-256 que o GitHub dá é o do .anc da V3.7T');
  Confere((P.Firmware.Arquivo = '') and (P.Programa.Arquivo = ''),
          'atualização: a V3.7T é de fora da FAIXA, e ainda não há Tempra Link publicado');
  Json := '[' + Rel('link-1.2', '', Arq('TempraLink-BJ8300_v1.2.exe', 5000000)) + ',' +
         Rel('v4.0T', '', Arq('BJ8300_BANK_v4.0T.anc', TAMANHO_ANC)) + ',' +
         Rel('v3.9T', ', "prerelease": true', Arq('BJ8300_BANK_v3.9T.anc', TAMANHO_ANC) + ',' +
             Arq('TempraLink-BJ8300_v9.0.exe', 1)) + ',' +
         Rel('v3.9.1T', ', "draft": true', Arq('BJ8300_BANK_v3.9.1T.anc', TAMANHO_ANC)) + ',' +
         Rel('v3.8T', '', Arq('BJ8300_BANK_v3.8T.anc', TAMANHO_ANC) + ',' + Arq('TempraLink-BJ8300_v1.1.exe', 5000000) +
             ',' + Arq('UV17L_v1.7T.kdhx', 60000)) + ',' + Real + ']';
  Inventada.Minima := 'V3.8T';
  Inventada.Maxima := 'V3.8T';
  Confere(LeReleases(Json, Inventada, 'BJ8300_BANK_v', '.anc', 'TempraLink-BJ8300_v', P) = '',
          'atualização: a lista com os releases inventados se lê');
  Confere((P.Firmware.Versao = '3.8T') and (P.Firmware.Tag = 'v3.8T') and (P.Firmware.Sha256 = 'ab12') and
          (P.MaisNovo.Versao = '4.0T') and (P.Programa.Versao = '1.2') and (P.Programa.Tag = 'link-1.2'),
          Format('atualização: o firmware da faixa (%s), o mais novo (%s) e o Tempra Link (%s), sem a pré-release nem ' +
                 'o rascunho', [P.Firmware.Versao, P.MaisNovo.Versao, P.Programa.Versao]));
  Confere(LeReleases('{"message": "Not Found"}', FAIXA, 'BJ8300_BANK_v', '.anc', 'TempraLink-BJ8300_v', P) <> '',
          'atualização: a resposta que não é lista é recusada');
  Confere(LeReleases('isto não é json', FAIXA, 'BJ8300_BANK_v', '.anc', 'TempraLink-BJ8300_v', P) <> '',
          'atualização: o que não é JSON é recusado');
  Confere((ComparaVersoes('v3.7T', 'V3.7T') = 0) and (ComparaVersoes('V0.12', 'v3.7T') < 0) and
          (ComparaVersoes('1.10T', '1.9T') > 0) and (ComparaVersoes('v1.6T', '1.6') = 0) and
          (ComparaVersoes('V3.8T', 'v3.7T') > 0), 'atualização: as versões se comparam número a número');
  Confere(Inventada.Contem('V3.8T') and Inventada.Contem('3.8T') and not Inventada.Contem('V3.7T') and not Inventada.Contem('V3.9T') and
          not Inventada.Contem(''), 'compatibilidade: a faixa tem as duas pontas, e nada fora delas');
  // a FAIXA vai até a versão do firmware deste repositório: subiu a versão, o Tempra Link tem de ser revisto
  VersaoH := TFile.ReadAllText(TPath.Combine(Pasta, '..\firmware\src\versao.h'));
  Confere(TRegEx.Match(VersaoH, '#define VERSAO "([^"]+)"').Groups[1].Value = FAIXA.Maxima,
          'compatibilidade: a FAIXA.Maxima (' + FAIXA.Maxima + ') é a do firmware/src/versao.h');
  // a troca do programa, numa pasta de teste: o novo no lugar e o velho ao lado; o que não é programa não entra
  Exe := TPath.Combine(TPath.GetTempPath, 'TempraLinkTroca\TempraLink.exe');
  TDirectory.CreateDirectory(ExtractFileDir(Exe));
  Velho := TEncoding.ASCII.GetBytes('MZ velho');
  Novo := TEncoding.ASCII.GetBytes('MZ novo');
  TFile.WriteAllBytes(Exe, Velho);
  Erro := TrocaPrograma(Exe, TEncoding.ASCII.GetBytes('PK zip'));
  Confere((Erro <> '') and (TEncoding.ASCII.GetString(TFile.ReadAllBytes(Exe)) = 'MZ velho') and
          not TFile.Exists(Exe + '.novo'), 'atualização: o que não é programa não troca nada');
  Erro := TrocaPrograma(Exe, Novo);
  Confere((Erro = '') and (TEncoding.ASCII.GetString(TFile.ReadAllBytes(Exe)) = 'MZ novo') and
          (TEncoding.ASCII.GetString(TFile.ReadAllBytes(Exe + '.antigo')) = 'MZ velho') and not TFile.Exists(Exe + '.novo'),
          'atualização: o programa novo no lugar, e o velho ao lado (' + Erro + ')');
  ApagaAntigo(Exe);
  Sleep(1500);
  Confere(not TFile.Exists(Exe + '.antigo'), 'atualização: o velho se apaga na abertura seguinte');
  TDirectory.Delete(ExtractFileDir(Exe), True);
  // o programa aberto se renomeia (é o que a troca faz com o Tempra Link que está rodando): este teste, ida e volta
  Confere(RenameFile(ParamStr(0), ParamStr(0) + '.prova') and RenameFile(ParamStr(0) + '.prova', ParamStr(0)),
          'atualização: o Windows deixa renomear o programa que está aberto');
end;

begin
  try
    Pasta := ParamStr(1);
    Menu := TMenu.Create(TFile.ReadAllText(TPath.Combine(Pasta, 'menu-bj8300.txt'), TEncoding.UTF8));
    try
      TesteMenu;
      TesteCifra;
      TesteLeitura;
      TesteGravacao;
      TesteRecusas;
      TesteRegistros;
      TesteBackup;
      TesteCsv;
      TesteLogo;
      TesteRemoto;
      TesteRoger;
      TesteTelaApagada;
      TesteReligando;
      TesteVelocidade;
      TestePttMudo;
      TesteFirmwareNoRadio;
      TesteGravadorBJ;
      TesteAtualizacao;
    finally
      Menu.Free;
    end;
  except
    on E: Exception do
    begin
      Writeln('ERRO ', E.ClassName, ': ', E.Message);
      Inc(Erros);
    end;
  end;
  if Erros = 0 then
    Writeln('todos os testes passaram')
  else
    Writeln(Erros, ' teste(s) falharam');
  ExitCode := Erros;
end.
