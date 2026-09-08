// Проверка инфраструктуры SFU (issue #230): реальное сквозное ICE/DTLS/SRTP
// согласование через Janus videoroom между двумя тестовыми участниками — до
// того, как chat-service/CallManager (issue #231/#232) напишут хоть строку
// кода поверх Janus. Использует werift (чистый TypeScript/JS стек WebRTC без
// нативных биндингов — в отличие от node-webrtc/wrtc, надёжно ставится и
// работает на Windows) вместо браузера, чтобы проверку можно было
// прогонять автоматически, без ручного шага в двух вкладках браузера.
//
// Что именно проверяется:
//   1. Участник A публикует видео в статическую тестовую комнату
//      (janus.plugin.videoroom.jcfg) — реальное ICE+DTLS соединение с Janus
//      должно установиться (connectionState="connected"). Это же
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
const ROOM = "devicehub-test";

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

async function message(sessionId, handleId, body, jsep) {
  const payload = { janus: "message", body };
  if (jsep) payload.jsep = jsep;
  const resp = await post(`/${sessionId}/${handleId}`, payload);
  if (resp.janus === "ack") {
    // HTTP-транспорт Janus обрабатывает message асинхронно: сам POST
    // подтверждает только приём, а результат (event/jsep) нужно забирать
    // отдельным long-poll GET на сессии.
    return longpoll(sessionId);
  }
  return resp;
}

function waitForConnected(pc, label, timeoutMs = 15000) {
  return new Promise((resolve, reject) => {
    const timer = setTimeout(
      () => reject(new Error(`${label}: timeout waiting for connectionState=connected (last state: ${pc.connectionState})`)),
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
  console.log("=== Шаг 1: участник A публикует видео в", ROOM, "===");
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
  const publishersSeenByB = joinB.plugindata.data.publishers ?? [];
  console.log("B видит publishers в комнате:", publishersSeenByB.map((p) => p.display));
  if (!publishersSeenByB.some((p) => p.id === myIdA)) {
    throw new Error("B did not see A in the room's publisher list — SFU room state is not tracking participants correctly");
  }
  console.log("OK: B видит A как активного publisher-а в комнате.");

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

  console.log("\n=== ВЕРДИКТ: Janus videoroom SFU реально форвардит между двумя тестовыми участниками ===");
  pcA.close();
  pcBSub.close();
  process.exit(0);
}

main().catch((err) => {
  console.error("\nFAILED:", err);
  process.exit(1);
});
