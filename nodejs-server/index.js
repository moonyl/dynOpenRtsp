const net = require("net");

const path = process.platform === "win32" ? "\\\\?\\pipe\\audioStream" : "/tmp/audioStream";
let chunks = [];
let interval = 0;
let current = 0;

const server = net
  .createServer(socket => {
    socket.on("data", data => {
      console.log("data: ", data.length);
      chunks.push(data);
    });
  })
  .on("error", err => {
    console.error(err);
  });

server.listen(path, () => {
  console.log("audioStream open: ", server.address());
});

const PORT = process.env.PORT || 8080;

const WebSocket = require("ws");
const wss = new WebSocket.Server({ port: PORT });
console.log("Server ready on port " + PORT);
wss.on("connection", function connection(ws) {
  console.log("Socket connected. sending data...");
  if (interval) {
    clearInterval(interval);
  }
  ws.on("error", function error(error) {
    console.log("WebSocket error");
  });
  ws.on("close", function close(msg) {
    console.log("WebSocket close");
  });

  interval = setInterval(function() {
    sendChunk();
  }, 1800);
});

function sendChunk() {
  //let chunk,
  let anyOneThere = false;
  if (chunks.length === 0) {
    console.log("not ready");
    return;
  }
  //chunk = chunks[current];
  current++;
  //if (current == total) current = 0;
  wss.clients.forEach(function each(client) {
    if (client.readyState === WebSocket.OPEN) {
      anyOneThere = true;
      //console.log(index);
      try {
        chunks.forEach(chunk => {
          client.send(chunk);
        });
      } catch (e) {
        console.log(`Sending failed:`, e);
      }
      if (current % 50 == 0) {
        console.log(`I am serving, no problem!`);
      }
    }
  });
  chunks = [];

  if (!anyOneThere) {
    if (interval) {
      current = 0;
      clearInterval(interval);
      console.log("nobody is listening. Removing interval for now...");
    }
  }
}
