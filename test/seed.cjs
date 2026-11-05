// Test data for a manual check of Ruumble Client on the LOCAL test server (local-stack.sh up):
//   - room descriptions (the binders in rooms and corridors)
//   - everyone may administer the building (create, remove, move rooms and people, ACL, kick, ban), so the
//     right-click menus and drag and drop can be tried with throwaway test users. Never run this against a real server.
// Uses the setup-only WRITE secret like tools/live-test/src/setup.cjs.
const { connect } = require("../server/tools/live-test/src/ice.cjs");

const DESCRIPTIONS = {
  "Ben's office": "<p>Ben's office: <b>backend</b> and the build.</p><ul><li>Stand-up at 9:30</li><li>Wiki: <a href=\"https://example.com/wiki\">example.com/wiki</a></li></ul>",
  DEVELOPMENT: "<p>Development floor. Quiet hours 13:00 to 15:00.</p>",
  "Clara's office": "<p>Clara: design and the web UI. Knock first.</p>",
  Kitchen: "<p>Coffee machine descaled on Mondays.</p>",
};

(async () => {
  const { communicator, server, MumbleServer: M } = await connect({ secret: "s1-write-secret-only-for-setup" });
  try {
    const channels = [...(await server.getChannels()).values()];
    for (const [name, html] of Object.entries(DESCRIPTIONS)) {
      const channel = channels.find((c) => c.name === name);
      if (!channel) { console.error("no channel", name); continue; }
      const state = await server.getChannelState(channel.id);
      state.description = html;
      await server.setChannelState(state);
      console.log("description:", name);
    }
    const admin = M.PermissionWrite | M.PermissionMakeChannel | M.PermissionMove | M.PermissionMuteDeafen |
      M.PermissionLinkChannel | M.PermissionKick | M.PermissionBan | M.PermissionRegister;
    const [acls, groups, inherit] = await server.getACL(0);
    const own = acls.filter((a) => !a.inherited);
    if (!own.some((a) => a.group === "all" && (a.allow & admin) === admin)) {
      own.push(new M.ACL(true, true, false, -1, "all", admin, 0));
      await server.setACL(0, own, groups, inherit);
    }
    console.log("everyone administers the test building");
  } finally {
    await communicator.destroy();
  }
})().catch((e) => { console.error(e); process.exit(1); });
