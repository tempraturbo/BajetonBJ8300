unit RegistroCelular;

{ O registro do app no celular: cada linha com a hora, em memória (as últimas, para a página Registro) e no arquivo
  registro.txt da pasta do app, que o adb lê sem root num APK de depuração (run-as tempralink.bj8300 cat
  files/registro.txt). É por ele que se sabe o que o rádio fez num teste longe do PC: as quedas do controle remoto, o
  PTT, os intervalos sem resposta e os erros da leitura e da gravação. De qualquer thread. }

interface

uses
  System.SysUtils, System.Classes, System.SyncObjs, System.IOUtils;

// A pasta do app: a do Android, ou, no PC (o app rodando nos testes), uma no TEMP, para não sujar os Documentos.
function PastaDoApp: string;
procedure Anota(const S: string);
// As últimas linhas, a mais nova primeiro.
function UltimasLinhas: TArray<string>;
function ArquivoDoRegistro: string;

implementation

const
  EM_MEMORIA = 300;
  ARQUIVO_MAX = 512 * 1024;   // passando disso na partida, fica a metade de baixo

var
  Trava: TCriticalSection;
  Linhas: TStringList;
  Arquivo: string;

function PastaDoApp: string;
begin
{$IFDEF ANDROID}
  Result := TPath.GetDocumentsPath;
{$ELSE}
  Result := TPath.Combine(TPath.GetTempPath, 'TempraLinkCelular');
  TDirectory.CreateDirectory(Result);
{$ENDIF}
end;

function ArquivoDoRegistro: string;
begin
  Result := Arquivo;
end;

procedure Anota(const S: string);
var
  L: string;
begin
  L := FormatDateTime('dd/mm hh:nn:ss.zzz', Now) + '  ' + S;
  Trava.Enter;
  try
    Linhas.Add(L);
    if Linhas.Count > EM_MEMORIA then
      Linhas.Delete(0);
    try
      TFile.AppendAllText(Arquivo, L + sLineBreak, TEncoding.UTF8);
    except
      // sem o arquivo, o registro fica só na memória: não é motivo para parar o app
    end;
  finally
    Trava.Leave;
  end;
end;

function UltimasLinhas: TArray<string>;
var
  K: Integer;
begin
  Trava.Enter;
  try
    SetLength(Result, Linhas.Count);
    for K := 0 to Linhas.Count - 1 do
      Result[K] := Linhas[Linhas.Count - 1 - K];
  finally
    Trava.Leave;
  end;
end;

procedure Encurta;
var
  B: TBytes;
  P: Integer;
begin
  if not TFile.Exists(Arquivo) then
    Exit;
  B := TFile.ReadAllBytes(Arquivo);
  if Length(B) <= ARQUIVO_MAX then
    Exit;
  P := Length(B) div 2;
  while (P < Length(B)) and (B[P - 1] <> 10) do
    Inc(P);
  TFile.WriteAllBytes(Arquivo, Copy(B, P, MaxInt));
end;

initialization
  Trava := TCriticalSection.Create;
  Linhas := TStringList.Create;
  Arquivo := TPath.Combine(PastaDoApp, 'registro.txt');
  try
    Encurta;
  except
  end;

finalization
  Linhas.Free;
  Trava.Free;

end.
