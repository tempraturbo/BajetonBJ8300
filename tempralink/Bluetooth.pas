unit Bluetooth;

{ O Bluetooth do BJ-8300 pelo Bluetooth LE do Windows e do Android: EXPERIMENTAL. Ninguém provou ainda que o rádio programa por
  ele. O que se sabe: o rádio tem o Bluetooth do próprio chip (JieLi AC695X, com JL_SPP e BLE no firmware), o item
  SYSTEM > BLUETOOTH o liga, e o driver do CHIRP diz que o RT-900 BT, o primo dele, programa por BLE com o mesmo
  protocolo do cabo.

  E o que a leitura do firmware mostrou (sem teste no rádio): a tabela do GATT tem o serviço FFE0, com a
  característica FFE1 de escrita e notificação, e o que se escreve nela cai no mesmo buffer que o cabo enche, com o
  mesmo magic; a resposta volta por notificação na FFE1. O AE00 da JieLi (AE01 e AE02) é outro protocolo, e o SPP
  clássico não chega a esse buffer: parear o rádio como porta COM não serve para programar. Enquanto a sessão pelo
  BLE não fecha, o rádio ignora o cabo.

  Então isto procura os aparelhos BLE, conecta no escolhido, escreve no Registro todos os serviços e características
  que ele anuncia (para o próximo passo, se não der certo) e usa a FFE1; sem ela, o primeiro serviço fora dos padrões
  que tenha uma característica de escrita e uma de notificação. O protocolo em cima é o mesmo do cabo (CaboBJ.pas). }

interface

uses
  System.SysUtils, System.Classes, System.SyncObjs, System.Bluetooth, Transporte;

type
  TRadioBLE = record
    Nome, Endereco: string;
    Aparelho: TBluetoothLEDevice;  // é do TBluetoothLEManager, que o guarda entre as procuras
  end;

  TTransporteBLE = class(TInterfacedObject, ITransporte)
  private
    FAparelho: TBluetoothLEDevice;
    FEscrita, FAviso: TBluetoothGattCharacteristic;
    FChegou: TBytes;
    FTrava: TCriticalSection;
    FSinal: TEvent;
    procedure Leu(const Sender: TObject; const C: TBluetoothGattCharacteristic; Status: TBluetoothGattStatus);
  public
    constructor Create(Aparelho: TBluetoothLEDevice; Registra: TProc<string>);
    destructor Destroy; override;
    procedure Escreve(const B: TBytes);
    function Le: TBytes;
    procedure LimpaEntrada;
    function Agora: UInt64;
    procedure Dorme(Ms: Cardinal);
    function Velocidade(Baud: Cardinal = 0): Cardinal;
  end;

// Os aparelhos BLE em volta, procurando por EsperaMs (fora da thread da janela: espera o fim da procura).
function ProcuraRadios(EsperaMs: Cardinal): TArray<TRadioBLE>;

implementation

uses
  Idioma;

const
{$IFDEF ANDROID}
  AQUI_PT = 'celular';
  AQUI_EN = 'phone';
{$ELSE}
  AQUI_PT = 'PC';
  AQUI_EN = 'PC';
{$ENDIF}
  PEDACO = 20;
  FFE1: TBluetoothUUID = '{0000FFE1-0000-1000-8000-00805F9B34FB}';  // o que cabe numa escrita com o MTU de 23 bytes, o menor que o BLE garante

type
  // o fim da procura chega por evento: um objeto só para segurar o sinal
  TEspera = class
    Fim: TEvent;
    procedure Acabou(const Sender: TObject; const Lista: TBluetoothLEDeviceList);
  end;

procedure TEspera.Acabou(const Sender: TObject; const Lista: TBluetoothLEDeviceList);
begin
  Fim.SetEvent;
end;

