object FormPrincipal: TFormPrincipal
  Left = 0
  Top = 0
  Caption = 'Tempra Link'
  ClientHeight = 720
  ClientWidth = 1180
  Color = clWindow
  Constraints.MinHeight = 640
  Constraints.MinWidth = 1080
  Font.Charset = DEFAULT_CHARSET
  Font.Color = clWindowText
  Font.Height = -12
  Font.Name = 'Segoe UI'
  Font.Style = []
  KeyPreview = True
  Position = poScreenCenter
  OnCloseQuery = FormCloseQuery
  OnCreate = FormCreate
  OnDestroy = FormDestroy
  OnKeyDown = FormKeyDown
  PixelsPerInch = 96
  TextHeight = 15
  object PainelNav: TPanel
    Left = 0
    Top = 0
    Width = 200
    Height = 720
    Align = alLeft
    BevelOuter = bvNone
    Color = 856074
    ParentBackground = False
    TabOrder = 0
    object PaintMarca: TPaintBox
      Left = 18
      Top = 20
      Width = 42
      Height = 42
      OnPaint = PaintMarcaPaint
    end
    object LabelMarca: TLabel
      Left = 68
      Top = 20
      Width = 112
      Height = 25
      Caption = 'Tempra Link'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = 10081076
      Font.Height = -19
      Font.Name = 'Segoe UI Semibold'
      Font.Style = []
      ParentFont = False
    end
    object LabelModelo: TLabel
      Left = 68
      Top = 46
      Width = 86
      Height = 15
      Caption = 'Baofeng UV-17L'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = 11781283
      Font.Height = -12
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentFont = False
    end
    object LabelVersao: TLabel
      Left = 20
      Top = 690
      Width = 52
      Height = 15
      Anchors = [akLeft, akBottom]
      Caption = 'versao 1.0'
      Font.Charset = DEFAULT_CHARSET
      Font.Color = 11781283
      Font.Height = -12
      Font.Name = 'Segoe UI'
      Font.Style = []
      ParentFont = False
    end
    object ListaNav: TListBox
      Left = 0
      Top = 92
      Width = 200
      Height = 580
      Style = lbOwnerDrawFixed
      Anchors = [akLeft, akTop, akRight, akBottom]
      BorderStyle = bsNone
      Color = 856074
      Font.Charset = DEFAULT_CHARSET
      Font.Color = 11781283
      Font.Height = -13
      Font.Name = 'Segoe UI'
      Font.Style = []
      ItemHeight = 40
      ParentFont = False
      TabOrder = 0
      OnClick = ListaNavClick
      OnDrawItem = ListaNavDrawItem
    end
  end
  object PainelConteudo: TPanel
    Left = 200
    Top = 0
    Width = 980
    Height = 720
    Align = alClient
    BevelOuter = bvNone
    Color = clWindow
    ParentBackground = False
    TabOrder = 1
    object PainelTopo: TPanel
      Left = 0
      Top = 0
      Width = 980
      Height = 72
      Align = alTop
      BevelOuter = bvNone
      Color = clWindow
      ParentBackground = False
      TabOrder = 0
      object LabelTitulo: TLabel
        Left = 24
        Top = 10
        Width = 63
        Height = 28
        Caption = 'Canais'
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clWindowText
        Font.Height = -21
        Font.Name = 'Segoe UI Semibold'
        Font.Style = []
        ParentFont = False
      end
      object LabelRadio: TLabel
        Left = 24
        Top = 46
        Width = 932
        Height = 15
        Anchors = [akLeft, akTop, akRight]
        AutoSize = False
        Caption = 'Nenhum radio lido'
        EllipsisPosition = epEndEllipsis
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clGrayText
        Font.Height = -12
        Font.Name = 'Segoe UI'
        Font.Style = []
        ParentFont = False
      end
      object ComboPorta: TComboBox
        Left = 264
        Top = 15
        Width = 220
        Height = 23
        Style = csDropDownList
        Anchors = [akTop, akRight]
        TabOrder = 0
        OnChange = ComboPortaChange
      end
      object BotaoAtualizar: TButton
        Left = 490
        Top = 14
        Width = 80
        Height = 25
        Anchors = [akTop, akRight]
        Caption = 'Atualizar'
        TabOrder = 1
        OnClick = BotaoAtualizarClick
      end
      object BotaoDesfazer: TButton
        Left = 586
        Top = 10
        Width = 90
        Height = 32
        Anchors = [akTop, akRight]
        Caption = 'Desfazer'
        TabOrder = 2
        OnClick = BotaoDesfazerClick
      end
      object BotaoLer: TButton
        Left = 684
        Top = 10
        Width = 120
        Height = 32
        Anchors = [akTop, akRight]
        Caption = 'Ler do radio'
        TabOrder = 3
        OnClick = BotaoLerClick
      end
      object BotaoGravarRadio: TButton
        Left = 812
        Top = 10
        Width = 144
        Height = 32
        Anchors = [akTop, akRight]
        Caption = 'Gravar no radio'
        TabOrder = 4
        OnClick = BotaoGravarRadioClick
      end
    end
    object PainelBase: TPanel
      Left = 0
      Top = 680
      Width = 980
      Height = 40
      Align = alBottom
      BevelOuter = bvNone
      Color = 856074
      ParentBackground = False
      TabOrder = 2
      object LabelEstado: TLabel
        Left = 24
        Top = 12
        Width = 570
        Height = 15
        Anchors = [akLeft, akTop, akRight]
        AutoSize = False
        EllipsisPosition = epEndEllipsis
        Font.Charset = DEFAULT_CHARSET
        Font.Color = clGrayText
        Font.Height = -12
        Font.Name = 'Segoe UI'
        Font.Style = []
        ParentFont = False
      end
      object Barra: TProgressBar
        Left = 606
        Top = 12
        Width = 240
        Height = 16
        Anchors = [akTop, akRight]
        TabOrder = 0
        Visible = False
      end
      object BotaoCancelar: TButton
        Left = 856
        Top = 7
        Width = 100
        Height = 26
        Anchors = [akTop, akRight]
        Caption = 'Cancelar'
        TabOrder = 1
        Visible = False
        OnClick = BotaoCancelarClick
      end
    end
    object Cartoes: TCardPanel
      Left = 0
      Top = 72
      Width = 980
      Height = 608
      Align = alClient
      ActiveCard = CartaoCanais
      BevelOuter = bvNone
      Caption = 'Cartoes'
      Color = clWindow
      ParentBackground = False
      TabOrder = 1
      object CartaoCanais: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoCanais'
        CardIndex = 0
        TabOrder = 0
        object LabelVazio: TLabel
          AlignWithMargins = True
          Left = 80
          Top = 3
          Width = 820
          Height = 602
          Margins.Left = 80
          Margins.Right = 80
          Align = alClient
          Alignment = taCenter
          Caption = 'Nenhum radio lido ainda.'
          Font.Charset = DEFAULT_CHARSET
          Font.Color = clGrayText
          Font.Height = -15
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
          Layout = tlCenter
          WordWrap = True
        end
        object PainelFiltro: TPanel
          Left = 0
          Top = 0
          Width = 980
          Height = 48
          Align = alTop
          BevelOuter = bvNone
          Color = clWindow
          ParentBackground = False
          TabOrder = 0
          object LabelContagem: TLabel
            Left = 776
            Top = 15
            Width = 180
            Height = 15
            Alignment = taRightJustify
            Anchors = [akTop, akRight]
            AutoSize = False
            Font.Charset = DEFAULT_CHARSET
            Font.Color = clGrayText
            Font.Height = -12
            Font.Name = 'Segoe UI'
            Font.Style = []
            ParentFont = False
          end
          object Busca: TSearchBox
            Left = 24
            Top = 12
            Width = 230
            Height = 23
            TabOrder = 0
            OnChange = BuscaChange
          end
          object CheckVazios: TCheckBox
            Left = 266
            Top = 15
            Width = 130
            Height = 17
            Caption = 'Mostrar os vazios'
            TabOrder = 1
            OnClick = BuscaChange
          end
          object BotaoNovoCanal: TButton
            Left = 404
            Top = 10
            Width = 96
            Height = 28
            Caption = 'Novo canal'
            TabOrder = 2
            OnClick = BotaoNovoCanalClick
          end
          object BotaoApagarCanal: TButton
            Left = 506
            Top = 10
            Width = 76
            Height = 28
            Caption = 'Apagar'
            TabOrder = 3
            OnClick = BotaoApagarCanalClick
          end
          object BotaoImportarCsv: TButton
            Left = 588
            Top = 10
            Width = 106
            Height = 28
            Caption = 'Importar CSV...'
            TabOrder = 4
            OnClick = BotaoImportarCsvClick
          end
          object BotaoExportarCsv: TButton
            Left = 700
            Top = 10
            Width = 106
            Height = 28
            Caption = 'Exportar CSV...'
            TabOrder = 5
            OnClick = BotaoExportarCsvClick
          end
        end
        object Grade: TStringGrid
          AlignWithMargins = True
          Left = 24
          Top = 48
          Width = 932
          Height = 552
          Margins.Left = 24
          Margins.Top = 0
          Margins.Right = 24
          Margins.Bottom = 8
          Align = alClient
          BorderStyle = bsNone
          ColCount = 16
          DefaultDrawing = False
          DefaultRowHeight = 26
          FixedCols = 0
          RowCount = 2
          GridLineWidth = 0
          Options = [goFixedVertLine, goFixedHorzLine, goRangeSelect, goThumbTracking, goFixedRowDefAlign]
          PopupMenu = MenuCanais
          TabOrder = 1
          OnDblClick = GradeDblClick
          OnSelectCell = GradeSelectCell
          OnDrawCell = GradeDrawCell
          OnKeyDown = GradeKeyDown
          OnKeyPress = GradeKeyPress
        end
      end
      object CartaoBancos: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoBancos'
        CardIndex = 1
        TabOrder = 1
        object LabelBancosNota: TLabel
          Left = 420
          Top = 16
          Width = 440
          Height = 96
          AutoSize = False
          Font.Charset = DEFAULT_CHARSET
          Font.Color = clGrayText
          Font.Height = -12
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
          WordWrap = True
        end
        object GradeBancos: TStringGrid
          Left = 24
          Top = 16
          Width = 372
          Height = 446
          BorderStyle = bsNone
          ColCount = 4
          DefaultDrawing = False
          DefaultRowHeight = 26
          FixedCols = 0
          RowCount = 17
          GridLineWidth = 0
          Options = [goFixedVertLine, goFixedHorzLine, goThumbTracking, goFixedRowDefAlign]
          ScrollBars = ssNone
          TabOrder = 0
          OnDblClick = GradeDblClick
          OnSelectCell = GradeSelectCell
          OnDrawCell = GradeDrawCell
          OnKeyDown = GradeKeyDown
          OnKeyPress = GradeKeyPress
        end
      end
      object CartaoAjustes: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoAjustes'
        CardIndex = 2
        TabOrder = 2
        object PainelPastas: TPanel
          Left = 0
          Top = 0
          Width = 230
          Height = 608
          Align = alLeft
          BevelOuter = bvNone
          Color = clWindow
          ParentBackground = False
          TabOrder = 0
          object BuscaAjuste: TSearchBox
            AlignWithMargins = True
            Left = 24
            Top = 12
            Width = 198
            Height = 23
            Margins.Left = 24
            Margins.Top = 12
            Margins.Right = 8
            Margins.Bottom = 8
            Align = alTop
            TabOrder = 0
            OnChange = BuscaAjusteChange
          end
          object ListaPastas: TListBox
            AlignWithMargins = True
            Left = 24
            Top = 43
            Width = 198
            Height = 557
            Margins.Left = 24
            Margins.Top = 0
            Margins.Right = 8
            Margins.Bottom = 8
            Align = alClient
            BorderStyle = bsNone
            Style = lbOwnerDrawFixed
            ItemHeight = 30
            TabOrder = 1
            OnClick = ListaPastasClick
            OnDrawItem = ListaPastasDrawItem
          end
        end
        object PainelValores: TPanel
          Left = 230
          Top = 0
          Width = 750
          Height = 608
          Align = alClient
          BevelOuter = bvNone
          Color = clWindow
          ParentBackground = False
          TabOrder = 1
          object GradeAjustes: TStringGrid
            AlignWithMargins = True
            Left = 16
            Top = 12
            Width = 710
            Height = 368
            Margins.Left = 16
            Margins.Top = 12
            Margins.Right = 24
            Margins.Bottom = 8
            Align = alClient
            BorderStyle = bsNone
            ColCount = 2
            DefaultDrawing = False
            DefaultRowHeight = 26
            FixedCols = 0
            RowCount = 2
            GridLineWidth = 0
            Options = [goFixedVertLine, goFixedHorzLine, goThumbTracking, goFixedRowDefAlign]
            TabOrder = 1
            OnDblClick = GradeDblClick
            OnSelectCell = GradeSelectCell
            OnDrawCell = GradeDrawCell
            OnKeyDown = GradeKeyDown
            OnKeyPress = GradeKeyPress
          end
        end
      end
      object CartaoLogo: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoLogo'
        CardIndex = 3
        TabOrder = 3
        object LabelLogoInfo: TLabel
          Left = 360
          Top = 106
          Width = 300
          Height = 48
          AutoSize = False
          WordWrap = True
        end
        object LabelLogoPassos: TLabel
          Left = 24
          Top = 312
          Width = 612
          Height = 64
          AutoSize = False
          Font.Charset = DEFAULT_CHARSET
          Font.Color = clGrayText
          Font.Height = -12
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
          WordWrap = True
        end
        object PainelPrevia: TPanel
          Left = 24
          Top = 16
          Width = 324
          Height = 280
          BevelOuter = bvNone
          Color = clBlack
          ParentBackground = False
          TabOrder = 6
          object Previa: TImage
            Left = 2
            Top = 2
            Width = 320
            Height = 276
            Stretch = True
          end
        end
        object BotaoImagem: TButton
          Left = 360
          Top = 16
          Width = 240
          Height = 30
          Caption = 'Escolher imagem...'
          TabOrder = 0
          OnClick = BotaoImagemClick
        end
        object RadioPreencher: TRadioButton
          Left = 360
          Top = 56
          Width = 300
          Height = 19
          Caption = 'Preencher a tela'
          Checked = True
          TabOrder = 1
          TabStop = True
          OnClick = RadioAjusteClick
        end
        object RadioInteira: TRadioButton
          Left = 360
          Top = 78
          Width = 300
          Height = 19
          Caption = 'Mostrar inteira'
          TabOrder = 2
          OnClick = RadioAjusteClick
        end
        object BotaoLerLogo: TButton
          Left = 360
          Top = 164
          Width = 240
          Height = 30
          Caption = 'Ler do radio'
          TabOrder = 3
          OnClick = BotaoLerLogoClick
        end
        object BotaoSalvarLogo: TButton
          Left = 360
          Top = 200
          Width = 240
          Height = 30
          Caption = 'Salvar como PNG...'
          TabOrder = 4
          OnClick = BotaoSalvarLogoClick
        end
        object BotaoGravarLogo: TButton
          Left = 360
          Top = 244
          Width = 240
          Height = 32
          Caption = 'Gravar no radio'
          TabOrder = 5
          OnClick = BotaoGravarLogoClick
        end
      end
      object CartaoControle: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoControle'
        CardIndex = 4
        TabOrder = 4
        object LabelRemoto: TLabel
          Left = 216
          Top = 421
          Width = 252
          Height = 20
          AutoSize = False
          Font.Charset = DEFAULT_CHARSET
          Font.Color = clWindowText
          Font.Height = -13
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
        end
        object LabelControleNota: TLabel
          Left = 24
          Top = 460
          Width = 444
          Height = 128
          AutoSize = False
          Font.Charset = DEFAULT_CHARSET
          Font.Color = clGrayText
          Font.Height = -12
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
          WordWrap = True
        end
        object LabelAvisoCabo: TLabel
          Left = 492
          Top = 398
          Width = 312
          Height = 80
          AutoSize = False
          Font.Charset = DEFAULT_CHARSET
          Font.Color = 2408443
          Font.Height = -12
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
          WordWrap = True
        end
        object PainelVisor: TPanel
          Left = 24
          Top = 16
          Width = 444
          Height = 384
          BevelOuter = bvNone
          Color = 856074
          ParentBackground = False
          TabOrder = 0
          object PaintVisor: TPaintBox
            Left = 0
            Top = 0
            Width = 444
            Height = 384
            Align = alClient
            OnPaint = PaintVisorPaint
          end
        end
        object BotaoConectar: TButton
          Left = 24
          Top = 412
          Width = 180
          Height = 36
          Caption = 'Conectar'
          TabOrder = 1
          OnClick = BotaoConectarClick
        end
        object PainelTeclado: TPanel
          Left = 492
          Top = 16
          Width = 312
          Height = 370
          BevelOuter = bvNone
          Color = clWindow
          ParentBackground = False
          TabOrder = 2
        end
      end
      object CartaoFirmware: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoFirmware'
        CardIndex = 5
        TabOrder = 5
        object LabelInfo: TLabel
          Left = 24
          Top = 16
          Width = 680
          Height = 40
          AutoSize = False
          WordWrap = True
        end
        object LabelPassos: TLabel
          Left = 24
          Top = 64
          Width = 680
          Height = 320
          AutoSize = False
          Font.Charset = DEFAULT_CHARSET
          Font.Color = clGrayText
          Font.Height = -12
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
          WordWrap = True
        end
      end
      object CartaoBackup: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoBackup'
        CardIndex = 6
        TabOrder = 6
        object LabelBackup: TLabel
          Left = 24
          Top = 16
          Width = 680
          Height = 40
          AutoSize = False
          WordWrap = True
        end
        object LabelBackupNota: TLabel
          Left = 24
          Top = 116
          Width = 680
          Height = 64
          AutoSize = False
          Font.Charset = DEFAULT_CHARSET
          Font.Color = clGrayText
          Font.Height = -12
          Font.Name = 'Segoe UI'
          Font.Style = []
          ParentFont = False
          WordWrap = True
        end
        object BotaoAbrirBackup: TButton
          Left = 24
          Top = 68
          Width = 160
          Height = 33
          Caption = 'Abrir backup...'
          TabOrder = 0
          OnClick = BotaoAbrirBackupClick
        end
        object BotaoSalvarBackup: TButton
          Left = 194
          Top = 68
          Width = 160
          Height = 33
          Caption = 'Salvar backup...'
          TabOrder = 1
          OnClick = BotaoSalvarBackupClick
        end
        object BotaoPastaBackups: TButton
          Left = 364
          Top = 68
          Width = 200
          Height = 33
          Caption = 'Abrir a pasta dos backups'
          TabOrder = 2
          OnClick = BotaoPastaBackupsClick
        end
      end
      object CartaoRegistro: TCard
        Left = 0
        Top = 0
        Width = 980
        Height = 608
        Caption = 'CartaoRegistro'
        CardIndex = 7
        TabOrder = 7
        object MemoRegistro: TMemo
          AlignWithMargins = True
          Left = 24
          Top = 8
          Width = 932
          Height = 592
          Margins.Left = 24
          Margins.Top = 8
          Margins.Right = 24
          Margins.Bottom = 8
          Align = alClient
          ReadOnly = True
          ScrollBars = ssVertical
          TabOrder = 0
        end
      end
    end
  end
  object DialogoImagem: TOpenDialog
    Options = [ofHideReadOnly, ofFileMustExist, ofEnableSizing]
    Left = 1060
    Top = 560
  end
  object DialogoSalvar: TSaveDialog
    DefaultExt = 'png'
    FileName = 'logo-uv17l.png'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofEnableSizing]
    Left = 1100
    Top = 560
  end
  object DialogoBackup: TOpenDialog
    DefaultExt = 'tempra'
    Options = [ofHideReadOnly, ofFileMustExist, ofEnableSizing]
    Left = 1020
    Top = 600
  end
  object DialogoSalvarBackup: TSaveDialog
    DefaultExt = 'tempra'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofEnableSizing]
    Left = 1060
    Top = 600
  end
  object DialogoCsv: TOpenDialog
    DefaultExt = 'csv'
    Options = [ofHideReadOnly, ofFileMustExist, ofEnableSizing]
    Left = 1100
    Top = 600
  end
  object DialogoSalvarCsv: TSaveDialog
    DefaultExt = 'csv'
    Options = [ofOverwritePrompt, ofHideReadOnly, ofEnableSizing]
    Left = 1140
    Top = 600
  end
  object MenuCanais: TPopupMenu
    Left = 1140
    Top = 560
    object ItemCopiar: TMenuItem
      Caption = 'Copiar'
      OnClick = ItemCopiarClick
    end
    object ItemColar: TMenuItem
      Caption = 'Colar'
      OnClick = ItemColarClick
    end
    object ItemApagar: TMenuItem
      Caption = 'Apagar'
    end
  end
end
