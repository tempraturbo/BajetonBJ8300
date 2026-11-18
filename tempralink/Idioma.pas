unit Idioma;

{ Os textos saem em português num sistema em português e em inglês nos outros: no Windows pela língua da interface,
  nos outros (o celular) pelo locale. }

interface

function Tr(const Pt, En: string): string;

implementation

uses
{$IFDEF MSWINDOWS}
  Winapi.Windows;
{$ELSE}
  System.SysUtils;
{$ENDIF}

var
  Portugues: Boolean;

function Tr(const Pt, En: string): string;
begin
  if Portugues then
    Result := Pt
  else
    Result := En;
end;

initialization
{$IFDEF MSWINDOWS}
  Portugues := GetUserDefaultUILanguage and $3FF = LANG_PORTUGUESE;
{$ELSE}
  Portugues := TLanguages.UserDefaultLocale.ToLower.StartsWith('pt');
{$ENDIF}
end.
