unit Csv;

{ Os canais no CSV do CHIRP (File > Export / Import), para quem já tem a lista pronta: as mesmas colunas e os mesmos
  modos de tom (Tone, TSQL, DTCS e Cross). Os campos que o CHIRP não tem (o BCL, o SCRAMBLE, o ENCRYPT, o grupo DTMF,
  o PTT-ID e os bancos) ficam como estavam no canal, ou no de fábrica num canal novo. Sem nada de janela, para dar
  para testar. }

interface

uses
  System.SysUtils, System.Classes, BJ8300;

const
  CABECALHO_CHIRP = 'Location,Name,Frequency,Duplex,Offset,Tone,rToneFreq,cToneFreq,DtcsCode,DtcsPolarity,RxDtcsCode,' +
                    'CrossMode,Mode,TStep,Skip,Power,Comment,URCALL,RPT1CALL,RPT2CALL,DVCODE';

function CanaisParaCsv(M: TMemoriaBJ8300): string;
// Os canais do CSV, prontos para o PoeCanal (com o que o CHIRP não tem vindo de Base, o canal de agora); as linhas
// que não servem vão em Erros, com o número da linha.
function CsvParaCanais(const Texto: string; Base: TFunc<Integer, TCanal>; out Erros: TArray<string>): TArray<TCanal>;

implementation

uses
  System.Math, System.StrUtils, Idioma;

const
  POTENCIAS: array[TPotencia] of string = ('High', 'Mid', 'Low');

function Mhz(F: Cardinal): string;
begin
  Result := Format('%d.%.6d', [F div 100000, (F mod 100000) * 10]);
end;

function Ctcss(T: Word): string;
begin
  Result := Format('%d.%d', [T div 10, T mod 10]);
end;

function EhCtcss(T: Word): Boolean;
begin
  Result := (T >= 600) and (T < 3967);
end;

function EhDcs(T: Word): Boolean;
begin
  Result := (T >= 1) and (T <= 2 * Length(TONS_DCS));
end;

function DcsCodigo(T: Word): string;
begin
  Result := Format('%.3d', [TONS_DCS[(T - 1) mod Length(TONS_DCS)]]);
end;

function DcsPolaridade(T: Word): Char;
begin
  if T > Length(TONS_DCS) then
    Result := 'R'
  else
    Result := 'N';
end;

function Aspas(const S: string): string;
begin
  if S.Contains(',') or S.Contains('"') then
    Result := '"' + S.Replace('"', '""') + '"'
  else
    Result := S;
end;

function CanaisParaCsv(M: TMemoriaBJ8300): string;
var
  N: Integer;
  C: TCanal;
  Duplex, Offset, Tom, RTone, CTone, Dtcs, Pol, RxDtcs, Cruzado, Modo: string;
  L: TStringList;
begin
  L := TStringList.Create;
  try
    L.Add(CABECALHO_CHIRP);
    for N := 1 to CANAIS do
    begin
      C := M.Canal(N);
      if not C.Existe then
        Continue;
      Offset := '0.000000';
      if C.TX = 0 then
        Duplex := 'off'
      else if C.TX = C.RX then
        Duplex := ''
      else if Abs(Int64(C.TX) - C.RX) > 7000000 then  // mais de 70 MHz: outra faixa
      begin
        Duplex := 'split';
        Offset := Mhz(C.TX);
      end
      else
      begin
        if C.TX > C.RX then
          Duplex := '+'
        else
          Duplex := '-';
        Offset := Mhz(Abs(Int64(C.TX) - C.RX));
      end;
      RTone := '88.5';
      CTone := '88.5';
      Dtcs := '023';
      RxDtcs := '023';
      Pol := 'NN';
      Cruzado := 'Tone->Tone';
      Tom := '';
      if EhDcs(C.TomTX) then
        Dtcs := DcsCodigo(C.TomTX);
      if EhDcs(C.TomRX) then
        RxDtcs := DcsCodigo(C.TomRX);
      Pol := IfThen(EhDcs(C.TomTX), DcsPolaridade(C.TomTX), 'N') + IfThen(EhDcs(C.TomRX), DcsPolaridade(C.TomRX), 'N');
      if EhCtcss(C.TomTX) then
        RTone := Ctcss(C.TomTX);
      if EhCtcss(C.TomRX) then
        CTone := Ctcss(C.TomRX);
      if (C.TomTX = 0) and (C.TomRX = 0) then
        Tom := ''
      else if EhCtcss(C.TomTX) and (C.TomRX = 0) then
        Tom := 'Tone'
      else if EhCtcss(C.TomTX) and (C.TomRX = C.TomTX) then
        Tom := 'TSQL'
      else if EhDcs(C.TomTX) and EhDcs(C.TomRX) and (DcsCodigo(C.TomTX) = DcsCodigo(C.TomRX)) then
        Tom := 'DTCS'
      else
      begin
        Tom := 'Cross';
        Cruzado := IfThen(EhCtcss(C.TomTX), 'Tone', IfThen(EhDcs(C.TomTX), 'DTCS', '')) + '->' +
                   IfThen(EhCtcss(C.TomRX), 'Tone', IfThen(EhDcs(C.TomRX), 'DTCS', ''));
        if EhDcs(C.TomTX) and EhDcs(C.TomRX) then
          RxDtcs := DcsCodigo(C.TomRX);
      end;
      if C.AM and C.Estreito then
        Modo := 'NAM'
      else if C.AM then
        Modo := 'AM'
      else if C.Estreito then
        Modo := 'NFM'
      else
        Modo := 'FM';
      L.Add(Format('%d,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,%s,5.00,%s,%s,,,,,',
                   [N, Aspas(C.Nome), Mhz(C.RX), Duplex, Offset, Tom, RTone, CTone, Dtcs, Pol, RxDtcs, Cruzado, Modo,
                    IfThen(C.Varre, '', 'S'), POTENCIAS[C.Potencia]]));
    end;
    Result := L.Text;
  finally
    L.Free;
  end;
