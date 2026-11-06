// Ruumble Client: take over an installed official Mumble (ticket #15). See MumbleMigration.h.
#include "MumbleMigration.h"

#include <QtCore/QDateTime>
#include <QtCore/QDir>
#include <QtCore/QFile>
#include <QtCore/QFileInfo>
#include <QtCore/QJsonDocument>
#include <QtCore/QLocale>
#include <QtCore/QSaveFile>
#include <QtCore/QStandardPaths>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QMessageBox>
#include <QtWidgets/QPushButton>

namespace ruumbledesk {

namespace {

	const char *const ANSWER_FILE   = "mumble-migration";
	const char *const BACKUP_SUFFIX = ".before-mumble-migration-";

	/** the dialog's texts; Mumble's settings (and with them its language) are not loaded yet: the system decides */
	struct Texts {
		QString title, question, details, overwrite, takeOver, overwriteButton, notNow, doNotAsk, legacy, failed;
	};

	Texts texts() {
		if (QLocale::system().language() == QLocale::German) {
			return { QStringLiteral("Mumble übernehmen"),
					 QStringLiteral("Ruumble Client hat auf diesem Computer eine Mumble-Einrichtung gefunden. "
									"Übernehmen, damit alle deine Server hier so funktionieren wie in Mumble?"),
					 QStringLiteral("Dazu gehören dein Zertifikat (deine Identität auf den Servern), deine Server mit "
									"ihren akzeptierten Zertifikaten und Zugriffscodes sowie deine Einstellungen für "
									"Audio, Tastenkürzel und Netzwerk."),
					 QStringLiteral("Ruumble Client ist auf diesem Computer schon eingerichtet. Sein Zertifikat und "
									"seine Einstellungen werden durch die von Mumble ersetzt; Server, die nur Ruumble "
									"Client kennt, bleiben. Eine Sicherung liegt neben den Einstellungen."),
					 QStringLiteral("Übernehmen"),
					 QStringLiteral("Überschreiben"),
					 QStringLiteral("Nicht jetzt"),
					 QStringLiteral("Nicht mehr fragen"),
					 QStringLiteral("Ruumble Client hat die Einrichtung eines älteren Mumble (1.4 oder älter) gefunden "
									"und kann sie nicht übernehmen. Starte einmal ein aktuelles Mumble, damit es seine "
									"Einrichtung umstellt; danach bietet Ruumble Client die Übernahme beim nächsten "
									"Start an."),
					 QStringLiteral("Die Mumble-Einrichtung konnte nicht übernommen werden: %1\n\nDie Einrichtung von "
									"Ruumble Client ist unverändert.") };
		}
		return { QStringLiteral("Take over Mumble"),
				 QStringLiteral("Ruumble Client found a Mumble setup on this computer. Take it over, so that all your "
								"servers work here as they do in Mumble?"),
				 QStringLiteral("This brings your certificate (your identity on the servers), your servers with their "
								"accepted certificates and access tokens, and your settings for audio, shortcuts and "
								"network."),
				 QStringLiteral("Ruumble Client is already set up on this computer. Its certificate and settings will be "
								"replaced by Mumble's; servers only Ruumble Client knows stay. A backup is kept next to "
								"the settings."),
				 QStringLiteral("Take over"),
				 QStringLiteral("Overwrite"),
				 QStringLiteral("Not now"),
				 QStringLiteral("Do not ask again"),
				 QStringLiteral("Ruumble Client found the setup of an older Mumble (1.4 or older) and cannot take it "
								"over. Start a current Mumble once, so that it updates its setup; Ruumble Client then "
								"offers to take it over on its next start."),
				 QStringLiteral("Mumble's setup could not be taken over: %1\n\nRuumble Client's own setup is "
								"unchanged.") };
	}

	QJsonObject readJson(const QString &path) {
		QFile file(path);
		if (!file.open(QIODevice::ReadOnly)) {
			return {};
		}
		return QJsonDocument::fromJson(file.readAll()).object();
	}

