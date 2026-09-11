// Проверка инфраструктуры SFU (issue #230): реальное сквозное ICE/DTLS/SRTP
// согласование через Janus videoroom между двумя тестовыми участниками — до
// того, как chat-service/CallManager (issue #231/#232) напишут хоть строку
// кода поверх Janus. Использует werift (чистый TypeScript/JS стек WebRTC без
// нативных биндингов — в отличие от node-webrtc/wrtc, надёжно ставится и
// работает на Windows) вместо браузера, чтобы проверку можно было
// прогонять автоматически, без ручного шага в двух вкладках браузера.
//
// Что именно проверяется:
//   1. Участник A создаёт свежую комнату (ad-hoc, не статическую
//      "devicehub-test" из janus.plugin.videoroom.jcfg — см. doc-комментарий
//      у ROOM ниже) и публикует в неё видео — реальное ICE+DTLS соединение
//      с Janus должно установиться (connectionState="connected"). Это же
//      подтверждает, что media.rtp_port_range в janus.jcfg совпадает с
//      проброшенным в docker-compose.yml диапазоном портов, и что
//      STUN-конфигурация рабочая.
//   2. Участник B, присоединяясь следом, должен увидеть A в списке
//      publishers комнаты — подтверждает, что плагин videoroom корректно
//      отслеживает состояние комнаты между независимыми сессиями.
//   3. B подписывается на видео-фид A и должен установить своё собственное
//      ICE+DTLS соединение с Janus (Janus пересылает медиа через себя,
//      участники никогда не соединяются друг с другом напрямую — это и
//      есть SFU в отличие от mesh) — сам факт успешного подключения
//      подписчика и есть подтверждение рабочего пути форвардинга.
//   4. Участник C, присоединяясь третьим (issue #233 — предварительное
//      условие для удаления mesh-кода: подтверждённый работающий
//      SFU-звонок минимум с 3 участниками, а не "теоретическая замена"),
//      должен увидеть ОБОИХ — и A, и B — в списке publishers. Это
//      отдельная проверка от шага 2: там комната содержала только одного
//      существующего publisher'а, здесь — двух, и именно multi-publisher
//      discovery — то, от чего реально зависит масштабирование SFU за
//      пределы двух участников.
//   5. C подписывается на оба фида (A и B) и должен установить два
//      отдельных ICE+DTLS-соединения с Janus — та же проверка
//      форвардинга, что и в шаге 3, но с несколькими одновременными
//      subscribe-соединениями у одного участника.
//
// Важно про сетевое окружение: этот скрипт нужно запускать в контейнере,
// подключённом к той же docker-сети, что и сервис janus (например,
// `docker run --rm --network exp_default -v ...:/work -w /work node:24
// node verify-forwarding.mjs`, с JANUS_HOST=janus) — при запуске с хоста
// ICE зависает в состоянии "connecting", потому что srflx-кандидат,
// полученный Janus через публичный STUN, для процесса на том же хосте
// упирается в NAT hairpin, который поддерживают не все роутеров/провайдеров
// (см. обсуждение в PR issue #230). Для реального удалённого клиента (не
// на этой же машине) это ограничение не действует.
import { RTCPeerConnection } from "werift";

const BASE = `http://${process.env.JANUS_HOST ?? "127.0.0.1"}:8088/janus`;
// Уникальная комната на каждый прогон (issue #233), а не статическая
// "devicehub-test" из janus.plugin.videoroom.jcfg — та же комната,
// использованная повторными прогонами подряд, накапливает "призрачных"
// publisher'ов от предыдущих (в т.ч. упавших без явной очистки) попыток,
// раз Janus не закрывает handle/session сразу же при завершении процесса
// клиента. Ad-hoc создание комнаты (request:"create") ближе и к тому,
// как её реально создаёт chat-service (JanusClient::ensureRoomExists(),
// issue #231) — там комнаты тоже не статические.
const ROOM = `devicehub-verify-${Date.now()}`;

function txId() {
  return Math.random().toString(36).slice(2);
}

async function post(path, body) {
  const res = await fetch(`${BASE}${path}`, {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ transaction: txId(), ...body }),
  });
  const json = await res.json();
  if (json.janus === "error") {
    throw new Error(`Janus error: ${JSON.stringify(json.error)}`);
  }
  return json;
}

async function createSession() {
  const resp = await post("", { janus: "create" });
  return resp.data.id;
}

async function attachVideoroom(sessionId) {
  const resp = await post(`/${sessionId}`, {
    janus: "attach",
    plugin: "janus.plugin.videoroom",
  });
  return resp.data.id;
}

async function longpoll(sessionId) {
  const res = await fetch(`${BASE}/${sessionId}?maxev=1&rid=${Date.now()}`);
  return res.json();
}

