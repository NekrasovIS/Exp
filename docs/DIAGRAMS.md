# Диаграммы

Архитектурные диаграммы DeviceHub (см. [README](../README.md) за
общим описанием проекта). PlantUML-диаграммы классов и sequence-диаграммы для нетривиальных
потоков — исходники `.puml` в [docs/diagrams/](diagrams/), рядом
с ними PNG-рендер того же имени; PNG перерендеривается в том же
коммите, что и любая правка `.puml` (`java -jar plantuml.jar -tpng
docs/diagrams/*.puml`).

- [devices.puml](diagrams/devices.puml) — `src/devices`
  ([devices.png](diagrams/devices.png))
- [auth.puml](diagrams/auth.puml) — `src/auth`
  ([auth.png](diagrams/auth.png))
- [chat.puml](diagrams/chat.puml) — `src/chat`, включая
  `CallManager` и SFU-звонки
  ([chat.png](diagrams/chat.png))
- [ui.puml](diagrams/ui.puml) — `src/ui`
  ([ui.png](diagrams/ui.png))
- [user.puml](diagrams/user.puml) — `src/user` (issue #110,
  `IdentityKeyStore` добавлен issue #136)
  ([user.png](diagrams/user.png))
- [mic-capture-sequence.puml](diagrams/mic-capture-sequence.puml) —
  запуск захвата с микрофона
  ([mic-capture-sequence.png](diagrams/mic-capture-sequence.png))
- [call-video-receive-sequence.puml](diagrams/call-video-receive-sequence.puml) —
  приём видео от участника звонка (issue #91)
  ([call-video-receive-sequence.png](diagrams/call-video-receive-sequence.png))
- [call-minimize-sequence.puml](diagrams/call-minimize-sequence.puml) —
  сворачивание/разворачивание звонка в плавающие мини-панели (issue #215)
  ([call-minimize-sequence.png](diagrams/call-minimize-sequence.png))

