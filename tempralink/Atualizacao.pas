unit Atualizacao;

{ O que o Tempra Link procura no GitHub, pela lista dos releases (a API sem login; rascunho e pré-release ficam de fora):
  o firmware mais novo com que ele trabalha (a faixa dele, Compatibilidade), o mais novo de todos (que pode pedir outro
  Tempra Link) e o próprio Tempra Link mais novo. Um release pode trazer o firmware, o programa ou os dois, e a versão
  de cada um vem do nome do arquivo (BJ8300_BANK_v3.8T.anc, TempraLink-BJ8300_v1.1.exe). O download é conferido pelo
  tamanho e pelo SHA-256 que o próprio GitHub dá (o campo digest de cada arquivo), e o programa novo entra no lugar do
  que está aberto (TrocaPrograma). O mesmo nos Tempra Link do UV-17L e do BJ-8300; quem chama diz o repositório e os
  começos dos nomes. Sem nada de janela, para dar para testar: a leitura da resposta (LeReleases) e a troca do
  programa ficam fora da internet. }

interface

uses
  System.SysUtils, Compatibilidade;

type
  TRelease = record
    Tag: string;       // o release: v3.8T
    Pagina: string;    // a página do release no GitHub
    Arquivo: string;   // o nome do arquivo (vazio: nenhum)
    Versao: string;    // a do nome do arquivo: BJ8300_BANK_v3.8T.anc -> 3.8T
    Url: string;
    Tamanho: Int64;
    Sha256: string;    // em minúsculas; vazio se o GitHub não deu
  end;

  TProcura = record
    Firmware: TRelease;   // o firmware mais novo dentro da faixa
    MaisNovo: TRelease;   // o firmware mais novo de todos
    Programa: TRelease;   // o Tempra Link mais novo
  end;

// Lê a resposta da lista de releases: os firmwares são os arquivos PrefixoFirmware*ExtensaoFirmware, e os programas,
// PrefixoPrograma*.exe (sem diferença de maiúsculas). '' = ok (mesmo sem nenhum arquivo); senão, o que falhou.
function LeReleases(const Json: string; const Faixa: TFaixa; const PrefixoFirmware, ExtensaoFirmware,
  PrefixoPrograma: string; out P: TProcura): string;
// Pergunta ao GitHub os releases do repositório ('dono/nome'). '' = ok.
function ProcuraReleases(const Repo: string; const Faixa: TFaixa; const PrefixoFirmware, ExtensaoFirmware,
  PrefixoPrograma: string; out P: TProcura): string;
// Baixa um arquivo e confere o tamanho (se Tamanho > 0) e o SHA-256 (se Sha256 não é vazio). '' = ok.
function Baixa(const Url: string; Tamanho: Int64; const Sha256: string; out Dados: TBytes): string;
// O SHA-256 em minúsculas.
function Sha256De(const Dados: TBytes): string;
// A versão do nome de um arquivo: o que vem entre o Prefixo e a extensão.
function VersaoDoArquivo(const Nome, Prefixo: string): string;
// Põe o programa Novo no lugar do Exe, que pode estar aberto (o Windows deixa renomear): grava o novo ao lado
// (Exe.novo), passa o Exe para Exe.antigo e o novo para Exe; se um passo falha, desfaz o que fez. '' = ok.
function TrocaPrograma(const Exe: string; const Novo: TBytes): string;
// Apaga o Exe.antigo de uma troca, em segundo plano: o programa velho pode estar acabando de fechar.
procedure ApagaAntigo(const Exe: string);

var
  // O começo dos endereços da API; só a prova contra um servidor local o troca.
  ApiBase: string = 'https://api.github.com';

implementation

uses
  System.Classes, System.IOUtils, System.JSON, System.Hash, System.Net.HttpClient, System.Net.URLClient, Idioma;

function Sha256De(const Dados: TBytes): string;
var
  H: THashSHA2;
begin
  H := THashSHA2.Create;
  H.Update(Dados);
  Result := LowerCase(H.HashAsString);
end;

function VersaoDoArquivo(const Nome, Prefixo: string): string;
begin
  Result := TPath.GetFileNameWithoutExtension(Nome);
  if Result.StartsWith(Prefixo, True) then
    Result := Copy(Result, Length(Prefixo) + 1, MaxInt);
  if (Result <> '') and CharInSet(Result[1], ['v', 'V']) then
    Delete(Result, 1, 1);
