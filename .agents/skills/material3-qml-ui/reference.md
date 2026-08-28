# Material 3 — ссылки и чеклисты

## Официальная база (приоритет)

| Тема | URL |
|------|-----|
| Старт | https://m3.material.io/get-started |
| UX writing | https://m3.material.io/foundations/content-design/style-guide/ux-writing-best-practices |
| Notifications | https://m3.material.io/foundations/content-design/notifications |
| Customization | https://m3.material.io/foundations/customization |
| Design tokens | https://m3.material.io/foundations/design-tokens/overview |
| States | https://m3.material.io/foundations/interaction/states/overview |
| Layout | https://m3.material.io/foundations/layout/layout-overview/overview |
| Scaffold | https://m3.material.io/foundations/layout/scaffold/overview |
| Grids & spacing (desktop) | https://m3.material.io/foundations/layout/grids-spacing/overview |
| Styles | https://m3.material.io/styles |
| Components (каталог) | https://m3.material.io/components |
| Blog | https://m3.material.io/blog |
| M3 Expressive | https://m3.material.io/blog/building-with-m3-expressive?utm_source=GD&utm_medium=referral&utm_campaign=IO25 |

## Дополнительно

- Android Compose Material 3 (RU): https://developer.android.com/develop/ui/compose/designsystems/material3?hl=ru

## Проект (корень = репозиторий unicase)

| Ресурс | Путь |
|--------|------|
| QmlMaterial | `3rdparty/QmlMaterial` |
| Примеры | `3rdparty/QmlMaterial/example/` |
| kddockwidgets | `3rdparty/kddockwidgets` |
| Dashboard QML (референс стиля) | `src/dashboard/qml/` |

## Чеклист перед сдачей QML

- [ ] Компонент выбран после сравнения альтернатив из [Components](https://m3.material.io/components)
- [ ] Прочитаны **Guidelines** (do / don't) для выбранного компонента
- [ ] Desktop layout / scaffold согласованы с bento и nav-каналом
- [ ] Цвета, тип, shape, elevation — через `MD.Token`, не хардкод
- [ ] Состояния через `mdState` / `State*`, не дублированы
- [ ] Нет лишних одноразовых `property` и пиксельных костылей
- [ ] Используется `MD.*`, не смешение с немaterial Controls без причины
- [ ] Проверено наличие в QmlMaterial; при отсутствии — явно указано + идея PR

## Чеклист UX / notifications

- [ ] Текст кнопок и заголовков — глагол / конкретика ([UX writing](https://m3.material.io/foundations/content-design/style-guide/ux-writing-best-practices))
- [ ] Ошибки и успех — правильный канал ([notifications](https://m3.material.io/foundations/content-design/notifications)): snackbar vs dialog vs inline
- [ ] Не злоупотреблять модальными dialog для некритичного

## Desktop vs touch

- Target size: курсорная точность, не 48dp touch везде
- Navigation: side sheet / rail / persistent nav предпочтительнее bottom nav
- Плотность: desktop grids из [grids & spacing](https://m3.material.io/foundations/layout/grids-spacing/overview)

## Страница компонента M3 (как читать)

1. **Overview** — задача компонента  
2. **Specs** — размеры и токены для сверки с `MD.Token`  
3. **Guidelines** — решающий аргумент при выборе между двумя кандидатами  

## kddockwidgets + M3

- Dock title bar / tab strip: по возможности те же `MD` паттерны, что в dashboard (`WorkspaceTabBar.qml` и др.)
- Контент дока — QmlMaterial внутри, scaffold body не дублировать глобальный app bar в каждом доке без нужды

## Cursor workspace

Skills лежат в `.cursor/skills/` этого репозитория. В multi-root workspace добавьте папку **unicase** (например `unicase_ws/src/unicase`), иначе Cursor может не подхватить project skills из outer `unicase_ws`.
