unit Atualizacao;

{ A versão mais nova do firmware publicada no GitHub (a API dos releases, sem login: o /releases/latest, que não
  devolve rascunho nem pré-release) e o download do arquivo dela, conferido pelo tamanho e pelo SHA-256 que o próprio
  GitHub dá (o campo digest de cada arquivo). O mesmo nos Tempra Link do UV-17L e do BJ-8300; quem chama diz o
  repositório e o começo e o fim do nome do arquivo de firmware. Sem nada de janela, para dar para testar: a leitura da
  resposta (LeRelease) e a comparação das versões (ComparaVersoes) ficam fora da internet. }

interface

uses
  System.SysUtils;

type
  TRelease = record
    Tag: string;       // v3.7T
    Titulo: string;
    Pagina: string;    // a página do release no GitHub
    Arquivo: string;   // o nome do arquivo de firmware
    Url: string;
    Tamanho: Int64;
    Sha256: string;    // em minúsculas; vazio se o GitHub não deu
  end;

// Lê a resposta do /releases/latest e acha o arquivo cujo nome começa com Prefixo e termina com Extensao (sem
// diferença de maiúsculas). '' = ok; senão, o que faltou.
function LeRelease(const Json, Prefixo, Extensao: string; out R: TRelease): string;
// Pergunta ao GitHub o último release do repositório ('dono/nome'). '' = ok.
function UltimoRelease(const Repo, Prefixo, Extensao: string; out R: TRelease): string;
// Baixa um arquivo e confere o tamanho (se Tamanho > 0) e o SHA-256 (se Sha256 não é vazio). '' = ok.
function Baixa(const Url: string; Tamanho: Int64; const Sha256: string; out Dados: TBytes): string;
// O SHA-256 em minúsculas.
function Sha256De(const Dados: TBytes): string;
// Compara duas versões como 'v1.6T', '1.5T', 'V3.7T' e 'V0.12' (sem o v e o T, número a número): <0, 0 ou >0.
function ComparaVersoes(const A, B: string): Integer;

var
  // O começo dos endereços da API; só a prova contra um servidor local o troca.
  ApiBase: string = 'https://api.github.com';

implementation

uses
  System.Classes, System.JSON, System.Hash, System.Net.HttpClient, System.Net.URLClient, Idioma;

function Sha256De(const Dados: TBytes): string;
var
  H: THashSHA2;
begin
  H := THashSHA2.Create;
  H.Update(Dados);
  Result := LowerCase(H.HashAsString);
end;

function Numeros(const V: string): TArray<Integer>;
var
  S, Parte: string;
begin
  S := V.Trim;
  if (S <> '') and CharInSet(S[1], ['v', 'V']) then
    Delete(S, 1, 1);
  S := S.TrimRight(['T', 't']);
  Result := nil;
  for Parte in S.Split(['.']) do
    Result := Result + [StrToIntDef(Parte, 0)];
end;

function ComparaVersoes(const A, B: string): Integer;
var
  X, Y: TArray<Integer>;
  I, VX, VY: Integer;
begin
  X := Numeros(A);
  Y := Numeros(B);
  for I := 0 to Length(X) + Length(Y) do
  begin
    if I < Length(X) then VX := X[I] else VX := 0;
    if I < Length(Y) then VY := Y[I] else VY := 0;
    if VX <> VY then
      Exit(VX - VY);
  end;
  Result := 0;
end;

function LeRelease(const Json, Prefixo, Extensao: string; out R: TRelease): string;
var
  V: TJSONValue;
  O, Arq: TJSONObject;
  Lista: TJSONArray;
  I: Integer;
  Nome, Digest: string;
begin
  R := Default(TRelease);
  V := TJSONObject.ParseJSONValue(Json);
  try
    if not (V is TJSONObject) then
      Exit(Tr('O GitHub respondeu algo que não é um release.', 'GitHub answered something that is not a release.'));
    O := TJSONObject(V);
    R.Tag := O.GetValue<string>('tag_name', '');
    R.Titulo := O.GetValue<string>('name', '');
    R.Pagina := O.GetValue<string>('html_url', '');
    if R.Tag = '' then
      Exit(Tr('O GitHub respondeu um release sem versão.', 'GitHub answered a release with no version.'));
    if not O.TryGetValue<TJSONArray>('assets', Lista) then
      Lista := nil;
    if Lista <> nil then
      for I := 0 to Lista.Count - 1 do
        if Lista.Items[I] is TJSONObject then
        begin
          Arq := TJSONObject(Lista.Items[I]);
          Nome := Arq.GetValue<string>('name', '');
          if Nome.StartsWith(Prefixo, True) and Nome.EndsWith(Extensao, True) then
          begin
            R.Arquivo := Nome;
            R.Url := Arq.GetValue<string>('browser_download_url', '');
            R.Tamanho := Arq.GetValue<Int64>('size', 0);
            Digest := '';
            if (Arq.GetValue('digest') <> nil) and not (Arq.GetValue('digest') is TJSONNull) then
              Digest := Arq.GetValue<string>('digest', '');
            if Digest.StartsWith('sha256:', True) then
              R.Sha256 := LowerCase(Copy(Digest, 8, MaxInt));
            Break;
          end;
        end;
    if (R.Arquivo = '') or (R.Url = '') then
      Exit(Format(Tr('O release %s não tem o arquivo de firmware (%s*%s).', 'Release %s has no firmware file (%s*%s).'),
                  [R.Tag, Prefixo, Extensao]));
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

function UltimoRelease(const Repo, Prefixo, Extensao: string; out R: TRelease): string;
var
  C: THTTPClient;
  Resp: IHTTPResponse;
begin
  R := Default(TRelease);
  C := NovoCliente;
  try
    try
      C.Accept := 'application/vnd.github+json';
      Resp := C.Get(ApiBase + '/repos/' + Repo + '/releases/latest');
      if Resp.StatusCode = 404 then
        Exit(Format(Tr('O projeto %s não tem release publicado, ou não é público ainda.',
                       'Project %s has no published release, or is not public yet.'), [Repo]));
      if Resp.StatusCode <> 200 then
        Exit(Format(Tr('O GitHub respondeu %d %s.', 'GitHub answered %d %s.'), [Resp.StatusCode, Resp.StatusText]));
      Result := LeRelease(Resp.ContentAsString(TEncoding.UTF8), Prefixo, Extensao, R);
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

end.