end;

function LeReleases(const Json: string; const Faixa: TFaixa; const PrefixoFirmware, ExtensaoFirmware,
  PrefixoPrograma: string; out P: TProcura): string;

  // o arquivo, se for mais novo que o Atual (no empate fica o que veio antes: a lista vem do mais novo ao mais antigo)
  procedure Considera(var Atual: TRelease; const R: TRelease);
  begin
    if (Atual.Arquivo = '') or (ComparaVersoes(R.Versao, Atual.Versao) > 0) then
      Atual := R;
  end;

var
  V: TJSONValue;
  Rel, Arq: TJSONObject;
  Arquivos: TJSONArray;
  I, K: Integer;
  R: TRelease;
  Digest: string;
begin
  P := Default(TProcura);
  V := TJSONObject.ParseJSONValue(Json);
  try
    if not (V is TJSONArray) then
      Exit(Tr('O GitHub respondeu algo que não é uma lista de releases.',
              'GitHub answered something that is not a list of releases.'));
    for I := 0 to TJSONArray(V).Count - 1 do
    begin
      if not (TJSONArray(V).Items[I] is TJSONObject) then
        Continue;
      Rel := TJSONObject(TJSONArray(V).Items[I]);
      if Rel.GetValue<Boolean>('draft', False) or Rel.GetValue<Boolean>('prerelease', False) or
         not Rel.TryGetValue<TJSONArray>('assets', Arquivos) then
        Continue;
      for K := 0 to Arquivos.Count - 1 do
        if Arquivos.Items[K] is TJSONObject then
        begin
          Arq := TJSONObject(Arquivos.Items[K]);
          R := Default(TRelease);
          R.Tag := Rel.GetValue<string>('tag_name', '');
          R.Pagina := Rel.GetValue<string>('html_url', '');
          R.Arquivo := Arq.GetValue<string>('name', '');
          R.Url := Arq.GetValue<string>('browser_download_url', '');
          R.Tamanho := Arq.GetValue<Int64>('size', 0);
          Digest := '';
          if (Arq.GetValue('digest') <> nil) and not (Arq.GetValue('digest') is TJSONNull) then
            Digest := Arq.GetValue<string>('digest', '');
          if Digest.StartsWith('sha256:', True) then
            R.Sha256 := LowerCase(Copy(Digest, 8, MaxInt));
          if R.Url = '' then
            Continue;
          if R.Arquivo.StartsWith(PrefixoFirmware, True) and R.Arquivo.EndsWith(ExtensaoFirmware, True) then
          begin
            R.Versao := VersaoDoArquivo(R.Arquivo, PrefixoFirmware);
            Considera(P.MaisNovo, R);
            if Faixa.Contem(R.Versao) then
              Considera(P.Firmware, R);
          end
          else if R.Arquivo.StartsWith(PrefixoPrograma, True) and R.Arquivo.EndsWith('.exe', True) then
          begin
            R.Versao := VersaoDoArquivo(R.Arquivo, PrefixoPrograma);
            Considera(P.Programa, R);
          end;
        end;
    end;
    Result := '';
  finally
    V.Free;
  end;
end;

function NovoCliente: THTTPClient;
begin
  Result := THTTPClient.Create;
  Result.UserAgent := 'TempraLink';
  Result.ConnectionTimeout := 15000;
  Result.ResponseTimeout := 60000;
  Result.HandleRedirects := True;
end;

function ProcuraReleases(const Repo: string; const Faixa: TFaixa; const PrefixoFirmware, ExtensaoFirmware,
  PrefixoPrograma: string; out P: TProcura): string;
var
  C: THTTPClient;
  Resp: IHTTPResponse;
