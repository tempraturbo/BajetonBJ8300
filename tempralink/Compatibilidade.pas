unit Compatibilidade;

{ Com quais firmwares o Tempra Link trabalha. Cada Tempra Link conhece uma faixa de versões (TFaixa, as duas pontas
  incluídas) e só fala com o rádio que diz uma delas: o firmware de fábrica, um TEMPRA que não diz a versão e uma
  versão de fora da faixa ficam de fora, porque o que eles guardam e respondem pode ser outro, e gravar ali estragaria
  o que o rádio tem. A faixa é a do código: mudou o que o firmware guarda ou responde, muda a versão dele, e o Tempra
  Link que sabe lidar com ela sai com a faixa nova. O mesmo nos Tempra Link do UV-17L e do BJ-8300 (e no do celular).
  Sem nada de janela, para dar para testar. }

interface

uses
  System.SysUtils;

type
  TFaixa = record
    Minima, Maxima: string;
    function Contem(const Versao: string): Boolean;
  end;

  // por que o firmware do rádio não serve
  TIncompativel = (inFabrica, inSemVersao, inAntiga, inNova);

  EIncompativel = class(Exception)
  public
    Motivo: TIncompativel;
    Versao: string;   // a que o rádio disse ('' no de fábrica e no que não diz)
    constructor Cria(AMotivo: TIncompativel; const AVersao: string);
  end;

// Compara duas versões como 'v1.6T', '1.5T', 'V3.7T' e 'V0.12' (sem o v e o T, número a número): <0, 0 ou >0.
function ComparaVersoes(const A, B: string): Integer;
// Levanta o EIncompativel se a Versao que o rádio disse ('' = não disse) está fora da Faixa.
procedure ConfereVersao(const Versao: string; const Faixa: TFaixa);

implementation

uses
  Idioma;

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

function TFaixa.Contem(const Versao: string): Boolean;
begin
  Result := (Versao <> '') and (ComparaVersoes(Versao, Minima) >= 0) and (ComparaVersoes(Versao, Maxima) <= 0);
end;

constructor EIncompativel.Cria(AMotivo: TIncompativel; const AVersao: string);
begin
  case AMotivo of
    inFabrica:
      inherited Create(Tr('Este rádio está com o firmware de fábrica, e o Tempra Link só trabalha com o firmware TEMPRA.',
                          'This radio runs the factory firmware, and Tempra Link only works with the TEMPRA firmware.'));
    inSemVersao:
      inherited Create(Tr('Este rádio está com um firmware TEMPRA antigo, que não diz a versão, e este Tempra Link não ' +
                          'trabalha com ele.',
                          'This radio runs an old TEMPRA firmware that does not report its version, and this Tempra ' +
                          'Link does not work with it.'));
    inAntiga:
      inherited CreateFmt(Tr('Este rádio está com o firmware %s, antigo para este Tempra Link.',
                             'This radio runs firmware %s, too old for this Tempra Link.'), [AVersao]);
  else
    inherited CreateFmt(Tr('Este rádio está com o firmware %s, mais novo que este Tempra Link.',
                           'This radio runs firmware %s, newer than this Tempra Link.'), [AVersao]);
  end;
  Motivo := AMotivo;
  Versao := AVersao;
end;

procedure ConfereVersao(const Versao: string; const Faixa: TFaixa);
begin
  if Versao = '' then
    raise EIncompativel.Cria(inSemVersao, '');
  if ComparaVersoes(Versao, Faixa.Minima) < 0 then
    raise EIncompativel.Cria(inAntiga, Versao);
  if ComparaVersoes(Versao, Faixa.Maxima) > 0 then
    raise EIncompativel.Cria(inNova, Versao);
end;

end.
