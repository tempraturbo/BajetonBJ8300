program TesteTempraLink;

{ Os testes do Tempra Link do BJ-8300, sem rádio: o menu, a cifra, a leitura pelo cabo contra o rádio simulado e
  contra o que o driver do CHIRP lê da mesma memória (testes/fixtures.py gera), a gravação (que o fixtures.py confere
  depois pelo driver), as recusas (firmware de fábrica, rádio mudo, conflito, firmware que não guarda), o backup, o CSV
  e o logo.
  Uso: TesteTempraLink.exe <pasta tempralink> }

{$APPTYPE CONSOLE}

uses
  System.SysUtils, System.Classes, System.IOUtils, System.ZLib, System.Math, Idioma, Transporte, CaboBJ, Ajustes,
  BJ8300, Csv, Logo, SimuladorBJ;

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
  Confere((Menu.Pastas.Count = 15) and (Itens = 88), Format('menu: %d pastas e %d itens', [Menu.Pastas.Count, Itens]));
  Confere(Ok, 'menu: todo item com dois valores ou mais, de textos diferentes');
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
    Confere(R.Lidos = BlocosDaLeitura + 1, Format('leitura: %d blocos (e o de teste do firmware)', [R.Lidos]));
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
  R: TRadioBJSimulado;
  T: ITransporte;
  M: TMemoriaBJ8300;
  C: TCaboBJ;
  Msg: string;
begin
  // o firmware de fábrica: recusado na entrada, com a sessão fechada e o cabo em ordem
  R := NovoRadio;
  R.Fabrica := True;
  T := R;
  Msg := '';
  try
    Le(T).Free;
  except
    on E: ENaoTempra do
      Msg := E.Message;
  end;
  Confere((Msg <> '') and not R.NaSessao and not R.Lixo, 'recusa: o firmware de fábrica, com a sessão fechada');
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
