# SUB-MIDI vox  (by ПЕПЕL)

Вокальный плагин для FL Studio в стиле «8-бит аниме + VHS, ночное небо и кровь».
Автотюн (PSOLA), гармонайзер, вокодер, гранулятор, bitcrusher, фазер, фленджер, дисторшн, эксайтер,
тремоло — и **SubVox**: голос превращается в бас, а то, что ты спел, записывается в MIDI прямо внутри плагина.

Форматы: **VST3** (Windows / macOS / Linux), **AU** (только macOS), **Standalone**.
Интерфейс нарисован вручную (JUCE 8.0.6, C++17), шрифты VT323 и Press Start 2P (OFL, лицензии в `Assets/`).

---

## Две страницы

Переключатель в правом верхнем углу шапки: **VOICE FX** и **SUBVOX**.

### VOICE FX
AUTO-TUNE, PITCH + HARMONY (с вокодером), GRANULAR, BITCRUSHER, PHASER, FLANGER, MASTER, DISTORTION, EXCITER, TREMOLO.
Двойной клик по слайдеру сбрасывает значение. Внизу «погода»: чем грязнее звук, тем хуже погода
(ясно, рассвет, закат, дождь, гроза с кроваво-красной луной). Город на горизонте пляшет от уровня входа.

### SUBVOX: голос в бас
Плагин слушает голос (детектор высоты YIN), определяет ноту и играет по ней саб-синтезатор.
Режимы (параметр **Mode**):

| Режим | Звук |
|-------|------|
| SINE  | чистый синусный саб |
| 808   | синус с «пинком» питча (Punch), затухающий хвост, глайд |
| REESE | три расстроенные пилы + сабовый синус, фильтр и воблер |
| GROWL | FM-бас, «рычание» зависит от громкости и яркости голоса |
| 8-BIT | пульсовая волна с PWM, квантованная под чиптюн |
| TALK  | вокодер-бас: тембр твоего голоса накладывается на бас («говорящий» саб) |

Остальные параметры: **Octave** (на сколько октав ниже голоса), **Sub** (громкость саба), **Voice** (сколько голоса остаётся),
**Syn-Lock** (притягивает бас к ладу из AUTO-TUNE: Key/Scale), **Glide**, **Drive**, **Punch**.
**SUB FILTER**: Cutoff, Reso, Follow (яркость голоса открывает фильтр), Gate (порог, ниже которого бас молчит).
**WOBBLE**: LFO на фильтр / FM / PWM, Detune для REESE.

### Запись MIDI внутри плагина
1. Выбери слот **A / B / C / D** (в плагине 4 независимых клипа, они сохраняются вместе с проектом FL).
2. Нажми **REC** и пой/читай в микрофон (или пусти вокал по треку). Ноты появляются в piano roll сразу.
3. **STOP**. Запись сохраняется в слот. **PLAY** проигрывает клип в цикле через саб-синтезатор. **CLEAR** очищает слот.
4. **Grid** (OFF, 1/4, 1/8, 1/16, 1/32) квантует ноты; квантование не портит запись и применяется при показе, проигрывании и экспорте.
5. **Transp** сдвигает MIDI в полутонах. В MIDI попадает то же, что играет саб: нота голоса + **Octave** + **Transp**.
   Для партии лида/клавиш поставь Octave 0 и подними Transp.
6. **Min Len** отсекает слишком короткие «ноты» (дыхание, согласные). **Gate** отсекает тихие звуки.
7. Темп берётся из FL Studio; если хост его не отдаёт, используется параметр **Tempo**.

### Как перенести MIDI в другие инструменты
* **DRAG MIDI**: схвати кнопку и перетащи в Playlist или Piano roll FL Studio (или на любой другой синтезатор/DAW). Создаётся `.mid` во временной папке.
* **SAVE .MID**: сохраняет файл, дальше в FL: *File > Import > MIDI file* или перетащить файл из проводника.
* **COPY / PASTE**: копирует клип в буфер обмена как текст `PEPELMIDI1` и вставляет его в любой слот,
  в том числе в **другом экземпляре SUB-MIDI vox** или другом проекте. Это не буфер Piano roll самой FL Studio:
  FL не принимает чужой формат, для неё используй DRAG / SAVE.
* **MIDI Out**: включи параметр *MIDI Out*, и плагин шлёт ноты голоса (и проигрываемого клипа) в виде MIDI-событий.
  В FL: окно плагина > значок шестерёнки > *Settings > MIDI > Output port*, и другой инструмент с тем же *Input port*
  услышит ноты в реальном времени.

---

## Установка в FL Studio (Windows)

