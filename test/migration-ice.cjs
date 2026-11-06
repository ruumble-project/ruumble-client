// Test helper for test/migration.sh (ticket #15), local test server ONLY: uses the Ice WRITE secret of the test stack.
//   node migration-ice.cjs prepare <certHash> <token>   registers "Mia" with that certificate and makes "Token room"
//                                                       (Enter only with the token); prints {userId, tokenRoom}
//   node migration-ice.cjs check <name> <channelId>     waits up to 30 s for <name> online; prints
//                                                       {session, userId, mayEnter}
const { connect } = require("../server/tools/live-test/src/ice.cjs");

const [, , command, ...args] = process.argv;
const sleep = (ms) => new Promise((r) => setTimeout(r, ms));

(async () => {
  const { communicator, server, MumbleServer } = await connect({ secret: "s1-write-secret-only-for-setup" });
  try {
    if (command === "prepare") {
      const [hash, token] = args;
      const registered = await server.getRegisteredUsers("Mia");
      for (const [id, name] of registered) if (name === "Mia") await server.unregisterUser(id);
      const info = new Map([
        [MumbleServer.UserInfo.UserName, "Mia"],
        [MumbleServer.UserInfo.UserHash, hash],
      ]);
      const userId = await server.registerUser(info);
      const channels = await server.getChannels();
      const existing = [...channels.values()].find((c) => c.name === "Token room");
      const tokenRoom = existing ? existing.id : await server.addChannel("Token room", 0);
      const deny = new MumbleServer.ACL(true, true, false, -1, "all", 0, MumbleServer.PermissionEnter);
      const allow = new MumbleServer.ACL(true, true, false, -1, `#${token}`, MumbleServer.PermissionEnter, 0);
      await server.setACL(tokenRoom, [deny, allow], [], false);
      console.log(JSON.stringify({ userId, tokenRoom }));
    } else if (command === "check") {
      const [name, channel] = args;
      for (let i = 0; i < 60; i++) {
        const user = [...(await server.getUsers()).values()].find((u) => u.name === name);
        if (user) {
          const mayEnter = await server.hasPermission(user.session, Number(channel), MumbleServer.PermissionEnter);
          console.log(JSON.stringify({ session: user.session, userId: user.userid, mayEnter }));
          return;
        }
        await sleep(500);
      }
      console.log(JSON.stringify({ session: null }));
    } else {
      throw new Error(`unknown command ${command}`);
    }
  } finally {
    await communicator.destroy();
  }
})().catch((e) => { console.error(e); process.exit(1); });