	QString firstExisting(const QStringList &paths) {
		for (const QString &path : paths) {
			if (QFileInfo(path).isFile()) {
				return QFileInfo(path).absoluteFilePath();
			}
		}
		return {};
	}

	void writeAnswer(const QString &configDir, const QString &what) {
		QDir().mkpath(configDir);
		QFile file(MumbleMigration::answerFile(configDir));
		if (file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
			file.write(QStringLiteral("%1 %2\n").arg(QDateTime::currentDateTime().toString(Qt::ISODate), what).toUtf8());
		}
	}

	/** a copy next to the file, named with the time; true when there was nothing to back up */
	bool backUp(const QString &path, const QString &stamp, QString *backup) {
		if (!QFile::exists(path)) {
			return true;
		}
		*backup = path + QLatin1String(BACKUP_SUFFIX) + stamp;
		QFile::remove(*backup);
		return QFile::copy(path, *backup);
	}

	/** the rows of one table and the columns that name the same thing in both databases (Mumble's row replaces it) */
	struct TableRule {
		const char *table;
		QList< QStringList > keys;
	};

	// Database.cpp of Mumble 1.6; caches (comments, blobs, pingcache) are left out
	const QList< TableRule > &tableRules() {
		static const QList< TableRule > rules = {
			{ "servers", { { "hostname", "port" } } },
			{ "cert", { { "hostname", "port" } } },
			{ "tokens", { { "digest" } } },
			{ "shortcut", { { "digest" } } },
			{ "udp", { { "digest" } } },
			{ "friends", { { "hash" }, { "name" } } },
			{ "ignored", { { "hash" } } },
			{ "ignored_tts", { { "hash" } } },
			{ "muted", { { "hash" } } },
			{ "volume", { { "hash" } } },
			{ "nicknames", { { "hash" } } },
			{ "filtered_channels", { { "server_cert_digest", "channel_id" } } },
		};
		return rules;
	}

	QStringList columns(QSqlDatabase &db, const QString &schema, const QString &table) {
		QStringList result;
		QSqlQuery query(db);
		if (query.exec(QStringLiteral("PRAGMA %1.table_info(`%2`)").arg(schema, table))) {
			while (query.next()) {
				result << query.value(1).toString();
			}
		}
		return result;
	}

