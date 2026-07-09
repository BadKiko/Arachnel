---
name: material3-qml-ui
description: Recommends Material Design 3 components, layout, tokens, and QmlMaterial implementations for Qt/QML desktop UI. Use when choosing widgets, adapting UI to M3, reviewing QML for Material compliance, or when the user mentions Material 3, M3 Expressive, QmlMaterial, design tokens, scaffold, or bento layout.
---

# Material 3 + QmlMaterial (Qt desktop)

## Scope

Помогает выбрать компонент и организацию UI по [Material Design 3](https://m3.material.io/get-started), с реализацией на **Qt Quick + QmlMaterial** (`3rdparty/QmlMaterial`) и докированием через **kddockwidgets** (`3rdparty/kddockwidgets`).

Корень путей — **репозиторий unicase** (не outer `unicase_ws`). В workspace unicase обычно лежит в `src/unicase/`.

По умолчанию все новые и существующие виджеты оформляются через QmlMaterial. Примеры — `3rdparty/QmlMaterial/example/` (главный каталог: `Components.qml`).

**Цель кода:** минимум лишнего — без одноразовых `property` под одну задачу, без раздувания пиксельными смещениями. Лаконично и понятно. Следовать форматированию C++/QML проекта (`.clang-format`, `.qmlformat.ini` в корне unicase и в QmlMaterial).

## Когда применять skill

- Выбор компонента M3 для фичи
- Адаптация виджета под Material 3 / M3 Expressive
- Ревью QML на соответствие гайдам и токенам
- Вопросы про layout, scaffold, навигацию, уведомления, UX-текст

## Рабочий процесс (обязательный)

### 1. Понять задачу

Зафиксировать: цель экрана, первичное действие, частота использования, плотность данных, нужна ли модальность, контекст (настройки, рабочая область, диалог, панель дока).

**Контекст платформы:** десктоп (мышь/клавиатура), сенсор — редко. Ориентиры layout — desktop из [grids & spacing](https://m3.material.io/foundations/layout/grids-spacing/overview), не mobile-first touch targets.

**Визуальный язык:** [M3 Expressive](https://m3.material.io/blog/building-with-m3-expressive?utm_source=GD&utm_medium=referral&utm_campaign=IO25) в рамках того, что поддерживает QmlMaterial.

### 2. Сверка с Material 3 (все релевантные компоненты)

Перед рекомендацией **пробежать по каталогу** [Components](https://m3.material.io/components): для задачи сравнить несколько кандидатов, не останавливаться на первом знакомом.

Для каждого финального кандидата открыть (или опереться на знание) три слоя страницы компонента:

| Слой | Зачем |
|------|--------|
| Overview | Для чего компонент |
| Specs | Размеры, токены, состояния |
| **Guidelines** | **Как надо и как НЕ надо** — приоритет при споре |

Дополнительно по теме:

- [UX writing](https://m3.material.io/foundations/content-design/style-guide/ux-writing-best-practices)
- [Notifications](https://m3.material.io/foundations/content-design/notifications)
- [Customization](https://m3.material.io/foundations/customization), [Design tokens](https://m3.material.io/foundations/design-tokens/overview)
- [Interaction states](https://m3.material.io/foundations/interaction/states/overview)
- [Layout overview](https://m3.material.io/foundations/layout/layout-overview/overview), [Scaffold](https://m3.material.io/foundations/layout/scaffold/overview), [Grids & spacing](https://m3.material.io/foundations/layout/grids-spacing/overview)
- [Styles](https://m3.material.io/styles) (color, type, shape, elevation)
- [Material blog](https://m3.material.io/blog) — тренды и паттерны
- [Compose Material 3 (ru)](https://developer.android.com/develop/ui/compose/designsystems/material3?hl=ru) — идеи API/композиции, не копировать код 1:1

**Scaffold / навигация:** учитывать [scaffold](https://m3.material.io/foundations/layout/scaffold/overview) (где app bar, nav, FAB, body). Продукт ориентируется на **bento**-композицию панелей; навигация часто через боковую панель / rail / tabs / меню — не дублировать каналы без нужды.

### 3. Проверка QmlMaterial

1. Найти реализацию в [qmlmaterial-catalog.md](qmlmaterial-catalog.md) или `3rdparty/QmlMaterial/qml/**`.
2. Показать ближайший пример из `example/*.qml` (часто `Components.qml`).
3. Если компонента **нет** — явно сообщить; предложить ближайшую замену из библиотеки и отметить, что **новый компонент в QmlMaterial — хороший PR вне текущей задачи**.

### 4. Реализация в QML

```qml
import Qcm.Material as MD
```

**Токены (обязательно):** цвета `MD.Token.color.*`, типографика `MD.Token.typescale.*`, форма `MD.Token.shape.corner.*`, elevation `MD.Token.elevation.*`, иконки `MD.Token.icon.*`. Состояния — через `mdState` / `State*.qml`, не дублировать логику hover/pressed/disabled вручную.

**Избегать:**

- `color: "#..."`, фиксированные `font.pixelSize`, `radius`, `anchors.margins` без опоры на токены/типичные шаги сетки
- Лишние обёртки и одноразовые alias-property
- Копирование визуала «с нуля», если есть `MD.*` control

**Предпочитать:**

- Готовые `MD.Button`, `MD.TextField`, `MD.Card`, layout из `ColumnLayout`/`RowLayout` с `spacing` из сетки (8dp-кратности по M3)
- Паттерны из `example/Components.qml` и соседних example-файлов
- `MD.Pane`, `MD.Page`, navigation-компоненты для оболочки

**kddockwidgets:** зоны докирования — не замена M3-компонентов; внутри доков — QmlMaterial. Не смешивать «сырой» Qt Quick Controls стиль с MD в одной панели без причины.

### 5. Формат ответа пользователю

```markdown
## Рекомендация
**Компонент M3:** … (ссылка на m3.material.io/components/…)
**Почему:** …
**Guidelines (do / don't):** …

## QmlMaterial
**Тип:** `MD.…` (путь к qml)
**Пример:** `example/…` или фрагмент из проекта
**Отсутствует в библиотеке:** да/нет → …

## Layout / scaffold
… (desktop, bento, nav)

## Черновик (если уместно)
[краткий QML, только существенное]
```

## Быстрые правила выбора (шпаргалка)

| Задача | Частый выбор M3 | QmlMaterial |
|--------|-----------------|-------------|
| Подтверждение / форма | Dialog | `MD.Dialog` |
| Краткая обратная связь | Snackbar | `MD.SnakeView` |
| Настройки, редкие действия | Menu | `MD.Menu` |
| Основная навигация разделов | Tabs / Navigation rail | `MD.TabBar`, `MD.RailItem`, drawer |
| Список с действиями | List | `MD.ListItem`, `MD.VerticalListView` |
| Выбор из многих | Menu / autocomplete | `MD.ComboBox`, `MD.TextField` |
| Вкл/выкл | Switch | `MD.Switch` |
| Один из многих (мало опций) | Radio / segmented | `MD.RadioButton`, `MD.SegmentedButton` |
| Много опций, фильтр | Filter chips / search | `MD.FilterChip`, `MD.SearchBar` |
| Прогресс | Linear / circular indicator | `MD.LinearIndicator`, `MD.CircularIndicator` |

Полная таблица соответствий — [qmlmaterial-catalog.md](qmlmaterial-catalog.md).

## Дополнительные ресурсы

- Детальные ссылки и чеклист токенов: [reference.md](reference.md)
- Полный инвентарь QML-файлов библиотеки: [qmlmaterial-catalog.md](qmlmaterial-catalog.md)
