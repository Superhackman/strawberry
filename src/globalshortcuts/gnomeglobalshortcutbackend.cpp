/*
 * Strawberry Music Player
 * This file was part of Clementine.
 * Copyright 2010, David Sansome <me@davidsansome.com>
 *
 * Strawberry is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Strawberry is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Strawberry.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "config.h"

#ifdef HAVE_DBUS
#  include <dbus/gnomesettingsdaemon.h>
#endif

#include <QAction>
#include <QDateTime>
#include <QMap>
#include <QtDebug>

#ifdef HAVE_DBUS
# include <QCoreApplication>
# include <QDBusConnectionInterface>
# include <QDBusMessage>
# include <QDBusPendingCall>
# include <QDBusPendingReply>
#endif

#include "core/closure.h"
#include "core/logging.h"
#include "globalshortcuts.h"
#include "globalshortcuts/globalshortcutbackend.h"
#include "gnomeglobalshortcutbackend.h"

const char *GnomeGlobalShortcutBackend::kGsdService = "org.gnome.SettingsDaemon";
const char *GnomeGlobalShortcutBackend::kGsdPath = "/org/gnome/SettingsDaemon/MediaKeys";
const char *GnomeGlobalShortcutBackend::kGsdInterface = "org.gnome.SettingsDaemon.MediaKeys";

GnomeGlobalShortcutBackend::GnomeGlobalShortcutBackend(GlobalShortcuts *parent)
    : GlobalShortcutBackend(parent),
      interface_(nullptr),
      is_connected_(false) {}

bool GnomeGlobalShortcutBackend::DoRegister() {

#ifdef HAVE_DBUS
  qLog(Debug) << "registering";
  // Check if the GSD service is available
  if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(kGsdService)) {
    qLog(Warning) << "gnome settings daemon not registered";
    return false;
  }

  if (!interface_) {
    interface_ = new OrgGnomeSettingsDaemonMediaKeysInterface(kGsdService, kGsdPath, QDBusConnection::sessionBus(), this);
  }

  QDBusPendingReply<> reply = interface_->GrabMediaPlayerKeys(QCoreApplication::applicationName(), QDateTime::currentDateTime().toTime_t());

  QDBusPendingCallWatcher *watcher = new QDBusPendingCallWatcher(reply, this);
  NewClosure(watcher, SIGNAL(finished(QDBusPendingCallWatcher*)), this, SLOT(RegisterFinished(QDBusPendingCallWatcher*)), watcher);

  return true;
#else
  qLog(Warning) << "dbus not available";
  return false;
#endif

}

void GnomeGlobalShortcutBackend::RegisterFinished(QDBusPendingCallWatcher *watcher) {

#ifdef HAVE_DBUS
  QDBusMessage reply = watcher->reply();
  watcher->deleteLater();

  if (reply.type() == QDBusMessage::ErrorMessage) {
    qLog(Warning) << "Failed to grab media keys" << reply.errorName() <<reply.errorMessage();
    return;
  }

  connect(interface_, SIGNAL(MediaPlayerKeyPressed(QString, QString)), this, SLOT(GnomeMediaKeyPressed(QString, QString)));
  is_connected_ = true;

  qLog(Debug) << "registered";
#endif

}

void GnomeGlobalShortcutBackend::DoUnregister() {

  qLog(Debug) << "unregister";

#ifdef HAVE_DBUS
  // Check if the GSD service is available
  if (!QDBusConnection::sessionBus().interface()->isServiceRegistered(kGsdService))
    return;
  if (!interface_ || !is_connected_) return;

  is_connected_ = false;

  interface_->ReleaseMediaPlayerKeys(QCoreApplication::applicationName());
  disconnect(interface_, SIGNAL(MediaPlayerKeyPressed(QString, QString)), this, SLOT(GnomeMediaKeyPressed(QString, QString)));
#endif

}

void GnomeGlobalShortcutBackend::GnomeMediaKeyPressed(const QString&, const QString& key) {
  if (key == "Play") manager_->shortcuts()["play_pause"].action->trigger();
  if (key == "Stop") manager_->shortcuts()["stop"].action->trigger();
  if (key == "Next") manager_->shortcuts()["next_track"].action->trigger();
  if (key == "Previous") manager_->shortcuts()["prev_track"].action->trigger();
}

