unit Ajustes;

{ Os ajustes do BJ-8300 como o menu do rádio os mostra, a partir da descrição em menu-bj8300.txt (gerada pelo
  gera_menu.py com o driver do CHIRP deste repositório, e embutida no .exe): as pastas, os itens, o texto de cada valor
  e os bits que cada valor põe na memória. O Tempra Link não interpreta nada: lê o valor de um item procurando o valor
  cujos bits estão na memória, e grava um valor pondo os bits dele. Sem nada de janela, para dar para testar. }

interface

uses
  System.SysUtils, System.Classes, System.Generics.Collections;

type
  TByteDoValor = record
    Endereco: Cardinal;  // no rádio
    Mascara, Valor: Byte;
  end;

  TValor = record
    Texto: string;
    Bytes: TArray<TByteDoValor>;
  end;

  TItem = class
    Nome: string;
    Lado: Char;       // '-' vale para o rádio; 'A', 'B' ou 'C' é de um VFO
    Fora: Integer;    // o valor que o rádio mostra quando os bits não batem com nenhum; -1: não há
    Valores: TArray<TValor>;
    function Rotulo: string;
  end;

  TPasta = class
    Nome: string;
    Itens: TObjectList<TItem>;
    constructor Create;
    destructor Destroy; override;
  end;

  // Um registro com assinatura e soma (bancos.c): sem as duas, o rádio o lê como zerado.
  TRegistroDesc = record
    Endereco: Cardinal;
    Magia: TBytes;
    Tamanho: Integer;
  end;

  TTextoDesc = record
    Nome: string;
    Registro: Cardinal;
    PosTamanho, Inicio, Maximo: Integer;
  end;

  TMenu = class
  public
    Registros: TArray<TRegistroDesc>;
    Vfos: array[0..2] of Cardinal;  // onde começa o VFO A, o B e o C
    Textos: TArray<TTextoDesc>;
    Pastas: TObjectList<TPasta>;
    constructor Create(const Texto: string);
    destructor Destroy; override;
    class function DoRecurso: TMenu;
    function Item(const Pasta, Nome: string; Lado: Char = '-'): TItem;
    function Texto(const Nome: string): TTextoDesc;
  end;

// O valor de agora: o que tem todos os bits na memória; sem nenhum, o Fora; sem Fora, o que mais acerta.
function ValorDoItem(const It: TItem; Le: TFunc<Cardinal, Byte>): Integer;

implementation

uses
  System.Types, Winapi.Windows;

{ TItem }

function TItem.Rotulo: string;
begin
  if Lado = '-' then
    Result := Nome
  else
    Result := Nome + ' (VFO ' + Lado + ')';
end;

{ TPasta }

constructor TPasta.Create;
begin
  Itens := TObjectList<TItem>.Create;
end;

destructor TPasta.Destroy;
begin
  Itens.Free;
  inherited;
end;

{ TMenu }

function Hex(const S: string): Cardinal;
begin
  Result := StrToInt('$' + S);
end;

constructor TMenu.Create(const Texto: string);
var
  Linha, Parte: string;
  C, Nums: TArray<string>;
  Pasta: TPasta;
  It: TItem;
  V: TValor;
  B: TByteDoValor;
  R: TRegistroDesc;
  T: TTextoDesc;
begin
  Pastas := TObjectList<TPasta>.Create;
  Pasta := nil;
  It := nil;
  for Linha in Texto.Split([#10]) do
  begin
    if Linha.Trim = '' then
      Continue;
    C := Linha.TrimRight([#13]).Split([#9]);
    case C[0][1] of
      'R':
        begin
          R.Endereco := Hex(C[1]);
          R.Magia := TEncoding.ASCII.GetBytes(C[2]);
          R.Tamanho := StrToInt(C[3]);
          Registros := Registros + [R];
        end;
      'F':
        Vfos[Ord(C[1][1]) - Ord('A')] := Hex(C[2]);
      'T':
        begin
          T.Nome := C[1];
          T.Registro := Hex(C[2]);
          T.PosTamanho := StrToInt(C[3]);
          T.Inicio := StrToInt(C[4]);
          T.Maximo := StrToInt(C[5]);
          Textos := Textos + [T];
        end;
      'P':
        begin
          Pasta := TPasta.Create;
          Pasta.Nome := C[1];
          Pastas.Add(Pasta);
        end;
      'I':
        begin
          It := TItem.Create;
          It.Nome := C[1];
          It.Lado := C[2][1];
          It.Fora := -1;
          if (Length(C) > 3) and (C[3] <> '') then
            It.Fora := StrToInt(C[3]);
          Pasta.Itens.Add(It);
        end;
      'V':
        begin
          V := Default(TValor);
          V.Texto := C[1];
          for Parte in C[2].Split([',']) do
          begin
            // "endereço/máscara=valor", em hexadecimal
            Nums := Parte.Split(['/', '=']);
            B.Endereco := Hex(Nums[0]);
            B.Mascara := Hex(Nums[1]);
            B.Valor := Hex(Nums[2]);
            V.Bytes := V.Bytes + [B];
          end;
          It.Valores := It.Valores + [V];
        end;
    end;
  end;
end;

destructor TMenu.Destroy;
begin
  Pastas.Free;
  inherited;
end;

class function TMenu.DoRecurso: TMenu;
var
  R: TResourceStream;
  B: TBytes;
begin
  R := TResourceStream.Create(HInstance, 'MENU_BJ8300', RT_RCDATA);
  try
    SetLength(B, R.Size);
    R.ReadBuffer(B, Length(B));
  finally
    R.Free;
  end;
  Result := TMenu.Create(TEncoding.UTF8.GetString(B));
end;

function TMenu.Item(const Pasta, Nome: string; Lado: Char): TItem;
var
  P: TPasta;
  It: TItem;
begin
  for P in Pastas do
    if P.Nome = Pasta then
      for It in P.Itens do
        if (It.Nome = Nome) and (It.Lado = Lado) then
          Exit(It);
  Result := nil;
end;

function TMenu.Texto(const Nome: string): TTextoDesc;
var
  T: TTextoDesc;
begin
  for T in Textos do
    if T.Nome = Nome then
      Exit(T);
  raise EArgumentException.CreateFmt('texto %s fora do menu', [Nome]);
end;

function ValorDoItem(const It: TItem; Le: TFunc<Cardinal, Byte>): Integer;
var
  V, Acertos, Melhor: Integer;
  B: TByteDoValor;
begin
  Result := -1;
  Melhor := -1;
  for V := 0 to High(It.Valores) do
  begin
    Acertos := 0;
    for B in It.Valores[V].Bytes do
      if Le(B.Endereco) and B.Mascara = B.Valor then
        Inc(Acertos);
    if Acertos = Length(It.Valores[V].Bytes) then
      Exit(V);
    if Acertos > Melhor then
    begin
      Melhor := Acertos;
      Result := V;
    end;
  end;
  if It.Fora >= 0 then
    Result := It.Fora;
end;

end.
