#include "QtMumbleApi.h"

#include "Channel.h"
#include "ClientUser.h"
#include "Database.h"
#include "Global.h"
#include "Log.h"
#include "MainWindow.h"
#include "Mumble.pb.h"
#include "ServerHandler.h"

#include <QtCore/QCoreApplication>
#include <QtCore/QMetaObject>
#include <QtCore/QThread>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <memory>
#include <mutex>
#include <optional>

namespace ruumbledesk {

namespace {

	/** the plugin API allows a plugin call up to 800 ms as well (Ruumble Analyse 2.1) */
	constexpr std::chrono::milliseconds kTimeout{ 800 };
	constexpr unsigned int kRootChannel = 0;

	/**
	 * Runs fn on the main thread and returns its result, or fallback after kTimeout. A late
	 * result is dropped: the shared state outlives the waiting worker.
	 */
	template< typename T, typename Fn > T onMain(Fn fn, T fallback) {
		QCoreApplication *app = QCoreApplication::instance();
		if (!app) {
			return fallback;
		}
		if (QThread::currentThread() == app->thread()) {
			return fn();
		}

		struct State {
			std::mutex mutex;
			std::condition_variable cv;
			std::optional< T > result;
		};
		auto state = std::make_shared< State >();

		QMetaObject::invokeMethod(
			app,
			[state, fn]() mutable {
				T value = fn();
				std::lock_guard< std::mutex > lock(state->mutex);
				state->result = std::move(value);
				state->cv.notify_all();
			},
			Qt::QueuedConnection);

		std::unique_lock< std::mutex > lock(state->mutex);
		if (!state->cv.wait_for(lock, kTimeout, [&] { return state->result.has_value(); })) {
			return fallback;
		}
		return std::move(*state->result);
	}

	bool synchronized() { return Global::get().sh && Global::get().uiSession != 0; }

} // namespace

bool QtMumbleApi::connected() {
	return onMain([] { return synchronized(); }, false);
}

std::optional< uint32_t > QtMumbleApi::localSession() {
	return onMain(
		[]() -> std::optional< uint32_t > {
			if (!synchronized()) {
				return std::nullopt;
			}
			return Global::get().uiSession;
		},
		std::optional< uint32_t >());
}

std::string QtMumbleApi::userHash(uint32_t session) {
	return onMain(
		[session]() -> std::string {
			if (!synchronized()) {
				return "";
			}
			const ClientUser *user = ClientUser::get(session);
			return user ? user->qsHash.toStdString() : std::string();
		},
		std::string());
}

std::optional< int32_t > QtMumbleApi::channelOf(uint32_t session) {
	return onMain(
		[session]() -> std::optional< int32_t > {
			if (!synchronized()) {
				return std::nullopt;
			}
			const ClientUser *user = ClientUser::get(session);
			if (!user || !user->cChannel) {
				return std::nullopt;
			}
			return static_cast< int32_t >(user->cChannel->iId);
		},
		std::optional< int32_t >());
}

bool QtMumbleApi::requestMove(uint32_t session, int32_t channelId) {
	return onMain(
		[session, channelId]() -> bool {
			if (!synchronized() || channelId < 0) {
				return false;
			}
			const ClientUser *user = ClientUser::get(session);
			const Channel *channel = Channel::get(static_cast< unsigned int >(channelId));
			if (!user || !channel) {
				return false;
			}
			// like the plugin API: only ask the server if the user is not in that channel already
			if (channel != user->cChannel) {
				Global::get().sh->joinChannel(user->uiSession, channel->iId);
			}
			return true;
		},
		false);
}

bool QtMumbleApi::setMute(bool on) {
	return onMain(
		[on]() -> bool {
			if (!Global::get().mw) {
				return false;
			}
			Global::get().mw->setAudioMute(on);
			return true;
		},
		false);
}

bool QtMumbleApi::setDeaf(bool on) {
	return onMain(
		[on]() -> bool {
			if (!Global::get().mw) {
				return false;
			}
			Global::get().mw->setAudioDeaf(on);
			return true;
		},
		false);
}

bool QtMumbleApi::isMuted() {
	return onMain([] { return Global::get().s.bMute; }, false);
}

bool QtMumbleApi::isDeafened() {
	return onMain([] { return Global::get().s.bDeaf; }, false);
}

ruumble::Description QtMumbleApi::rootDescription() {
	return onMain(
		[this]() -> ruumble::Description {
			ruumble::Description d;
			if (!synchronized()) {
				return d;
			}
			Channel *root = Channel::get(kRootChannel);
			if (!root) {
				return d;
			}
			if (root->qsDesc.isEmpty() && !root->qbaDescHash.isEmpty()) {
				root->qsDesc = QString::fromUtf8(Global::get().db->blob(root->qbaDescHash));
				if (root->qsDesc.isEmpty()) {
					// Unlike a plugin, the client can fetch the text itself, so nobody has to click
					// on the root channel first (ADR-0010).
					if (!m_blobRequested.exchange(true)) {
						MumbleProto::RequestBlob mprb;
						mprb.add_channel_description(root->iId);
						Global::get().sh->sendMessage(mprb);
					}
					d.status = ruumble::Description::Status::Pending;
					return d;
				}
			}
			d.status = ruumble::Description::Status::Ok;
			d.text   = root->qsDesc.toStdString();
			return d;
		},
		ruumble::Description());
}

std::string QtMumbleApi::rootName() {
	return onMain(
		[]() -> std::string {
			const Channel *root = synchronized() ? Channel::get(kRootChannel) : nullptr;
			return root ? root->qsName.toStdString() : std::string("Root");
		},
		std::string("Root"));
}

void QtMumbleApi::log(const std::string &message) {
	// live tests follow the log; Mumble itself does not write it to the console
	static const bool logToStderr = std::getenv("RUUMBLE_LOG_STDERR") != nullptr;
	if (logToStderr) {
		std::fprintf(stderr, "ruumble-log: %s\n", message.c_str());
	}
	const QString text = QString::fromUtf8(message.c_str());
	QMetaObject::invokeMethod(
		QCoreApplication::instance(),
		[text] {
			Log::logOrDefer(Log::PluginMessage, QString::fromLatin1("<b>Ruumble:</b> %1").arg(text.toHtmlEscaped()));
		},
		Qt::QueuedConnection);
}

} // namespace ruumbledesk
