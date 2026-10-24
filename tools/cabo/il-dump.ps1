# Desmonta o IL de metodos de um assembly .NET sem executa-lo (contexto ReflectionOnly).
# Uso: il-dump.ps1 -Path <assembly> -Types <Tipo1,Tipo2> [-Methods <m1,m2>]
param([string]$Path, [string[]]$Types, [string[]]$Methods)
$ErrorActionPreference = 'Stop'

# Dependencias do framework resolvidas por nome, tambem em modo somente-leitura.
[AppDomain]::CurrentDomain.add_ReflectionOnlyAssemblyResolve({
  param($s, $e) [Reflection.Assembly]::ReflectionOnlyLoad([AppDomain]::CurrentDomain.ApplyPolicy($e.Name))
})
$asm = [Reflection.Assembly]::ReflectionOnlyLoadFrom($Path)
$mod = $asm.ManifestModule

# Tabela opcode -> OpCode (1 e 2 bytes)
$one = @{}; $two = @{}
foreach ($f in [Reflection.Emit.OpCodes].GetFields('Public,Static')) {
  $op = $f.GetValue($null)
  $v = [int]$op.Value -band 0xFFFF   # Value e Int16: os opcodes de 2 bytes (0xFExx) vem negativos
  if ($op.Size -eq 1) { $one[[int]$v] = $op } else { $two[[int]($v -band 0xFF)] = $op }
}

function Name-Of($m) {
  try { if ($m.DeclaringType) { "$($m.DeclaringType.Name)::$($m.Name)" } else { $m.Name } } catch { "<$m>" }
}

$flags = [Reflection.BindingFlags]'Public,NonPublic,Instance,Static,DeclaredOnly'
foreach ($tn in $Types) {
  $t = $asm.GetType($tn, $true)
  $members = @($t.GetConstructors($flags)) + @($t.GetMethods($flags))
  foreach ($m in $members) {
    if ($Methods -and ($Methods -notcontains $m.Name)) { continue }
    $body = $m.GetMethodBody()
    if (-not $body) { continue }
    $il = $body.GetILAsByteArray()
    "`n===== $($t.Name)::$($m.Name)($(($m.GetParameters() | ForEach-Object { "$($_.ParameterType.Name) $($_.Name)" }) -join ', ')) locals: $(($body.LocalVariables | ForEach-Object { $_.LocalType.Name }) -join ', ')"
    $i = 0
    while ($i -lt $il.Length) {
      $at = $i
      $b = $il[$i++]
      if ($b -eq 0xFE) { $op = $two[[int]$il[$i++]] } else { $op = $one[[int]$b] }
      $arg = ''
      switch ($op.OperandType.ToString()) {
        'InlineNone' { }
        'ShortInlineBrTarget' { $d = [sbyte]$(if ($il[$i] -gt 127) { $il[$i] - 256 } else { $il[$i] }); $i++; $arg = 'IL_{0:x4}' -f ($i + $d) }
        'ShortInlineI' { $arg = [string]$(if ($il[$i] -gt 127) { $il[$i] - 256 } else { $il[$i] }); $i++ }
        'ShortInlineVar' { $arg = [string]$il[$i]; $i++ }
        'InlineBrTarget' { $d = [BitConverter]::ToInt32($il, $i); $i += 4; $arg = 'IL_{0:x4}' -f ($i + $d) }
        'InlineI' { $v = [BitConverter]::ToInt32($il, $i); $i += 4; $arg = '{0} (0x{0:x})' -f $v }
        'InlineI8' { $arg = [BitConverter]::ToInt64($il, $i); $i += 8 }
        'ShortInlineR' { $arg = [BitConverter]::ToSingle($il, $i); $i += 4 }
        'InlineR' { $arg = [BitConverter]::ToDouble($il, $i); $i += 8 }
        'InlineVar' { $arg = [BitConverter]::ToUInt16($il, $i); $i += 2 }
        'InlineString' { $tok = [BitConverter]::ToInt32($il, $i); $i += 4; $arg = '"' + $mod.ResolveString($tok) + '"' }
        'InlineSwitch' { $n = [BitConverter]::ToInt32($il, $i); $i += 4; $base = $i + 4 * $n
          $arg = (0..($n - 1) | ForEach-Object { 'IL_{0:x4}' -f ($base + [BitConverter]::ToInt32($il, $i + 4 * $_)) }) -join ','; $i = $base }
        default {
          $tok = [BitConverter]::ToInt32($il, $i); $i += 4
          try {
            switch ($op.OperandType.ToString()) {
              'InlineMethod' { $arg = Name-Of ($mod.ResolveMethod($tok)) }
              'InlineField' { $arg = Name-Of ($mod.ResolveField($tok)) }
              'InlineType' { $arg = $mod.ResolveType($tok).FullName }
              default { $arg = Name-Of ($mod.ResolveMember($tok)) }
            }
          } catch { $arg = '0x{0:x8}' -f $tok }
        }
      }
      '  IL_{0:x4}: {1,-12} {2}' -f $at, $op.Name, $arg
    }
  }
}
