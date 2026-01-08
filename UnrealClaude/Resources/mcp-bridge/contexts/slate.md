# Unreal Engine 5.7 Slate UI Context

This context is automatically loaded when working with Slate widget or Editor UI tasks.

## Widget Creation Macros

### SNew - Create and Forget
Creates a widget without storing a reference.

```cpp
TSharedRef<SWidget> CreateMyWidget()
{
    return SNew(SVerticalBox)
        + SVerticalBox::Slot()
        .AutoHeight()
        .Padding(4.0f)
        [
            SNew(STextBlock)
            .Text(FText::FromString(TEXT("Hello World")))
        ];
}
```

### SAssignNew - Create and Store Reference
Creates a widget AND assigns it to a member variable for later access.

```cpp
TSharedPtr<SEditableTextBox> MyTextBox;

void CreateWidget()
{
    SAssignNew(MyTextBox, SEditableTextBox)
        .HintText(FText::FromString(TEXT("Enter text...")))
        .OnTextCommitted(this, &SMyWidget::OnTextCommitted);
}
```

## Layout Widgets

### SVerticalBox - Stack Vertically
```cpp
SNew(SVerticalBox)
    + SVerticalBox::Slot()
    .AutoHeight()           // Size to content
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("Top")))
    ]
    + SVerticalBox::Slot()
    .FillHeight(1.0f)       // Fill remaining space
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("Fill")))
    ]
```

### SHorizontalBox - Stack Horizontally
```cpp
SNew(SHorizontalBox)
    + SHorizontalBox::Slot()
    .AutoWidth()            // Size to content
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("Left")))
    ]
    + SHorizontalBox::Slot()
    .FillWidth(1.0f)        // Fill remaining
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("Fill")))
    ]
```

### SScrollBox - Scrollable Container
```cpp
SNew(SScrollBox)
    + SScrollBox::Slot()
    [
        SNew(STextBlock).Text(FText::FromString(TEXT("Scrollable content")))
    ]
```

### SSplitter - Resizable Split Panes
```cpp
SNew(SSplitter)
    .Orientation(Orient_Horizontal)
    + SSplitter::Slot()
    .Value(0.3f)            // 30% width
    [
        LeftPanel
    ]
    + SSplitter::Slot()
    .Value(0.7f)            // 70% width
    [
        RightPanel
    ]
```

## Common Widgets

### STextBlock - Display Text
```cpp
SNew(STextBlock)
    .Text(FText::FromString(TEXT("Static text")))
    .Font(FCoreStyle::GetDefaultFontStyle("Bold", 14))
    .ColorAndOpacity(FSlateColor(FLinearColor::White))
```

### SEditableTextBox - Text Input
```cpp
SNew(SEditableTextBox)
    .Text(FText::FromString(TEXT("Editable")))
    .HintText(FText::FromString(TEXT("Placeholder...")))
    .OnTextCommitted(this, &SMyWidget::OnTextCommitted)
```

### SButton - Clickable Button
```cpp
SNew(SButton)
    .OnClicked(this, &SMyWidget::OnButtonClicked)
    [
        SNew(STextBlock)
        .Text(FText::FromString(TEXT("Click Me")))
    ]

// Handler
FReply SMyWidget::OnButtonClicked()
{
    // Do something
    return FReply::Handled();
}
```

### SCheckBox - Toggle Checkbox
```cpp
SNew(SCheckBox)
    .IsChecked(ECheckBoxState::Checked)
    .OnCheckStateChanged(this, &SMyWidget::OnCheckChanged)
```

### SComboBox - Dropdown Selection
```cpp
SNew(SComboBox<TSharedPtr<FString>>)
    .OptionsSource(&Options)
    .OnSelectionChanged(this, &SMyWidget::OnSelectionChanged)
    .OnGenerateWidget(this, &SMyWidget::GenerateComboItem)
    [
        SNew(STextBlock).Text(this, &SMyWidget::GetSelectedText)
    ]
```

### SImage - Display Image
```cpp
SNew(SImage)
    .Image(FAppStyle::GetBrush("Icons.Warning"))
    .DesiredSizeOverride(FVector2D(32, 32))
```

## Slot Properties

| Property | Description |
|----------|-------------|
| `.AutoHeight()` / `.AutoWidth()` | Size to fit content |
| `.FillHeight(1.0f)` / `.FillWidth(1.0f)` | Fill available space |
| `.MaxHeight(100)` / `.MaxWidth(100)` | Set maximum size |
| `.Padding(FMargin(4))` | Add spacing around content |
| `.VAlign(VAlign_Center)` | Vertical alignment |
| `.HAlign(HAlign_Fill)` | Horizontal alignment |

## Alignment Values

```cpp
// Vertical: VAlign_Top, VAlign_Center, VAlign_Bottom, VAlign_Fill
// Horizontal: HAlign_Left, HAlign_Center, HAlign_Right, HAlign_Fill
```

## Creating Custom Widgets

### SLATE_BEGIN_ARGS Pattern
```cpp
class SMyWidget : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SMyWidget)
        : _Text(FText::GetEmpty())
        , _OnClicked()
    {}
        SLATE_ARGUMENT(FText, Text)           // Required argument
        SLATE_ATTRIBUTE(FText, DynamicText)   // Bindable attribute
        SLATE_EVENT(FOnClicked, OnClicked)    // Event delegate
    SLATE_END_ARGS()

    void Construct(const FArguments& InArgs);

private:
    FText Text;
    TAttribute<FText> DynamicText;
    FOnClicked OnClicked;
};

void SMyWidget::Construct(const FArguments& InArgs)
{
    Text = InArgs._Text;
    DynamicText = InArgs._DynamicText;
    OnClicked = InArgs._OnClicked;

    ChildSlot
    [
        SNew(SButton)
        .OnClicked(OnClicked)
        [
            SNew(STextBlock)
            .Text(DynamicText)  // Binds to attribute
        ]
    ];
}
```

## Editor Tabs and Windows

### Register Editor Tab
```cpp
FGlobalTabmanager::Get()->RegisterNomadTabSpawner(
    TabName,
    FOnSpawnTab::CreateRaw(this, &FMyModule::SpawnTab))
    .SetDisplayName(FText::FromString(TEXT("My Tab")))
    .SetMenuType(ETabSpawnerMenuType::Hidden);

TSharedRef<SDockTab> FMyModule::SpawnTab(const FSpawnTabArgs& Args)
{
    return SNew(SDockTab)
        .TabRole(ETabRole::NomadTab)
        [
            SNew(SMyWidget)
        ];
}
```

### Invoke Tab
```cpp
FGlobalTabmanager::Get()->TryInvokeTab(TabName);
```

## Style and Theming

### Using FAppStyle (UE 5.x)
```cpp
// Get standard brush
const FSlateBrush* Brush = FAppStyle::GetBrush("Icons.Error");

// Get standard color
FSlateColor Color = FAppStyle::GetSlateColor("Colors.AccentBlue");

// Apply widget style
SNew(SButton)
    .ButtonStyle(FAppStyle::Get(), "SimpleButton")
```

## Best Practices

1. **Use SAssignNew for widgets you need to reference later**
2. **Prefer TAttribute for dynamic values** - enables automatic refresh
3. **Use ChildSlot for single-child widgets** - SCompoundWidget pattern
4. **Clean up tab spawners in module shutdown**
5. **Use FAppStyle for consistent editor styling** (replaces FEditorStyle in UE5)