/// Ждёт именно тот event, что относится к @p handleId (issue #233 — как
/// только сессия обзаводится больше чем одним handle'ом, а второй handle
/// реально что-то делает — не только "join как publisher чтобы
/// подсмотреть список", как раньше, но и настоящая публикация — long-poll
/// сессии в целом может отдать событие ДРУГОГО handle'а первым, если оно
/// оказалось в очереди раньше нашего; Janus помечает каждый event полем
/// "sender" = id того handle'а, который его сгенерировал, так что можно
/// пропускать чужие и забирать следующий, пока не найдётся свой (или не
/// истечёт общий тайм-аут).
async function longpollForHandle(sessionId, handleId, timeoutMs = 10000) {
  const deadline = Date.now() + timeoutMs;
  while (Date.now() < deadline) {
    const event = await longpoll(sessionId);
    if (event.sender === handleId || event.janus === "error") {
      return event;
    }
  }
  throw new Error(`longpollForHandle: timed out waiting for an event from handle ${handleId}`);
}

async function message(sessionId, handleId, body, jsep) {
  const payload = { janus: "message", body };
  if (jsep) payload.jsep = jsep;
  const resp = await post(`/${sessionId}/${handleId}`, payload);
  if (resp.janus === "ack") {
    // HTTP-транспорт Janus обрабатывает message асинхронно: сам POST
    // подтверждает только приём, а результат (event/jsep) нужно забирать
    // отдельным long-poll GET на сессии.
    return longpollForHandle(sessionId, handleId);
  }
  return resp;
}

function waitForConnected(pc, label, timeoutMs = 15000) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(
      () =>
        reject(
          new Error(
            `${label}: timeout waiting for connectionState=connected (last state: ${pc.connectionState})`,
          ),
        ),
      timeoutMs,
    );
    pc.connectionStateChange.subscribe((state) => {
      console.log(`[${label}] connectionState -> ${state}`);
      if (state === "connected") {
        clearTimeout(timer);
        resolve();
      } else if (state === "failed" || state === "closed") {
        clearTimeout(timer);
        reject(new Error(`${label}: connectionState=${state}`));
      }
    });
  });
}

