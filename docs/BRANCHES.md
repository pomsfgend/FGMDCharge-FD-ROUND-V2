# Branches

`main` is the published tree. Releases and the prebuilt image come from here.

`develop` is the branch for the next change. Open a pull request into `develop`. Do not commit experiments straight to `main`.

Fixed bugs do not get a permanent branch. A branch that points at the same commit as `main` would look like the bug is still there. Each fix is a closed issue and a section in [CHANGELOG.md](../CHANGELOG.md).

Do not add a tag `v2.8.10` (or any older number) on this tree. That tag would offer today's source under an old version. Historical text uses `notes/x.y.z` and says the old tree was not kept.

There is no branch per folder. CAN, the site, the display and the bootloader are one firmware and break if they are split apart.

# Ветки

`main` — то, что опубликовано. Релизы и готовый bin идут отсюда.

`develop` — следующая правка. Запрос на слияние открывается в `develop`, а не прямо в `main`.

Уже исправленный баг не получает вечную ветку. Ветка на том же коммите, что и `main`, выглядела бы так, будто баг ещё жив. Исправление — закрытая задача и абзац в [CHANGELOG.md](../CHANGELOG.md).

Метку `v2.8.10` и другие старые номера на это дерево не ставить: под старым номером скачается сегодняшний код. Для журнала есть `notes/x.y.z`, и там написано, что старого дерева нет.

Отдельной ветки на каждый каталог нет. CAN, сайт, экран и загрузчик — одна прошивка, по частям она не собирается.
