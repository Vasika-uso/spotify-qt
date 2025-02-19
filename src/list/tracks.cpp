#include "list/tracks.hpp"
#include "mainwindow.hpp"
#include "dialog/createplaylist.hpp"

#include <QShortcut>

List::Tracks::Tracks(lib::spt::api &spotify, lib::settings &settings, lib::cache &cache,
	QWidget *parent)
	: QTreeWidget(parent),
	settings(settings),
	cache(cache),
	spotify(spotify)
{
	constexpr int emptyPixmapSize = 64;
	constexpr int columnCount = 5;

	// Empty icon used as replacement for play icon
	QPixmap emptyPixmap(emptyPixmapSize, emptyPixmapSize);
	emptyPixmap.fill(Qt::transparent);
	emptyIcon = QIcon(emptyPixmap);

	setEditTriggers(QAbstractItemView::NoEditTriggers);
	setSelectionBehavior(QAbstractItemView::SelectRows);
	setSelectionMode(QAbstractItemView::ExtendedSelection);
	setSortingEnabled(true);
	setRootIsDecorated(false);
	setAllColumnsShowFocus(true);
	setColumnCount(columnCount);
	setHeaderLabels({
		settings.general.track_numbers == lib::spotify_context::all ? "#" : "",
		"Title", "Artist", "Album", "Length", "Added"
	});
	header()->setSectionsMovable(false);
	header()->setSortIndicator(settings.general.song_header_sort_by + 1, Qt::AscendingOrder);

	updateResizeMode(settings.general.track_list_resize_mode);

	// Hide specified columns
	for (const auto &value: settings.general.hidden_song_headers)
	{
		header()->setSectionHidden(value + 1, true);
	}

	// Play tracks on click or enter/special key
	QTreeWidget::connect(this, &QTreeWidget::itemDoubleClicked,
		this, &List::Tracks::onDoubleClicked);

	// Song context menu
	setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	QWidget::connect(this, &QWidget::customContextMenuRequested,
		this, &List::Tracks::onMenu);

	// Songs header context menu
	header()->setContextMenuPolicy(Qt::ContextMenuPolicy::CustomContextMenu);
	QLabel::connect(header(), &QWidget::customContextMenuRequested,
		this, &List::Tracks::onHeaderMenu);

	QShortcut::connect(new QShortcut(static_cast<int>(Shortcut::NewPlaylist), this),
		&QShortcut::activated, this, &List::Tracks::onNewPlaylist);

	auto *deleteTrack = new QShortcut(static_cast<int>(Shortcut::Delete), this);
	deleteTrack->setContext(Qt::WidgetShortcut);

	QShortcut::connect(deleteTrack, &QShortcut::activated,
		this, &List::Tracks::onDelete);

	auto *playSelectedRow = new QShortcut(static_cast<int>(Shortcut::PlaySelectedRow), this);
	playSelectedRow->setContext(Qt::WidgetShortcut);

	QShortcut::connect(playSelectedRow, &QShortcut::activated,
		this, &List::Tracks::onPlaySelectedRow);
}

void List::Tracks::onMenu(const QPoint &pos)
{
	const auto &items = selectedItems();

	QList<PlaylistTrack> tracks;
	tracks.reserve(items.size());

	for (const auto *item: items)
	{
		const auto track = item->data(0, static_cast<int>(DataRole::Track))
			.value<lib::spt::track>();

		if (!track.is_valid())
		{
			continue;
		}

		auto index = item->data(0, static_cast<int>(DataRole::Index)).toInt();
		tracks.push_back(PlaylistTrack(index, track));
	}

	if (tracks.isEmpty())
	{
		return;
	}

	auto *songMenu = new Menu::Track(tracks, spotify, cache, parentWidget());
	songMenu->popup(mapToGlobal(pos));
}

