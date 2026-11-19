unit Idioma;

{ Os textos saem em português num sistema em português e em inglês nos outros: no Windows pela língua da interface,
  no Android pela do Locale do Java (o TLanguages do Delphi lá diz sempre en_US). }

interface

function Tr(const Pt, En: string): string;

implementation

uses
{$IFDEF MSWINDOWS}
  Winapi.Windows;
{$ENDIF}
{$IFDEF ANDROID}
  Androidapi.Helpers, Androidapi.JNI.JavaTypes;
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
{$ENDIF}
{$IFDEF ANDROID}
  Portugues := JStringToString(TJLocale.JavaClass.getDefault.getLanguage) = 'pt';
{$ENDIF}
end.
