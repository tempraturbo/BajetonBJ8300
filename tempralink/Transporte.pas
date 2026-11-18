unit Transporte;

{ O caminho dos bytes até o rádio: a serial do cabo (PortaSerial.pas), o Bluetooth (Bluetooth.pas) e, nos testes, o
  rádio simulado. O protocolo (CaboBJ.pas) só conversa com isto. }

interface

uses
  System.SysUtils;

type
  ITransporte = interface
    procedure Escreve(const B: TBytes);
    function Le: TBytes;  // o que chegou, esperando até uns 20 ms pelo primeiro byte
    procedure LimpaEntrada;
    function Agora: UInt64;  // ms
    procedure Dorme(Ms: Cardinal);
  end;

  EFalha = class(Exception);       // a conversa parou no meio
  ESemRadio = class(Exception);    // o rádio nunca respondeu
  ECancelado = class(Exception);

implementation

end.