void List::Tracks::onDoubleClicked(QTreeWidgetItem *item, int /*column*/)
{
	if (item->isDisabled())
	{
		return;
	}

	auto *mainWindow = MainWindow::find(parentWidget());

	bool indexFound;
	auto trackIndex = item->data(0, static_cast<int>(DataRole::Index)).toInt(&indexFound);
	if (!indexFound)
	{
		StatusMessage::error(QStringLiteral("Failed to start playback: track not found"));
		return;
	}

	auto callback = [this, mainWindow, item](const std::string &status)
	{
		if (!status.empty())
		{
			StatusMessage::error(QString("Failed to start playback: %1")
				.arg(QString::fromStdString(status)));
		}
		else
		{
			this->setPlayingTrackItem(item);
		}

		mainWindow->refresh();
	};

	const auto &context = mainWindow->getSptContext();
	if (context.empty())
	{
		auto allTracks = mainWindow->currentTracks();
		this->spotify.play_tracks(currentIndex().row(), allTracks, callback);
	}
	else
	{
		this->spotify.play_tracks(trackIndex, context, callback);
	}
}

void List::Tracks::onHeaderMenu(const QPoint &pos)
{
	auto *menu = new QMenu(header());
	auto *showHeaders = menu->addMenu(Icon::get("visibility"), "Columns to show");
	auto *sortBy = menu->addMenu(Icon::get("view-sort-ascending"), "Default sorting");
	QStringList headerTitles({
		"Title", "Artist", "Album", "Length", "Added"
	});
	const auto &headers = this->settings.general.hidden_song_headers;

	for (int i = 0; i < headerTitles.size(); i++)
	{
		auto *showTitle = showHeaders->addAction(headerTitles.at(i));
		showTitle->setCheckable(true);
		showTitle->setChecked(!lib::set::contains(headers, i));
		showTitle->setData(QVariant(i));

		auto *sortTitle = sortBy->addAction(headerTitles.at(i));
		sortTitle->setCheckable(true);
		sortTitle->setChecked(i == this->settings.general.song_header_sort_by);
		sortTitle->setData(QVariant(titleOffset + i));
	}

	QMenu::connect(menu, &QMenu::triggered, this, &List::Tracks::onHeaderMenuTriggered);

	menu->popup(header()->mapToGlobal(pos));
}

void List::Tracks::onHeaderMenuTriggered(QAction *action)
{
	int index = action->data().toInt();

	// Columns to show
	if (index < titleOffset)
	{
		header()->setSectionHidden(index + 1, !action->isChecked());
		if (action->isChecked())
		{
			settings.general.hidden_song_headers.erase(index);
		}
		else
		{
			settings.general.hidden_song_headers.emplace(index);
		}
		settings.save();
		return;
	}

	// Sort by
	index -= titleOffset;
	if (settings.general.song_header_sort_by == index)
	{
		index = -1;
	}
	header()->setSortIndicator(index + 1, Qt::AscendingOrder);
	settings.general.song_header_sort_by = index;
	settings.save();
}

void List::Tracks::onNewPlaylist()
{
	const auto trackIds = getSelectedTrackIds();
	auto *dialog = new Dialog::CreatePlaylist(trackIds, spotify, window());
	dialog->show();
}

void List::Tracks::onDelete()
{
	auto *mainWindow = qobject_cast<MainWindow *>(window());
	if (mainWindow == nullptr)
	{
		return;
	}

	auto *playlistItem = mainWindow->getCurrentPlaylistItem();
	if (playlistItem == nullptr)
	{
		return;
	}

	const auto playlistId = playlistItem->data(static_cast<int>(DataRole::PlaylistId))
		.toString().toStdString();

	if (playlistId.empty())
	{
		return;
	}

	const auto items = selectedItems();
	if (items.empty())
	{
		return;
	}

	std::vector<std::pair<int, std::string>> tracks;
	tracks.reserve(items.size());

	for (const auto *item: items)
	{
		const auto data = item->data(0, static_cast<int>(DataRole::Track));
		const auto track = data.value<lib::spt::track>();

		if (!track.is_valid())
		{
			continue;
		}

		auto index = item->data(0, static_cast<int>(DataRole::Index)).toInt();
		tracks.emplace_back(index, track.id);
	}

	spotify.remove_from_playlist(playlistId, tracks,
		[this, items, playlistId](const std::string &status)
		{
			if (!status.empty())
			{
				StatusMessage::error(QString("Failed to remove track from playlist: %1")
					.arg(QString::fromStdString(status)));
				return;
			}

			for (auto *item: items)
			{
				takeTopLevelItem(indexOfTopLevelItem(item));
			}
		});
}

