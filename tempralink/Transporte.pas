unit Transporte;

{ O caminho dos bytes até o rádio: a serial do cabo (PortaSerial.pas), o Bluetooth (Bluetooth.pas) e, nos testes, o
  rádio simulado. O protocolo (CaboBJ.pas) só conversa com isto. }

interface

uses
  System.SysUtils;

const
  // a serial do cabo, na ordem em que se tenta: o firmware v160 em diante liga a 460800, da v157 à v159 a 115200, o de
  // fábrica e o TEMPRA até a v156 a 57600 (o testes/fixtures.py confere as três contra o driver do CHIRP)
  BAUD_CABO = 460800;
  BAUD_CABO_V157 = 115200;
  BAUD_CABO_ANTIGA = 57600;
  BAUDS_CABO: array[0..2] of Cardinal = (BAUD_CABO, BAUD_CABO_V157, BAUD_CABO_ANTIGA);

type
  ITransporte = interface
    procedure Escreve(const B: TBytes);
    function Le: TBytes;  // o que chegou, esperando até uns 20 ms pelo primeiro byte
    procedure LimpaEntrada;
    function Agora: UInt64;  // ms
    procedure Dorme(Ms: Cardinal);
    // A velocidade da serial (o cabo); no Bluetooth não muda nada. A atual, se Baud = 0.
    function Velocidade(Baud: Cardinal = 0): Cardinal;
  end;

  EFalha = class(Exception);       // a conversa parou no meio
  ESemRadio = class(Exception);    // o rádio nunca respondeu
  ECancelado = class(Exception);

implementation

end.
