#pragma once

#include "artistview.hpp"
#include "audiofeaturesview.hpp"
#include "darkpalette.hpp"
#include "icon.hpp"
#include "lyricsview.hpp"
#include "mainmenu.hpp"
#include "mediaplayer/service.hpp"
#include "playlistmenu.hpp"
#include "searchview.hpp"
#include "settingsdialog.hpp"
#include "songmenu.hpp"
#include "spotify/audiofeatures.hpp"
#include "spotify/clienthandler.hpp"
#include "spotify/playback.hpp"
#include "spotify/playlist.hpp"
#include "spotify/spotify.hpp"
#include "spotify/user.hpp"
#include "spotify/webplayer.hpp"
#include "trayicon.hpp"
#include "volumebutton.hpp"
#include "whatsnewdialog.hpp"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QDockWidget>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QLabel>
#include <QListWidget>
#include <QListWidget>
#include <QMainWindow>
#include <QMapIterator>
#include <QMenu>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QSlider>
#include <QSplitter>
#include <QStatusBar>
#include <QTextEdit>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>

class MainWindow : public QMainWindow
{
	Q_OBJECT
public:
	MainWindow(Settings &settings, QWidget *parent = nullptr);
	~MainWindow() override;

protected:
	void closeEvent(QCloseEvent *event) override;

public:
	static void applyPalette(Settings::Palette palette);
	static bool hasDarkBackground();
	// Constants for track info
	enum DataRole {
		RoleTrackId		= 0x0100,	// 256
		RoleArtistId	= 0x0101,	// 257
		RoleAlbumId		= 0x0102,	// 258
		RoleIndex		= 0x0103,	// 259
		RolePlaylistId	= 0x0104	// 260
	};
	enum class MaskShape {
		App,
		Pie
	};
	QByteArray get(const QString &url);
	QJsonDocument getJson(const QString &url);
	void setStatus(const QString &message, bool important = false);
	bool loadAlbum(const QString &albumId, bool ignoreEmpty = true);
	void openArtist(const QString &artistId);
	static QWidget *layoutToWidget(QLayout *layout);
	QPixmap getAlbum(const QString &url);
	QPixmap getImage(const QString &type, const QString &url);
	void refreshPlaylist(spt::Playlist &playlist);
	bool loadPlaylist(spt::Playlist &playlist);
	spt::Playback currentPlayback();
	void openAudioFeaturesWidget(const QString &trackId, const QString &artist, const QString &name);
	void openLyrics(const QString &artist, const QString &name);
	bool hasPlaylistSelected();
	QVector<spt::Track> playlistTracks(const QString &playlistId);
	QString currentLibraryItem();
	void reloadTrayIcon();
	spt::User getCurrentUser();
	void refreshPlaylists();
	void setFixedWidthTime(bool value);
	static QPixmap mask(const QPixmap &source, MaskShape shape = MaskShape::App, const QVariant &data = QVariant());
	// I know these should be methods, I'm just lazy
	QString					cacheLocation;
	QVector<spt::Playlist>	*sptPlaylists;
	QListWidget				*playlists;
	QAction					*search;
	QTreeWidget				*songs;
	// What Spotify context we're currently in
	QString sptContext;
private:
	// Widgets
	QLabel		*nowPlaying,	*position,	*nowAlbum;
	QSlider		*progress;
	QAction		*playPause,		*repeat,	*shuffle;
	VolumeButton	*volumeButton;
	// Everything else
	spt::Spotify			*spotify;
	QNetworkAccessManager	*network;
	spt::ClientHandler		*sptClient;
	QDockWidget				*searchView;
	spt::Playback			current;
	mp::Service				*mediaPlayer;
	QTreeWidget				*libraryList;
	QDockWidget				*artistView;
	AudioFeaturesView		*audioFeaturesView;
	LyricsView				*lyricsView;
	TrayIcon				*trayIcon;
	static bool				darkBackground;
	int						refreshCount;
	QTreeWidgetItem			*playingTrackItem;
	spt::User				currentUser;
	Settings				&settings;
	QHash<QString, QTreeWidgetItem*>	trackItems;
	// Methods
	QWidget *createCentralWidget();
	QToolBar *createToolBar();
	bool loadSongs(const QVector<spt::Track> &tracks);
	void setAlbumImage(const QString &url);
	static QString formatTime(int ms);
	void refresh();
	void refreshed(const spt::Playback &playback);
	void cachePlaylist(spt::Playlist &playlist);
	bool loadPlaylistFromCache(spt::Playlist &playlist);
	QMenu *songMenu(const QString &trackId, const QString &artist,
		const QString &name, const QString &artistId, const QString &albumId);
	QTreeWidgetItem *treeItem(QTreeWidget *tree, const QString &name, const QString &toolTip = QString::Null(), const QStringList &childrenItems = QStringList(QString::Null()));
	QStringList currentTracks();
	void setPlayingTrackItem(QTreeWidgetItem *item);
	QSet<QString> allArtists();
};
