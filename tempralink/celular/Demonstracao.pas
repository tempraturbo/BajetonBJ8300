unit Demonstracao;

{ A demonstração, sem rádio: o BJ-8300 simulado dos testes (testes/SimuladorBJ.pas) no tempo de verdade, com o S-meter
  andando, para ver o app sem o rádio. O simulado tem o relógio dele (cada leitura vazia anda 20 ms); aqui a leitura
  vazia espera 20 ms de verdade, e o tempo é o do sistema. }

interface

uses
  System.SysUtils, System.Classes, System.Math, Transporte, SimuladorBJ;

type
  TDemonstracao = class(TInterfacedObject, ITransporte)
  private
    FRadio: TRadioBJSimulado;
    FInicio: UInt64;
    procedure Anda;
  public
    constructor Create;
    destructor Destroy; override;
    procedure Escreve(const B: TBytes);
    function Le: TBytes;
    procedure LimpaEntrada;
    function Agora: UInt64;
    procedure Dorme(Ms: Cardinal);
    function Velocidade(Baud: Cardinal = 0): Cardinal;
  end;

implementation

constructor TDemonstracao.Create;
begin
  inherited;
  FRadio := TRadioBJSimulado.Create;
  FRadio._AddRef;   // é interface também: a contagem fica com quem a criou
  FInicio := TThread.GetTickCount64;
end;

destructor TDemonstracao.Destroy;
begin
  FRadio._Release;
  inherited;
end;

{ A cada 6 s: 4 recebendo, com o sinal subindo e descendo (o lado B em evidência), e 2 em espera. }
procedure TDemonstracao.Anda;
var
  T: Double;
begin
  T := (TThread.GetTickCount64 - FInicio) mod 6000 / 1000;
  if T < 4 then
  begin
    FRadio.Status[1] := 4;
    FRadio.Status[2] := Round(9 + 8 * Sin(T * 2.4) + 3 * Sin(T * 7.1));
  end
  else
  begin
    FRadio.Status[1] := 2;
    FRadio.Status[2] := 255;
  end;
end;

procedure TDemonstracao.Escreve(const B: TBytes);
begin
  Anda;
  FRadio.Escreve(B);
end;

function TDemonstracao.Le: TBytes;
begin
  Result := FRadio.Le;
  if Result = nil then
    TThread.Sleep(20);
end;

procedure TDemonstracao.LimpaEntrada;
begin
  FRadio.LimpaEntrada;
end;

function TDemonstracao.Agora: UInt64;
begin
  Result := TThread.GetTickCount64;
end;

procedure TDemonstracao.Dorme(Ms: Cardinal);
begin
  TThread.Sleep(Ms);
end;

function TDemonstracao.Velocidade(Baud: Cardinal): Cardinal;
begin
  Result := 0;
end;

end.