end;

// os campos de uma linha de CSV, com as aspas do CHIRP
function Campos(const Linha: string): TArray<string>;
var
  I: Integer;
  Atual: string;
  Dentro: Boolean;
begin
  Result := nil;
  Atual := '';
  Dentro := False;
  I := 1;
  while I <= Length(Linha) do
  begin
    if Dentro then
    begin
      if Linha[I] = '"' then
        if (I < Length(Linha)) and (Linha[I + 1] = '"') then
        begin
          Atual := Atual + '"';
          Inc(I);
        end
        else
          Dentro := False
      else
        Atual := Atual + Linha[I];
    end
    else if Linha[I] = '"' then
      Dentro := True
    else if Linha[I] = ',' then
    begin
      Result := Result + [Atual];
      Atual := '';
    end
    else
      Atual := Atual + Linha[I];
    Inc(I);
  end;
  Result := Result + [Atual];
end;

// "462.125000" (até 6 casas) -> 10 Hz; o que não cai em 10 Hz não serve
function MhzParaFreq(const S: string; out F: Cardinal): Boolean;
var
  Partes: TArray<string>;
  Inteira, Fracao: Integer;
begin
  Partes := S.Trim.Replace(',', '.').Split(['.']);
  Result := (Length(Partes) in [1, 2]) and TryStrToInt(Partes[0], Inteira) and (Inteira >= 0) and (Inteira < 10000);
  Fracao := 0;
  if Result and (Length(Partes) = 2) then
    Result := (Length(Partes[1]) <= 6) and TryStrToInt((Partes[1] + '000000').Substring(0, 6), Fracao);
  Result := Result and (Fracao mod 10 = 0);
  if Result then
    F := Cardinal(Inteira) * 100000 + Cardinal(Fracao div 10);
end;

function CsvParaCanais(const Texto: string; Base: TFunc<Integer, TCanal>; out Erros: TArray<string>): TArray<TCanal>;
var
  Linhas: TStringList;
  Cab, F: TArray<string>;
  I, N: Integer;
  C: TCanal;
  Freq, Desv: Cardinal;
  Cruz: TArray<string>;
  Pol: string;
  Watts: Double;
  Ok: Boolean;
  Erros_: TArray<string>;

  function Campo(const Nome: string): string;
  var
    K: Integer;
  begin
    Result := '';
    for K := 0 to High(Cab) do
      if SameText(Cab[K].Trim, Nome) and (K < Length(F)) then
        Exit(F[K].Trim);
  end;

  procedure Erro(const S: string);
  begin
    Erros_ := Erros_ + [Format(Tr('linha %d: %s', 'line %d: %s'), [I + 1, S])];
  end;

  // um lado do tom: o tipo (Tone, DTCS ou nada) e o valor do CTCSS ou do DCS daquele lado
  function Lado(const Tipo, Ctcss, Dcs: string; Polaridade: Char; out T: Word): Boolean;
  begin
    if Tipo = 'Tone' then
      Result := TomDeTexto(Ctcss, T)
    else if Tipo = 'DTCS' then
      Result := TomDeTexto('D' + Dcs + IfThen(Polaridade = 'R', 'I', 'N'), T)
    else
    begin
      T := 0;
      Result := Tipo = '';
    end;
  end;