function Gerenciador: TBluetoothLEManager;
begin
  try
    Result := TBluetoothLEManager.Current;
  except
    on E: Exception do
      raise ESemRadio.Create(Tr('O Bluetooth deste ' + AQUI_PT + ' não respondeu (' + E.Message + '). Ele precisa de ' +
                                'Bluetooth 4.0 ou mais novo, ligado.',
                                'This ' + AQUI_EN + '''s Bluetooth did not answer (' + E.Message + '). It needs ' +
                                'Bluetooth 4.0 or newer, turned on.'));
  end;
  if Result = nil then
    raise ESemRadio.Create(Tr('Este ' + AQUI_PT + ' não tem Bluetooth LE.', 'This ' + AQUI_EN + ' has no Bluetooth LE.'));
end;

function ProcuraRadios(EsperaMs: Cardinal): TArray<TRadioBLE>;
var
  M: TBluetoothLEManager;
  E: TEspera;
  A: TBluetoothLEDevice;
  R: TRadioBLE;
begin
  Result := nil;
  M := Gerenciador;
  E := TEspera.Create;
  E.Fim := TEvent.Create;
  try
    M.OnDiscoveryEnd := E.Acabou;
    if not M.StartDiscovery(EsperaMs) then
      raise ESemRadio.Create(Tr('O Bluetooth não começou a procurar: confira se ele está ligado no ' + AQUI_PT + '.',
                                'Bluetooth did not start searching: check that it is turned on on this ' + AQUI_EN +
                                '.'));
    E.Fim.WaitFor(EsperaMs + 3000);
    M.OnDiscoveryEnd := nil;
    if M.LastDiscoveredDevices <> nil then
      for A in M.LastDiscoveredDevices do
      begin
        R.Nome := A.DeviceName;
        if R.Nome = '' then
          R.Nome := Tr('(sem nome)', '(no name)');
        R.Endereco := A.Address;
        R.Aparelho := A;
        Result := Result + [R];
      end;
  finally
    E.Fim.Free;
    E.Free;
  end;
end;

{ TTransporteBLE }

function Propriedades(P: TBluetoothPropertyFlags): string;
const
  NOMES: array[TBluetoothProperty] of string = ('broadcast', 'extended', 'notify', 'indicate', 'read', 'write',
                                                'write-no-response', 'signed-write');
var
  K: TBluetoothProperty;
begin
  Result := '';
  for K := Low(TBluetoothProperty) to High(TBluetoothProperty) do
    if K in P then
      Result := Result + ' ' + NOMES[K];
end;

// os serviços padrão do BLE (acesso genérico, atributos, informação do aparelho, bateria): nunca são a serial
function Padrao(const U: TBluetoothUUID): Boolean;
var
  S: string;
begin
  S := GUIDToString(U).ToUpper;
  Result := S.EndsWith('-0000-1000-8000-00805F9B34FB}') and
            (S.StartsWith('{00001800') or S.StartsWith('{00001801') or S.StartsWith('{0000180A') or
             S.StartsWith('{0000180F'));
end;

constructor TTransporteBLE.Create(Aparelho: TBluetoothLEDevice; Registra: TProc<string>);
var
  S: TBluetoothGattService;
  C, Escrita, Aviso: TBluetoothGattCharacteristic;
begin
  inherited Create;
  FTrava := TCriticalSection.Create;
  FSinal := TEvent.Create(nil, False, False, '');
  FAparelho := Aparelho;
  Registra(Format(Tr('Bluetooth: conectando em %s (%s)...', 'Bluetooth: connecting to %s (%s)...'),
                  [Aparelho.DeviceName, Aparelho.Address]));
  if not Aparelho.IsConnected then
    Aparelho.Connect;
  if not Aparelho.DiscoverServices or (Aparelho.Services = nil) or (Aparelho.Services.Count = 0) then
    raise ESemRadio.Create(Tr('O aparelho não mostrou nenhum serviço Bluetooth. Confira se o BLUETOOTH do rádio (menu ' +
                              'SYSTEM) está ON.',
                              'The device showed no Bluetooth service. Check that the radio''s BLUETOOTH (SYSTEM ' +
                              'menu) is ON.'));
  // tudo o que ele anuncia vai para o Registro: é o mapa para o próximo passo
  for S in Aparelho.Services do
  begin
    Registra(Format('  %s %s', [Tr('serviço', 'service'), GUIDToString(S.UUID)]));
    for C in S.Characteristics do
      Registra(Format('    %s%s', [GUIDToString(C.UUID), Propriedades(C.Properties)]));
  end;
  for S in Aparelho.Services do
    for C in S.Characteristics do
      if (C.UUID = FFE1) and (FEscrita = nil) then
      begin
        FEscrita := C;
        FAviso := C;
      end;
  for S in Aparelho.Services do
  begin
    if Padrao(S.UUID) or (FEscrita <> nil) then
      Continue;
    Escrita := nil;
    Aviso := nil;
    for C in S.Characteristics do
    begin
      if (Escrita = nil) and ((TBluetoothProperty.Write in C.Properties) or
                              (TBluetoothProperty.WriteNoResponse in C.Properties)) then
        Escrita := C;
      if (Aviso = nil) and ((TBluetoothProperty.Notify in C.Properties) or
                            (TBluetoothProperty.Indicate in C.Properties)) then
        Aviso := C;
    end;
    if (Escrita <> nil) and (Aviso <> nil) then
    begin
      FEscrita := Escrita;
      FAviso := Aviso;
      Break;
    end;
  end;
  if FEscrita = nil then
    raise EFalha.Create(Tr('O rádio não anuncia um canal serial por BLE (um serviço com escrita e notificação). O que ' +
                           'ele anuncia ficou no Registro.',
                           'The radio does not offer a BLE serial channel (a service with write and notify). What it ' +
                           'offers is in the Log.'));
  Registra(Format(Tr('Bluetooth: escrevendo em %s e ouvindo %s.', 'Bluetooth: writing to %s and listening to %s.'),
                  [GUIDToString(FEscrita.UUID), GUIDToString(FAviso.UUID)]));
  Aparelho.OnCharacteristicRead := Leu;
  if not Aparelho.SetCharacteristicNotification(FAviso, True) then
    raise EFalha.Create(Tr('O rádio não aceitou mandar as notificações.', 'The radio did not accept sending notifications.'));
end;

destructor TTransporteBLE.Destroy;
begin
  if FAparelho <> nil then
  begin
    FAparelho.OnCharacteristicRead := nil;
    if FAviso <> nil then
      FAparelho.SetCharacteristicNotification(FAviso, False);
    FAparelho.Disconnect;
  end;
  FSinal.Free;
  FTrava.Free;
  inherited;
end;

// a notificação chega numa thread do Bluetooth
procedure TTransporteBLE.Leu(const Sender: TObject; const C: TBluetoothGattCharacteristic; Status: TBluetoothGattStatus);
begin
  if (C <> FAviso) or (Status <> TBluetoothGattStatus.Success) then
    Exit;
  FTrava.Enter;
  try
    FChegou := FChegou + C.GetValue;
  finally
    FTrava.Leave;
  end;
  FSinal.SetEvent;
end;

procedure TTransporteBLE.Escreve(const B: TBytes);
var
  P: Integer;
begin
  P := 0;
  while P < Length(B) do
  begin
    FEscrita.SetValue(Copy(B, P, PEDACO));
    if not FAparelho.WriteCharacteristic(FEscrita) then
      raise EFalha.Create(Tr('O Bluetooth não conseguiu escrever no rádio.', 'Bluetooth could not write to the radio.'));
    Inc(P, PEDACO);
  end;
end;

function TTransporteBLE.Le: TBytes;
begin
  FTrava.Enter;
  try
    Result := FChegou;
    FChegou := nil;
  finally
    FTrava.Leave;
  end;
  if Result <> nil then
    Exit;
  FSinal.WaitFor(20);
  FTrava.Enter;
  try
    Result := FChegou;
    FChegou := nil;
  finally
    FTrava.Leave;
  end;
end;

procedure TTransporteBLE.LimpaEntrada;
begin
  FTrava.Enter;
  try
    FChegou := nil;
  finally
    FTrava.Leave;
  end;
end;

function TTransporteBLE.Agora: UInt64;
begin
  Result := TThread.GetTickCount64;
end;

procedure TTransporteBLE.Dorme(Ms: Cardinal);
begin
  TThread.Sleep(Ms);
end;

// Pelo BLE não há velocidade de serial: a de sempre, sem mudar nada.
function TTransporteBLE.Velocidade(Baud: Cardinal): Cardinal;
begin
  Result := 0;
end;

end.