1. Скачай `SUB-MIDI-vox-Windows.zip` (см. ниже, как получить сборку) и распакуй.
2. Папку `VST3\SUB-MIDI vox.vst3` скопируй в `C:\Program Files\Common Files\VST3\`.
3. FL Studio: *Options > Manage plugins > Find installed plugins* (Start scan).
4. Добавь SUB-MIDI vox на мастер-канал или на канал микрофона: *Mixer > слот эффекта > SUB-MIDI vox*.
5. Вход: микрофон в *Mixer > Input* трека. Выход MIDI: см. «MIDI Out» выше.

Задержка плагина (около 37 мс на 48 кГц, нужна для PSOLA) репортуется хосту, FL компенсирует её сама.
При записи вокала с мониторингом включай *Options > Audio settings > Latency compensation* или мониторь без плагина.

Standalone-версия (`Standalone\SUB-MIDI vox.exe`) запускается отдельно, для проверки с микрофоном.

## Как получить сборку: GitHub через браузер

Ничего ставить не нужно: сборку делает GitHub Actions.

1. Зайди на https://github.com и войди в аккаунт (или зарегистрируйся).
2. Справа вверху **+ > New repository**. Имя: `sub-midi-vox`. Public или Private, затем **Create repository**.
3. На странице нового репозитория нажми **uploading an existing file**.
4. Распакуй архив проекта. Перетащи в окно браузера **содержимое** папки `PepelVox`
   (папки `Source`, `Assets`, `Tests`, `Tools`, `.github`, файлы `CMakeLists.txt`, `README.md`, `.gitignore`).
   Папка `.github` скрытая: если проводник её не показывает, включи «Показывать скрытые элементы».
   Если браузер не принимает перетаскивание папок, загружай по подпапкам через **Add file > Upload files**,
   сохраняя структуру (для вложенных папок: *Add file > Create new file* и имя вида `Source/Dsp.h`).
5. Внизу нажми **Commit changes**.
6. Открой вкладку **Actions**. Запустится **Build SUB-MIDI vox**. Если Actions выключены, нажми **I understand my workflows, enable them**.
7. Сборка идёт 10-20 минут. Когда все три задачи (Windows, macOS, Linux) позеленеют, открой запуск и внизу в разделе **Artifacts**
   скачай `SUB-MIDI-vox-Windows`.
8. Релиз со ссылками для скачивания: на странице репозитория *Releases > Draft a new release*, тег `v1.0.0` > **Create new tag** > **Publish**.
   Workflow сам прикрепит zip-файлы к релизу.

> На macOS плагин не подписан. При первом запуске: *Системные настройки > Конфиденциальность и безопасность > Всё равно открыть*,
> либо `xattr -dr com.apple.quarantine "SUB-MIDI vox.vst3"`. AU/VST3 кладутся в
> `~/Library/Audio/Plug-Ins/Components` и `~/Library/Audio/Plug-Ins/VST3`.

## Локальная сборка

```bash
# Linux: sudo apt install libasound2-dev libfreetype-dev libfontconfig1-dev libx11-dev libxcomposite-dev \
#        libxcursor-dev libxext-dev libxinerama-dev libxrandr-dev libxrender-dev libglu1-mesa-dev mesa-common-dev
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --parallel 2       # на малой памяти

# тест DSP и скриншоты интерфейса
cmake -S . -B build-tools -DCMAKE_BUILD_TYPE=Release -DPEPEL_BUILD_TOOLS=ON -DPEPEL_BUILD_PLUGIN=OFF
cmake --build build-tools --parallel 2
./build-tools/PepelDspTest
xvfb-run -a ./build-tools/PepelScreenshot shots
```

Windows: `cmake -S . -B build -G "Visual Studio 17 2022" -A x64` и `cmake --build build --config Release`.
Кириллица в исходниках хранится в UTF-8, для MSVC включён `/utf-8`.

## Структура

```
Source/ParamList.h     X-макросы параметров, форматирование, пресеты (чистый C++)
Source/Dsp.h           весь DSP без JUCE: YIN, PSOLA, вокодер, гранулятор, эффекты, SubVox, трекер нот, рекордер, клипы
Source/Params.h        ID, ParameterLayout, применение пресетов
Source/PluginProcessor APVTS, программы = пресеты, запись/проигрывание MIDI, экспорт .mid, состояние
Source/PluginEditor    палитра pal, pixelfont, PixelLookAndFeel, ParamRow, piano roll, анимации, VHS-эффекты
Source/Sprites.h       спрайты (генерируются Tools/gen_sprites.py)
Tests/DspTest.cpp      тест DSP
Tools/Screenshot.cpp   PepelScreenshot (PNG интерфейса)
```

Звуковой поток без аллокаций и блокировок (клипы передаются в аудиопоток через try-lock и фиксированные массивы).

## Честные ограничения

* Определяется одноголосый вокал; аккорды, сильный шум и реверб дают ложные ноты. Для чистого MIDI пой в сухой микрофон.
* Детектор работает от ~75 Гц, поэтому очень низкие голоса надо петь чуть выше и опускать Octave.
* Перетаскивание файла из окна плагина зависит от хоста; если FL Studio не принял drag, используй SAVE .MID.
* Задержка PSOLA около 37 мс фиксирована.
