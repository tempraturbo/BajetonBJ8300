program TempraLinkCelular;

{ O Tempra Link do BJ-8300 no celular (Android): o controle remoto, os canais, os ajustes e os backups pelo Bluetooth
  do rádio, e a demonstração sem ele. Compila e instala pelo compilar.ps1 desta pasta (o msbuild do Delphi, sem a IDE);
  com -Windows, sai também para o PC (Win64), para testar as telas com o rádio simulado. }

uses
  System.StartUpCopy,
  FMX.Forms,
  FMX.Types,
  JanelaCelular in 'JanelaCelular.pas' {Janela},
  TelaConectar in 'TelaConectar.pas',
  TelaRadio in 'TelaRadio.pas',
  TemaCelular in 'TemaCelular.pas',
  ModeloRadio in 'ModeloRadio.pas',
  Demonstracao in 'Demonstracao.pas',
  ListaCelular in 'ListaCelular.pas',
  RadioCelular in 'RadioCelular.pas',
  RegistroCelular in 'RegistroCelular.pas',
  RadiosLembrados in 'RadiosLembrados.pas';

begin
  Application.Initialize;
  // a frente do rádio é de pé: deitada, as teclas não cabem
  Application.FormFactor.Orientations := [TFormOrientation.Portrait];
  Application.CreateForm(TJanela, Janela);
  Application.Run;
end.
