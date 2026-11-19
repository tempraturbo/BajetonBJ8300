unit RadiosLembrados;

{ Os rádios que o app já usou, para conectar de novo sem a procura inteira: o endereço do Bluetooth, o nome que o rádio
  anuncia (o de fábrica é walkie-talkie, igual em todos) e o apelido que se dá a cada um, para separar dois ou três
  rádios iguais. Ficam no radios.ini da pasta do app, o usado por último primeiro. Um rádio entra na lista quando o
  controle remoto fala com ele pela primeira vez: só assim se sabe que o aparelho tocado era mesmo o rádio. }

interface

uses
  System.SysUtils;

type
  TLembrado = record
    Endereco, Nome, Apelido: string;
    Usado: TDateTime;
    // o apelido, ou o nome do Bluetooth
    function Titulo: string;
  end;

const
  LEMBRADOS_MAX = 8;

// Os lembrados, o usado por último primeiro.
function Lembrados: TArray<TLembrado>;
// O rádio usado agora: entra na lista (ou sobe para o primeiro lugar), com o apelido que já tinha.
procedure Lembra(const Endereco, Nome: string);
// O apelido ('' volta ao nome do Bluetooth).
procedure DaApelido(const Endereco, Apelido: string);
procedure Esquece(const Endereco: string);

implementation

uses
  System.Classes, System.IniFiles, System.IOUtils, System.DateUtils, System.Generics.Defaults,
  System.Generics.Collections, RegistroCelular;

function TLembrado.Titulo: string;
begin
  if Apelido <> '' then
    Result := Apelido
  else
    Result := Nome;
end;

function Arquivo: string;
begin
  Result := TPath.Combine(PastaDoApp, 'radios.ini');
end;

function Lembrados: TArray<TLembrado>;
var
  Ini: TMemIniFile;
  Secoes: TStringList;
  S: string;
  L: TLembrado;
begin
  Result := nil;
  Ini := TMemIniFile.Create(Arquivo, TEncoding.UTF8);
  Secoes := TStringList.Create;
  try
    Ini.ReadSections(Secoes);
    for S in Secoes do
    begin
      L.Endereco := S;
      L.Nome := Ini.ReadString(S, 'nome', '');
      L.Apelido := Ini.ReadString(S, 'apelido', '');
      if not TryISO8601ToDate(Ini.ReadString(S, 'usado', ''), L.Usado, False) then
        L.Usado := 0;
      Result := Result + [L];
    end;
  finally
    Secoes.Free;
    Ini.Free;
  end;
  TArray.Sort<TLembrado>(Result, TComparer<TLembrado>.Construct(
    function(const A, B: TLembrado): Integer
    begin
      Result := CompareDateTime(B.Usado, A.Usado);
    end));
end;

// Muda o arquivo pelo Muda e o grava; o que não grava fica no registro do app (a lista é só uma comodidade).
procedure Altera(const Muda: TProc<TMemIniFile>);
var
  Ini: TMemIniFile;
begin
  try
    Ini := TMemIniFile.Create(Arquivo, TEncoding.UTF8);
    try
      Muda(Ini);
      Ini.UpdateFile;
    finally
      Ini.Free;
    end;
  except
    on E: Exception do
      Anota('A lista dos rádios não foi guardada: ' + E.Message);
  end;
end;

procedure Lembra(const Endereco, Nome: string);
var
  Outros: TArray<TLembrado>;
  L: TLembrado;
begin
  Outros := nil;
  for L in Lembrados do
    if not SameText(L.Endereco, Endereco) then
      Outros := Outros + [L];
  Altera(
    procedure(Ini: TMemIniFile)
    var
      K: Integer;
    begin
      Ini.WriteString(Endereco, 'nome', Nome);
      Ini.WriteString(Endereco, 'usado', DateToISO8601(Now, False));
      // passando do máximo, saem os usados há mais tempo (o de agora é o mais novo)
      for K := LEMBRADOS_MAX - 1 to High(Outros) do
        Ini.EraseSection(Outros[K].Endereco);
    end);
end;

procedure DaApelido(const Endereco, Apelido: string);
begin
  Altera(
    procedure(Ini: TMemIniFile)
    begin
      if Ini.SectionExists(Endereco) then
        Ini.WriteString(Endereco, 'apelido', Apelido.Trim);
    end);
end;

procedure Esquece(const Endereco: string);
begin
  Altera(
    procedure(Ini: TMemIniFile)
    begin
      Ini.EraseSection(Endereco);
    end);
end;

end.
