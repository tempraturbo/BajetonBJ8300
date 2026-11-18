unit Idioma;

{ Os textos saem em português num Windows em português e em inglês nos outros. }

interface

function Tr(const Pt, En: string): string;

implementation

uses
  Winapi.Windows;

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
  Portugues := GetUserDefaultUILanguage and $3FF = LANG_PORTUGUESE;
end.
