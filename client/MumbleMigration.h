// Ruumble Client: take over an installed official Mumble (ticket #15), so that every connection works as it did there.
#pragma once

#include <QtCore/QJsonObject>
#include <QtCore/QString>
#include <QtCore/QStringList>

namespace ruumbledesk {

/** Where an official Mumble keeps its setup; empty paths when nothing was found */
struct MumbleSetup {
	QString settings; ///< mumble_settings.json (Mumble 1.5 and newer)
	QString database; ///< mumble.sqlite: favourites, accepted server certificates, tokens, ...
	QString legacy;   ///< Mumble.conf / mumble.ini of Mumble 1.4 and older, only when no settings file was found

	bool found() const { return !settings.isEmpty(); }
};

/**
 * Takes over an official Mumble's setup into Ruumble Client's own one, before Mumble's settings are loaded.
 *
 * On every start, as long as a Mumble setup is there and the question has not been answered with "Take over" or
 * "Do not look again", a small dialog suggests the migration; with an existing Ruumble Client setup it warns gently
 * that it will be overwritten. Mumble always wins: its certificate, its settings for audio, shortcuts and network,
 * and on the same server its rows in the database. Servers only known to Ruumble Client stay. A backup of Ruumble
 * Client's files is made first. The answer is the file `mumble-migration` next to the settings; deleting it makes the
 * next start ask again.
 *
 * Headless tests answer without the dialog: RUUMBLE_MUMBLE_MIGRATION=take|not-now|never.
 */
class MumbleMigration {
public:
	/** the whole flow at start: look, ask, take over, remember the answer */
	static void run();

	/** looks for an official Mumble's setup below the given home (native, Flatpak and Snap layouts) */
	static MumbleSetup find(const QString &home, const QString &configHome, const QString &dataHome);

	/** Ruumble Client's settings after taking over Mumble's (the parts that make connections and audio the same) */
	static QJsonObject mergeSettings(const QJsonObject &mumble, QJsonObject ruumble);

	/** merges Mumble's database into Ruumble Client's (created as a copy when there is none); false with a reason */
	static bool mergeDatabase(const QString &mumble, const QString &ruumble, QString *error);

	/** the migration itself into the given Ruumble Client files; false with a reason, the files are then unchanged */
	static bool migrate(const MumbleSetup &setup, const QString &settingsFile, const QString &databaseFile,
						QString *error);

	/** the file that holds the answer, next to Ruumble Client's settings */
	static QString answerFile(const QString &configDir);
};

} // namespace ruumbledesk
