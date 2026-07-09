# QmlMaterial ↔ Material 3 (inventory)

Базовый путь: `3rdparty/QmlMaterial/qml/`. Импорт: `import Qcm.Material as MD`.

Примеры: `3rdparty/QmlMaterial/example/Components.qml`, `Typography.qml`, `Color.qml`, `Effect.qml`.

## Реализовано (основное)

| M3 / паттерн | QmlMaterial | Примечание |
|--------------|-------------|------------|
| Buttons (filled, tonal, outlined, text, elevated) | `control/Button.qml`, `component/button/*` | `mdState.type`: `MD.Enum.Bt*` |
| Icon button | `component/button/IconButton.qml`, `StandardIconButton.qml`, `SmallIconButton.qml` | |
| FAB | `component/button/FAB.qml` | |
| Split button | `component/button/SplitButton.qml` | |
| Segmented button | `control/SegmentedButton.qml`, `SegmentedButtonGroup.qml` | |
| Checkbox | `control/CheckBox.qml` | |
| Radio | `control/RadioButton.qml` | |
| Switch | `control/Switch.qml` | |
| Chips (assist, filter, input, suggestion, embed) | `component/chip/*.qml` | |
| Text field | `component/TextField.qml`, `state/StateTextField.qml` | outline/filled shapes |
| Combo box | `control/ComboBox.qml` | |
| Search | `state/StateSearchBar.qml` | проверить обёртку в example |
| Card | `component/card/Card.qml`, `state/StateCard.qml` | |
| Dialog | `control/Dialog.qml` | токены shape/elevation в control |
| Menu | `control/Menu.qml`, `MenuItem.qml`, `MenuSeparator.qml` | |
| Tooltip | `control/ToolTip.qml`, `component/extra/RichToolTip.qml` | |
| Snackbar | `component/extra/SnakeView.qml` | имя «Snake» в коде |
| Tabs | `control/TabBar.qml`, `TabButton.qml` | |
| Navigation bar | `component/navigation/NavBar.qml` | |
| Navigation rail | `component/navigation/RailItem.qml` | |
| Drawer | `control/Drawer.qml`, `StandardDrawer.qml`, `DrawerItem.qml` | |
| App bar | `state/StateAppBar.qml` | |
| Lists | `flickable/ListView.qml`, `VerticalListView.qml`, `delegate/ListItem.qml` | |
| Table | `flickable/TableView.qml`, `control/TableViewDelegate.qml` | |
| Slider | `control/Slider.qml`, `SliderM2.qml` | |
| Progress (linear/circular) | `component/progressindicator/*` | |
| Busy | `control/BusyIndicator.qml` | |
| Divider | `component/extra/Divider.qml`, `AutoDivider.qml` | |
| Badge | `component/extra/Badge.qml` | |
| Page / window shell | `control/Page.qml`, `ApplicationWindow.qml`, `container/PageContainer.qml` | |
| Pane | `control/Pane.qml` | |
| Scrollbar | `control/ScrollBar.qml`, `ScrollIndicator.qml` | |
| Popup / stack | `control/Popup.qml`, `StackView.qml` | side sheet — оценить Popup+Drawer |
| Split view | `control/SplitView.qml` | layout, не M3-компонент |
| Date picker | `component/datepicker/*` | |
| Color picker | `component/colorpicker/*` | |
| Label / text | `control/Label.qml` + typescale на `MD.Text` | см. example Typography |
| Stepper / number | `component/extra/NumberStepper.qml`, `StepperInput.qml` | extra, не канон M3 |
| Motion | `motion/FadeInMotion.qml`, … | |
| Elevation / ripple / shape | `Token` (`elevation`, `shape`), `RippleSkia.qml` | |

Состояния (hover, pressed, disabled, focus): `qml/state/State*.qml` — использовать вместо ручных binding'ов.

## Часто отсутствует или частично

Сверять с [m3.material.io/components](https://m3.material.io/components) перед утверждением:

| M3 | Статус в QmlMaterial |
|----|----------------------|
| Carousel | Нет отдельного компонента |
| Bottom sheet (mobile) | Нет; для desktop — `Dialog` / `Drawer` / dock |
| Time picker | Нет (есть date picker) |
| Bottom app bar | Нет; desktop — nav rail / side bar |
| Extended FAB menu | Уточнять по `FAB` + меню |
| Banner (inline) | Часто `Card` / custom row |
| Data grid (rich) | `TableView` — базово |

При отсутствии: сообщить пользователю, предложить PR в QmlMaterial, в задаче — ближайший `MD.*` + соблюдение guidelines (без тяжёлой кастомной отрисовки).

## Токены (использовать вместо констант)

| Категория | Доступ в QML |
|-----------|----------------|
| Color roles | `MD.Token.color.*` (через `MdColorMgr`) |
| Typography | `MD.Token.typescale.*` (`.size`, `.weight`, `.tracking`) |
| Shape corners | `MD.Token.shape.corner.*` |
| Elevation | `MD.Token.elevation.level0` … `level5` |
| Icons | `MD.Token.icon.*` |
| Theme | `MD.Token.themeMode`, `isDarkTheme` |

Образец: `qml/control/Dialog.qml`, `qml/state/StateTextField.qml`, `example/Effect.qml` (shape scale).

## Обновление каталога

При сомнении выполнить поиск по библиотеке (из корня unicase):

```bash
find 3rdparty/QmlMaterial/qml -name '*.qml' | sort
```

Новые файлы в `qml/control`, `qml/component`, `qml/state` — дописать в таблицу выше.
