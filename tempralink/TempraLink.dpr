program TempraLink;

{ O Tempra Link do BJ-8300: lê, edita, guarda e grava o BJ-8300 com o firmware TEMPRA pelo cabo de programação, ou
  (experimental) pelo Bluetooth: os canais, os bancos, os ajustes do menu, os textos do APRS e do Morse e o logo de
  abertura, sem instalar nada. Compila com o compilar.bat, que gera o TempraLink-BJ8300.exe. }

{$R 'Recursos.res' 'Recursos.rc'}

uses
  Vcl.Forms,
  Idioma in 'Idioma.pas',
  Transporte in 'Transporte.pas',
  CaboBJ in 'CaboBJ.pas',
  Logo in 'Logo.pas',
  Ajustes in 'Ajustes.pas',
  BJ8300 in 'BJ8300.pas',
  Csv in 'Csv.pas',
  Visual in 'Visual.pas',
  PortaSerial in 'PortaSerial.pas',
  Bluetooth in 'Bluetooth.pas',
  Melodia in 'Melodia.pas',
  EditorRoger in 'EditorRoger.pas',
  GravadorBJ in 'GravadorBJ.pas',
  Atualizacao in 'Atualizacao.pas',
  Compatibilidade in 'Compatibilidade.pas',
  Principal in 'Principal.pas' {FormPrincipal};

begin
  Application.Initialize;
  LigaEstilo;
  Application.MainFormOnTaskbar := True;
  Application.Title := 'Tempra Link';
  Application.CreateForm(TFormPrincipal, FormPrincipal);
  Application.Run;
end.