async function main() {
  console.log("=== Шаг 0: создание комнаты", ROOM, "===");
  const roomSession = await createSession();
  const roomHandle = await attachVideoroom(roomSession);
  await message(roomSession, roomHandle, { request: "create", room: ROOM, publishers: 10 });

  console.log("\n=== Шаг 1: участник A публикует видео в", ROOM, "===");
  const sessionA = await createSession();
  const handleA = await attachVideoroom(sessionA);
  const joinA = await message(sessionA, handleA, {
    request: "join",
    room: ROOM,
    ptype: "publisher",
    display: "test-pub-A",
  });
  const myIdA = joinA.plugindata.data.id;

  const pcA = new RTCPeerConnection();
  pcA.addTransceiver("video", { direction: "sendonly" });
  const offerA = await pcA.createOffer();
  await pcA.setLocalDescription(offerA);
  const configureA = await message(
    sessionA,
    handleA,
    { request: "configure", audio: false, video: true },
    { type: "offer", sdp: pcA.localDescription.sdp },
  );
  if (!configureA.jsep) {
    throw new Error("A: Janus did not return a jsep answer for configure");
  }
  await pcA.setRemoteDescription(configureA.jsep);
  await waitForConnected(pcA, "A (publisher)");
  console.log("A: ICE/DTLS соединение с Janus установлено (publish).");

  console.log("\n=== Шаг 2: участник B присоединяется и видит A в списке publishers ===");
  const sessionB = await createSession();
  const handleB = await attachVideoroom(sessionB);
  const joinB = await message(sessionB, handleB, {
    request: "join",
    room: ROOM,
    ptype: "publisher",
    display: "test-pub-B",
  });
  const myIdB = joinB.plugindata.data.id;
  const publishersSeenByB = joinB.plugindata.data.publishers ?? [];
  console.log(
    "B видит publishers в комнате:",
    publishersSeenByB.map((p) => p.display),
  );
  if (!publishersSeenByB.some((p) => p.id === myIdA)) {
    throw new Error(
      "B did not see A in the room's publisher list — SFU room state is not tracking participants correctly",
    );
  }
  console.log("OK: B видит A как активного publisher-а в комнате.");

  // B реально публикуется (issue #233, шаг 4 ниже нужен настоящий, а не
  // только заявленный publisher B, чтобы у C было двух реальных
  // publisher'ов для discovery) — тот же handleB, что и для join выше:
  // отдельный второй publisher-role handle на той же сессии Janus не
  // принимает (join как publisher уже занял единственный "слот" сессии).
  const pcB = new RTCPeerConnection();
  pcB.addTransceiver("video", { direction: "sendonly" });
  const offerB = await pcB.createOffer();
  await pcB.setLocalDescription(offerB);
  const configureB = await message(
    sessionB,
    handleB,
    { request: "configure", audio: false, video: true },
    { type: "offer", sdp: pcB.localDescription.sdp },
  );
  if (!configureB.jsep) {
    throw new Error("B: Janus did not return a jsep answer for configure");
  }
  await pcB.setRemoteDescription(configureB.jsep);
  await waitForConnected(pcB, "B (publisher)");
  console.log("B: ICE/DTLS соединение с Janus установлено (publish).");

  console.log("\n=== Шаг 3: B подписывается на видео-фид A (реальный forwarding-путь SFU) ===");
  const handleBSub = await attachVideoroom(sessionB);
  const subscribeB = await message(sessionB, handleBSub, {
    request: "join",
    room: ROOM,
    ptype: "subscriber",
    feed: myIdA,
  });
  if (!subscribeB.jsep) {
    throw new Error("B: Janus did not send an SDP offer for the subscription to A's feed");
  }
  const pcBSub = new RTCPeerConnection();
  await pcBSub.setRemoteDescription(subscribeB.jsep);
  const answerBSub = await pcBSub.createAnswer();
  await pcBSub.setLocalDescription(answerBSub);
  await message(
    sessionB,
    handleBSub,
    { request: "start", room: ROOM },
    { type: "answer", sdp: pcBSub.localDescription.sdp },
  );
  await waitForConnected(pcBSub, "B (subscriber to A's feed)");
  console.log("B: ICE/DTLS соединение с Janus установлено (subscribe на фид A).");

  console.log("\n=== Шаг 4: участник C присоединяется и видит ОБОИХ A и B в списке publishers ===");
  const sessionC = await createSession();
  const handleC = await attachVideoroom(sessionC);
  const joinC = await message(sessionC, handleC, {
    request: "join",
    room: ROOM,
    ptype: "publisher",
    display: "test-pub-C",
  });
  const publishersSeenByC = joinC.plugindata.data.publishers ?? [];
  console.log(
    "C видит publishers в комнате:",
    publishersSeenByC.map((p) => p.display),
  );
  if (!publishersSeenByC.some((p) => p.id === myIdA)) {
    throw new Error("C did not see A in the room's publisher list");
  }
  if (!publishersSeenByC.some((p) => p.id === myIdB)) {
    throw new Error(
      "C did not see B in the room's publisher list — multi-publisher discovery is broken beyond the first publisher",
    );
  }
  console.log("OK: C видит и A, и B как активных publisher-ов в комнате.");

  console.log("\n=== Шаг 5: C подписывается на видео-фиды A и B ===");
  const handleCSubA = await attachVideoroom(sessionC);
  const subscribeCA = await message(sessionC, handleCSubA, {
    request: "join",
    room: ROOM,
    ptype: "subscriber",
    feed: myIdA,
  });
  if (!subscribeCA.jsep) {
    throw new Error("C: Janus did not send an SDP offer for the subscription to A's feed");
  }
  const pcCSubA = new RTCPeerConnection();
  await pcCSubA.setRemoteDescription(subscribeCA.jsep);
  const answerCSubA = await pcCSubA.createAnswer();
  await pcCSubA.setLocalDescription(answerCSubA);
  await message(
    sessionC,
    handleCSubA,
    { request: "start", room: ROOM },
    { type: "answer", sdp: pcCSubA.localDescription.sdp },
  );
  await waitForConnected(pcCSubA, "C (subscriber to A's feed)");
  console.log("C: ICE/DTLS соединение с Janus установлено (subscribe на фид A).");

  const handleCSubB = await attachVideoroom(sessionC);
  const subscribeCB = await message(sessionC, handleCSubB, {
    request: "join",
    room: ROOM,
    ptype: "subscriber",
    feed: myIdB,
  });
  if (!subscribeCB.jsep) {
    throw new Error("C: Janus did not send an SDP offer for the subscription to B's feed");
  }
  const pcCSubB = new RTCPeerConnection();
  await pcCSubB.setRemoteDescription(subscribeCB.jsep);
  const answerCSubB = await pcCSubB.createAnswer();
  await pcCSubB.setLocalDescription(answerCSubB);
  await message(
    sessionC,
    handleCSubB,
    { request: "start", room: ROOM },
    { type: "answer", sdp: pcCSubB.localDescription.sdp },
  );
  await waitForConnected(pcCSubB, "C (subscriber to B's feed)");
  console.log("C: ICE/DTLS соединение с Janus установлено (subscribe на фид B).");

  console.log("\n=== ВЕРДИКТ: Janus videoroom SFU реально форвардит между тремя тестовыми участниками ===");
  pcA.close();
  pcB.close();
  pcBSub.close();
  pcCSubA.close();
  pcCSubB.close();
  process.exit(0);
}

main().catch((err) => {
  console.error("\nFAILED:", err);
  process.exit(1);
});