	bool mergeTables(QSqlDatabase &db, QString *error) {
		QSqlQuery query(db);
		auto run = [&](const QString &sql) {
			if (!query.exec(sql)) {
				*error = query.lastError().text();
				return false;
			}
			return true;
		};
		for (const TableRule &rule : tableRules()) {
			const QString table = QString::fromLatin1(rule.table);
			QSqlQuery schema(db);
			schema.prepare(QStringLiteral("SELECT sql FROM m.sqlite_master WHERE type = 'table' AND name = ?"));
			schema.addBindValue(table);
			if (!schema.exec() || !schema.next()) {
				continue; // Mumble has no such table: nothing to take over
			}
			if (columns(db, QStringLiteral("main"), table).isEmpty() && !run(schema.value(0).toString())) {
				return false;
			}
			const QStringList theirs = columns(db, QStringLiteral("m"), table);
			QStringList shared;
			for (const QString &column : columns(db, QStringLiteral("main"), table)) {
				if (column != QLatin1String("id") && theirs.contains(column)) {
					shared << QStringLiteral("`%1`").arg(column);
				}
			}
			if (shared.isEmpty()) {
				continue;
			}
			for (const QStringList &key : rule.keys) {
				QStringList same;
				for (const QString &column : key) {
					same << QStringLiteral("s.`%1` IS main.`%2`.`%1`").arg(column, table);
				}
				if (!run(QStringLiteral("DELETE FROM main.`%1` WHERE EXISTS (SELECT 1 FROM m.`%1` s WHERE %2)")
							 .arg(table, same.join(QLatin1String(" AND "))))) {
					return false;
				}
			}
			const QString list = shared.join(QLatin1String(", "));
			if (!run(QStringLiteral("INSERT INTO main.`%1` (%2) SELECT %2 FROM m.`%1`").arg(table, list))) {
				return false;
			}
		}
		return true;
	}

} // namespace

QString MumbleMigration::answerFile(const QString &configDir) {
	return QDir(configDir).filePath(QLatin1String(ANSWER_FILE));
}

MumbleSetup MumbleMigration::find(const QString &home, const QString &configHome, const QString &dataHome) {
	// native, Flatpak and Snap; inside each the places Mumble's Settings::findSettingsLocation and
	// Database::findOrCreateDatabase look at, under the organization and application name "Mumble"
	struct Root {
		QString home, config, data;
		bool native;
	};
	const QString flatpak = QDir(home).filePath(QStringLiteral(".var/app/info.mumble.Mumble"));
	const QString snap    = QDir(home).filePath(QStringLiteral("snap/mumble/current"));
	const QList< Root > roots = {
		{ home, configHome, dataHome, true },
		{ flatpak, flatpak + QStringLiteral("/config"), flatpak + QStringLiteral("/data"), false },
		{ snap, snap + QStringLiteral("/.config"), snap + QStringLiteral("/.local/share"), false },
	};

	MumbleSetup legacyOnly;
	for (const Root &root : roots) {
		MumbleSetup setup;
		setup.settings = firstExisting({ root.data + QStringLiteral("/Mumble/Mumble/mumble_settings.json"),
										 root.config + QStringLiteral("/Mumble/Mumble/mumble_settings.json"),
										 root.config + QStringLiteral("/mumble_settings.json") });
		if (!setup.found()) {
			const QString legacy = firstExisting({ root.config + QStringLiteral("/Mumble/Mumble.conf"),
												   root.config + QStringLiteral("/Mumble/mumble.conf"),
												   root.config + QStringLiteral("/Mumble/Mumble.ini"),
												   root.config + QStringLiteral("/Mumble/mumble.ini") });
			if (legacyOnly.legacy.isEmpty()) {
				legacyOnly.legacy = legacy;
			}
			continue;
		}
		QStringList databases;
		const QString configured = readJson(setup.settings)
									   .value(QStringLiteral("misc"))
									   .toObject()
									   .value(QStringLiteral("database_location"))
									   .toString();
		if (!configured.isEmpty()) {
			databases << configured;
		}
		databases << root.data + QStringLiteral("/Mumble/Mumble/mumble.sqlite");
		if (root.native) {
			databases << root.config + QStringLiteral("/Mumble/mumble.sqlite");
		}
		databases << root.home + QStringLiteral("/mumble.sqlite") << root.home + QStringLiteral("/.mumble.sqlite");
		setup.database = firstExisting(databases);
		return setup;
	}
	return legacyOnly;
}

QJsonObject MumbleMigration::mergeSettings(const QJsonObject &mumble, QJsonObject ruumble) {
	// identity: the certificate decides who the user is on every server (and which pairing Ruumble knows)
	const QString certificate = mumble.value(QStringLiteral("certificate")).toString();
	if (!certificate.isEmpty()) {
		ruumble.insert(QStringLiteral("certificate"), certificate);
	}
	// audio: Mumble's values win, except the mute and deaf state of its last session
	QJsonObject audio = ruumble.value(QStringLiteral("audio")).toObject();
	const QJsonObject theirAudio = mumble.value(QStringLiteral("audio")).toObject();
	for (auto it = theirAudio.begin(); it != theirAudio.end(); ++it) {
		if (it.key() != QLatin1String("mute") && it.key() != QLatin1String("deaf")) {
			audio.insert(it.key(), it.value());
		}
	}
	if (!audio.isEmpty()) {
		ruumble.insert(QStringLiteral("audio"), audio);
	}
	// whole groups: the audio devices, positional audio, shortcuts (push-to-talk among them) and the network
	// (TCP only, QoS, reconnect, proxy)
	for (const char *group : { "audio_backend", "positional_audio", "shortcuts", "network" }) {
		const QString key = QString::fromLatin1(group);
		if (mumble.contains(key)) {
			ruumble.insert(key, mumble.value(key));
		}
	}
	// the name the user connects with
	const QString username =
		mumble.value(QStringLiteral("last_connection")).toObject().value(QStringLiteral("username")).toString();
	if (!username.isEmpty()) {
		QJsonObject last = ruumble.value(QStringLiteral("last_connection")).toObject();
		last.insert(QStringLiteral("username"), username);
		ruumble.insert(QStringLiteral("last_connection"), last);
	}
	// audio set up already: no audio wizard on the first start
	if (mumble.value(QStringLiteral("misc")).toObject().value(QStringLiteral("audio_wizard_has_been_shown")).toBool()) {
		QJsonObject misc = ruumble.value(QStringLiteral("misc")).toObject();
		misc.insert(QStringLiteral("audio_wizard_has_been_shown"), true);
		ruumble.insert(QStringLiteral("misc"), misc);
	}
	if (!ruumble.contains(QStringLiteral("settings_version"))) {
		ruumble.insert(QStringLiteral("settings_version"), mumble.value(QStringLiteral("settings_version")).toInt(1));
	}
	return ruumble;
}

bool MumbleMigration::mergeDatabase(const QString &mumble, const QString &ruumble, QString *error) {
	QDir().mkpath(QFileInfo(ruumble).absolutePath());
	if (!QFile::exists(ruumble)) {
		if (!QFile::copy(mumble, ruumble)) {
			*error = QStringLiteral("could not copy %1 to %2").arg(mumble, ruumble);
			return false;
		}
		QFile(ruumble).setPermissions(QFile::ReadOwner | QFile::WriteOwner);
		return true;
	}
	const QString name = QStringLiteral("ruumble-mumble-migration");
	bool ok            = false;
	{
		QSqlDatabase db = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
		db.setDatabaseName(ruumble);
		if (!db.open()) {
			*error = db.lastError().text();
		} else {
			QSqlQuery attach(db);
			attach.prepare(QStringLiteral("ATTACH DATABASE ? AS m"));
			attach.addBindValue(mumble);
			if (!attach.exec()) {
				*error = attach.lastError().text();
			} else {
				db.transaction();
				ok = mergeTables(db, error);
				if (ok) {
					ok = db.commit();
					if (!ok) {
						*error = db.lastError().text();
					}
				} else {
					db.rollback();
				}
				QSqlQuery(db).exec(QStringLiteral("DETACH DATABASE m"));
			}
			db.close();
		}
	}
	QSqlDatabase::removeDatabase(name);
	return ok;
}

bool MumbleMigration::migrate(const MumbleSetup &setup, const QString &settingsFile, const QString &databaseFile,
							  QString *error) {
	const QJsonObject mumble = readJson(setup.settings);
	if (mumble.isEmpty()) {
		*error = QStringLiteral("%1 could not be read").arg(setup.settings);
		return false;
	}
	const QString stamp = QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"));
	QString settingsBackup, databaseBackup;
	if (!backUp(settingsFile, stamp, &settingsBackup) || !backUp(databaseFile, stamp, &databaseBackup)) {
		*error = QStringLiteral("no backup could be made next to %1").arg(settingsFile);
		return false;
	}
	const bool hadDatabase = QFile::exists(databaseFile);
	if (!setup.database.isEmpty() && !mergeDatabase(setup.database, databaseFile, error)) {
		return false; // one transaction: Ruumble Client's database is as it was
	}
	const QJsonObject merged = mergeSettings(mumble, readJson(settingsFile));
	QDir().mkpath(QFileInfo(settingsFile).absolutePath());
	QSaveFile out(settingsFile);
	if (!out.open(QIODevice::WriteOnly) || out.write(QJsonDocument(merged).toJson(QJsonDocument::Indented)) < 0
		|| !out.commit()) {
		*error = QStringLiteral("%1 could not be written").arg(settingsFile);
		// keep both files consistent: the database goes back to how it was
		QFile::remove(databaseFile);
		if (hadDatabase) {
			QFile::copy(databaseBackup, databaseFile);
		}
		return false;
	}
	return true;
}

void MumbleMigration::run() {
	const QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
	if (QFile::exists(answerFile(configDir))) {
		return;
	}
	const MumbleSetup setup =
		find(QDir::homePath(), QStandardPaths::writableLocation(QStandardPaths::GenericConfigLocation),
			 QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation));
	if (!setup.found() && setup.legacy.isEmpty()) {
		return; // no Mumble here: nothing to ask, and nothing to remember (a later Mumble is offered then)
	}