begin
  Result := nil;
  Erros_ := nil;
  Linhas := TStringList.Create;
  try
    Linhas.Text := Texto;
    if Linhas.Count > 0 then
    begin
      Cab := Campos(Linhas[0]);
      I := 0;
      F := Cab;
      if (Campo('Location') = '') or (Campo('Frequency') = '') then
        Erro(Tr('não é o CSV do CHIRP (faltam as colunas Location e Frequency)',
                'not a CHIRP CSV (the Location and Frequency columns are missing)'))
      else
        for I := 1 to Linhas.Count - 1 do
        begin
          if Linhas[I].Trim = '' then
            Continue;
          F := Campos(Linhas[I]);
          if not TryStrToInt(Campo('Location'), N) or (N < 1) or (N > CANAIS) then
          begin
            Erro(Tr('o número do canal vai de 1 a 1000', 'the channel number goes from 1 to 1000'));
            Continue;
          end;
          C := Base(N);
          if not C.Existe then
          begin
            // um canal novo, como o SAVE do rádio o grava: alta, larga, na varredura, sem banco
            C := Default(TCanal);
            C.Varre := True;
          end;
          C.Numero := N;
          C.Existe := True;
          if not MhzParaFreq(Campo('Frequency'), Freq) or not FaixaRX(Freq) then
          begin
            Erro(Tr('frequência fora das faixas do rádio', 'frequency outside the radio''s bands'));
            Continue;
          end;
          C.RX := Freq;
          Ok := MhzParaFreq(IfThen(Campo('Offset') = '', '0', Campo('Offset')), Desv);
          if Campo('Duplex') = '' then
            C.TX := C.RX
          else if Campo('Duplex') = '+' then
            C.TX := C.RX + Desv
          else if (Campo('Duplex') = '-') and (Desv < C.RX) then
            C.TX := C.RX - Desv
          else if Campo('Duplex') = 'split' then
            C.TX := Desv
          else if Campo('Duplex') = 'off' then
            C.TX := 0
          else
            Ok := False;
          if not Ok then
          begin
            Erro(Tr('o Duplex ou o Offset não servem', 'the Duplex or the Offset do not fit'));
            Continue;
          end;
          C.Nome := NomeValido(Campo('Name'));
          Pol := Campo('DtcsPolarity') + 'NN';
          if Campo('Tone') = '' then
            Ok := Lado('', '', '', 'N', C.TomTX) and Lado('', '', '', 'N', C.TomRX)
          else if Campo('Tone') = 'Tone' then
            Ok := Lado('Tone', Campo('rToneFreq'), '', 'N', C.TomTX) and Lado('', '', '', 'N', C.TomRX)
          else if Campo('Tone') = 'TSQL' then
            Ok := Lado('Tone', Campo('cToneFreq'), '', 'N', C.TomTX) and Lado('Tone', Campo('cToneFreq'), '', 'N', C.TomRX)
          else if Campo('Tone') = 'DTCS' then
            Ok := Lado('DTCS', '', Campo('DtcsCode'), Pol[1], C.TomTX) and
                  Lado('DTCS', '', Campo('DtcsCode'), Pol[2], C.TomRX)
          else if Campo('Tone') = 'Cross' then
          begin
            Cruz := (Campo('CrossMode') + '->').Split(['->']);
            Ok := (Length(Cruz) >= 2) and Lado(Cruz[0], Campo('rToneFreq'), Campo('DtcsCode'), Pol[1], C.TomTX) and
                  Lado(Cruz[1], Campo('cToneFreq'), Campo('RxDtcsCode'), Pol[2], C.TomRX);
          end
          else
            Ok := False;
          if not Ok then
          begin
            Erro(Tr('o tom não serve', 'the tone does not fit'));
            Continue;
          end;
          if (Campo('Mode') = 'AM') or (Campo('Mode') = 'NAM') then
          begin
            C.AM := True;
            C.Estreito := Campo('Mode') = 'NAM';
          end
          else if (Campo('Mode') = 'FM') or (Campo('Mode') = 'NFM') then
          begin
            C.AM := False;
            C.Estreito := Campo('Mode') = 'NFM';
          end
          else
          begin
            Erro(Tr('o modo vai em FM, NFM, AM ou NAM', 'the mode goes in FM, NFM, AM or NAM'));
            Continue;
          end;
          C.Varre := Campo('Skip') <> 'S';
          if SameText(Campo('Power'), 'High') then
            C.Potencia := ptAlta
          else if SameText(Campo('Power'), 'Mid') then
            C.Potencia := ptMedia
          else if SameText(Campo('Power'), 'Low') then
            C.Potencia := ptBaixa
          else if TryStrToFloat(Campo('Power').TrimRight(['W', 'w']), Watts, TFormatSettings.Invariant) then
            if Watts >= 4 then
              C.Potencia := ptAlta
            else if Watts >= 2 then
              C.Potencia := ptMedia
            else
              C.Potencia := ptBaixa;
          Result := Result + [C];
        end;
    end;
  finally
    Linhas.Free;
  end;
  Erros := Erros_;
end;

end.