begin
  P := Default(TProcura);
  C := NovoCliente;
  try
    try
      C.Accept := 'application/vnd.github+json';
      Resp := C.Get(ApiBase + '/repos/' + Repo + '/releases?per_page=100');
      if Resp.StatusCode = 404 then
        Exit(Format(Tr('O projeto %s não está no GitHub, ou não é público ainda.',
                       'Project %s is not on GitHub, or is not public yet.'), [Repo]));
      if Resp.StatusCode <> 200 then
        Exit(Format(Tr('O GitHub respondeu %d %s.', 'GitHub answered %d %s.'), [Resp.StatusCode, Resp.StatusText]));
      Result := LeReleases(Resp.ContentAsString(TEncoding.UTF8), Faixa, PrefixoFirmware, ExtensaoFirmware,
                           PrefixoPrograma, P);
    except
      on E: Exception do
        Result := Format(Tr('Sem acesso ao GitHub: %s', 'Could not reach GitHub: %s'), [E.Message]);
    end;
  finally
    C.Free;
  end;
end;

function Baixa(const Url: string; Tamanho: Int64; const Sha256: string; out Dados: TBytes): string;
var
  C: THTTPClient;
  Resp: IHTTPResponse;
  S: TBytesStream;
  Calculado: string;
begin
  Dados := nil;
  C := NovoCliente;
  S := TBytesStream.Create;
  try
    try
      Resp := C.Get(Url, S);
      if Resp.StatusCode <> 200 then
        Exit(Format(Tr('O download respondeu %d %s.', 'The download answered %d %s.'), [Resp.StatusCode, Resp.StatusText]));
    except
      on E: Exception do
        Exit(Format(Tr('O download falhou: %s', 'The download failed: %s'), [E.Message]));
    end;
    Dados := Copy(S.Bytes, 0, S.Size);
    if (Tamanho > 0) and (Length(Dados) <> Tamanho) then
    begin
      Result := Format(Tr('O download veio com %d bytes, e o arquivo tem %d.', 'The download came with %d bytes, and the ' +
                          'file has %d.'), [Length(Dados), Tamanho]);
      Dados := nil;
      Exit;
    end;
    if Sha256 <> '' then
    begin
      Calculado := Sha256De(Dados);
      if not SameText(Calculado, Sha256) then
      begin
        Result := Format(Tr('O download veio estragado: o SHA-256 dele é %s, e o do arquivo é %s.',
                            'The download came damaged: its SHA-256 is %s, and the file''s is %s.'), [Calculado, Sha256]);
        Dados := nil;
        Exit;
      end;
    end;
    Result := '';
  finally
    S.Free;
    C.Free;
  end;
end;

function TrocaPrograma(const Exe: string; const Novo: TBytes): string;
var
  Antigo, Proximo: string;
begin
  if (Length(Novo) < 2) or (Novo[0] <> Ord('M')) or (Novo[1] <> Ord('Z')) then
    Exit(Tr('O arquivo baixado não é um programa do Windows.', 'The downloaded file is not a Windows program.'));
  Antigo := Exe + '.antigo';
  Proximo := Exe + '.novo';
  try
    TFile.WriteAllBytes(Proximo, Novo);
  except
    on E: Exception do
      Exit(Format(Tr('Não deu para gravar na pasta do programa (%s): %s', 'Could not write to the program''s folder ' +
                     '(%s): %s'), [ExtractFileDir(Exe), E.Message]));
  end;
  DeleteFile(Antigo);   // o de uma troca anterior, se sobrou
  if not RenameFile(Exe, Antigo) then
  begin
    Result := Format(Tr('Não deu para tirar o programa do lugar: %s', 'Could not move the program aside: %s'),
                     [SysErrorMessage(GetLastError)]);
    DeleteFile(Proximo);
    Exit;
  end;
  if not RenameFile(Proximo, Exe) then
  begin
    Result := Format(Tr('Não deu para pôr o programa novo no lugar: %s', 'Could not put the new program in place: %s'),
                     [SysErrorMessage(GetLastError)]);
    RenameFile(Antigo, Exe);
    DeleteFile(Proximo);
    Exit;
  end;
  Result := '';
end;

procedure ApagaAntigo(const Exe: string);
var
  Antigo: string;
begin
  Antigo := Exe + '.antigo';
  if not FileExists(Antigo) then
    Exit;
  TThread.CreateAnonymousThread(
    procedure
    var
      I: Integer;
    begin
      for I := 1 to 20 do
        if DeleteFile(Antigo) or not FileExists(Antigo) then
          Break
        else
          TThread.Sleep(500);
    end).Start;
end;

end.