	const QString settingsFile = QDir(configDir).filePath(QStringLiteral("mumble_settings.json"));
	// Ruumble Client's database: where its settings point to, else where Mumble's Global puts the data (qdBasePath)
	QString databaseFile =
		readJson(settingsFile).value(QStringLiteral("misc")).toObject().value(QStringLiteral("database_location")).toString();
	if (databaseFile.isEmpty()) {
		databaseFile = QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
						   .filePath(QStringLiteral("mumble.sqlite"));
	}
	const bool existing = QFile::exists(settingsFile);
	const Texts text    = texts();

	// headless tests answer without the dialog
	const QString preset = qEnvironmentVariable("RUUMBLE_MUMBLE_MIGRATION");
	bool takeOver = false, doNotAsk = false;
	if (!preset.isEmpty()) {
		takeOver = preset == QLatin1String("take");
		doNotAsk = preset == QLatin1String("never");
	} else {
		QMessageBox box(setup.found() ? QMessageBox::Question : QMessageBox::Information, text.title,
						setup.found() ? text.question : text.legacy);
		if (setup.found()) {
			box.setInformativeText(existing ? text.details + QLatin1String("\n\n") + text.overwrite : text.details);
		}
		QPushButton *yes =
			setup.found() ? box.addButton(existing ? text.overwriteButton : text.takeOver, QMessageBox::AcceptRole)
						  : nullptr;
		QPushButton *no = box.addButton(setup.found() ? text.notNow : QStringLiteral("OK"), QMessageBox::RejectRole);
		box.setDefaultButton(yes ? yes : no);
		auto *check = new QCheckBox(text.doNotAsk);
		box.setCheckBox(check);
		box.exec();
		takeOver = yes && box.clickedButton() == yes;
		doNotAsk = check->isChecked();
	}

	if (!setup.found()) {
		qInfo("Ruumble Client: only an older Mumble setup was found (%s), it cannot be taken over",
			  qPrintable(setup.legacy));
		if (doNotAsk) {
			writeAnswer(configDir, QStringLiteral("do not ask again (older Mumble setup at ") + setup.legacy + ')');
		}
		return;
	}
	if (takeOver) {
		QString error;
		if (migrate(setup, settingsFile, databaseFile, &error)) {
			writeAnswer(configDir, QStringLiteral("taken over from %1 and %2").arg(setup.settings, setup.database));
			qInfo("Ruumble Client: took over Mumble's setup from %s and %s", qPrintable(setup.settings),
				  qPrintable(setup.database));
			return;
		}
		qWarning("Ruumble Client: Mumble's setup was not taken over: %s", qPrintable(error));
		if (preset.isEmpty()) {
			QMessageBox::warning(nullptr, text.title, text.failed.arg(error));
		}
	}
	if (doNotAsk) {
		writeAnswer(configDir, QStringLiteral("do not ask again (Mumble setup at ") + setup.settings + ')');
	}
}

} // namespace ruumbledesk
