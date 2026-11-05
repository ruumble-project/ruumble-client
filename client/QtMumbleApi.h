// Ruumble Client: ruumble::MumbleApi on top of the client's own classes (ADR-0024, phase 0).
// Mirrors Ruumble/plugin/src/plugin.cpp, but reads ClientUser/Channel/ServerHandler directly
// instead of going through the plugin API.

#ifndef MUMBLE_MUMBLE_RUUMBLE_QTMUMBLEAPI_H_
#define MUMBLE_MUMBLE_RUUMBLE_QTMUMBLEAPI_H_

#include "core.h"

#include <atomic>

namespace ruumbledesk {

/**
 * Called by the core's worker thread. Every call runs on the main thread (the client objects live
 * there) and waits at most kTimeout for it, so the worker can never block shutdown for long.
 */
class QtMumbleApi : public ruumble::MumbleApi {
public:
	bool connected() override;
	std::optional< uint32_t > localSession() override;
	std::string userHash(uint32_t session) override;
	std::optional< int32_t > channelOf(uint32_t session) override;
	bool requestMove(uint32_t session, int32_t channel) override;
	bool setMute(bool on) override;
	bool setDeaf(bool on) override;
	bool isMuted() override;
	bool isDeafened() override;
	ruumble::Description rootDescription() override;
	std::string rootName() override;
	void log(const std::string &message) override;

	/** forget that the root description was requested (new connection) */
	void resetBlobRequest() { m_blobRequested = false; }

private:
	std::atomic< bool > m_blobRequested{ false };
};

} // namespace ruumbledesk

#endif