void List::Tracks::onPlaySelectedRow()
{
	const auto &items = selectedItems();
	if (items.length() == 1)
	{
		onDoubleClicked(items.first(), 0);
	}
}

void List::Tracks::resizeEvent(QResizeEvent *event)
{
	if (settings.general.track_list_resize_mode != lib::resize_mode::auto_size)
	{
		return;
	}

	resizeHeaders(event->size());
}

void List::Tracks::resizeHeaders(const QSize &newSize)
{
	constexpr int indexSize = 60;
	constexpr int lengthSize = 70;
	constexpr int addedSize = 140;
	constexpr int minSize = 60;
	constexpr int columnCount = 7;

	auto size = (newSize.width() - indexSize - lengthSize - addedSize) / columnCount;

	if (size < minSize)
	{
		size = minSize;
	}

	header()->resizeSection(static_cast<int>(Column::Index), indexSize);
	header()->resizeSection(static_cast<int>(Column::Title), size * 3);
	header()->resizeSection(static_cast<int>(Column::Artist), size * 2);
	header()->resizeSection(static_cast<int>(Column::Album), size * 2);
	header()->resizeSection(static_cast<int>(Column::Length), lengthSize);
	header()->resizeSection(static_cast<int>(Column::Added), addedSize);
}

void List::Tracks::updateResizeMode(lib::resize_mode mode)
{
	QHeaderView::ResizeMode resizeMode;

	switch (mode)
	{
		case lib::resize_mode::auto_size:
			resizeMode = QHeaderView::Fixed;
			break;

		case lib::resize_mode::fit_content:
			resizeMode = QHeaderView::ResizeToContents;
			break;

		case lib::resize_mode::custom:
			resizeMode = QHeaderView::Interactive;
			break;

		default:
			resizeMode = header()->sectionResizeMode(0);
			break;
	}

	header()->setSectionResizeMode(resizeMode);
	resizeHeaders(size());
}

auto List::Tracks::getAddedText(const std::string &date) const -> QString
{
	if (date.empty())
	{
		return {};
	}

	if (settings.general.relative_added)
	{
		return DateTime::toRelative(date);
	}

	const auto locale = QLocale::system();
	const auto parsed = DateTime::parseIsoDate(date).date();

	return parsed.isValid()
		? locale.toString(parsed, QLocale::ShortFormat)
		: QString();
}

auto List::Tracks::getSelectedTrackIds() const -> std::vector<std::string>
{
	const auto &items = selectedItems();

	std::vector<std::string> trackIds;
	trackIds.reserve(items.size());

	for (const auto *item: items)
	{
		const auto data = item->data(0, static_cast<int>(DataRole::Track));
		const auto track = data.value<lib::spt::track>();

		if (track.is_valid())
		{
			trackIds.push_back(track.id);
		}
	}

	return trackIds;
}

void List::Tracks::load(const std::vector<lib::spt::track> &tracks,
	const std::string &selectedId, const std::string &addedAt)
{
	clear();
	trackItems.clear();
	playingTrackItem = nullptr;
	auto fieldWidth = static_cast<int>(std::to_string(tracks.size()).size());
	auto current = getCurrent();
	auto anyHasDate = false;

	setSortingEnabled(false);

	for (size_t i = 0; i < tracks.size(); i++)
	{
		const auto index = static_cast<int>(i);
		const auto &track = tracks.at(i);

		const auto &added = track.added_at.empty() && !addedAt.empty()
			? addedAt
			: track.added_at;

		auto *item = new ListItem::Track({
			settings.general.track_numbers == lib::spotify_context::all
				? QString("%1").arg(i + 1, fieldWidth)
				: QString(),
			QString::fromStdString(track.name),
			QString::fromStdString(lib::spt::entity::combine_names(track.artists)),
			QString::fromStdString(track.album.name),
			QString::fromStdString(lib::format::time(track.duration)),
			getAddedText(added),
		}, track, emptyIcon, index);

		if (!anyHasDate && !added.empty())
		{
			anyHasDate = true;
		}

		if (track.id == current.playback.item.id)
		{
			setPlayingTrackItem(item);
		}

		insertTopLevelItem(index, item);
		trackItems[track.id] = item;

		if (!selectedId.empty() && track.id == selectedId)
		{
			setCurrentItem(item);
		}
	}

	setSortingEnabled(true);

	header()->setSectionHidden(static_cast<int>(Column::Added), !anyHasDate
		|| lib::set::contains(settings.general.hidden_song_headers,
			static_cast<int>(Column::Added)));
}

void List::Tracks::load(const std::vector<lib::spt::track> &tracks)
{
	load(tracks, std::string());
}

void List::Tracks::load(const std::vector<lib::spt::track> &tracks, const std::string &selectedId)
{
	load(tracks, selectedId, std::string());
}

void List::Tracks::load(const lib::spt::playlist &playlist)
{
	const auto &tracks = playlist.tracks.empty()
		? cache.get_playlist(playlist.id).tracks
		: playlist.tracks;

	if (!tracks.empty())
	{
		load(tracks);
	}
	else
	{
		setEnabled(false);
	}

	auto *mainWindow = MainWindow::find(parentWidget());

	const auto &snapshot = playlist.snapshot;
	spotify.playlist(playlist.id,
		[this, snapshot, mainWindow](const lib::spt::playlist &loadedPlaylist)
		{
			const auto &currentUser = mainWindow != nullptr
				? mainWindow->getCurrentUser()
				: lib::spt::user();

			if (this->isEnabled()
				&& this->topLevelItemCount() == loadedPlaylist.tracks_total
				&& loadedPlaylist.is_up_to_date(snapshot, currentUser))
			{
				return;
			}
			this->refreshPlaylist(loadedPlaylist);
		});

	if (mainWindow != nullptr)
	{
		mainWindow->setSptContext(playlist);
	}

	settings.general.last_playlist = playlist.id;
	settings.save();
}

void List::Tracks::refreshPlaylist(const lib::spt::playlist &playlist)
{
	auto *mainWindow = MainWindow::find(parentWidget());
	if (lib::spt::api::to_uri("playlist", playlist.id) != mainWindow->getSptContext())
	{
		return;
	}

	spotify.playlist_tracks(playlist,
		[this, playlist](const std::vector<lib::spt::track> &tracks)
		{
			auto newPlaylist = playlist;
			newPlaylist.tracks = tracks;
			this->load(newPlaylist.tracks);
			this->setEnabled(true);
			this->cache.set_playlist(newPlaylist);
		});
}

void List::Tracks::load(const lib::spt::album &album, const std::string &trackId)
{
	auto tracks = cache.get_tracks(album.id);
	if (!tracks.empty())
	{
		load(tracks, trackId, album.release_date);
	}
	else
	{
		setEnabled(false);
	}

	spotify.album_tracks(album,
		[this, album, trackId](const std::vector<lib::spt::track> &tracks)
		{
			this->load(tracks, trackId, album.release_date);
			this->setEnabled(true);

			cache.set_tracks(album.id, tracks);
			cache.set_album(album);

			auto *mainWindow = MainWindow::find(this->parentWidget());
			if (mainWindow != nullptr)
			{
				mainWindow->setSptContext(album);
			}
		});
}

void List::Tracks::setPlayingTrackItem(QTreeWidgetItem *item)
{
	if (playingTrackItem != nullptr)
	{
		playingTrackItem->setIcon(0, emptyIcon);
	}

	if (item == nullptr)
	{
		playingTrackItem = nullptr;
		return;
	}
	item->setIcon(0, Icon::get("media-playback-start"));
	playingTrackItem = item;
}

void List::Tracks::setPlayingTrackItem(const std::string &itemId)
{
	setPlayingTrackItem(trackItems.find(itemId) != trackItems.end()
		? trackItems[itemId]
		: nullptr);
}

auto List::Tracks::getCurrent() -> const spt::Current &
{
	auto *mainWindow = MainWindow::find(parentWidget());
	return mainWindow->getCurrent();
}
